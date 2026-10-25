// Production metadata/root capture + resolver + transform/queue/lifecycle, with
// scene ownership and public/legacy NiOverride recording providers, not Skyrim.
#include "HighHeelRootFixture.inc"
#include <new>
#include <thread>
// Test executable only: count ordinary C++ allocations in the actual stable
// check body, excluding the fake scheduler's queue allocation before dispatch.
std::atomic_size_t allocations{0};
void* operator new(std::size_t size) {
  if (auto* p = std::malloc(size ? size : 1)) { ++allocations; return p; }
  throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void Attach(RE::Actor& actor, RE::NiAVObject& node) {
  node.parent = &actor.root; actor.root.children.emplace_back(&node);
}
void Detach(RE::Actor& actor, RE::NiAVObject& node) {
  std::erase_if(actor.root.children, [&](const auto& p) { return p.get() == &node; });
  node.parent = nullptr;
}
void SetHeelMetadata(RE::NiAVObject& node, float height, bool sdta) {
  node.hasHeel = !sdta;
  node.heel.value = height;
  node.sdta = sdta ? "[{\"name\":\"NPC\",\"pos\":[0,0," + std::to_string(height) + "]}]" : "";
}
void TestPositionMetadata() {
  RE::NiAVObject parent, child, last;
  parent.children.emplace_back(&child); child.parent = &parent;
  parent.children.emplace_back(&last); last.parent = &parent;
  parent.sdta = R"([{"name":"NPC","pos":[1,2,9]}])";
  const auto expect = [&](std::array<float, 3> value, const char* message) {
    Check(FindAutomaticNpcPosition(&parent) == value, message);
  };
  expect({1,2,9}, "SDTA selects complete NPC position, not just its height");
  parent.hasHeel = true; parent.heel.value = 7;
  expect({0,0,7}, "same-object HH overrides SDTA and explicitly resets X/Y like RaceMenu");
  child.sdta = R"([{"name":"NPC","pos":[3,4,-2.5]}])";
  expect({3,4,-2.5F}, "child SDTA overrides parent HH in parent-before-child order; negative offsets remain valid");
  last.hasHeel = true; last.heel.value = 0;
  expect({0,0,0}, "last sibling's explicit zero HH overrides earlier SDTA");
  last.heel.value = std::numeric_limits<float>::quiet_NaN();
  last.sdta = R"([{"name":"NPC","pos":[-1,6,0]}])";
  expect({-1,6,0}, "invalid HH cannot poison valid SDTA on the same node, including zero height");
  last.sdta = R"([{"name":"NPC","pos":[0,0,1e300]}])";
  expect({3,4,-2.5F}, "SDTA float overflow cannot overwrite an earlier finite source");
  const std::unordered_set<RE::NiAVObject*> hidden{&child};
  Check(FindAutomaticNpcPosition(&parent, &hidden) == std::array<float,3>{0,0,7},
        "remaining-source scan skips hidden subtree using the same metadata precedence");
  parent.children.clear(); child.parent = nullptr; last.parent = nullptr;
  parent.hasHeel = false;
  for (const char* invalid : {"", "[", "{}", "null",
      R"([{"name":"NPC Head [Head]","pos":[0,0,9]}])",
      R"([{"name":"NPC","scale":1,"rot":[0,0,9]}])",
      R"([{"name":"NPC","pos":[0,9]}])",
      R"([{"name":"NPC","pos":[0,0,9,1]}])",
      R"([{"name":"NPC","pos":[0,0,"9"]}])",
      R"([{"name":"NPC","pos":[0,null,9]}])",
      R"([{"name":7,"pos":[0,0,9]}])"}) {
    parent.sdta = invalid;
    Check(!SceneHasNpcPositionSource(&parent) && !FindAutomaticNpcPosition(&parent),
          "malformed data, non-NPC transforms and rotation/scale alone never enroll a heel root");
  }
  parent.sdta = R"([/* supported JSON comments */ {"name":"NPC","pos":[1,2,3]},
      {"name":"Other","pos":[9,9,9]}, {"name":"NPC","pos":[4,5,6]},
      {"name":"NPC","pos":[0,0,"invalid"]}])";
  expect({4,5,6}, "last valid NPC entry wins and unrelated or malformed later positions do not erase it");
  Check(child.references == 0 && last.references == 0,
        "metadata traversal retains no node references");
}
void TestRegisteredSdta(bool nativeCapture) {
  RE::Actor actor{0x14}; RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  Reset(actor); actor.displayed.insert(boots.id);
  RE::NiAVObject branch, shape;
  branch.children.emplace_back(&shape); shape.parent = &branch;
  shape.sdta = R"([{"name":"NPC","pos":[2,-3,9]}])";
  actor.scenePositions = {{2,-3,9}, {8,7,17}}; // a different actual boot wins RaceMenu's scan
  RegisteredAppearanceAttachmentObserver observer;
  auto before = sfs::native::racemenu::CaptureAttachmentScene(&actor);
  if (nativeCapture) {
    Attach(actor, branch);
    sfs::native::racemenu::MorphNewRegisteredAppearanceNodes(&actor, before, boots.id, true);
    sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor);
  } else {
    observer.OnAttach(&actor, &boots, &addon, &branch, false, &actor.root, &actor.root);
    SKSE::tasks.Drain();
    Check(branch.references == 1 && actor.fullUpdates == 0,
          "pre-graft SDTA root waits without reading detached metadata as an active position");
    Attach(actor, branch);
  }
  DrainAll();
  Check(actor.displayedHeight == 9 && actor.internalXY == std::array<float,2>{2,-3} &&
        g_registeredAppearanceAttachmentRoots.at(actor.id).size() == 1,
        "callback/native capture selects child SDTA over different actual boots without a body-morph interface");
  const auto queue = [&] { sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); };
  const std::array<RE::FormID,1> actors{actor.id};
  for (bool removeHeel : {false, true}) {
    g_highHeelScenes.Update(0x120, actors, removeHeel);
    actor.sexLabPosition = -4.5F; actor.otherModHeight = 3;
    queue(); DrainAll();
    Check(actor.displayedHeight == 3 && actor.internalPosition == 4.5F &&
          actor.internalXY == std::array<float,2>{2,-3} && actor.sexLabPosition == -4.5F,
          "SDTA scene correction changes only Z, preserves X/Y and SexLab's key with stripped feet ON/OFF");
  }
  const auto metadataReads = RE::NiAVObject::metadataReads;
  const auto reads = actor.positionReads, updates = actor.fullUpdates, inventory = actor.inventoryReads;
  const auto vmCalls = RE::BSScript::Internal::vm.calls.size();
  for (unsigned i = 0; i < 128; ++i) {
    sfs::native::racemenu::QueueHighHeelSceneCheck();
    const auto allocated = allocations.load();
    SKSE::tasks.Drain();
    if (allocations != allocated) Check(false, "SDTA stable check body must not allocate");
  }
  Check(RE::NiAVObject::metadataReads == metadataReads && actor.positionReads == reads &&
        actor.fullUpdates == updates && actor.inventoryReads == inventory &&
        RE::BSScript::Internal::vm.calls.size() == vmCalls,
        "128 stable SDTA frames perform no metadata scan/JSON parse, inventory query, transform update or VM call");
  actor.sexLabPosition.reset(); actor.otherModHeight = 0;
  g_highHeelScenes.Update(0x120, {}); queue(); DrainAll();
  Check(actor.displayedHeight == 9 && actor.internalXY == std::array<float,2>{2,-3},
        "scene exit restores current registered SDTA XYZ");
  shape.sdta = R"([{"name":"NPC","pos":[5,6,-2]}])"; queue(); DrainAll();
  Check(actor.displayedHeight == -2 && actor.internalXY == std::array<float,2>{5,6},
        "next event rereads changed SDTA metadata instead of reusing a stale offset");
  RE::NiAVObject actual; actual.sdta = R"([{"name":"NPC","pos":[-2,4,7]}])";
  Attach(actor, actual); actor.scenePositions = {{-2,4,7}, {5,6,-2}};
  actor.displayed.clear(); queue(); DrainAll();
  Check(actor.displayedHeight == 7 && actor.internalXY == std::array<float,2>{-2,4},
        "hidden registered SDTA root leaves actual SDTA XYZ intact even if its stale metadata scans last");
  actor.displayed.insert(boots.id); queue(); DrainAll();
  Check(actor.displayedHeight == -2 && actor.internalXY == std::array<float,2>{5,6},
        "manual SDTA redress restores current metadata without replacing the actual equipment");
  Detach(actor, actual); Detach(actor, branch);
  actor.displayed.clear(); actor.scenePositions.clear(); queue(); DrainAll();
  Check(!actor.internalPosition && branch.references == 0 && g_queuedHighHeelSyncs.Size() == 0,
        "stripped SDTA root and automatic position expire without retained references or polling");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);
  branch.children.clear(); shape.parent = nullptr;
  Check(shape.references == 0 && actor.root.references == 0, "SDTA native/callback lifecycle releases child and skeleton");
}
void TestReportedFootwearSequence(bool sdta, bool removeHeel, bool initiallyRetained) {
  RE::Actor actor{0x14}; RE::TESObjectARMO outfit{0x910}, physical{0x920};
  RE::TESObjectARMA addon{0x911}; physical.slots.bits = 0x80;
  Reset(actor); actor.displayed.insert(outfit.id);
  actor.inventory.feet = initiallyRetained ? &physical : nullptr;
  RE::NiAVObject heel; SetHeelMetadata(heel, 9, sdta); Attach(actor, heel);
  actor.sceneOffsets = {9};
  const std::array<RE::FormID,1> actors{actor.id};
  g_highHeelScenes.Update(0x120, actors, removeHeel);
  if (removeHeel) actor.sexLabPosition = -4.5F;
  // Reproduce the known initial-application overwrite boundary from v1.7.8.
  // No manual equipment/visibility action may be required to recover it.
  actor.rescanAfterUpdates = 1;
  RegisteredAppearanceAttachmentObserver observer;
  observer.OnAttach(&actor, &outfit, &addon, &heel, false, &actor.root, &actor.root);
  DrainAll();
  const float initialHeight = !removeHeel && initiallyRetained ? 9.0F : 0.0F;
  Check(actor.displayedHeight == initialHeight && actor.displayed.contains(outfit.id) &&
        actor.inventory.feet == (initiallyRetained ? &physical : nullptr),
        "reported initial stripped/retained-foot sequence reaches current ON/OFF target without manual hide/equip");

  // In the reported ON case, either flat or heeled actual footwear used to
  // trigger recovery. Execute the actual production slot-37 event body here.
  for (float actualHeight : {0.0F, 13.0F}) {
    actor.inventory.feet = &physical;
    actor.sceneOffsets = {9, actualHeight};
    sfs::native::racemenu::ObserveHighHeelEquipmentEvent(&actor, &physical);
    Check(!SKSE::tasks.pending.empty(), "flat and heeled physical boots both request slot-37 reconciliation");
    DrainAll();
    Check(actor.displayedHeight == (removeHeel ? 0.0F : 9.0F) && actor.inventory.feet == &physical,
          "ON remains aligned with either actual shoe; OFF preserves displayed heel policy while actual feet are retained");
    actor.inventory.feet = nullptr; actor.sceneOffsets = {9};
    sfs::native::racemenu::ObserveHighHeelEquipmentEvent(&actor, &physical); DrainAll();
    Check(actor.displayedHeight == 0 && actor.displayed.contains(outfit.id),
          "subsequent actual footwear stripping aligns ON/OFF without hiding the registered heel");
  }
  const auto correction = actor.sexLabPosition;
  actor.displayed.clear();
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); DrainAll();
  Check(actor.displayedHeight == 0 && actor.sexLabPosition == correction,
        "hiding the scene's registered heel preserves alignment and SexLab's correction key");
  actor.displayed.insert(outfit.id);
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); DrainAll();
  Check(actor.displayedHeight == 0 && actor.inventory.feet == nullptr,
        "showing the registered heel again stays aligned without equipping any real item");
  actor.sexLabPosition.reset(); g_highHeelScenes.Update(0x120, {});
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); DrainAll();
  Check(actor.displayedHeight == 9 && actor.inventory.feet == nullptr,
        "scene exit restores displayed heel height without redressing physical inventory");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true); Detach(actor, heel);
  Check(heel.references == 0 && actor.root.references == 0 && g_queuedHighHeelSyncs.Size() == 0,
        "reported footwear sequence leaves no queued work or owned scene references");
}
void TestSceneHeight(bool sdta) {
  RE::Actor actor{0x14}; RE::TESObjectARMO armor{0x910}; RE::TESObjectARMA addon{0x911};
  RE::NiAVObject branch; SetHeelMetadata(branch, 9, sdta);
  Reset(actor); actor.displayed.insert(armor.id); actor.sceneOffsets = {9};
  Attach(actor, branch);
  RegisteredAppearanceAttachmentObserver observer;
  observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
  const auto queue = [&] { sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); };
  actor.displayed.clear(); actor.internalPosition = 9.0F;
  DrainAll();
  Check(actor.displayedHeight == 0,
        "hide after RaceMenu attach but before first SFS success still clears the owned automatic height");
  actor.displayed.insert(armor.id); queue();
  DrainAll();
  const std::array<RE::FormID, 1> actors{actor.id};
  RE::TESObjectARMO actualBoots{0x920};
  actor.inventory.feet = &actualBoots;
  g_highHeelScenes.Update(0x120, actors, false); queue(); DrainAll();
  Check(actor.displayedHeight == 9,"OFF with retained physical boots preserves registered heel height");
  actor.inventoryAvailable = false; queue(); DrainAll();
  Check(actor.displayedHeight == 9,"OFF does not mistake unavailable physical inventory for proof of barefoot state");
  actor.inventoryAvailable = true;
  actor.inventory.feet = nullptr; queue(); DrainAll();
  Check(actor.displayedHeight == 0,"OFF with actual feet stripped aligns registered heels to barefoot animation");
  actor.inventory.feet = &actualBoots; queue(); DrainAll();
  Check(actor.displayedHeight == 9,"physical feet redress during OFF scene restores ordinary registered height");
  g_highHeelScenes.Update(0x120, actors);
  queue(); DrainAll();
  Check(actor.displayedHeight == 0, "ON suppresses registered heel height even while actual boots are retained");
  actor.inventoryAvailable = false; queue(); DrainAll();
  Check(actor.displayedHeight == 0,"ON remains effective when physical inventory cannot be read");
  actor.inventoryAvailable = true;
  actor.inventory.feet = nullptr; queue(); DrainAll();
  Check(actor.displayedHeight == 0, "ON with stripped feet and absent SexLab transform still suppresses registered height");
  for (float correction : {-9.0F, -7.0F, 0.0F}) {
    actor.sexLabPosition = correction; actor.otherModHeight = 3;
    queue(); DrainAll();
    Check(actor.displayedHeight == 3 && actor.sexLabPosition == correction && !actor.temporary,
          "matching/stale/zero SexLab correction is preserved without double height; unrelated +3 survives");
  }
  actor.otherModHeight = 0; actor.sexLabPosition.reset();
  g_highHeelScenes.Update(0x120, {}); queue(); DrainAll();
  Check(actor.displayedHeight == 9, "scene end restores currently displayed heels, not a saved outfit");
  g_highHeelScenes.Update(0x120, actors); queue(); SKSE::tasks.Drain();
  g_highHeelScenes.Update(0x120, {}); queue(); DrainAll();
  Check(actor.displayedHeight == 9 && !actor.temporary,
        "scene ending during an outstanding legacy/public update finishes at current height");
  RE::Actor ordinary{0x25}; ordinary.sceneOffsets = {7}; ordinary.internalPosition = 7.0F;
  const std::array<RE::FormID, 1> ordinaryIDs{ordinary.id};
  g_highHeelScenes.Update(0x121, ordinaryIDs);
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&ordinary); DrainAll();
  Check(ordinary.fullUpdates == 0 && ordinary.inventoryReads == 0 && ordinary.internalPosition == 7.0F,
        "scene member without any SFS-owned heel does not change real-equipment height");
  g_highHeelScenes.Update(0x121, {});

  // A hidden root can outlive ALL bounded tasks; its metadata is not visible
  // footwear. Keep a real SDTA source and put the obsolete root last in scan order.
  RE::NiAVObject actual;
  actual.sdta = R"([{"name":"NPC","pos":[3,4,7]}])";
  Attach(actor, actual); actor.sceneOffsets = {7, 9}; actor.displayed.clear();
  queue(); DrainAll();
  Check(actor.displayedHeight == 7 && actor.internalXY == std::array<float, 2>{3, 4},
        "hidden attached heel cannot overwrite remaining actual SDTA position, including X/Y");
  // Re-enroll, then hide the last source without an actual source.
  Detach(actor, actual); actor.displayed.insert(armor.id); actor.sceneOffsets = {9};
  queue(); DrainAll(); actor.displayed.clear();
  queue(); DrainAll();
  Check(actor.displayedHeight == 0 && !actor.internalPosition && SKSE::tasks.pending.empty(),
        "last hidden heel clears now, even if physical root detaches after retry window");
  Detach(actor, branch); actor.sceneOffsets.clear(); DrainAll();
  Check(actor.displayedHeight == 0, "late removal needs no extra callback to clear already-reconciled height");

  actor.displayed.insert(armor.id); Attach(actor, branch); actor.sceneOffsets = {9};
  observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
  DrainAll();
  actor.displayed.clear(); actor.actualVisible.insert(armor.id); queue(); DrainAll();
  Check(actor.displayedHeight == 9, "same ARMO still visible as actual gear retains its genuine heel height");
  actor.actualVisible.clear(); actor.displayed.insert(armor.id); queue(); DrainAll();

  g_highHeelScenes.Update(0x120, actors); actor.sexLabPosition = -9.0F;
  queue(); DrainAll(); actor.displayed.clear(); queue(); DrainAll();
  Check(actor.displayedHeight == 0, "redress-OFF hidden heel stays aligned while scene compensation exists");
  actor.sexLabPosition.reset(); g_highHeelScenes.Update(0x120, {}); queue(); DrainAll();
  Check(actor.displayedHeight == 0 && actor.displayed.empty(), "scene end with redress-OFF does not restore hidden appearances");
  actor.displayed.insert(armor.id); queue(); DrainAll();
  Check(actor.displayedHeight == 9, "manual display after redress-OFF restores ordinary height");

  SetHeelMetadata(branch, 0, sdta); actor.sceneOffsets = {0}; g_highHeelScenes.Update(0x120, actors);
  queue(); DrainAll();
  Check(actor.displayedHeight == 0, "registered flat footwear remains flat during the scene");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);
  Check(!g_highHeelScenes.Contains(actor.id), "deleted actor releases scene ownership");
  Detach(actor, branch);
  Check(branch.references == 0, "scene regression retains no heel root references");
}

