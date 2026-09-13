// Compile the real exported provider against fake engine/task/message boundaries.
// This tests ABI/lifecycle/reentry, not an in-game renderer or IED hook.
#include "api/RenderedOutfitState.h"
#include <atomic>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace RE {
using FormID = std::uint32_t;
enum class BSEventNotifyControl { kContinue };
template<class T> struct BSTEventSource {};
template<class T> struct BSTEventSink {
  virtual BSEventNotifyControl ProcessEvent(const T*, BSTEventSource<T>*) = 0;
};
struct TESObjectLoadedEvent { std::uint32_t formID; bool loaded; };
struct TESFormDeleteEvent { std::uint32_t formID; };
struct Actor {
  std::uint32_t id{0x14};
  void* root{reinterpret_cast<void*>(1)};
  bool deleted{false}, disabled{false};
  void* Get3D(bool) const { return root; }
  bool IsDeleted() const { return deleted; }
  bool IsDisabled() const { return disabled; }
  bool Is3DLoaded() const { return root != nullptr; }
  std::uint32_t GetFormID() const { return id; }
};
struct BGSKeyword { std::uint32_t flag; };
struct TESObjectARMO {
  std::uint32_t id, mask, effectiveMask, keywords, type;
  bool protectedSlot{false}, internalCover{false};
  std::uint32_t GetFormID() const { return id; }
  struct Mask { std::uint32_t bits; std::uint32_t underlying() const { return bits; } };
  Mask GetSlotMask() const { return {mask}; }
  bool HasKeyword(const BGSKeyword* k) const { return k && (keywords & k->flag); }
  bool IsClothing() const { return type == 0; }
  bool IsLightArmor() const { return type == 1; }
  bool IsHeavyArmor() const { return type == 2; }
};
inline std::unordered_map<std::uint32_t, Actor> actors;
inline std::uint64_t lookups{0};
struct TESForm {
  template<class T> static T* LookupByEditorID(const std::string_view id) {
    static BGSKeyword armor{1}, clothing{2};
    return id == "ArmorCuirass" ? &armor : id == "ClothingBody" ? &clothing : nullptr;
  }
  template<class T> static T* LookupByID(std::uint32_t id) {
    ++lookups;
    auto it = actors.find(id); return it == actors.end() ? nullptr : &it->second;
  }
};
struct ScriptEventSourceHolder {
  template<class T> static inline BSTEventSink<T>* sink{};
  static ScriptEventSourceHolder* GetSingleton() { static ScriptEventSourceHolder s; return &s; }
  template<class T> void AddEventSink(BSTEventSink<T>* value) { sink<T> = value; }
  template<class T> static void Send(const T& event) {
    if (sink<T>) { sink<T>->ProcessEvent(&event, nullptr); }
  }
};
template<class T> using BSTSmartPointer = std::shared_ptr<T>;
namespace BSScript {
struct Variable {};
struct Object {};
struct IStackCallbackFunctor {
  virtual ~IStackCallbackFunctor() = default;
  virtual void operator()(Variable) = 0;
  virtual void SetObject(const BSTSmartPointer<Object>&) = 0;
};
namespace Internal {
struct VirtualMachine {
  static inline bool available = true;
  static inline std::vector<Actor*> dispatched;
  static VirtualMachine* GetSingleton() {
    static VirtualMachine instance;
    return available ? &instance : nullptr;
  }
  bool DispatchStaticCall(std::string_view script, std::string_view method,
                         Actor* actor, const BSTSmartPointer<IStackCallbackFunctor>&) {
    if (script != "IED" || method != "Evaluate") { std::abort(); }
    dispatched.push_back(actor);
    return true;
  }
};
}
}
Actor* MakeFunctionArguments(Actor* actor) { return actor; }
}
namespace logger {
template<class... T> void error(T&&...) {}
template<class... T> void warn(T&&...) {}
}
namespace SKSE {
struct Tasks {
  std::uint64_t queued{0}, executed{0};
  std::vector<std::function<void()>> work;
  void AddTask(std::function<void()> f) { ++queued; work.push_back(std::move(f)); }
  void Run() {
    auto tasks = std::exchange(work, {});
    for (auto& f : tasks) { ++executed; f(); }
  }
};
inline Tasks tasks;
Tasks* GetTaskInterface() { return &tasks; }
struct Messages {
  std::vector<sfs::rendered_outfit_api::Changed> delivered;
  std::function<void()> callback;
  bool Dispatch(std::uint32_t type, void* data, std::uint32_t size, const char*) {
    if (type != sfs::rendered_outfit_api::kChangedMessage ||
        size != sizeof(sfs::rendered_outfit_api::Changed)) { std::abort(); }
    delivered.push_back(*static_cast<sfs::rendered_outfit_api::Changed*>(data));
    if (callback) { callback(); }
    return true;
  }
};
inline Messages messages;
Messages* GetMessagingInterface() { return &messages; }
}
#define SFS_RENDERED_OUTFIT_TEST
#include "api/RenderedOutfitProvider.cpp"

