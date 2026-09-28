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
  }
  std::printf("%u production heel-root checks passed; not in-game proof.\n", checks);
}