void TestLateFullRescan(bool sdta) {
  RE::Actor actor{0x14}; RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  Reset(actor); actor.displayed.insert(boots.id); actor.sceneOffsets = {9};
  RE::NiAVObject heel; SetHeelMetadata(heel, 9, sdta); Attach(actor, heel);
  const std::array<RE::FormID, 1> actors{actor.id};
  g_highHeelScenes.Update(0x120, actors, true);
  RegisteredAppearanceAttachmentObserver observer;
  observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
  SKSE::tasks.Drain();
  if (g_transformInterfaceVersion < 3) {
    const auto pendingBefore = SKSE::tasks.pending.size();
    for (unsigned frame = 0; frame < 256; ++frame) {
      sfs::native::racemenu::QueueHighHeelSceneCheck();
    }
    Check(!g_hasSceneHeightWatches && SKSE::tasks.pending.size() == pendingBefore,
          "waiting for legacy completion does not schedule frame checks");
  }
  DrainAll();
  Check(actor.displayedHeight == 0 && g_sceneHeightWatches.size() == 1,
        "ON scene arms one watch after successful height reconciliation");
  const auto idleUpdates = actor.fullUpdates, idleInventoryReads = actor.inventoryReads;
  const auto idleVmCalls = RE::BSScript::Internal::vm.calls.size();
  const auto idleMetadataReads = RE::NiAVObject::metadataReads;
  for (unsigned frame = 0; frame < 256; ++frame) {
    sfs::native::racemenu::QueueHighHeelSceneCheck();
    sfs::native::racemenu::QueueHighHeelSceneCheck();
    if (SKSE::tasks.pending.size() != 1) { Check(false, "frame checks coalesce"); }
    const auto beforeCheckAllocations = allocations.load();
    SKSE::tasks.Drain();
    if (allocations != beforeCheckAllocations) {
      Check(false, "stable small-scene check body must not allocate");
    }
  }
  Check(true, "256 stable small-scene checks allocate no temporary actor lists");
  Check(actor.fullUpdates == idleUpdates && actor.inventoryReads == idleInventoryReads &&
        RE::BSScript::Internal::vm.calls.size() == idleVmCalls &&
        RE::NiAVObject::metadataReads == idleMetadataReads,
        "256 stable scene frames do not scan metadata/inventory, call Papyrus or recalculate transforms");
  for (unsigned i = 0; i < 16; ++i) {
    // No attachment or SexLab position-key event. Models full SetTransforms
    // from UpdateAllReferenceTransforms, ActorUpdateManager or its private task.
    skee::transform.UpdateNodeAllTransforms(&actor);
    Check(actor.displayedHeight == 9, "late full rescan restores HH=9 before repair");
    sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainAll();
    Check(actor.displayedHeight == 0 && !actor.temporary,
          "late full rescan is repaired without changing any mod key or retaining bootstrap");
  }
  actor.otherModHeight = 3;
  skee::transform.UpdateNodeAllTransforms(&actor);
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainAll();
  Check(actor.displayedHeight == 3 && actor.otherModHeight == 3,
        "repair preserves a newly changed unrelated mod height, not the old skeleton result");
  actor.sexLabPosition = -6.0F;
  skee::transform.UpdateNodeAllTransforms(&actor);
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainAll();
  Check(actor.displayedHeight == 3 && actor.sexLabPosition == -6,
        "late rescan with a new SexLab key reconciles using current compensation");
  g_highHeelScenes.Update(0x120, {}); actor.sexLabPosition.reset();
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); DrainAll();
  Check(actor.displayedHeight == 12 && g_sceneHeightWatches.empty() && actor.root.references == 0,
        "scene end restores ordinary heel and releases watched skeleton reference");
  sfs::native::racemenu::QueueHighHeelSceneCheck();
  Check(SKSE::tasks.pending.empty(), "no frame task remains outside an owned scene");
  // OFF + actual footwear retained uses the same late-rescan repair but keeps
  // the registered heel. Bare actual feet continue to follow scene alignment.
  g_highHeelScenes.Update(0x120, actors, false); actor.inventory.feet = &boots;
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); DrainAll();
  actor.sceneOffsets = {9, 15}; skee::transform.UpdateNodeAllTransforms(&actor);
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainAll();
  Check(actor.displayedHeight == 12, "OFF retained footwear preserves registered height after unrelated scan order");
  actor.inventory.feet = nullptr;
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); DrainAll();
  skee::transform.UpdateNodeAllTransforms(&actor);
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainAll();
  Check(actor.displayedHeight == 3, "OFF with actual feet stripped still repairs barefoot alignment");
  skee::transform.UpdateNodeAllTransforms(&actor);
  auto *savedTransform = g_transformInterface.load();
  g_transformInterface = nullptr;
  RE::BSScript::Internal::vm.fail = "AddNodeTransformScale";
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainAll();
  const auto failedUpdates = actor.fullUpdates;
  const auto failedVmCalls = RE::BSScript::Internal::vm.calls.size();
  for (unsigned frame = 0; frame < 64; ++frame) {
    sfs::native::racemenu::QueueHighHeelSceneCheck();
    Check(SKSE::tasks.pending.empty(), "failed provider leaves no idle frame-check task");
  }
  Check(g_sceneHeightWatches.empty() && actor.root.references == 0 && !g_hasSceneHeightWatches,
        "failed provider releases its watch and skeleton immediately");
  Check(actor.fullUpdates == failedUpdates && RE::BSScript::Internal::vm.calls.size() == failedVmCalls,
        "provider failure disarms repair rather than retrying transforms or Papyrus every frame");
  g_transformInterface = savedTransform; RE::BSScript::Internal::vm.fail.clear();
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); DrainAll();
  Check(actor.displayedHeight == 3 && g_sceneHeightWatches.at(actor.id).armed,
        "next legitimate event recovers and rearms after a temporary provider failure");
  const auto oldGeneration = g_sceneHeightWatches.at(actor.id).generation;
  skee::transform.UpdateNodeAllTransforms(&actor);
  sfs::native::racemenu::QueueHighHeelSceneCheck(); SKSE::tasks.Drain();
  actor.loaded = false; DrainAll();
  Check(g_sceneHeightWatches.empty() && actor.root.references == 0 && !g_hasSceneHeightWatches,
        "actor becoming unavailable before repair starts cannot retain a disarmed older watch");
  actor.loaded = true;
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); DrainAll();
  FinishQueuedHighHeelSync(actor.id, oldGeneration);
  Check(g_sceneHeightWatches.at(actor.id).armed && actor.displayedHeight == 3,
        "old completion cannot discard a newer successful watch");
  sfs::native::racemenu::QueueHighHeelSceneCheck();
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false); DrainAll();
  Check(g_sceneHeightWatches.empty() && actor.root.references == 0,
        "unload cancels an already-queued check and releases its watched skeleton");
  for (unsigned cycle = 0; cycle < 128; ++cycle) {
    g_highHeelScenes.Update(0x120, actors, true);
    g_highHeelAttachmentActors.insert(actor.id);
    observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
    DrainAll();
    sfs::native::racemenu::QueueHighHeelSceneCheck();
    sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false);
    DrainAll();
    if (!g_sceneHeightWatches.empty() || actor.root.references != 0 || g_hasSceneHeightWatches) {
      Check(false, "repeated watched scenes release every skeleton reference");
    }
  }
  Check(g_sceneHeightWatches.empty() && actor.root.references == 0,
        "128 watched scene/unload cycles retain no skeletons or watch state");
  // A stale frame check from before revert must not examine a replacement scene.
  observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root); DrainAll();
  sfs::native::racemenu::QueueHighHeelSceneCheck();
  sfs::native::racemenu::ForgetAllRegisteredAppearanceNodes();
  const auto fullUpdates = actor.fullUpdates;
  DrainAll();
  Check(g_sceneHeightWatches.empty() && actor.root.references == 0 && actor.fullUpdates == fullUpdates,
        "revert invalidates pending frame checks and releases their nodes");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);
  Detach(actor, heel);
}

