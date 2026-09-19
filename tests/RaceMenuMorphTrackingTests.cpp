// Behavioral regression test: compile actual production tracking functions with
// fake engine ownership/attachments and a recording RaceMenu interface.
// Does not simulate renderer output or substitute for in-game backend tests.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <exception>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "native/RegisteredAppearanceMorphRules.h"
#include "native/ActorResourceWork.h"

namespace RE {
using FormID = std::uint32_t;
template<class T> struct NiPointer {
  T* value{};
  NiPointer() = default;
  NiPointer(T* p) : value(p) {}
  T* get() const { return value; }
  T* operator->() const { return value; }
  explicit operator bool() const { return value != nullptr; }
};
using BSFixedString = std::string;
struct NiAVObject {
  NiAVObject* parent{};
  bool bodyTri{true}, shapeData{true};
  const void* GetExtraData(const BSFixedString& name) {
    return (name == "BODYTRI" ? bodyTri : shapeData) ? this : nullptr;
  }
  NiAVObject* AsNode() { return nullptr; }
  std::vector<NiPointer<NiAVObject>> GetChildren() { return {}; }
};
using NiNode = NiAVObject;
struct TESForm {
  static inline std::unordered_map<FormID, TESForm*> forms;
  FormID id;
  explicit TESForm(FormID v) : id(v) { forms[v] = this; }
  FormID GetFormID() const { return id; }
  template<class T> static T* LookupByID(FormID id) {
    auto it = forms.find(id);
    return it == forms.end() ? nullptr : static_cast<T*>(it->second);
  }
};
struct TESObjectREFR : TESForm {
  using TESForm::TESForm;
  template<class T> T* As() { return static_cast<T*>(this); }
};
struct TESObjectARMO : TESForm { using TESForm::TESForm; };
struct TESObjectARMA : TESForm { using TESForm::TESForm; };
struct Actor : TESObjectREFR {
  using TESObjectREFR::TESObjectREFR;
  NiAVObject roots[2];
  std::unordered_set<FormID> displayed;
  bool loaded{true};
  bool Is3DLoaded() const { return loaded; }
  NiAVObject* Get3D(bool fp) { return &roots[fp]; }
};
}
namespace logger {
template<class... T> void debug(T&&...) {}
template<class... T> void info(T&&...) {}
template<class... T> void warn(T&&...) {}
template<class... T> void error(T&&...) {}
}
namespace skee {
struct IBodyMorphInterface {
  std::unordered_map<RE::FormID, unsigned> writes;
  void ApplyVertexDiff(RE::Actor* actor, RE::NiAVObject* node, bool attach) {
    if (attach) { std::fputs("Unexpected attach-time vertex reset", stderr); std::abort(); }
    ++writes[actor->id];
    node->shapeData = true;
  }
};
struct IAddonAttachmentInterface {
  virtual void OnAttach(RE::TESObjectREFR*, RE::TESObjectARMO*,
    RE::TESObjectARMA*, RE::NiAVObject*, bool, RE::NiNode*, RE::NiNode*) = 0;
};
}
namespace SKSE {
struct Tasks {
  std::vector<std::function<void()>> pending;
  template<class F> void AddTask(F f) { pending.emplace_back(std::move(f)); }
  void Drain() {
    auto batch = std::move(pending);
    pending.clear();
    for (auto& f : batch) f();
  }
} tasks;
Tasks* GetTaskInterface() { return &tasks; }
}
namespace sfs::native {
namespace dye {
std::unordered_map<RE::FormID, unsigned> restoreRequests;
void QueueSavedWorldTintRestore(RE::Actor* actor) { ++restoreRequests[actor->id]; }
}
std::function<void()> beforeDisplayedQuery;
bool IsDisplayedFittingArmor(RE::Actor* actor, const RE::TESObjectARMO* armor) {
  if (beforeDisplayedQuery) {
    auto hook = std::move(beforeDisplayedQuery);
    beforeDisplayedQuery = {};
    hook();
  }
  return actor->displayed.contains(armor->id);
}
namespace racemenu {
void QueueRegisteredAppearanceHighHeelSync(RE::Actor*) {}
}
}
unsigned highHeelObservations{};
class SceneObservation;
bool ShouldResyncHighHeelAfterAttachment(RE::Actor*, bool) { return false; }
bool RememberHighHeelAttachmentRoots(RE::Actor*,
  const std::vector<RE::NiPointer<RE::NiAVObject>>&, RE::FormID, bool, const SceneObservation*) {
  ++highHeelObservations;
  return false;
}
namespace rules = sfs::native::racemenu::rules;
std::mutex g_nodeMutex;
sfs::native::resource_work::ActorBuilds g_sceneObservations;
std::mutex g_highHeelQueueMutex;
sfs::native::resource_work::ActorTasks g_queuedHighHeelSyncs;
std::unordered_set<RE::FormID> g_pendingHighHeelResyncs;
std::unordered_set<RE::FormID> g_registeredAppearanceHighHeelActors;
std::unordered_map<RE::FormID, std::vector<RE::NiPointer<RE::NiAVObject>>> g_registeredAppearanceAttachmentRoots;
rules::ActorMorphActivity g_morphActivity;
rules::ActorMorphRequests g_morphRequests;
std::uint64_t g_nodeObservation{};
std::unordered_set<RE::FormID> g_highHeelAttachmentActors;
std::atomic<skee::IBodyMorphInterface*> g_bodyMorphInterface;
using ApplyBodyMorphsFn = void (*)(skee::IBodyMorphInterface*, RE::TESObjectREFR*, bool);
std::atomic<ApplyBodyMorphsFn> g_originalApplyBodyMorphs;
thread_local unsigned g_updateModelWeightTaskDepth{};
unsigned originalCalls{};