// Execute the actual pre-existing Helgen-safe reevaluation queue, now also
// used by the condition bridge. Only engine/task/VM boundaries are substituted.
namespace ied_queue {
std::mutex g_queuedIedEvaluationMutex;
std::unordered_map<std::uint32_t, std::uint64_t> g_queuedIedEvaluations;
std::uint64_t g_nextIedEvaluationToken{0};
std::atomic_bool g_iedEvaluateDispatchWarningLogged{false};
#include "IedEvaluationQueue.production.inc"
}

namespace ro = sfs::api::rendered;
namespace abi = sfs::rendered_outfit_api;
namespace sfs::armor {
std::uint32_t GetArmorSlotMask(std::uint32_t slot) { return 1u << (slot - 30); }
std::uint32_t GetArmorDisplaySlotMask(const RE::TESObjectARMO* armor) { return armor->mask; }
bool IsTngGenitalCoverArmor(const RE::TESObjectARMO* armor) { return armor && armor->internalCover; }
}
struct DisplaySet {
  bool active{false}, genitalCompatibilityAvailable{false};
  std::vector<const RE::TESObjectARMO*> armors;
  std::vector<std::uint32_t> armorSlotMasks;
  std::unordered_set<std::uint32_t> hiddenArmorFormIDs, forceVisibleArmorFormIDs;
  std::uint32_t hiddenSlotMask{0};
};
std::uint32_t GetProtectedActualSlotMask(const RE::TESObjectARMO* armor) {
  return armor->protectedSlot ? armor->mask : 0;
}
std::uint32_t GetSkinningSlotMask(const RE::TESObjectARMO* armor, bool) { return armor->effectiveMask; }
#include "RenderedOutfitVisibility.production.inc"
namespace sfs::native {
#include "RenderedOutfitBodyKeyword.production.inc"
}
#include "RenderedOutfitProducer.production.inc"
std::unordered_map<std::uint32_t, ro::Value> inputs;
unsigned captures{0};
bool useGameTaskQuery{false};
bool ro::HasDisplayConfiguration(std::uint32_t actor) { ++captures; return inputs.contains(actor); }
void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
abi::Status Query(std::uint32_t actor, abi::Snapshot& out, abi::Item* items = nullptr,
                  std::uint32_t count = 0) {
  out.structSize = sizeof(out);
  const auto query = useGameTaskQuery ? SkyrimFittingSystem_QueryRenderedOutfitOnGameTask :
                                      SkyrimFittingSystem_QueryRenderedOutfit;
  return query(actor, &out, items, count, sizeof(abi::Item));
}
void Tick() {
  // Simulate already-computed display producer input, never evaluate from Query.
  for (auto& [id, value] : inputs) { ro::PrepareValue(id, value); }
  ro::QueuePump(); SKSE::tasks.Run();
}
int main() {
  Check(SkyrimFittingSystem_GetRenderedOutfitAPIVersion() == 1, "export version");
  abi::Snapshot out{};
  Check(Query(0, out) == abi::Status::InvalidActor, "zero is not player alias");
  Check(SkyrimFittingSystem_QueryRenderedOutfit(1, nullptr, nullptr, 0, sizeof(abi::Item)) ==
        abi::Status::InvalidArgument, "null header");
  out.structSize = sizeof(out) - 1;
  Check(SkyrimFittingSystem_QueryRenderedOutfit(1, &out, nullptr, 0, sizeof(abi::Item)) ==
        abi::Status::InvalidArgument, "short header");
  out.structSize = sizeof(out);
  Check(SkyrimFittingSystem_QueryRenderedOutfit(1, &out, nullptr, 1, sizeof(abi::Item)) ==
        abi::Status::InvalidArgument, "null item buffer with nonzero capacity");
  Check(SkyrimFittingSystem_QueryRenderedOutfit(1, &out, nullptr, 0, sizeof(abi::Item) - 1) ==
        abi::Status::InvalidArgument, "wrong item stride");
  // Both exports share validation; the task-aware entry must not become an
  // unchecked cache copy just because it omits the legacy OS-thread proxy.
  const auto preValidationLookups = RE::lookups;
  for (const abi::Query query : {&SkyrimFittingSystem_QueryRenderedOutfit,
                                 &SkyrimFittingSystem_QueryRenderedOutfitOnGameTask}) {
    abi::Snapshot s{};
    Check(query(1, nullptr, nullptr, 0, sizeof(abi::Item)) == abi::Status::InvalidArgument,
          "both exports reject null output");
    s.structSize = sizeof(s) - 1;
    Check(query(1, &s, nullptr, 0, sizeof(abi::Item)) == abi::Status::InvalidArgument,
          "both exports reject short header");
    s.structSize = sizeof(s);
    Check(query(1, &s, nullptr, 1, sizeof(abi::Item)) == abi::Status::InvalidArgument &&
          query(1, &s, nullptr, 0, sizeof(abi::Item) - 1) == abi::Status::InvalidArgument,
          "both exports enforce buffer/stride contract");
    Check(query(0, &s, nullptr, 0, sizeof(abi::Item)) == abi::Status::InvalidActor,
          "both exports reject zero actor before engine access");
    Check(query(1, &s, nullptr, 0, sizeof(abi::Item)) == abi::Status::NotReady,
          "both exports defer before game readiness");
  }
  Check(RE::lookups == preValidationLookups, "invalid/unready queries never touch engine actors");
  Tick(); ro::SetGameReady(true); ro::RegisterEvents();
  RE::actors[0x14] = {}; RE::actors[0x22] = {};
  inputs[0x14] = {abi::Status::Ready, abi::ClothingBody,
      {{0x200, 0x200, 4, 4, abi::Registered, 0},
       {0x100, 0x100, 1u << 16, 1u << 16, abi::Actual, 0},
       {0x200, 0x200, 1u << 26, 1u << 26, abi::Registered, 0},
       {0x201, 0x201, 4, 4, abi::Registered, 0}}};
  Check(Query(0x14, out) == abi::Status::NotReady && captures == 0, "query never captures/evaluates");
  Tick();
  abi::Item item[4]{}; item[0].formID = 0xCAFE;
  Check(Query(0x14, out, item, 1) == abi::Status::BufferTooSmall &&
        out.requiredCount == 3 && item[0].formID == 0xCAFE, "no partial buffer writes");
  const auto revision = out.revision;
  Check(Query(0x14, out, item, 4) == abi::Status::Ready && item[0].formID == 0x100 &&
        item[1].visibleSlots == (4u | (1u << 26)) && item[2].formID == 0x201,
        "all slots, duplicates merged, overlapping different armors retained");
  Check(out.bodyFlags == abi::ClothingBody && out.revision == revision, "one consistent snapshot");
  SKSE::tasks.AddTask([&] {
    abi::Snapshot s{}; s.structSize = sizeof(s);
    abi::Item guarded[2]{}; guarded[0].formID = 0xABCD; guarded[1].formID = 0xDCBA;
    Check(SkyrimFittingSystem_QueryRenderedOutfitOnGameTask(0x14, &s, guarded, 1,
            sizeof(abi::Item)) == abi::Status::BufferTooSmall && s.requiredCount == 3 &&
          guarded[0].formID == 0xABCD && guarded[1].formID == 0xDCBA,
          "task export preserves all-or-nothing caller-buffer bounds");
  });
  SKSE::tasks.Run();
  const auto before = captures; Tick(); Tick();
  Check(captures == before, "no repeated inventory/condition capture when unchanged");
  const auto events = SKSE::messages.delivered.size();
  ro::NotifyRefresh(0x14, false); Tick();
  Check(SKSE::messages.delivered.size() == events, "same state suppresses ordinary duplicate notification");
  ro::NotifySkinning(0x14); Tick();
  Check(out.revision == revision && SKSE::messages.delivered.back().sceneGeneration >
        out.sceneGeneration, "same outfit new 3D not suppressed");

  inputs[0x14] = {abi::Status::Ready, 0, {}};
  ro::NotifyRefresh(0x14, true); Tick();
  Check(Query(0x14, out) == abi::Status::NotReady, "don't publish changed outfit before skinning");
  ro::NotifySkinning(0x14); Tick();
  Check(Query(0x14, out) == abi::Status::Ready && out.requiredCount == 0,
        "managed empty has no worn fallback");

  for (unsigned n = 0; n < 128; ++n) {
    inputs[0x14].items = {{0x200, 0x200, 4, 4, abi::Registered, 0}};
    ro::NotifyRefresh(0x14, true); ro::NotifySkinning(0x14); Tick();
    Check(Query(0x14, out, item, 4) == abi::Status::Ready, "preview/show ready");
    inputs[0x14].items.clear();
    ro::NotifyRefresh(0x14, true); ro::NotifySkinning(0x14); Tick();
    Check(Query(0x14, out) == abi::Status::Ready && out.requiredCount == 0, "cancel/hide restored");
  }
  inputs[0x22] = {abi::Status::Ready, 0, {{0x300, 0x300, 4, 4, abi::Actual, 0}}};
  Query(0x22, out); Tick();
  ro::NotifyRefresh(0x14, false); Tick();
  Check(Query(0x22, out, item, 4) == abi::Status::Ready && item[0].formID == 0x300, "actor isolation");
  inputs[0x14] = {abi::Status::NotManaged, 0, {}};
  ro::NotifyRefresh(0x14, false); Tick();
  Check(Query(0x14, out) == abi::Status::NotManaged, "release ownership permits fallback");
  abi::Status otherThread{};
  std::thread t([&] { abi::Snapshot s{}; otherThread = Query(0x14, s); }); t.join();
  Check(otherThread == abi::Status::WrongThread, "wrong thread rejected before engine access");
  // New clients enforce SKSE AddTask context themselves. Successive serialized
  // task batches can move OS threads, so the new export must not use the last
  // provider pump's OS-thread ID as a proxy for that context.
  useGameTaskQuery = true;
  SKSE::tasks.AddTask([&] { abi::Snapshot s{}; otherThread = Query(0x14, s); });
  std::thread migratedTask([] { SKSE::tasks.Run(); }); migratedTask.join();
  Check(otherThread == abi::Status::NotManaged, "migrated SKSE task can query current snapshot");
  for (unsigned transition = 0; transition < 128; ++transition) {
    inputs[0x14] = {abi::Status::Ready, 0, {}};
    const auto phase = transition % 4;
    if (phase < 2) {
      inputs[0x14].bodyFlags = abi::ArmorCuirass;
      inputs[0x14].items = {{phase ? 0x200u : 0x100u,
                            phase ? 0x200u : 0x100u, 4, 4,
                            phase ? abi::Registered : abi::Actual, 0}};
    } else if (phase == 3) {
      // The reported nude state still had a non-body item in slot 52. A
      // nonempty item array must not be mistaken for visible torso coverage.
      inputs[0x14].items = {{0x400, 0x400, 1u << 22, 1u << 22, abi::Registered, 0}};
    }
    ro::NotifyRefresh(0x14, true); ro::NotifySkinning(0x14); Tick();
    SKSE::tasks.AddTask([&] {
      abi::Snapshot s{}; abi::Item buf[4]{};
      Check(Query(0x14, s, buf, 4) == abi::Status::Ready &&
            s.requiredCount == inputs.at(0x14).items.size() &&
            s.bodyFlags == inputs.at(0x14).bodyFlags && s.visibleSlots ==
                (phase < 2 ? 4u : phase == 3 ? 1u << 22 : 0u),
            "every actual/displayed/hidden change reaches next migrated task");
      if (s.requiredCount) Check(buf[0].formID == inputs.at(0x14).items[0].formID &&
                                buf[0].source == inputs.at(0x14).items[0].source,
                                "migrated task uses visible source identity");
      const auto currentRevision = s.revision;
      const auto queuedBeforeIdle = SKSE::tasks.queued;
      const auto legacyLookups = RE::lookups;
      for (int idle = 0; idle < 3; ++idle) {
        ro::QueuePump();
        Check(SkyrimFittingSystem_QueryRenderedOutfit(0x14, &s, buf, 4,
                sizeof(abi::Item)) == abi::Status::WrongThread && RE::lookups == legacyLookups,
              "legacy export still rejects migrated thread before actor lookup");
      }
      Check(SKSE::tasks.queued == queuedBeforeIdle &&
            Query(0x14, s, buf, 4) == abi::Status::Ready && s.revision == currentRevision,
            "task export reads idle published state without forcing a pump or stale fallback");
    });
    std::thread task([] { SKSE::tasks.Run(); }); task.join();
  }
  // The remainder of the existing lifecycle/root/unload/epoch/idle-performance
  // tests now exercise the new export, not a less-validated snapshot shortcut.
  RE::actors[0x22].root = nullptr; Tick();
  Check(Query(0x22, out) == abi::Status::NotReady, "unloaded is not naked or unmanaged");
  RE::actors[0x22].root = reinterpret_cast<void*>(2); Tick();
  Check(Query(0x22, out, item, 4) == abi::Status::Ready, "same IDs reload re-ready");

  bool reentered = false;
  SKSE::messages.callback = [&] {
    abi::Snapshot s{}; abi::Item buf[4]{};
    Check(Query(0x22, s, buf, 4) == abi::Status::Ready, "query inside message without deadlock");
    reentered = true;
  };
  ro::NotifySkinning(0x22); Tick();
  Check(reentered, "real message callback ran");
  SKSE::messages.callback = {};
  const auto epoch = out.epoch;
  ro::SetGameReady(false); Tick();
  Check(SKSE::messages.delivered.back().actorFormID == 0 &&
        (SKSE::messages.delivered.back().reasons & abi::EpochChanged), "epoch message even with no actors");
  Check(Query(0x22, out) == abi::Status::NotReady && out.epoch > epoch, "old data invalid after load");

  ro::State state;
  auto ticket = state.Touch(1); auto oldEpoch = state.Epoch();
  state.Reset(); state.Touch(1);
  Check(!state.Publish(1, oldEpoch, ticket, inputs[0x22]), "stale load computation rejected");
  ticket = state.Touch(1, true);
  state.Publish(1, state.Epoch(), ticket, inputs[0x22]);
  auto ticket2 = state.Touch(1);
  inputs[0x22].bodyFlags = abi::ArmorCuirass;
  state.Publish(1, state.Epoch(), ticket2, inputs[0x22]);
  auto coalesced = state.Drain();
  Check((coalesced.back().reasons & abi::SceneChanged) != 0, "coalescing preserves scene reason");

  ro::Value compact{abi::Status::Ready, 0, {
      {2, 2, 4, 4, abi::Registered, 0}, {1, 1, 8, 8, abi::Actual, 0},
      {2, 0, 16, 16, abi::Registered, abi::OriginalUnknown}}};
  const auto* storage = compact.items.data();
  const auto capacity = compact.items.capacity();
  compact.Normalize();
  Check(compact.items.data() == storage && compact.items.capacity() == capacity &&
        compact.items.size() == 2 && compact.items[1].visibleSlots == 20 &&
        compact.items[1].originalFormID == 0 &&
        (compact.items[1].flags & abi::OriginalUnknown), "in-place compaction, safe provenance merge");

  // One thousand subscribed actors do NOT imply one thousand idle-frame reads.
  // Count production provider work; timings here would not represent Skyrim FPS.
  ro::SetGameReady(true); inputs.clear();
  constexpr unsigned actorCount = 1000;
  for (unsigned n = 0; n < actorCount; ++n) {
    const auto id = 0x1000 + n;
    RE::actors[id] = {};
    inputs[id] = {abi::Status::Ready, 0, {{id + 0x10000, id + 0x10000, 4, 4, abi::Registered, 0}}};
    Query(id, out);
  }
  const auto beforeBurst = RE::lookups;
  Tick();
  Check(RE::lookups - beforeBurst == 64, "burst is limited to 64 changed actors per task");
  for (unsigned n = 0; n < 15; ++n) { ro::QueuePump(); SKSE::tasks.Run(); }
  Check(RE::lookups - beforeBurst == actorCount, "batched burst covers every actor exactly once");
  for (unsigned n = 0; n < actorCount; ++n) {
    Check(Query(0x1000 + n, out, item, 4) == abi::Status::Ready, "batch did not drop actor readiness");
  }
  const auto idleQueued = SKSE::tasks.queued;
  const auto idleLookups = RE::lookups;
  const auto* preparedStorage = ro::prepared.at(0x1000).items.data();
  const auto* producerStorage = inputs.at(0x1000).items.data();
  for (unsigned n = 0; n < 1000; ++n) { ro::PrepareValue(0x1000, inputs[0x1000]); }
  Check(ro::prepared.at(0x1000).items.data() == preparedStorage &&
        inputs.at(0x1000).items.data() == producerStorage && !ro::state.HasWork(),
        "unchanged producer reuses item buffers without copying or scheduling");
  for (unsigned n = 0; n < 10000; ++n) { ro::QueuePump(); SKSE::tasks.Run(); }
  Check(SKSE::tasks.queued == idleQueued && RE::lookups == idleLookups,
        "10000 idle frames: zero queued tasks and zero actor lookups with 1000 subscribers");
  for (unsigned n = 0; n < 1000; ++n) { ro::NotifyRefresh(0x1000, false); }
  ro::QueuePump(); ro::QueuePump(); SKSE::tasks.Run();
  Check(SKSE::tasks.queued == idleQueued + 1 && RE::lookups == idleLookups + 1,
        "1000 repeated refresh requests coalesce to one actor in one task");

  // Actual event sinks, without Tick's simulated producer hiding missing data.
  RE::ScriptEventSourceHolder::Send(RE::TESObjectLoadedEvent{0x1000, false});
  ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(0x1000, out, item, 4) == abi::Status::NotReady, "unload invalidates without polling");
  RE::ScriptEventSourceHolder::Send(RE::TESObjectLoadedEvent{0x1000, true});
  ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(0x1000, out, item, 4) == abi::Status::NotReady, "reload cannot resurrect stale producer data");
  ro::PrepareValue(0x1000, inputs[0x1000]);
  ro::NotifySkinning(0x1000); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(0x1000, out, item, 4) == abi::Status::Ready, "late producer resumes after reload");
  // A root replacement not accompanied by an object event is still caught by
  // Query for that actor; it must invalidate and await a fresh display producer.
  RE::actors[0x1000].root = reinterpret_cast<void*>(0x8765);
  Check(Query(0x1000, out, item, 4) == abi::Status::NotReady, "query validates changed root without scanning others");
  ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(0x1000, out, item, 4) == abi::Status::NotReady, "old-root outfit stays invalid");
  ro::PrepareValue(0x1000, inputs[0x1000]);
  ro::NotifySkinning(0x1000); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(0x1000, out, item, 4) == abi::Status::Ready, "fresh root publication resumes");
  RE::actors[0x1000].disabled = true;
  Check(Query(0x1000, out, item, 4) == abi::Status::NotReady, "disabled actor cannot return ready cache");
  ro::QueuePump(); SKSE::tasks.Run();
  RE::actors[0x1000].disabled = false;
  Query(0x1000, out); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(0x1000, out, item, 4) == abi::Status::Ready,
        "query detects re-enable with the same root without background polling");

  // Unqueried producer caches must be freed on unload too.
  RE::actors[0x8000] = {};
  inputs[0x8000] = inputs[0x1000];
  ro::PrepareValue(0x8000, inputs[0x8000]);
  RE::ScriptEventSourceHolder::Send(RE::TESObjectLoadedEvent{0x8000, false});
  Query(0x8000, out); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(0x8000, out, item, 4) == abi::Status::NotReady, "unqueried actor warm cache erased on unload");
  RE::actors.erase(0x1000);
  RE::ScriptEventSourceHolder::Send(RE::TESFormDeleteEvent{0x1000});
  ro::QueuePump(); SKSE::tasks.Run();
  Check(SKSE::messages.delivered.back().status == abi::Status::InvalidActor,
        "deleted actor notification without background lookup");

  // A reentrant load cancels the rest of the old-epoch notification batch.
  ro::NotifySkinning(0x1001); ro::NotifySkinning(0x1002);
  const auto deliveredBeforeReset = SKSE::messages.delivered.size();
  SKSE::messages.callback = [] { ro::SetGameReady(false); };
  ro::QueuePump(); SKSE::tasks.Run();
  Check(SKSE::messages.delivered.size() == deliveredBeforeReset + 1,
        "callback epoch reset drops remaining stale notifications");
  SKSE::messages.callback = {};
  ro::QueuePump(); SKSE::tasks.Run();
  const auto resetEpoch = SKSE::messages.delivered.back().epoch;
  ro::SetGameReady(false); ro::QueuePump(); SKSE::tasks.Run();
  Check(SKSE::messages.delivered.back().epoch == resetEpoch && !ro::state.HasWork(),
        "repeated reset is idempotent and idle");
  Check(ro::prepared.empty() && ro::observations.empty(), "load reset releases all per-actor caches");

  ro::State fairness;
  for (unsigned n = 1; n <= 128; ++n) { fairness.Touch(n); }
  const auto firstBatch = fairness.PendingActors(64);
  for (const auto id : firstBatch) { fairness.Touch(id); }
  const auto secondBatch = fairness.PendingActors(64);
  Check(firstBatch.front() == 1 && secondBatch.front() == 65,
        "continuous low-ID changes do not starve later actors");
  ro::State notifications;
  for (unsigned n = 1; n <= 128; ++n) { notifications.Touch(n); notifications.Erase(n, abi::Status::InvalidActor); }
  Check(notifications.Drain().size() == 64 && notifications.HasWork() &&
        notifications.Drain().size() == 64 && !notifications.HasWork(),
        "deletion notification burst is bounded and completely drained");
  notifications.Reset();
  auto freshTicket = notifications.Touch(1);
  notifications.Publish(1, notifications.Epoch(), freshTicket, {});
  const auto resetAndActor = notifications.Drain();
  Check(resetAndActor.size() == 2 && resetAndActor.front().actorFormID == 0 &&
        resetAndActor.back().actorFormID == 1, "epoch invalidation precedes new actor notification");

  ro::SetGameReady(true);
  RE::Actor producerActor{}; producerActor.id = 0x9000;
  RE::actors[producerActor.id] = producerActor;
  RE::TESObjectARMO realBody{0xA001, 4, 4, abi::ArmorCuirass, 1};
  RE::TESObjectARMO fittingBody{0xA002, 4 | (1u << 16), 4, 0, 0};
  RE::TESObjectARMO accessory{0xA003, 1u << 26, 1u << 26, 0, 0};
  RE::TESObjectARMO dynamic{0xFF000001, 1u << 16, 1u << 16, 0, 0};
  RE::TESObjectARMO cover{0xA004, 1u << 22, 1u << 22, abi::ArmorCuirass, 1, false, true};
  DisplaySet display{}; display.active = true; display.hiddenSlotMask = 4;
  display.armors = {&fittingBody, &accessory, &dynamic};
  display.armorSlotMasks = {4, 1u << 26, 1u << 16};
  std::unordered_set<const RE::TESObjectARMO*> equipped{&realBody, &cover};
  PrepareOutfitValue(&producerActor, display, equipped);
  Query(producerActor.id, out); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(producerActor.id, out, item, 4) == abi::Status::Ready &&
        out.requiredCount == 3 && item[0].formID == fittingBody.id &&
        item[0].declaredSlots == (4u | (1u << 16)) && item[0].visibleSlots == 4 &&
        item[2].originalFormID == 0 && (item[2].flags & abi::OriginalUnknown) &&
        out.bodyFlags == abi::ClothingBody, "real producer: hidden actual, effective slots, body inference, dynamic provenance, internal cover exclusion");
  const auto noChangeRevision = out.revision;
  const auto noChangeTasks = SKSE::tasks.queued;
  for (unsigned n = 0; n < 1000; ++n) { PrepareOutfitValue(&producerActor, display, equipped); }
  ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(producerActor.id, out, item, 4) == abi::Status::Ready &&
        out.revision == noChangeRevision && SKSE::tasks.queued == noChangeTasks,
        "real producer: unchanged builds do not schedule publication");
  realBody.protectedSlot = true;
  PrepareOutfitValue(&producerActor, display, equipped); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(producerActor.id, out, item, 4) == abi::Status::Ready &&
        out.requiredCount == 4 && item[0].source == abi::Actual &&
        out.bodyFlags == (abi::ArmorCuirass | abi::ClothingBody), "real producer: protected actual slots remain visible");
  realBody.protectedSlot = false;
  display.forceVisibleArmorFormIDs.insert(realBody.id);
  PrepareOutfitValue(&producerActor, display, equipped); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(producerActor.id, out, item, 4) == abi::Status::Ready && out.requiredCount == 4,
        "real producer: force-visible actual remains visible");
  display.forceVisibleArmorFormIDs.clear();
  display.armors = {&accessory}; display.armorSlotMasks = {1u << 26};
  PrepareOutfitValue(&producerActor, display, equipped); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(producerActor.id, out, item, 4) == abi::Status::Ready && !out.bodyFlags,
        "real producer: arbitrary accessory does not become torso clothing");
  display.armors.clear(); display.armorSlotMasks.clear();
  PrepareOutfitValue(&producerActor, display, equipped); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(producerActor.id, out, item, 4) == abi::Status::Ready && !out.requiredCount,
        "real producer: managed empty remains distinct from fallback");
  display.active = false;
  PrepareOutfitValue(&producerActor, display, equipped); ro::QueuePump(); SKSE::tasks.Run();
  Check(Query(producerActor.id, out, item, 4) == abi::Status::NotManaged,
        "real producer: inactive SFS permits actual-equipment fallback");

  // IED worker access owns immutable IDs only; it never relaxes public Query's
  // game-thread contract or takes a second engine/condition evaluation path.
  ro::SetGameReady(false); Tick(); inputs.clear(); ro::SetGameReady(true);
  static unsigned observerCalls = 0;
  ro::SetDecisionObserver([](std::uint32_t id) {
    ++observerCalls;
    static_cast<void>(ro::AcquirePublished(id)); // Callback is outside the mutex.
  });
  ro::Value watched{abi::Status::Ready, 0, {{0x200, 0x200, 4, 4, abi::Registered, 0}}};
  ro::PrepareValue(0x14, watched); ro::QueuePump(); SKSE::tasks.Run();
  Check(observerCalls == 1, "IED enrollment comes from producer, no public query needed");
  std::shared_ptr<const ro::Value> held;
  const auto lookupCount = RE::lookups;
  std::thread reader([&] { for (int n = 0; n < 10000; ++n) { held = ro::AcquirePublished(0x14); } }); reader.join();
  Check(held && held->items[0].formID == 0x200 && RE::lookups == lookupCount,
        "worker reads cause no actor lookups or SFS reevaluation");
  const auto* address = held.get();
  ro::NotifySkinning(0x14); ro::QueuePump(); SKSE::tasks.Run();
  Check(observerCalls == 1 && ro::AcquirePublished(0x14).get() == address,
        "same-outfit SceneChanged does not copy worker view or loop IED.Evaluate");
  watched.items.clear(); ro::PrepareValue(0x14, watched);
  Check(!ro::AcquirePublished(0x14), "dirty view is not presented as ready");
  ro::QueuePump(); SKSE::tasks.Run();
  Check(observerCalls == 2 && ro::AcquirePublished(0x14)->items.empty() && held->items.size() == 1,
        "managed-empty update is immutable, preserves in-flight reader");
  ro::ForgetPreparedValue(0x14); ro::QueuePump(); SKSE::tasks.Run();
  Check(observerCalls == 3 && !ro::AcquirePublished(0x14), "ownership release refreshes IED original conditions");
  ro::SetGameReady(false);
  Check(!ro::AcquirePublished(0x14) && held->items.size() == 1, "epoch reset retires view without dangling reader");
  ro::QueuePump(); SKSE::tasks.Run(); ro::SetGameReady(true);
  constexpr std::uint32_t transitionActor = 0x9900;
  RE::actors[transitionActor] = {transitionActor};
  watched.items = {{0x201, 0x201, 4, 4, abi::Registered, 0}};
  inputs[transitionActor] = watched;
  ro::PrepareValue(transitionActor, watched); ro::QueuePump(); SKSE::tasks.Run();
  Check(observerCalls == 4, "initial decision enrolls cell-transition actor");
  auto beforeUnload = ro::AcquirePublished(transitionActor);
  RE::ScriptEventSourceHolder::Send(RE::TESObjectLoadedEvent{transitionActor, false});
  Check(!ro::AcquirePublished(transitionActor), "unload immediately invalidates IED worker view");
  // A trailing skinning notification must not revive the suspended actor,
  // even if the engine's old root pointer is not cleared yet.
  ro::NotifySkinning(transitionActor); ro::QueuePump(); SKSE::tasks.Run();
  Check(observerCalls == 4 && !ro::AcquirePublished(transitionActor) &&
        beforeUnload && beforeUnload->items[0].formID == 0x201,
        "unload skips IED reevaluation; in-flight readers own only immutable IDs");
  RE::actors[transitionActor].root = reinterpret_cast<void*>(0x2222);
  RE::ScriptEventSourceHolder::Send(RE::TESObjectLoadedEvent{transitionActor, true});
  ro::NotifySkinning(transitionActor);
  ro::PrepareValue(transitionActor, watched); ro::QueuePump(); SKSE::tasks.Run();
  Check(observerCalls == 5 && ro::AcquirePublished(transitionActor),
        "new cell/root resumes IED only after fresh published decision");
  RE::actors[transitionActor].deleted = true;
  RE::ScriptEventSourceHolder::Send(RE::TESFormDeleteEvent{transitionActor});
  ro::QueuePump(); SKSE::tasks.Run();
  Check(observerCalls == 5 && !ro::AcquirePublished(transitionActor) &&
        !ro::prepared.contains(transitionActor), "deleted actor releases data without IED reevaluation");
  ro::PrepareValue(0x14, watched); ro::QueuePump();
  ro::SetGameReady(false); SKSE::tasks.Run();
  Check(observerCalls == 5 && !ro::AcquirePublished(0x14),
        "load transition discards pending old-epoch IED publication");
  held.reset(); beforeUnload.reset(); ro::SetDecisionObserver(nullptr); inputs.clear();

  using VM = RE::BSScript::Internal::VirtualMachine;
  constexpr std::uint32_t queuedActor = 0x9901;
  RE::actors[queuedActor] = {queuedActor};
  const auto beforeQueue = SKSE::tasks.queued;
  for (int n = 0; n < 1000; ++n) { ied_queue::QueueIedEvaluateID(queuedActor); }
  Check(SKSE::tasks.queued == beforeQueue + 1 && VM::dispatched.empty(),
        "IED queue coalesces and never reenters a skin visitor synchronously");
  RE::actors[queuedActor].root = nullptr;
  SKSE::tasks.Run();
  Check(VM::dispatched.empty() && ied_queue::g_queuedIedEvaluations.empty(),
        "cell unload before task execution skips IED without retaining queue entries");
  for (int unavailable = 0; unavailable < 3; ++unavailable) {
    RE::actors[queuedActor] = {queuedActor};
    ied_queue::QueueIedEvaluateID(queuedActor);
    if (unavailable == 0) { RE::actors[queuedActor].deleted = true; }
    if (unavailable == 1) { RE::actors[queuedActor].disabled = true; }
    if (unavailable == 2) { RE::actors.erase(queuedActor); }
    SKSE::tasks.Run();
    Check(VM::dispatched.empty(), "deleted/disabled/missing actors cannot enter IED Evaluate");
  }
  RE::actors[queuedActor] = {queuedActor};
  ied_queue::QueueIedEvaluateID(queuedActor);
  ied_queue::ClearQueuedIedEvaluations();
  const auto canceledLookups = RE::lookups;
  SKSE::tasks.Run();
  Check(VM::dispatched.empty() && RE::lookups == canceledLookups,
        "load-canceled IED job does not even resolve the old actor ID");
  // Reusing an actor ID in a new load must not make the old token valid again.
  ied_queue::QueueIedEvaluateID(queuedActor);
  ied_queue::ClearQueuedIedEvaluations();
  RE::Actor oldActor{queuedActor};
  ied_queue::QueueIedEvaluate(&oldActor);
  SKSE::tasks.Run();
  Check(VM::dispatched.size() == 1 && VM::dispatched.back() == &RE::actors[queuedActor] &&
        VM::dispatched.back() != &oldActor && ied_queue::g_queuedIedEvaluations.empty(),
        "only new token runs, with freshly resolved actor rather than captured old pointer");
  VM::available = false;
  ied_queue::QueueIedEvaluateID(queuedActor); SKSE::tasks.Run();
  Check(VM::dispatched.size() == 1 && ied_queue::g_queuedIedEvaluations.empty(),
        "VM teardown retires queued job safely");
  VM::available = true;
  const auto invalidQueued = SKSE::tasks.queued;
  ied_queue::QueueIedEvaluateID(0); ied_queue::QueueIedEvaluate(nullptr);
  Check(SKSE::tasks.queued == invalidQueued, "null/zero actor never schedules IED work");
  std::puts("IED observer unload/delete/epoch and production queue lifecycle tests passed.");
  std::puts("Performance counters: 1000 actors / 10000 idle frames = 0 tasks, 0 actor lookups; 1000 duplicate refreshes = 1 task, 1 actor lookup.");
  std::puts("Rendered outfit ABI/provider/producer tests passed (production functions, fake engine).");
}
