#include "native/ArmorRefreshRules.h"
#include "native/ExternalEquipmentTransactionRules.h"
#include "native/FittingSlotStateRules.h"
#include "native/RegisteredAppearanceMorphRules.h"
#include "ui/MenuCharacterRotationRules.h"
#include "workbench/ExternalStripLinkRules.h"

#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace {
int g_failures = 0;

void Expect(const bool a_condition, const char *a_message) {
  if (!a_condition) {
    std::cerr << "FAILED: " << a_message << '\n';
    ++g_failures;
  }
}

void TestStripLinkPolicies() {
  using namespace sfs::workbench;
  ExternalStripLinkPolicyState policy;

  policy.configuredMode = ExternalModStripLinkMode::Disabled;
  Expect(!IsModSettingsPolicyActive(policy) &&
             !IsActualEquipmentPolicyActive(policy),
         "No Linking must activate neither external-strip pipeline");

  policy.configuredMode = ExternalModStripLinkMode::ModSettingsSlots;
  Expect(IsModSettingsPolicyActive(policy) &&
             !IsActualEquipmentPolicyActive(policy),
         "Mod-Configured must exclusively use the virtual-token pipeline");

  policy.configuredMode = ExternalModStripLinkMode::VanillaSlots;
  Expect(!IsModSettingsPolicyActive(policy) &&
             IsActualEquipmentPolicyActive(policy),
         "Automatic Vanilla must exclusively use actual-equipment anchors");

  const auto body = EquipmentSlotMask(32);
  const auto feet = EquipmentSlotMask(37);
  const auto extension = EquipmentSlotMask(49);
  policy = {};
  policy.configuredMode = ExternalModStripLinkMode::Custom;
  policy.customBaseMode = ExternalModStripLinkMode::ModSettingsSlots;
  policy.directAutomaticBaseMode =
      ExternalModStripLinkMode::ModSettingsSlots;
  policy.directOverrides[2] = true;
  policy.directMappings[2] = 49;
  Expect(IsModSettingsPolicyActive(policy) &&
             !IsActualEquipmentPolicyActive(policy),
         "Direct+ModSettings must not start a parallel actual observer");
  Expect(ResolveDirectSlotMask(policy, body, true) == extension,
         "Direct+ModSettings must remap one appearance to any 30-61 token");
  Expect(!ResolveDirectSlotMask(policy, feet, true).has_value(),
         "Untouched Direct+ModSettings slots must retain automatic base");

  policy.directMappings[2] = 0;
  Expect(ResolveDirectSlotMask(policy, body, true) == 0,
         "Direct Do Not Link must be an explicit zero result");

  policy.customBaseMode = ExternalModStripLinkMode::VanillaSlots;
  policy.directAutomaticBaseMode = ExternalModStripLinkMode::VanillaSlots;
  policy.directMappings[2] = 37;
  Expect(!IsModSettingsPolicyActive(policy) &&
             IsActualEquipmentPolicyActive(policy),
         "Direct+Vanilla must retain the actual-equipment base");
  Expect(ResolveDirectSlotMask(policy, body, false) == feet,
         "Direct+Vanilla must remap an edited card to a vanilla anchor");
  Expect(!ResolveDirectSlotMask(policy, feet, false).has_value(),
         "Untouched Direct+Vanilla slots must retain classifier fallback");

  policy.directMappings[2] = 49;
  Expect(ResolveDirectSlotMask(policy, body, false) == 0,
         "Direct+Vanilla automatic base must reject extension anchors");
  policy.directMappings[2] = 37;
  policy.protectedAppearanceSlotMask = feet;
  Expect(ResolveDirectSlotMask(policy, body, false) == 0,
         "Protected target slots must not participate in direct linking");

  policy.protectedAppearanceSlotMask = 0;
  policy.disabledAppearanceSlotMask = body;
  Expect(!IsStripLinkedAppearanceEnabled(policy, body, false),
         "A disabled Direct exception card must not follow its base");
  Expect(!IsStripLinkedAppearanceEnabled(policy, feet, true),
         "Locked/protected appearances must never enter external stripping");
}