void TestInitialAppliedHeight(bool sdta) {
  RE::Actor actor{0x14}; RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  RE::NiAVObject heel; SetHeelMetadata(heel, 9, sdta);
  Reset(actor); actor.displayed.insert(boots.id); actor.sceneOffsets = {9};
  Attach(actor, heel);
  const std::array<RE::FormID, 1> actors{actor.id};
  g_highHeelScenes.Update(0x120, actors, true);
  actor.sexLabPosition = -4.5F; actor.otherModHeight = 3;
  actor.rescanAfterUpdates = 1;
  RegisteredAppearanceAttachmentObserver observer;
  observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
  DrainAll();
  for (unsigned i = 0; i < 4; ++i) {
    sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainAll();
  }
  Check(actor.displayedHeight == 3 && actor.internalPosition == 4.5F &&
        actor.sexLabPosition == -4.5F && !actor.temporary,
        "first apply overwritten before baseline capture is repaired, not accepted as a stable raised height");

  const auto queue = [&] { sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); };
  auto scans = actor.fullUpdates;
  actor.ignoredPositionWrites = 1;
  queue(); DrainAll();
  Check(actor.displayedHeight == 3 && actor.fullUpdates == scans + 2,
        "lost first internal write is read back and retried once using the current state");

  bool changedCorrection = false;
  actor.afterPositionRead = [&](std::string_view key) {
    if (!changedCorrection && key == "internal") {
      changedCorrection = true;
      actor.sexLabPosition = -2;
      actor.SetDisplayedHeight(actor.internalPosition.value_or(0) + 1);
    }
  };
  queue(); DrainAll(); actor.afterPositionRead = {};
  Check(changedCorrection && actor.internalPosition == 2 && actor.displayedHeight == 3 &&
        actor.sexLabPosition == -2,
        "SexLab changing between apply and verification causes a current-value repair without changing its key");

  bool closedScene = false;
  actor.afterPositionRead = [&](std::string_view key) {
    if (!closedScene && key == "internal") {
      closedScene = true;
      actor.sexLabPosition.reset();
      g_highHeelScenes.Update(0x120, {});
      queue();
    }
  };
  queue(); DrainAll(); actor.afterPositionRead = {};
  Check(closedScene && actor.displayedHeight == 12 && g_sceneHeightWatches.empty(),
        "scene ending during first-apply verification restores current visible heels through the pending event");

  g_highHeelScenes.Update(0x120, actors, true); actor.sexLabPosition = -4.5F;
  scans = actor.fullUpdates; actor.ignoredPositionWrites = 100;
  queue(); DrainAll();
  Check(actor.fullUpdates == scans + 3 && g_sceneHeightWatches.empty() &&
        !g_hasSceneHeightWatches && !actor.temporary && g_queuedHighHeelSyncs.Size() == 0,
        "permanent readback mismatch stops at the shared three-attempt budget and releases the unarmed watch");
  for (unsigned i = 0; i < 128; ++i) sfs::native::racemenu::QueueHighHeelSceneCheck();
  Check(SKSE::tasks.pending.empty(), "failed initial verification cannot start idle polling");
  actor.ignoredPositionWrites = 0; queue(); DrainAll();
  Check(actor.displayedHeight == 3 && g_hasSceneHeightWatches,
        "verification failure never disables a later legitimate synchronization");

  const auto reads = actor.positionReads, updates = actor.nodeUpdates;
  const auto inventoryReads = actor.inventoryReads;
  for (unsigned i = 0; i < 128; ++i) {
    sfs::native::racemenu::QueueHighHeelSceneCheck(); SKSE::tasks.Drain();
  }
  Check(actor.positionReads == reads && actor.nodeUpdates == updates &&
        actor.inventoryReads == inventoryReads && RE::BSScript::Internal::vm.pending.empty(),
        "verified stable frames add no transform readbacks, writes, inventory reads or Papyrus work");

  bool hidden = false;
  actor.afterPositionRead = [&](std::string_view key) {
    if (!hidden && key == "internal") { hidden = true; actor.displayed.clear(); queue(); }
  };
  queue(); DrainAll(); actor.afterPositionRead = {};
  actor.sexLabPosition.reset(); g_highHeelScenes.Update(0x120, {}); queue(); DrainAll();
  Check(hidden && actor.displayedHeight == 3 && actor.displayed.empty() &&
        !g_registeredAppearanceHighHeelActors.contains(actor.id),
        "strip during verification and redress OFF never restore hidden heels or keep scene compensation");
  actor.displayed.insert(boots.id); queue(); DrainAll();
  Check(actor.displayedHeight == 12, "manual redress after verified cleanup restores the current registered heel");

  if (g_transformInterfaceVersion < 3) {
    g_highHeelScenes.Update(0x120, actors, true);
    queue(); SKSE::tasks.Drain();
    for (unsigned i = 0; i < 16 && SKSE::tasks.pending.empty(); ++i) RE::BSScript::Internal::vm.Drain();
    Check(!SKSE::tasks.pending.empty(), "legacy completion schedules its scene sample on the game task");
    SKSE::tasks.Drain();
    Check(!RE::BSScript::Internal::vm.pending.empty(), "legacy readback is pending before unload boundary");
    sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false);
    Check(actor.root.references == 0 && heel.references == 1,
          "pending verification adds no retained node beyond the attachment itself");
    DrainAll();
    Check(g_sceneHeightWatches.empty() && g_queuedHighHeelSyncs.Size() == 0,
          "stale legacy verification cannot publish ownership after unload");
  }
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);
  Detach(actor, heel);
  Check(heel.references == 0 && actor.root.references == 0,
        "initial verification retains no node after unload");
}
void TestLargeSceneWatch() {
  RE::Actor resetActor{0x14}; Reset(resetActor);
  RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  std::vector<std::unique_ptr<RE::Actor>> actors;
  std::vector<std::unique_ptr<RE::NiAVObject>> heels;
  std::array<RE::FormID, 33> ids{};
  RegisteredAppearanceAttachmentObserver observer;
  for (std::size_t i = 0; i < ids.size(); ++i) {
    ids[i] = 0x4000 + static_cast<RE::FormID>(i);
    actors.push_back(std::make_unique<RE::Actor>(ids[i]));
    heels.push_back(std::make_unique<RE::NiAVObject>(true));
    auto &actor = *actors.back(); auto &heel = *heels.back();
    heel.heel.value = 9; actor.sceneOffsets = {9}; actor.displayed.insert(boots.id);
    g_highHeelAttachmentActors.insert(actor.id);
    Attach(actor, heel);
  }
  g_highHeelScenes.Update(0x120, ids, true);
  for (std::size_t i = 0; i < ids.size(); ++i) {
    auto &actor = *actors[i];
    observer.OnAttach(&actor, &boots, &addon, heels[i].get(), false, &actor.root, &actor.root);
  }
  DrainAll();
  Check(g_sceneHeightWatches.size() == ids.size(), "large scene watches every owned heel");
  for (auto &actor : actors) { skee::transform.UpdateNodeAllTransforms(actor.get()); }
  sfs::native::racemenu::QueueHighHeelSceneCheck();
  sfs::native::racemenu::QueueHighHeelSceneCheck();
  Check(SKSE::tasks.pending.size() == 1, "33 actors share one frame-check task");
  DrainAll();
  bool allRepaired = true;
  for (const auto &actor : actors) { allRepaired &= actor->displayedHeight == 0; }
  Check(allRepaired, "overflow beyond inline snapshot capacity never drops a participant");

  // An unavailable actor can disappear without a delivered unload event.
  RE::TESForm::forms.erase(ids[0]);
  actors[1]->loaded = false;
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainAll();
  Check(g_sceneHeightWatches.size() == ids.size() - 2 &&
        actors[0]->root.references == 0 && actors[1]->root.references == 0,
        "missing/unloaded actors release watches without disabling other participants");
  for (std::size_t i = 0; i < ids.size(); ++i) {
    sfs::native::racemenu::ReleaseActorSceneResources(ids[i], true);
    Detach(*actors[i], *heels[i]);
    RE::TESForm::forms.erase(ids[i]);
  }
  Check(g_sceneHeightWatches.empty() && !g_hasSceneHeightWatches,
        "large scene releases every watch and returns to inactive fast path");
  for (unsigned frame = 0; frame < 1024; ++frame) {
    sfs::native::racemenu::QueueHighHeelSceneCheck();
  }
  Check(SKSE::tasks.pending.empty(), "1024 inactive frames enqueue no work");
}
void TestResetReleasesOutsideLocks() {
  RE::Actor actor{0x14}; Reset(actor);
  RE::NiAVObject morph, heel, skeleton;
  unsigned releases = 0;
  const auto onFinalRelease = [&] {
    bool nodeUnlocked = false, queueUnlocked = false;
    // try_lock on a different thread: never recursively lock std::mutex.
    std::thread probe([&] {
      nodeUnlocked = g_nodeMutex.try_lock();
      if (nodeUnlocked) { g_nodeMutex.unlock(); }
      queueUnlocked = g_highHeelQueueMutex.try_lock();
      if (queueUnlocked) { g_highHeelQueueMutex.unlock(); }
    });
    probe.join();
    Check(nodeUnlocked && queueUnlocked, "final scene-reference release runs outside both SFS mutexes");
    ++releases;
  };
  morph.onFinalRelease = heel.onFinalRelease = skeleton.onFinalRelease = onFinalRelease;
  g_registeredAppearanceNodes[actor.id].push_back({&morph, 0x910, 0x911, false});
  g_registeredAppearanceAttachmentRoots[actor.id].push_back({&heel, 0x910, false});
  g_sceneHeightWatches.emplace(actor.id, SceneHeightWatch{&skeleton, 1, 0, true});
  g_hasSceneHeightWatches = true;
  sfs::native::racemenu::ForgetAllRegisteredAppearanceNodes();
  Check(releases == 3 && morph.references == 0 && heel.references == 0 && skeleton.references == 0,
        "save/reset releases morph roots, heel roots and watched skeletons exactly once");
}
void TestDiscardedSceneCheck() {
  RE::Actor actor{0x14}; Reset(actor);
  RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  RE::NiAVObject heel{true}; heel.heel.value = 9; Attach(actor, heel);
  actor.displayed.insert(boots.id); actor.sceneOffsets = {9};
  const std::array<RE::FormID, 1> ids{actor.id};
  RegisteredAppearanceAttachmentObserver observer;
  const auto start = [&] {
    g_highHeelScenes.Update(0x120, ids, true);
    g_highHeelAttachmentActors.insert(actor.id);
    observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
    DrainAll();
  };
  start();
  sfs::native::racemenu::QueueHighHeelSceneCheck();
  // Simulate an engine queue discarding old work on load. Do not rely on that
  // old callback ever executing to make the next save's guard usable again.
  auto oldTasks = std::move(SKSE::tasks.pending); SKSE::tasks.pending.clear();
  sfs::native::racemenu::ForgetAllRegisteredAppearanceNodes();
  start();
  skee::transform.UpdateNodeAllTransforms(&actor);
  sfs::native::racemenu::QueueHighHeelSceneCheck();
  Check(SKSE::tasks.pending.size() == 1, "discarded old check cannot block checks in the next save");
  // If the old task instead arrives late, it must not clear the new guard.
  for (auto &task : oldTasks) { task(); }
  sfs::native::racemenu::QueueHighHeelSceneCheck();
  Check(SKSE::tasks.pending.size() == 1, "late old check cannot clear or duplicate the new queued check");
  DrainAll();
  Check(actor.displayedHeight == 0, "new save still repairs a late rescan after stale task delivery");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);
  Detach(actor, heel);
}
// SKSE drains jobs appended by another job in the SAME ProcessTasks call.
// A queue turn is not a rendered frame; the old batch-only fake hid that gap.
void DrainSkseTurn() {
  auto& vm = RE::BSScript::Internal::vm;
  for (unsigned turn = 0; turn < 64 &&
      (!SKSE::tasks.pending.empty() || !vm.pending.empty()); ++turn) {
    SKSE::tasks.Drain(); vm.Drain();
  }
  Check(SKSE::tasks.pending.empty() && vm.pending.empty(),
        "one SKSE drain reaches idle without a self-enqueuing retry loop");
}
void TestRetryWaitsForFrame(bool sdta, bool removeHeel) {
  RE::Actor actor{0x14}; Reset(actor);
  RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  RE::NiAVObject heel; SetHeelMetadata(heel, 9, sdta);
  actor.displayed.insert(boots.id);
  const std::array<RE::FormID, 1> ids{actor.id};
  g_highHeelScenes.Update(0x120, ids, removeHeel);
  RegisteredAppearanceAttachmentObserver observer;
  observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
  DrainSkseTurn();
  Check(heel.references == 1 && g_registeredAppearanceAttachmentRoots.contains(actor.id) &&
        actor.fullUpdates == 0,
        "unready HH/SDTA root survives one complete SKSE drain until a later frame");
  // No second attachment callback, physical shoes, or new SexLab event.
  Attach(actor, heel); actor.sceneOffsets = {9};
  skee::transform.UpdateNodeAllTransforms(&actor);
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainSkseTurn();
  Check(actor.displayedHeight == 0 && g_sceneHeightWatches.contains(actor.id),
        "next-frame graft reconciles barefoot scene without equipping any actual shoe");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);
  Detach(actor, heel);
  Check(heel.references == 0 && actor.root.references == 0,
        "frame-delayed repair releases heel and watch ownership on unload");
}
void TestProviderReadyNextFrame(bool sdta) {
  RE::Actor actor{0x14}; Reset(actor);
  RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  RE::NiAVObject heel; SetHeelMetadata(heel, 9, sdta); Attach(actor, heel);
  actor.displayed.insert(boots.id); actor.sceneOffsets = {9};
  const std::array<RE::FormID, 1> ids{actor.id};
  g_highHeelScenes.Update(0x120, ids, true);
  // Provider boundary: the first full update has not supplied internal yet.
  // The fixture switch models that result, not a change to the user's INI.
  actor.equippableTransforms = false;
  RegisteredAppearanceAttachmentObserver observer;
  observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
  DrainSkseTurn();
  Check(g_highHeelFrameRetries.contains(actor.id) && !actor.temporary && actor.fullUpdates == 1,
        "public and legacy routes clean bootstrap and defer a missing automatic source");
  actor.equippableTransforms = true;
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainSkseTurn();
  Check(actor.displayedHeight == 0 && actor.fullUpdates == 2 && !g_hasHighHeelFrameRetries,
        "provider readiness next frame repairs scene without an actual-footwear event");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true); Detach(actor, heel);
}
void TestFrameRetryLifetime(bool sdta) {
  RE::Actor actor{0x14}; Reset(actor);
  RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  RE::NiAVObject heel; SetHeelMetadata(heel, 9, sdta);
  actor.displayed.insert(boots.id);
  RegisteredAppearanceAttachmentObserver observer;
  const auto observe = [&] {
    observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
    DrainSkseTurn();
  };
  observe();
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainSkseTurn();
  Check(heel.references == 1 && g_hasHighHeelFrameRetries,
        "unready attachment receives a second distinct-frame opportunity");
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainSkseTurn();
  Check(heel.references == 0 && !g_hasHighHeelFrameRetries &&
        g_highHeelFrameRetries.empty() && g_queuedHighHeelSyncs.Size() == 0,
        "never-grafted root expires after initial plus two frame-separated attempts");
  for (unsigned frame = 0; frame < 1024; ++frame) sfs::native::racemenu::QueueHighHeelSceneCheck();
  Check(SKSE::tasks.pending.empty() && actor.fullUpdates == 0,
        "expired retry leaves no idle tasks or transform work");

  // Scene end and visibility changes must be resolved at retry time, not from
  // a captured target. No automatic redress and no stale barefoot suppression.
  const std::array<RE::FormID, 1> ids{actor.id};
  g_highHeelScenes.Update(0x120, ids, true); observe();
  g_highHeelScenes.Update(0x120, {});
  Attach(actor, heel); actor.sceneOffsets = {9};
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainSkseTurn();
  Check(actor.displayedHeight == 9 && g_sceneHeightWatches.empty(),
        "scene ending while deferred restores current visible height without changing equipment");
  Detach(actor, heel); actor.sceneOffsets.clear(); observe();
  actor.displayed.clear();
  sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainSkseTurn();
  Check(actor.displayedHeight == 0 && heel.references == 0 && !g_hasHighHeelFrameRetries,
        "strip or hide during retry removes height without restoring registered appearance");
  actor.displayed.insert(boots.id);

  for (unsigned cycle = 0; cycle < 128; ++cycle) {
    observe();
    if (cycle % 2) {
      // A retry may already have crossed the render/game handoff on unload.
      sfs::native::racemenu::QueueHighHeelSceneCheck();
    }
    sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false);
    DrainSkseTurn();
    Check(heel.references == 0 && g_highHeelFrameRetries.empty() &&
          !g_hasHighHeelFrameRetries && g_queuedHighHeelSyncs.Size() == 0,
          "unload cancels both deferred and dispatched retry tickets without retained nodes");
  }
  observe(); sfs::native::racemenu::QueueHighHeelSceneCheck();
  auto stale = std::move(SKSE::tasks.pending); SKSE::tasks.pending.clear();
  sfs::native::racemenu::ForgetAllRegisteredAppearanceNodes();
  g_highHeelAttachmentActors.insert(actor.id); observe();
  const auto current = g_highHeelFrameRetries.at(actor.id).generation;
  for (auto& task : stale) task();
  Check(g_highHeelFrameRetries.at(actor.id).generation == current && heel.references == 1,
        "stale retry after reset cannot clear a new actor generation");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);

  // Permanently unavailable automatic source: both API families are bounded
  // and remove the bootstrap; no per-frame attempts remain after exhaustion.
  Reset(actor); actor.displayed.insert(boots.id); Attach(actor, heel);
  actor.sceneOffsets = {9}; actor.equippableTransforms = false; observe();
  for (unsigned frame = 0; frame < 2; ++frame) {
    sfs::native::racemenu::QueueHighHeelSceneCheck(); DrainSkseTurn();
  }
  Check(actor.fullUpdates == 3 && !actor.temporary && !g_hasHighHeelFrameRetries &&
        g_queuedHighHeelSyncs.Size() == 0 && g_sceneHeightWatches.empty(),
        "unavailable provider exhausts the same three attempts on public and legacy routes");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true); Detach(actor, heel);
}
void TestLegacyBootstrapCancellation() {
  if (g_transformInterfaceVersion >= 3) return;
  for (unsigned cut = 0; cut < 10; ++cut) {
   for (bool reset : {false, true}) {
    for (bool replace : {false, true}) {
    RE::Actor actor{0x14}; Reset(actor);
    RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
    RE::NiAVObject heel{true}; heel.heel.value = 9; Attach(actor, heel);
    actor.displayed.insert(boots.id); actor.sceneOffsets = {9};
    actor.sexLabPosition = -4.5F;
    RegisteredAppearanceAttachmentObserver observer;
    observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
    SKSE::tasks.Drain();
    for (unsigned step = 0; step < cut; ++step) RE::BSScript::Internal::vm.Drain();
    if (reset) {
      // Pending morph closures also belong to the old save, even when no
      // morph task has crossed into SKSE yet.
      g_morphFrameRetries[actor.id] = [] { std::abort(); };
      g_hasMorphFrameRetries = true;
      sfs::native::racemenu::ForgetAllRegisteredAppearanceNodes();
      Check(g_morphFrameRetries.empty() && !g_hasMorphFrameRetries,
            "save reset releases pending morph closures and their activity flag");
    } else {
      actor.loaded = false;
      sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false);
    }
    if (replace) {
      actor.loaded = true;
      g_highHeelAttachmentActors.insert(actor.id);
      observer.OnAttach(&actor, &boots, &addon, &heel, false, &actor.root, &actor.root);
    }
    DrainSkseTurn();
    Check(!actor.temporary && g_queuedHighHeelSyncs.Size() == 0,
          "cancelled legacy bootstrap must not leave SFS_HH_SYNC after its VM callback completes");
    if (replace) {
      Check(actor.displayedHeight == 4.5F && g_registeredAppearanceHighHeelActors.contains(actor.id),
            "old bootstrap cleanup cannot interrupt a replacement generation's height");
      sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false);
    }
    Check(!g_registeredAppearanceHighHeelActors.contains(actor.id),
          "cancellation releases old heel ownership at every VM boundary");
    Check(actor.sexLabPosition == -4.5F && heel.references == 1,
          "bootstrap cleanup preserves SexLab and releases registered-root ownership");
    Detach(actor, heel);
    }
   }
  }
}
int main() {
  TestPositionMetadata();
  for (auto version : {1U, 2U, 3U, 4U, 99U}) {
    g_transformInterfaceVersion = version;
    g_transformInterface = version < 3 ? nullptr : &skee::transform;
    g_bodyMorphInterface = nullptr;
    TestLegacyBootstrapCancellation();
    for (bool sdta : {false, true}) {
      for (bool removeHeel : {false, true}) TestRetryWaitsForFrame(sdta, removeHeel);
      TestProviderReadyNextFrame(sdta);
      TestFrameRetryLifetime(sdta);
    }
    RE::Actor actor{0x14}; RE::TESObjectARMO armor{0x900}, actualArmor{0x800}; RE::TESObjectARMA addon{0x901};
    RE::NiAVObject branch{true}; branch.heel.value = 9;
    Reset(actor); actor.displayed.insert(armor.id);
    RegisteredAppearanceAttachmentObserver observer;
    const auto queue = [&] { sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); };
    observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
    SKSE::tasks.Drain();
    Check(g_registeredAppearanceAttachmentRoots.contains(actor.id) && branch.references == 1 &&
          g_highHeelFrameRetries.contains(actor.id) && actor.fullUpdates == 0,
          "pre-graft HH=9 root remains owned and pending; first pass does not complete or lower");
    Attach(actor, branch); actor.sceneOffsets = {9}; DrainAll();
    Check(actor.displayedHeight == 9 && !actor.temporary,
          "later graft applies registered HH=9 without equipping the actual boots or recapture");

    // No actual boots / identical boots / other HH boots / flat actual boots:
    // their scan order must not become the selected registered offset.
    actor.otherModHeight = 3;
    for (const auto& actualOffsets : {std::vector<float>{9}, std::vector<float>{9, 9},
                                     std::vector<float>{9, 17}, std::vector<float>{17, 9},
                                     std::vector<float>{9, 0}}) {
      actor.sceneOffsets = actualOffsets; queue(); DrainAll();
      Check(actor.displayedHeight == 12 && !actor.temporary,
            "registered HH=9 wins over none/same/different/flat actual heel scene; named +3 remains");
    }
    actor.sceneOffsets = {9}; actor.otherModHeight = 0;
    LaterAttachment(actor, actualArmor, false); DrainAll();
    Check(actor.displayedHeight == 9 && g_registeredAppearanceAttachmentRoots[actor.id].size() == 1,
          "ordinary late attachment repairs height without enrolling the ordinary armor");
    Detach(actor, branch); actor.sceneOffsets.clear(); queue(); SKSE::tasks.Drain();
    Check(actor.displayedHeight == 9 && branch.references == 1,
          "temporarily reparented registered heel does not lower or lose ownership on first pass");
    Attach(actor, branch); actor.sceneOffsets = {9}; DrainAll();
    Check(actor.displayedHeight == 9, "same-node reattachment recovers without another callback");

    // An older completion pass must not erase a newly observed instance.
    Detach(actor, branch); actor.sceneOffsets.clear(); queue();
    SKSE::tasks.Drain(); DispatchHighHeelFrameRetries(); SKSE::tasks.Drain();
    beforeDisplayLookup = [&] {
      observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
    };
    DispatchHighHeelFrameRetries(); SKSE::tasks.Drain();
    Check(!beforeDisplayLookup && g_registeredAppearanceAttachmentRoots[actor.id].size() == 1 && actor.displayedHeight == 9,
          "expired old snapshot cannot erase recaptured root or clear its pending height");
    Attach(actor, branch); actor.sceneOffsets = {9}; DrainAll();
    Check(actor.displayedHeight == 9 && branch.references == 2,
          "recapture is deduplicated and its new synchronization completes");

    // Strip / redress off: real equipment remains under the external mod.
    // Include a real HH/SDTA scene source while removing the SFS heel.
    RE::NiAVObject actualBranch{true}; actualBranch.heel.value = 7;
    Attach(actor, actualBranch); actor.sceneOffsets = {7};
    Detach(actor, branch); actor.displayed.clear(); queue(); DrainAll();
    Check(actor.displayedHeight == 7 && branch.references == 0 &&
          !g_registeredAppearanceHighHeelActors.contains(actor.id),
          "strip removes registered height while preserving current actual-gear height");
    for (unsigned i = 0; i < 8; ++i) SKSE::tasks.Drain();
    Check(actor.displayedHeight == 7 && SKSE::tasks.pending.empty(),
          "redress off does not restore registered heels or poll forever");
    Detach(actor, actualBranch);
    actor.displayed.insert(armor.id); Attach(actor, branch); actor.sceneOffsets = {9};
    observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
    DrainAll();
    Check(actor.displayedHeight == 9, "manual display/redress observes and reapplies the registered heel");

    // Real removal must expire, not retain every once-seen pending root.
    Detach(actor, branch); actor.sceneOffsets.clear(); queue(); DrainAll();
    Check(actor.displayedHeight == 0 && branch.references == 0 &&
          !g_registeredAppearanceAttachmentRoots.contains(actor.id) && g_queuedHighHeelSyncs.Size() == 0,
          "never-reattached active root expires after bounded checks and releases height and ownership");
    observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
    DrainAll();
    Check(branch.references == 0 && !g_registeredAppearanceAttachmentRoots.contains(actor.id),
          "never-grafted first attachment also expires without retained scene references");
    observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
    SKSE::tasks.Drain();
    actor.displayed.clear(); DrainAll();
    Check(actor.displayedHeight == 0 && branch.references == 0 &&
          !g_registeredAppearanceAttachmentRoots.contains(actor.id),
          "hide/strip during pending graft cancels that source instead of applying a hidden heel");
    actor.displayed.insert(armor.id);

    // A save/revert boundary cancels the pending root and its old queued job.
    observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
    SKSE::tasks.Drain();
    sfs::native::racemenu::ForgetAllRegisteredAppearanceNodes();
    Attach(actor, branch); actor.sceneOffsets = {9};
    DrainAll();
    Check(actor.displayedHeight == 0 && branch.references == 1 &&
          g_registeredAppearanceAttachmentRoots.empty() && g_queuedHighHeelSyncs.Size() == 0,
          "save/revert drops old pending ownership; stale job cannot raise later-grafted old scene");
    Detach(actor, branch); actor.sceneOffsets.clear();
    g_highHeelAttachmentActors.insert(actor.id);

    // Native/DAV capture path, including HH on a child shape rather than root.
    RE::NiAVObject root, shape{true}; shape.heel.value = 9;
    root.children.emplace_back(&shape); shape.parent = &root;
    auto before = sfs::native::racemenu::CaptureAttachmentScene(&actor);
    Attach(actor, root); actor.sceneOffsets = {9};
    sfs::native::racemenu::MorphNewRegisteredAppearanceNodes(&actor, before, armor.id, true);
    queue(); DrainAll();
    Check(actor.displayedHeight == 9 && root.references == 2 &&
          g_registeredAppearanceAttachmentRoots[actor.id].size() == 1,
          "native/DAV before-after capture reads child HH metadata without BodyMorph availability");
    shape.heel.value = 0; actor.sceneOffsets = {0}; queue(); DrainAll();
    Check(actor.displayedHeight == 0 && g_registeredAppearanceHighHeelActors.contains(actor.id),
          "valid HH zero remains an explicit selected offset");

    // Cancellation during the display query must not write to unloaded/new 3D.
    const auto beforeUnloadUpdates = actor.fullUpdates;
    beforeDisplayLookup = [&] { sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false); };
    queue(); DrainAll();
    Check(actor.fullUpdates == beforeUnloadUpdates && root.references == 1 &&
          !g_registeredAppearanceAttachmentRoots.contains(actor.id),
          "unload during resolution cancels transform writes and releases only tracked ownership");
    Detach(actor, root); root.children.clear(); shape.parent = nullptr;
    Check(root.references == 0 && shape.references == 0, "native captured root and child have no retained references");

    // Stale capture tickets, first person and invalid data cannot enroll roots.
    {
      SceneObservation observation(actor.id);
      sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false);
      Check(!RememberHighHeelAttachmentRoots(&actor, {&branch}, armor.id, false, &observation),
            "pre-unload capture cannot republish a root after scene cancellation");
    }
    Check(!RememberHighHeelAttachmentRoots(&actor, {&branch}, armor.id, true, nullptr),
          "first-person root never overrides third-person height");
    branch.heel.value = std::numeric_limits<float>::quiet_NaN();
    Check(!RememberHighHeelAttachmentRoots(&actor, {&branch}, armor.id, false, nullptr),
          "non-finite heel metadata is not an offset");
    branch.heel.value = 9;
    // 128 completed/unloaded scenes, with stale queued callbacks still pending.
    for (unsigned cycle = 0; cycle < 128; ++cycle) {
      g_highHeelAttachmentActors.insert(actor.id);
      observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
      SKSE::tasks.Drain();
      sfs::native::racemenu::ReleaseActorSceneResources(actor.id, false);
      DrainAll();
      if (branch.references != 0 || g_queuedHighHeelSyncs.Size() != 0 ||
          g_registeredAppearanceAttachmentRoots.contains(actor.id)) {
        Check(false, "128 unload/reload cycles leave no pending heel node or task ownership");
      }
    }
    Check(branch.references == 0, "128 pending-scene unload/reload cycles release all heel references");
    sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);
    actor.root.children.clear(); branch.parent = nullptr;
    Check(branch.references == 0, "unload/delete releases recorded root ownership");
    for (bool sdta : {false, true}) {
      for (bool removeHeel : {false, true}) {
        for (bool retained : {false, true}) TestReportedFootwearSequence(sdta, removeHeel, retained);
      }
      TestSceneHeight(sdta);
      TestLateFullRescan(sdta);
      TestInitialAppliedHeight(sdta);
    }
    TestRegisteredSdta(false);
    TestRegisteredSdta(true);
    TestLargeSceneWatch();
    TestDiscardedSceneCheck();
  }
  TestResetReleasesOutsideLocks();
  const std::array<RE::FormID, 1> actorIDs{0x14};
  g_highHeelScenes.Update(1, actorIDs); g_highHeelScenes.Update(2, actorIDs);
  g_highHeelScenes.Update(1, {});
  Check(g_highHeelScenes.Contains(0x14), "late old-thread end cannot cancel the actor's new scene");
  g_highHeelScenes.Update(2, {});
  Check(!g_highHeelScenes.Contains(0x14), "matching thread end releases scene membership");
  for (unsigned i = 0; i < 128; ++i) {
    g_highHeelScenes.Update(1, actorIDs); g_highHeelScenes.Update(1, {});
    Check(!g_highHeelScenes.Contains(0x14), "128 scene cycles release their membership");
  }
  std::printf("%u production heel-root checks passed; not in-game proof.\n", checks);
}
