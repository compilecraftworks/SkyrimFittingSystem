// Production metadata/root capture + resolver + transform/queue/lifecycle, with
// scene ownership and public/legacy NiOverride recording providers, not Skyrim.
#include "HighHeelRootFixture.inc"
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
  g_highHeelScenes.Update(0x120, actors);
  queue(); DrainAll();
  Check(actor.displayedHeight == 0, "scene with no SexLab compensation (feet stripped) suppresses only owned heel height");
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
  Check(ordinary.fullUpdates == 0 && ordinary.internalPosition == 7.0F,
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
  }
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
