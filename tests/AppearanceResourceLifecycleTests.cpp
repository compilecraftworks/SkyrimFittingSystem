// Production event routing, dye cleanup/build fencing and queued restoration.
// Fake engine and counted shared GPU ownership; no game/D3D rendering claim.
#include "native/ActorResourceWork.h"
#include <algorithm>
#include <atomic>
#include <barrier>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static void Check(bool ok, const char *message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
namespace RE {
using FormID = std::uint32_t;
struct Actor {
  FormID id; bool loaded{true};
  FormID GetFormID() const { return id; }
  bool Is3DLoaded() const { return loaded; }
};
struct TESForm {
  static inline std::unordered_map<FormID, Actor *> forms;
  template<class T> static T *LookupByID(FormID id) {
    auto it = forms.find(id); return it == forms.end() ? nullptr : it->second;
  }
};
enum class BSEventNotifyControl { kContinue };
template<class T> struct BSTEventSource {};
template<class T> struct BSTEventSink {
  virtual BSEventNotifyControl ProcessEvent(const T *, BSTEventSource<T> *) = 0;
};
struct TESObjectLoadedEvent { FormID formID; bool loaded; };
struct TESFormDeleteEvent { FormID formID; };
struct ScriptEventSourceHolder {
  static inline bool available{true};
  static ScriptEventSourceHolder *GetSingleton() { static ScriptEventSourceHolder source; return available ? &source : nullptr; }
  unsigned registrations{};
  template<class T> void AddEventSink(BSTEventSink<T> *) { ++registrations; }
};
}
namespace SKSE {
struct Tasks {
  std::vector<std::function<void()>> pending;
  void AddTask(std::function<void()> task) { pending.push_back(std::move(task)); }
  void Drain() {
    auto batch = std::move(pending); pending.clear();
    for (auto &task : batch) { task(); }
  }
} tasks;
Tasks *GetTaskInterface() { return &tasks; }
}
struct CountedResource {
  static inline unsigned alive{};
  CountedResource() { ++alive; }
  ~CountedResource() { --alive; }
};
namespace sfs::native::dye {
struct Target { RE::FormID actorFormID; std::shared_ptr<CountedResource> view; };
struct Preview { Target target; };
std::mutex g_worldTintMutex, g_savedWorldTintMutex, g_savedWorldTintRestoreQueueMutex;
std::unordered_map<std::uintptr_t, Target> g_worldTintTargets;
std::unordered_set<RE::FormID> g_worldTintActors;
resource_work::ActorBuilds g_worldTintBuilds;
std::atomic_bool g_worldTintTargetsActive{}, g_worldTintPreviewActive{};
std::optional<Preview> g_worldTintPreview;
std::unordered_map<RE::FormID, std::unordered_map<RE::FormID, int>> g_savedWorldTints;
resource_work::ActorTasks g_queuedSavedWorldTintRestores;
std::atomic<std::uint64_t> g_savedWorldTintRestoreEpoch{0};
std::unordered_map<RE::FormID, unsigned> restores;
void RestoreSavedWorldTintsForActor(RE::Actor *, std::uint64_t);
#include "DyeResourceLifecycle.production.inc"
void RestoreSavedWorldTintsForActor(RE::Actor *actor, std::uint64_t ticket) {
  WorldTintBuild build(actor->id, ticket);
  if (!build.Started()) { return; }
  std::lock_guard lock(g_worldTintMutex);
  if (!build.CurrentLocked()) { return; }
  ++restores[actor->id];
  g_worldTintTargets[actor->id] = {actor->id, std::make_shared<CountedResource>()};
  UpdateWorldTintActivityLocked();
}
}
namespace sfs::native::racemenu {
std::vector<std::pair<RE::FormID, bool>> releases;
void ReleaseActorSceneResources(RE::FormID id, bool deleted) { releases.emplace_back(id, deleted); }
}
namespace sfs::native::appearance_resources {
#include "AppearanceResourceEvents.production.inc"
}
static void DrainAll() {
  for (unsigned i = 0; i != 8 && !SKSE::tasks.pending.empty(); ++i) { SKSE::tasks.Drain(); }
  Check(SKSE::tasks.pending.empty(), "bounded tasks must end without polling");
}
int main() {
  using namespace sfs::native::dye;
  using namespace sfs::native::appearance_resources;
  RE::ScriptEventSourceHolder::available = false; RegisterEvents();
  RE::ScriptEventSourceHolder::available = true; RegisterEvents(); RegisterEvents();
  Check(RE::ScriptEventSourceHolder::GetSingleton()->registrations == 2, "register both events once, retry unavailable source");
  Events events;
  events.ProcessEvent(static_cast<const RE::TESObjectLoadedEvent *>(nullptr), nullptr);
  events.ProcessEvent(static_cast<const RE::TESFormDeleteEvent *>(nullptr), nullptr);
  RE::TESObjectLoadedEvent zero{0, false}; events.ProcessEvent(&zero, nullptr);
  Check(sfs::native::racemenu::releases.empty(), "ignore empty lifecycle input");

  RE::Actor actor{0x14}, other{0xA1};
  RE::TESForm::forms[actor.id] = &actor; RE::TESForm::forms[other.id] = &other;
  g_savedWorldTints[actor.id][0x800] = 1;
  g_savedWorldTints[other.id][0x800] = 2;
  const RE::TESObjectLoadedEvent load{actor.id, true}, unload{actor.id, false};
  const RE::TESFormDeleteEvent deleted{actor.id};
  RestoreActorSceneResources(other.id); DrainAll();
  auto otherResource = g_worldTintTargets.at(other.id).view;
  const auto otherCount = restores[other.id];
  for (unsigned cycle = 0; cycle != 128; ++cycle) {
    actor.loaded = true;
    events.ProcessEvent(&load, nullptr); DrainAll();
    Check(g_worldTintTargets.contains(actor.id), "load restores saved colors");
    auto activePass = g_worldTintTargets.at(actor.id).view;
    std::weak_ptr<CountedResource> releasedView = activePass;
    g_worldTintPreview = Preview{{actor.id, std::make_shared<CountedResource>()}};
    g_worldTintPreviewActive = true;
    // Keep another component build in flight through the unload boundary.
    WorldTintBuild oldBuild(actor.id);
    actor.loaded = false; events.ProcessEvent(&unload, nullptr);
    Check(!g_worldTintTargets.contains(actor.id) && !g_worldTintActors.contains(actor.id) &&
          !g_worldTintPreview && !g_worldTintPreviewActive && !oldBuild.CurrentLocked(),
          "unload releases own targets/preview and cancels in-flight builds");
    Check(!releasedView.expired(), "in-progress renderer pass retains its own restore reference");
    activePass.reset(); Check(releasedView.expired(), "last render-pass reference releases resource");
    Check(g_savedWorldTints.at(actor.id).at(0x800) == 1 &&
          g_worldTintTargets.at(other.id).view == otherResource && restores[other.id] == otherCount,
          "unload preserves saved colors and another actor");
    // Old task completion cannot erase the replacement task after same-ID load.
    actor.loaded = true; QueueSavedWorldTintRestore(&actor);
    actor.loaded = false; events.ProcessEvent(&unload, nullptr);
    actor.loaded = true; events.ProcessEvent(&load, nullptr); DrainAll();
    Check(g_worldTintTargets.contains(actor.id) && g_queuedSavedWorldTintRestores.Size() == 0,
          "same-ID load survives stale queued restore");
  }
  Check(g_worldTintBuilds.Size() == 0, "completed/cancelled builds retain no ticket map entries");
  {
    std::barrier rendezvous{2};
    bool oldCancelled = false;
    std::thread worker([&] {
      WorldTintBuild old(actor.id);
      rendezvous.arrive_and_wait();
      rendezvous.arrive_and_wait();
      std::lock_guard lock(g_worldTintMutex);
      oldCancelled = !old.CurrentLocked();
    });
    rendezvous.arrive_and_wait();
    events.ProcessEvent(&unload, nullptr);
    WorldTintBuild replacement(actor.id);
    rendezvous.arrive_and_wait(); worker.join();
    Check(oldCancelled && replacement.CurrentLocked(),
          "in-flight worker cancellation and old destructor cannot invalidate a replacement build");
  }
  const auto queued = SKSE::tasks.pending.size();
  for (RE::FormID id = 0x1000; id != 0x1100; ++id) {
    RE::TESObjectLoadedEvent unrelated{id, true}; events.ProcessEvent(&unrelated, nullptr);
  }
  Check(SKSE::tasks.pending.size() == queued, "unrelated object loads schedule no work");
  {
    WorldTintBuild first(actor.id), second(actor.id), peer(other.id);
    Check(first.CurrentLocked() && second.CurrentLocked(), "component builds do not invalidate each other");
    events.ProcessEvent(&unload, nullptr);
    Check(!first.CurrentLocked() && !second.CurrentLocked() && peer.CurrentLocked(), "cancellation is actor-local");
  }
  g_worldTintPreview = Preview{{other.id, std::make_shared<CountedResource>()}};
  g_worldTintPreviewActive = true;
  QueueSavedWorldTintRestore(&actor);
  RE::TESForm::forms.erase(actor.id); events.ProcessEvent(&deleted, nullptr); DrainAll();
  Check(g_worldTintPreview && g_worldTintPreviewActive && g_savedWorldTints.contains(actor.id),
        "form deletion keeps another actor preview and durable recipes");
  Check(sfs::native::racemenu::releases.back() == std::pair<RE::FormID, bool>{actor.id, true},
        "delete routes by ID without form lookup");
  ReleaseActorSceneResources(other.id); otherResource.reset();
  Check(!g_worldTintTargetsActive && !g_worldTintPreviewActive && CountedResource::alive == 0 &&
        g_queuedSavedWorldTintRestores.Size() == 0 && g_worldTintBuilds.Size() == 0,
        "last actor release returns renderer to dormant state with no resources/tasks/build records");
  actor.loaded = false; RE::TESForm::forms[actor.id] = &actor;
  QueueSavedWorldTintRestore(&actor); DrainAll();
  Check(g_queuedSavedWorldTintRestores.Size() == 0, "unready scene retries remain bounded");
  {
    WorldTintBuild old(actor.id);
    actor.loaded = true; QueueSavedWorldTintRestore(&actor);
    ClearWorldTint(); DrainAll();
    Check(!old.CurrentLocked() && !g_worldTintTargetsActive && g_savedWorldTints.contains(actor.id),
          "world scene clear fences builds/tasks but preserves recipes until deserialization");
    RevertSavedWorldTints();
    Check(g_savedWorldTints.empty(), "save revert retains its distinct durable-state reset");
  }
  std::puts("AppearanceResourceLifecycleTests passed: 128 unload/reload cycles, production routing/cleanup/queue, counted ownership (not D3D/game).");
}