void TestVanillaAnchorResolution() {
  using namespace sfs::workbench;
  const auto body = EquipmentSlotMask(32);
  const auto hands = EquipmentSlotMask(33);
  const auto feet = EquipmentSlotMask(37);
  VanillaAnchorPriority priority{{body, hands, feet}, 3};

  Expect(ResolveVanillaAnchorSlotMask(priority, 0, feet, body) == feet,
         "A stripped eligible vanilla anchor must beat a worn fallback");
  Expect(ResolveVanillaAnchorSlotMask(priority, 0, 0, feet) == feet,
         "A currently worn vanilla candidate must beat the default priority");
  Expect(ResolveVanillaAnchorSlotMask(priority, body, 0, hands) == hands,
         "Protected vanilla candidates must be skipped");
  Expect(ResolveVanillaAnchorSlotMask(priority, body | hands | feet, feet,
                                      body | hands) == 0,
         "All-protected vanilla priorities must fail closed");
}

void TestActorLocalModSettingsState() {
  using namespace sfs::native::fitting_slot_rules;
  const auto body = std::uint32_t{1} << 2;
  const auto feet = std::uint32_t{1} << 7;
  std::unordered_map<std::uint32_t, ActorFittingSlotState> actors;

  SetVirtualTokenSuppressed(actors[0x14], body, true);
  SetVirtualTokenSuppressed(actors[0x1234], feet, true);
  Expect(actors[0x14].virtualTokenSuppressedSlotMask == body,
         "Player ModSettings suppression must contain only player slots");
  Expect(actors[0x1234].virtualTokenSuppressedSlotMask == feet,
         "NPC ModSettings suppression must contain only that NPC's slots");

  SetVirtualTokenSuppressed(actors[0x14], body, false);
  Expect(actors[0x14].virtualTokenSuppressedSlotMask == 0 &&
             actors[0x1234].virtualTokenSuppressedSlotMask == feet,
         "Player redress must not release an NPC's suppression");

  SetHeadgearSuppressed(actors[0x1234], body, true);
  Expect(actors[0x1234].virtualTokenSuppressedSlotMask == feet &&
             GetEffectiveHeadgearSuppressedMask(actors[0x1234]) == body,
         "HT2 and ModSettings actor-local layers must remain independent");
}

void TestActorLocalActualEquipmentTransactions() {
  using namespace sfs::native::external_equipment::rules;
  const auto body = std::uint64_t{1} << 2;
  const auto hands = std::uint64_t{1} << 3;
  const auto feet = std::uint64_t{1} << 7;
  SuppressedEquipmentByActor durable{
      {0x14, {{0x100, body}}}, {0x1234, {{0x200, feet}}}};
  std::vector<PendingEquipmentMutation> pending{
      {.sequence = 30,
       .actorFormID = 0x14,
       .armorFormID = 0x100,
       .slotMask = body,
       .equipped = true},
      {.sequence = 20,
       .actorFormID = 0x14,
       .armorFormID = 0x300,
       .slotMask = hands,
       .equipped = false},
      {.sequence = 10,
       .actorFormID = 0x1234,
       .armorFormID = 0x200,
       .slotMask = feet,
       .equipped = true}};

  Expect(ResolveSuppressedSlotMask(0x14, durable, pending) == hands,
         "Player strip/redress mutations must be folded in call order");
  Expect(ResolveSuppressedSlotMask(0x1234, durable, pending) == 0,
         "NPC redress must resolve only that NPC's durable state");
  Expect(ResolveSuppressedSlotMask(0x9999, durable, pending) == 0,
         "Unmanaged actors must never inherit another actor's suppression");

  pending.push_back({.sequence = 40,
                     .actorFormID = 0x14,
                     .armorFormID = 0x300,
                     .slotMask = hands,
                     .equipped = true});
  Expect(ResolveSuppressedSlotMask(0x14, durable, pending) == 0,
         "Final redress must clear the matching pending strip");
}

