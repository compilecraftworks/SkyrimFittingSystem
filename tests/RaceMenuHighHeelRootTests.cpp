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
void TestSceneHeight() {
  RE::Actor actor{0x14}; RE::TESObjectARMO armor{0x910}; RE::TESObjectARMA addon{0x911};
  RE::NiAVObject branch{true}; branch.heel.value = 9;
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

  branch.heel.value = 0; actor.sceneOffsets = {0}; g_highHeelScenes.Update(0x120, actors);
  queue(); DrainAll();
  Check(actor.displayedHeight == 0, "registered flat footwear remains flat during the scene");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id, true);
  Check(!g_highHeelScenes.Contains(actor.id), "deleted actor releases scene ownership");
  Detach(actor, branch);
  Check(branch.references == 0, "scene regression retains no heel root references");
}

void TestLateFullRescan() {
  RE::Actor actor{0x14}; RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  Reset(actor); actor.displayed.insert(boots.id); actor.sceneOffsets = {9};
  RE::NiAVObject heel{true}; heel.heel.value = 9; Attach(actor, heel);
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
        RE::BSScript::Internal::vm.calls.size() == idleVmCalls,
        "256 stable scene frames do not scan inventory, call Papyrus or recalculate transforms");
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

void TestInitialAppliedHeight() {
  RE::Actor actor{0x14}; RE::TESObjectARMO boots{0x910}; RE::TESObjectARMA addon{0x911};
  RE::NiAVObject heel{true}; heel.heel.value = 9;
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
int main() {
  for (auto version : {1U, 2U, 3U, 4U, 99U}) {
    g_transformInterfaceVersion = version;
    g_transformInterface = version < 3 ? nullptr : &skee::transform;
    g_bodyMorphInterface = nullptr;
    RE::Actor actor{0x14}; RE::TESObjectARMO armor{0x900}, actualArmor{0x800}; RE::TESObjectARMA addon{0x901};
    RE::NiAVObject branch{true}; branch.heel.value = 9;
    Reset(actor); actor.displayed.insert(armor.id);
    RegisteredAppearanceAttachmentObserver observer;
    const auto queue = [&] { sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor); };
    observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
    SKSE::tasks.Drain();
    Check(g_registeredAppearanceAttachmentRoots.contains(actor.id) && branch.references == 1 &&
          SKSE::tasks.pending.size() == 1 && actor.fullUpdates == 0,
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
    SKSE::tasks.Drain(); SKSE::tasks.Drain();
    beforeDisplayLookup = [&] {
      observer.OnAttach(&actor, &armor, &addon, &branch, false, &actor.root, &actor.root);
    };
    SKSE::tasks.Drain();
    Check(g_registeredAppearanceAttachmentRoots[actor.id].size() == 1 && actor.displayedHeight == 9,
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
    TestSceneHeight();
    TestLateFullRescan();
    TestInitialAppliedHeight();
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