void QueuePendingMorphSync(RE::FormID, std::uint32_t = 2);
[[nodiscard]] std::size_t ApplyMorphsToRegisteredAppearanceNodes(
  skee::IBodyMorphInterface*, RE::Actor*, bool* = nullptr, bool = false);
#include "RaceMenuMorphTracking.production.inc"


static void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void DrainAll() {
  for (int frame = 0; frame != 8 && !SKSE::tasks.pending.empty(); ++frame)
    SKSE::tasks.Drain();
  Check(SKSE::tasks.pending.empty(), "completion must be bounded, not polling");
}
int main() {
  skee::IBodyMorphInterface morph;
  g_bodyMorphInterface = &morph;
  g_originalApplyBodyMorphs = +[](skee::IBodyMorphInterface*, RE::TESObjectREFR*, bool) { ++originalCalls; };
  RE::Actor player{0x14}, npc{0x1234}, unrelated{0x4567};
  RE::TESObjectARMO armor{0x800};
  RE::TESObjectARMA addon{0x801};
  RE::NiAVObject playerNode{player.Get3D(false)}, npcNode{npc.Get3D(false)};
  player.displayed.insert(armor.id); npc.displayed.insert(armor.id);
  RegisteredAppearanceAttachmentObserver observer;
  const auto attach = [&](RE::Actor& actor, RE::NiAVObject& node, bool fp = false) {
    observer.OnAttach(&actor, &armor, &addon, &node, fp, nullptr, nullptr);
  };
  const auto publish = [&](RE::Actor& actor, bool preview, bool shown = true) {
    SetRegisteredAppearanceDisplayActive(&actor,
      rules::ShouldTrackRegisteredAppearanceNodes(shown, 1),
      rules::ShouldApplyInitialNativeMorphs(preview));
  };
  const auto checkBoth = [&](const char* label) {
    DrainAll();
    const auto before = morph.writes[player.id];
    ApplyBodyMorphsHook(&morph, &player, false);
    Check(morph.writes[player.id] == before + 1, label);
    QueueUpdateModelWeightAppearanceSync(player.id); DrainAll();
    Check(morph.writes[player.id] == before + 2, label);
    std::printf("PASS: %s (public + deferred)\n", label);
  };

  publish(player, false); publish(npc, false);
  attach(player, playerNode); attach(npc, npcNode); DrainAll();
  Check(morph.writes[player.id] == 0, "OnAttach must not duplicate initial morphs");
  checkBoth("saved appearance");
  const auto npcBefore = morph.writes[npc.id];
  publish(player, true);
  checkBoth("replacement preview stays live");
  const auto hhBefore = highHeelObservations;
  attach(player, playerNode); DrainAll();
  Check(highHeelObservations == hhBefore, "preview tracking must not broaden HH observer");
  publish(player, false);
  checkBoth("preview end retaining the SAME node without OnAttach");
  Check(morph.writes[npc.id] == npcBefore, "player updates must not morph NPC");

  const auto beforeHidden = morph.writes[player.id];
  publish(player, false, false);
  ApplyBodyMorphsHook(&morph, &player, false);
  QueueUpdateModelWeightAppearanceSync(player.id); DrainAll();
  Check(morph.writes[player.id] == beforeHidden, "hidden display must not receive SFS morph writes");
  Check(g_morphRequests.HasRequest(player.id), "hidden update intent must survive");
  publish(player, false); DrainAll();
  Check(morph.writes[player.id] == beforeHidden + 1, "visibility restore must replay missed update");
  checkBoth("visibility restore without OnAttach");

  // Every feature below ultimately requests the same actor-local display
  // refresh. Re-publishing an unchanged final display must preserve the
  // remembered roots even when the backend retains them and emits no OnAttach.
  // This sequence locks the historical cross-feature regression where work on
  // HT2, dye, or paused camera handling accidentally disabled live BodyMorph.
  for (const auto *featureRefresh : {
           "equipment/outfit/condition display refresh",
           "kit and generator preview refresh",
           "protected/shield slot refresh",
           "Helmet Toggle 2 refresh",
           "fitting dye restore refresh",
           "paused camera/pose refresh",
           "Virtual Token Mod-Configured strip/redress refresh",
           "Vanilla-slot strip/redress refresh",
           "Direct+Mod-Configured strip/redress refresh",
           "Direct+Vanilla strip/redress refresh",
           "SexLab P+ adapter refresh",
           "Devious Devices adapter refresh",
           "DAVE/DAV/native backend refresh"}) {
    publish(player, false);
    checkBoth(featureRefresh);
  }

  playerNode.parent = nullptr;
  const auto beforeDetached = morph.writes[player.id];
  QueueUpdateModelWeightAppearanceSync(player.id); SKSE::tasks.Drain();
  for (int update = 0; update != 10; ++update)
    ApplyBodyMorphsHook(&morph, &player, false);
  Check(morph.writes[player.id] == beforeDetached, "detached root must not be written");
  playerNode.parent = player.Get3D(false);
  DrainAll();
  Check(morph.writes[player.id] == beforeDetached + 1, "bounded late graft must recover");

  playerNode.parent = nullptr;
  QueueUpdateModelWeightAppearanceSync(player.id); DrainAll();
  Check(g_registeredAppearanceNodes[player.id].empty(), "expired detached roots must release references");
  const auto beforeLate = morph.writes[player.id];
  playerNode.parent = player.Get3D(false); attach(player, playerNode); DrainAll();
  Check(morph.writes[player.id] == beforeLate + 1, "late callback must replay previously empty update");

  QueueUpdateModelWeightAppearanceSync(player.id);
  publish(player, false, false); DrainAll();
  const auto beforeResume = morph.writes[player.id];
  publish(player, false); DrainAll();
  Check(morph.writes[player.id] == beforeResume + 1, "inactive task must resume on reactivation");

  // Current display identity and root ownership remain mandatory.
  player.displayed.erase(armor.id);
  const auto beforeNotDisplayed = morph.writes[player.id];
  ApplyBodyMorphsHook(&morph, &player, false);
  Check(morph.writes[player.id] == beforeNotDisplayed, "non-displayed ARMO must not receive SFS writes");
  player.displayed.insert(armor.id);
  playerNode.parent = npc.Get3D(false);
  ApplyBodyMorphsHook(&morph, &player, false);
  Check(morph.writes[player.id] == beforeNotDisplayed, "another actor's scene must not receive writes");
  playerNode.parent = player.Get3D(false); DrainAll();
  Check(morph.writes[npc.id] == npcBefore, "actor isolation must survive ownership checks");

  ApplyBodyMorphsHook(&morph, &unrelated, false);
  QueueUpdateModelWeightAppearanceSync(unrelated.id); DrainAll();
  Check(!g_morphRequests.HasRequest(unrelated.id), "unrelated actors must not be enrolled");

  // Explicit actor forget cancels queued work, without invalidating another actor.
  QueueUpdateModelWeightAppearanceSync(player.id);
  QueueUpdateModelWeightAppearanceSync(npc.id);
  const auto beforeForget = morph.writes[player.id];
  ReleaseActorSceneResources(player.id, false);
  attach(player, playerNode); // Same FormID/node reused before the old task runs.
  DrainAll();
  Check(morph.writes[player.id] == beforeForget, "old actor task must be invalidated");
  Check(morph.writes[npc.id] == npcBefore + 1, "forget must not cancel another actor");
  ReleaseActorSceneResources(player.id, false);

  RE::NiAVObject previewRoot{player.Get3D(false)};
  previewRoot.shapeData = false;
  publish(player, true); DrainAll();
  const auto initialBefore = morph.writes[player.id];
  RememberAndMorphNewNodes(&morph, &player, {&previewRoot}, armor.id, false, false);
  DrainAll();
  Check(morph.writes[player.id] == initialBefore && !previewRoot.shapeData,
        "native replacement preview capture must record without initial application");
  ApplyBodyMorphsHook(&morph, &player, false);
  Check(morph.writes[player.id] == initialBefore + 1,
        "recorded preview root must accept a later explicit live update");
  ReleaseActorSceneResources(player.id, false);
  RE::NiAVObject savedRoot{player.Get3D(false)};
  savedRoot.shapeData = false;
  publish(player, false); DrainAll();
  const auto savedBefore = morph.writes[player.id];
  RememberAndMorphNewNodes(&morph, &player, {&savedRoot}, armor.id, false, true);
  DrainAll();
  Check(morph.writes[player.id] == savedBefore + 1,
        "native saved appearance must preserve initial fallback");
  RememberAndMorphNewNodes(&morph, &player, {&savedRoot}, armor.id, false, true); DrainAll();
  Check(morph.writes[player.id] == savedBefore + 1, "SHAPEDATA must prevent duplicate initial fallback");
  RE::NiAVObject fp{player.Get3D(true)};
  attach(player, fp, true); DrainAll();
  const auto fpBefore = morph.writes[player.id];
  ApplyBodyMorphsHook(&morph, &player, false);
  Check(morph.writes[player.id] == fpBefore + 2, "first/third person roots must both update once");
  for (int event = 0; event != 50; ++event) attach(player, savedRoot);
  Check(SKSE::tasks.pending.size() == 1, "attachment burst must coalesce per actor");
  DrainAll();
  g_bodyMorphInterface = nullptr;
  const auto lateDyeRequests = sfs::native::dye::restoreRequests[player.id];
  attach(player, playerNode);
  Check(sfs::native::dye::restoreRequests[player.id] == lateDyeRequests + 1,
        "late registered attachments must rearm dye even without a BodyMorph interface");
  const auto hhWithoutMorph = highHeelObservations;
  const auto npcNodes = g_registeredAppearanceNodes[npc.id].size();
  RE::NiAVObject heelOnlyRoot{npc.Get3D(false)};
  attach(npc, heelOnlyRoot);
  Check(highHeelObservations == hhWithoutMorph + 1 &&
            g_registeredAppearanceNodes[npc.id].size() == npcNodes,
        "high-heel observer must survive unavailable morph ABI without tracking it");
  g_bodyMorphInterface = &morph;
  Check(originalCalls != 0, "original public interface must still run");

  // Actual unload/delete cleanup, not the old test-only morph forget helper.
  const auto otherNodes = g_registeredAppearanceNodes[npc.id].size();
  for (unsigned cycle = 0; cycle != 128; ++cycle) {
    publish(player, false);
    player.loaded = true;
    playerNode.parent = player.Get3D(false);
    attach(player, playerNode); DrainAll();
    g_registeredAppearanceAttachmentRoots[player.id] = {&playerNode};
    g_registeredAppearanceHighHeelActors.insert(player.id);
    (void)g_queuedHighHeelSyncs.Start(player.id);
    g_pendingHighHeelResyncs.insert(player.id);
    QueueUpdateModelWeightAppearanceSync(player.id);
    player.loaded = false;
    ReleaseActorSceneResources(player.id, false);
    DrainAll();
    Check(!g_registeredAppearanceNodes.contains(player.id) &&
          !g_registeredAppearanceAttachmentRoots.contains(player.id) &&
          !g_registeredAppearanceHighHeelActors.contains(player.id) &&
          !g_morphRequests.HasRequest(player.id) &&
          !g_queuedHighHeelSyncs.Contains(player.id) &&
          !g_pendingHighHeelResyncs.contains(player.id), "unload frees scene roots and cancels actor-local work");
    Check(g_morphActivity.IsActive(player.id) && g_highHeelAttachmentActors.contains(player.id),
          "unload retains eligibility for DAVE reattachment before an SFS refresh");
    player.loaded = true;
    attach(player, playerNode); DrainAll(); // No publish/refresh before attachment.
    const auto before = morph.writes[player.id];
    ApplyBodyMorphsHook(&morph, &player, false);
    Check(morph.writes[player.id] == before + 1, "reload attachment resumes live morph without a new refresh");
  }
  Check(g_registeredAppearanceNodes[npc.id].size() == otherNodes, "unload never clears another actor");
  sfs::native::beforeDisplayedQuery = [&] {
    player.loaded = false;
    ReleaseActorSceneResources(player.id, false);
  };
  attach(player, playerNode); DrainAll();
  Check(!g_registeredAppearanceNodes.contains(player.id) && g_sceneObservations.Size() == 0,
        "unload during attachment cannot republish the old node or retain an observation");
  player.loaded = true;
  attach(player, playerNode); DrainAll();
  Check(g_registeredAppearanceNodes.contains(player.id) && g_sceneObservations.Size() == 0,
        "cancelled observation does not block next legitimate attachment");
  {
    const SceneObservation oldBuild(player.id);
    ReleaseActorSceneResources(player.id, false);
    RememberAndMorphNewNodes(&morph, &player, {&playerNode}, armor.id, false, false, &oldBuild);
    Check(!g_registeredAppearanceNodes.contains(player.id),
          "native capture started before unload cannot republish the old node");
  }
  attach(player, playerNode); DrainAll();
  QueueUpdateModelWeightAppearanceSync(player.id);
  RE::TESForm::forms.erase(player.id);
  ReleaseActorSceneResources(player.id, true);
  DrainAll();
  Check(!g_registeredAppearanceNodes.contains(player.id) &&
        !g_morphActivity.IsActive(player.id) && !g_highHeelAttachmentActors.contains(player.id),
        "delete removes eligibility and resources without looking up the form");
  for (RE::FormID id = 0xA000; id != 0xA080; ++id) {
    RE::Actor transient{id}; RE::NiAVObject node{transient.Get3D(false)};
    transient.displayed.insert(armor.id); publish(transient, false); attach(transient, node);
    RE::TESForm::forms.erase(id);
    ReleaseActorSceneResources(id, true); DrainAll();
    Check(!g_registeredAppearanceNodes.contains(id) && !g_morphActivity.IsActive(id),
          "128 distinct deleted actors leave no tracked scene records");
  }
  std::puts("RaceMenuMorphTrackingTests passed (production tracking functions, fake engine).");
}