void TestBodyMorphActivity() {
  using namespace sfs::native::racemenu::rules;
  Expect(ShouldTrackRegisteredAppearanceNodes(false, true, 1),
         "A visible saved registered appearance must enable morph tracking");
  Expect(!ShouldTrackRegisteredAppearanceNodes(true, true, 1),
         "A replacement preview must stay on RaceMenu's ordinary attach path");
  Expect(!ShouldTrackRegisteredAppearanceNodes(false, false, 1) &&
             !ShouldTrackRegisteredAppearanceNodes(false, true, 0),
         "Inactive or empty displays must not retain stale morph nodes");

  ActorMorphActivity activity;
  activity.SetActive(0x14, true);
  activity.SetActive(0x1234, true);
  Expect(activity.IsActive(0x14) && activity.IsActive(0x1234),
         "Player and NPC morph activity must coexist actor-locally");
  Expect(activity.MarkObserved(0x14, true) &&
             !activity.MarkObserved(0x14, true) &&
             activity.MarkObserved(0x1234, false),
         "Morph update diagnostics must deduplicate per actor and result kind");
  activity.SetActive(0x1234, false);
  Expect(activity.IsActive(0x14) && !activity.IsActive(0x1234),
         "Deactivating an NPC appearance must not disable player morphs");
}

void ExpectBackendFollowups(
    const sfs::native::refresh_rules::Plan &a_plan,
    const char *a_message) {
  Expect(a_plan.HasBackendWork() && a_plan.queuePoseSync &&
             a_plan.queueDyeRestore && a_plan.queueHighHeelSync,
         a_message);
}

void TestRefreshBackends() {
  using namespace sfs::native::refresh_rules;
  auto plan = BuildPlan({.daveApiReady = true});
  Expect(plan.backend == Backend::DaveApi,
         "DAVE API must be the primary display refresh backend");
  ExpectBackendFollowups(plan,
                         "DAVE must retain all actor-local refresh follow-ups");

  plan = BuildPlan(
      {.daveApiReady = true, .davLoaded = true, .emptyEquipment3DRefresh = true});
  Expect(plan.backend == Backend::DaveEmptyEquipment3D,
         "Empty-equipment DAVE actors must use the Update3D bootstrap");
  ExpectBackendFollowups(
      plan, "DAVE empty-equipment bootstrap must retain refresh follow-ups");

  plan = BuildPlan({.davLoaded = true, .davFallback3DRefresh = true});
  Expect(plan.backend == Backend::DavFallback3D,
         "DAV without DAVE API must use its bounded Update3D fallback");
  ExpectBackendFollowups(plan,
                         "DAV fallback must retain actor-local follow-ups");
  Expect(BuildPlan({.davLoaded = true}).backend == Backend::None,
         "Unchanged DAV signatures must not trigger redundant rebuilds");

  plan = BuildPlan({.emptyEquipment3DRefresh = true});
  Expect(plan.backend == Backend::NativeEmptyEquipment3D,
         "Native empty-equipment actors must use Update3D bootstrap");
  ExpectBackendFollowups(plan,
                         "Native empty bootstrap must retain refresh follow-ups");

  plan = BuildPlan({.nativeProcessAvailable = true});
  Expect(plan.backend == Backend::NativeEquipment,
         "Native actors with a process must use UpdateEquipment");
  ExpectBackendFollowups(plan,
                         "Native equipment refresh must retain follow-ups");
  Expect(BuildPlan({}).backend == Backend::None,
         "Unavailable native process must fail closed");
}

void TestPausedCharacterRotationIsolation() {
  using namespace sfs::ui::character_rotation;
  const auto paused = BuildPlan(true);
  Expect(!paused.rotateActor && paused.orbitCamera,
         "Paused right-drag must orbit the camera without moving actor/SMP roots");

  const auto running = BuildPlan(false);
  Expect(running.rotateActor && running.orbitCamera,
         "Unpaused right-drag must retain actor rotation and camera counter-rotation");
}
} // namespace

int main() {
  TestStripLinkPolicies();
  TestVanillaAnchorResolution();
  TestActorLocalModSettingsState();
  TestActorLocalActualEquipmentTransactions();
  TestBodyMorphActivity();
  TestRefreshBackends();
  TestPausedCharacterRotationIsolation();

  if (g_failures != 0) {
    std::cerr << g_failures << " core behavior regression test(s) failed\n";
    return 1;
  }
  std::cout << "Core behavior regression tests passed\n";
  return 0;
}
