#include "native/ArmorRefreshRules.h"
#include "native/BranchChainRules.h"
#include "native/ExternalEquipmentTransactionRules.h"
#include "native/FittingSlotStateRules.h"
#include "native/FinalRenderedOutfitRules.h"
#include "native/GenitalCompatibilityRules.h"
#include "native/HelmetToggle2Rules.h"
#include "native/IedVisitorRoutingRules.h"
#include "native/PapyrusObserverInstallRules.h"
#include "native/RegisteredAppearanceMorphRules.h"
#include "native/SexLabPPlusRules.h"
#include "TngGenitalCoverRules.h"
#include "ui/MenuCharacterRotationRules.h"
#include "ui/MenuInteractionRules.h"
#include "workbench/ExternalStripLinkRules.h"

#include <array>
#include <cstdint>
#include <cstring>
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

void TestPapyrusPostLinkInspectionBoundary() {
  using sfs::native::papyrus_observer::rules::
      ShouldInspectPostLinkMembers;
  using sfs::native::papyrus_observer::rules::
      ShouldRetryPostLinkMemberInspection;
  Expect(ShouldInspectPostLinkMembers("sslActorAlias") &&
             ShouldInspectPostLinkMembers("SSLACTORALIAS"),
         "Only the exact P+ alias type must permit delayed member inspection");
  Expect(!ShouldInspectPostLinkMembers("sslActorLibrary") &&
             !ShouldInspectPostLinkMembers("Actor") &&
             !ShouldInspectPostLinkMembers("FormArray") &&
             !ShouldInspectPostLinkMembers("sslActorAliasExtra"),
         "Generic and similarly named Papyrus types must remain globals-only");
  Expect(ShouldRetryPostLinkMemberInspection("sslActorAlias", 0, 0, 4) &&
             ShouldRetryPostLinkMemberInspection("sslActorAlias", 0, 2, 4),
         "A linked P+ alias with no published strip member must retry");
  Expect(!ShouldRetryPostLinkMemberInspection("sslActorAlias", 1, 0, 4) &&
             !ShouldRetryPostLinkMemberInspection("sslActorAlias", 0, 3, 4) &&
             !ShouldRetryPostLinkMemberInspection("FormArray", 0, 0, 4),
         "Published P+ members and generic types must not create retry loops");
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

  const auto circlet = std::uint32_t{1} << 12;
  SetHeadgearSuppressed(actors[0x1234], circlet, true);
  SetVirtualTokenSuppressed(actors[0x1234], feet, false);
  Expect(actors[0x1234].virtualTokenSuppressedSlotMask == 0 &&
             GetEffectiveHeadgearSuppressedMask(actors[0x1234]) ==
                 (body | circlet),
         "External redress must not release the same actor's independent HT2 headgear suppression");

  const auto hair = std::uint32_t{1} << 1;
  SetVirtualTokenSuppressed(actors[0x1234], hair, true);
  SetHeadgearSuppressed(actors[0x1234], circlet, false);
  Expect(actors[0x1234].virtualTokenSuppressedSlotMask == hair &&
             GetEffectiveHeadgearSuppressedMask(actors[0x1234]) == body,
         "HT2 show must not release ModSettings suppression, including pure Hair appearances");
}

void TestHelmetToggleAppearanceBoundary() {
  using namespace sfs::native::helmet_toggle::rules;
  constexpr auto head = std::uint32_t{1} << 0;
  constexpr auto hair = std::uint32_t{1} << 1;
  constexpr auto body = std::uint32_t{1} << 2;
  constexpr auto circlet = std::uint32_t{1} << 12;
  constexpr auto face = std::uint32_t{1} << 14;
  constexpr auto playerExtra = std::uint32_t{1} << 25;

  Expect(ProjectActualArmorSlotMask(hair | circlet, true) == circlet &&
             ProjectActualArmorSlotMask(head | hair | circlet, true) ==
                 (head | circlet) &&
             ProjectActualArmorSlotMask(playerExtra, false) == 0,
         "HT2 actual-equipment projection must exclude Hair and player-only slot 55 from NPCs");
  Expect(ResolveObservedControllerMask(head, head | hair | circlet, true) ==
             (head | circlet),
         "HT2 query coverage must recover multi-slot headgear without leaking Hair");
  Expect(ResolveObservedControllerMask(hair, hair, true) == 0 &&
             ResolveObservedControllerMask(hair, hair, false) == 0,
         "A pure Hair item must retain v1.5.0 behavior and never become a headgear controller by itself");
  Expect(ResolveRegisteredAppearanceSuppressionSlots(hair, circlet, false) ==
             0,
         "A pure registered Hair appearance must never follow HT2");
  Expect(ResolveRegisteredAppearanceSuppressionSlots(
             hair, kPlayerManagedActualSlotMask, false) == 0,
         "A managed pure-31 real helmet must not suppress a pure registered Hair appearance");
  Expect(ResolveRegisteredAppearanceSuppressionSlots(hair | circlet, circlet,
                                                      false) == circlet,
         "A registered Hair+Circlet card must follow HT2 through Circlet only");
  Expect(ResolveRegisteredAppearanceSuppressionSlots(body | circlet, circlet,
                                                      false) == circlet,
         "HT2 must not leak a multi-slot card's unrelated body bit into actor-wide suppression");
  Expect(ResolveRegisteredAppearanceSuppressionSlots(circlet, circlet,
                                                      false) == circlet &&
             ResolveRegisteredAppearanceSuppressionSlots(face, face, false) ==
                 face,
         "Registered Circlet and managed face headgear must follow the matching HT2 actual controller");
  Expect(ResolveRegisteredAppearanceSuppressionSlots(circlet, circlet, true) ==
             0,
         "Locked registered appearances must remain protected from HT2 suppression");
  Expect(ComputeActualHairSlotReleaseMask(true, hair | circlet, 0) == hair &&
             ComputeActualHairSlotReleaseMask(true, hair | circlet, hair) ==
                 0 &&
             ComputeActualHairSlotReleaseMask(false, hair | circlet, 0) == 0,
         "The proven v1.5.0 renderer-only Hair release must remain scoped to HT2 hidden state");
  Expect(ResolveSignalControllerMask(true, 0, false, circlet) == circlet,
         "Animated HT2 hide must reuse only the same actor's last visible controller mask");
  Expect(ResolveSignalControllerMask(true, 0, true, circlet) == 0,
         "An observed pure Hair item must not inherit a stale Circlet controller");
  Expect(ResolveSignalControllerMask(false, head, true, circlet) == head,
         "A shown-state observation must replace the cached controller");
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

void TestSexLabPPlusStripContract() {
  using namespace sfs::native::sexlab_pplus::rules;
  const auto head = EquipmentSlotMask(30);
  const auto hands = EquipmentSlotMask(33);
  const auto feet = EquipmentSlotMask(37);
  const auto body = EquipmentSlotMask(32);
  const auto extension = EquipmentSlotMask(49);

  Expect(kStripHelmet == 0x01 && kStripGloves == 0x02 &&
             kStripBoots == 0x04 && kStripDefault == 0x80 &&
             kStripAll == 0xFF,
         "SexLab P+ v2.12/v2.18 SE/AE StripData bits must stay exact");

  const std::vector<std::int32_t> defaults{
      static_cast<std::int32_t>(body | extension), 1};
  const std::vector<std::int32_t> noOverwrites;
  Expect(ResolveStripSlotMask(kStripDefault, defaults, noOverwrites) ==
             (body | extension),
         "P+ Default must use the supplied MCM slot mask");
  Expect(ResolveStripSlotMask(kStripDefault | kStripHelmet | kStripGloves |
                                 kStripBoots,
                             defaults, noOverwrites) ==
             (body | extension | head | hands | feet),
         "P+ explicit animation pieces must extend the default mask");

  const std::vector<std::int32_t> overwrites{
      static_cast<std::int32_t>(feet), 0};
  Expect(ResolveStripSlotMask(kStripDefault | kStripHelmet, defaults,
                             overwrites) == feet,
         "P+ per-actor overwrite must take precedence over animation bits");
  Expect(ResolveStripSlotMask(kStripAll, defaults, noOverwrites) ==
             UINT32_MAX,
         "P+ All must cover the full biped mask");
  Expect(ResolveStripSlotMask(kStripDefault, {}, {}) == 0,
         "Malformed P+ defaults must fail closed");

  Expect(ShouldStripAppearance(body, body, false, false) &&
             !ShouldStripAppearance(body, feet, false, false),
         "P+ ModSettings matching must use each actor appearance token mask");
  Expect(ShouldStripAppearance(0, feet, false, true),
         "P+ AlwaysStrip must override a non-matching slot mask");
  Expect(!ShouldStripAppearance(UINT32_MAX, body, true, true),
         "P+ NoStrip must retain precedence over AlwaysStrip");
  Expect(SelectRestoreTokenMask(body | extension, body | extension, false) ==
             body,
         "A multi-slot appearance must receive one deterministic restore token");
}

void TestBodyMorphActivity() {
  using namespace sfs::native::racemenu::rules;
  Expect(ResolveHighHeelTransformRoute(0) ==
             HighHeelTransformRoute::Unavailable,
         "A missing RaceMenu NiTransform interface must fail closed");
  Expect(ResolveHighHeelTransformRoute(1) ==
             HighHeelTransformRoute::LegacyPapyrus &&
             ResolveHighHeelTransformRoute(2) ==
                 HighHeelTransformRoute::LegacyPapyrus,
         "Pre-v3 RaceMenu transforms must use the ABI-neutral Papyrus route");
  Expect(ResolveHighHeelTransformRoute(3) ==
             HighHeelTransformRoute::PublicInterface &&
             ResolveHighHeelTransformRoute(4) ==
                 HighHeelTransformRoute::Unavailable &&
             ResolveHighHeelTransformRoute(UINT32_MAX) ==
                 HighHeelTransformRoute::Unavailable,
         "Only verified NiTransform v3 may use the public interface route");
  Expect(!IsPublicBodyMorphInterfaceCompatible(0) &&
             !IsPublicBodyMorphInterfaceCompatible(1) &&
             !IsPublicBodyMorphInterfaceCompatible(2) &&
             !IsPublicBodyMorphInterfaceCompatible(3) &&
             IsPublicBodyMorphInterfaceCompatible(4) &&
             IsPublicBodyMorphInterfaceCompatible(5) &&
             !IsPublicBodyMorphInterfaceCompatible(6) &&
             !IsPublicBodyMorphInterfaceCompatible(UINT32_MAX),
         "Only RaceMenu BodyMorph v4/v5 may use the public C++ ABI");

  Expect(ShouldTrackRegisteredAppearanceNodes(true, 1),
         "A visible saved registered appearance must enable morph tracking");
  Expect(!ShouldApplyInitialNativeMorphs(true) &&
             ShouldApplyInitialNativeMorphs(false),
         "Preview live tracking must not double-apply initial native morphs");
  Expect(!ShouldTrackRegisteredAppearanceNodes(false, 1) &&
             !ShouldTrackRegisteredAppearanceNodes(true, 0),
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

  ActorMorphRequests requests;
  requests.Request(0x14);
  const auto first = requests.Schedule(0x14);
  Expect(first && !requests.Schedule(0x14), "Actor tasks must coalesce");
  requests.Request(0x1234);
  Expect(requests.Schedule(0x1234).has_value(), "NPC tasks must be independent");
  requests.Clear();
  requests.Request(0x14);
  const auto afterLoad = requests.Schedule(0x14);
  Expect(afterLoad && !requests.Begin(0x14, *first) &&
             requests.Begin(0x14, *afterLoad), "Stale save tasks must not replay");
  Expect(requests.HasRequest(0x14), "Late attachment must retain update intent");
  requests.Forget(0x14);
  Expect(!requests.HasRequest(0x14), "Forgetting an actor must clear requests");
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

  constexpr auto head = std::uint32_t{1} << 0;
  constexpr auto hair = std::uint32_t{1} << 1;
  constexpr auto body = std::uint32_t{1} << 2;
  constexpr auto circlet = std::uint32_t{1} << 12;
  constexpr auto genitals = std::uint32_t{1} << 22;
  constexpr auto hands = std::uint32_t{1} << 3;
  const auto ht2ResolvedActualMask = body | hands;
  Expect(MergeDaveResolvedWornMask(ht2ResolvedActualMask, 0) ==
             ht2ResolvedActualMask,
         "DAVE/HT2-cleared actual head, hair, and circlet bits must not be reconstructed from raw ARMO slots");
  Expect(MergeDaveResolvedWornMask(ht2ResolvedActualMask, hair) ==
             (ht2ResolvedActualMask | hair),
         "A pure registered Hair appearance must remain independent of HT2 actual equipment");
  Expect(MergeDaveResolvedWornMask(ht2ResolvedActualMask, head | circlet) ==
             (ht2ResolvedActualMask | head | circlet),
         "Only displayed registered headgear slots may extend DAVE's actual-equipment result");
  Expect(MergeDaveResolvedWornMask(body, 0) == body,
         "A genital slot removed by SFS's DAVE conceal variant must stay removed");
  Expect(MergeDaveResolvedWornMask(body, genitals) == (body | genitals),
         "SOS/TNG reveal correction must still add the resolved genital display slot");
  const auto rawShownHelmet = head | hair | circlet;
  Expect(MergeDaveResolvedWornMask(body | rawShownHelmet, 0,
                                  rawShownHelmet) == body,
         "SFS-hidden actual HT2 headgear must release its head ownership without rebuilding DAVE state");
  Expect(MergeDaveResolvedWornMask(body | rawShownHelmet, circlet,
                                  rawShownHelmet) == (body | circlet),
         "A displayed registered Circlet may replace only its own slot after the SFS-hidden real helmet is removed");
  Expect(ResolveSfsHiddenWornSlotMask(head | hair | circlet, hair) ==
             (head | circlet),
         "A slot still used by visible actual armor must not be removed with another SFS-hidden armor");
}

void TestIedVisitorRoutingIsolation() {
  using namespace sfs::native::ied::rules;
  constexpr std::uintptr_t engineTarget = 0x1000;
  constexpr std::uintptr_t iedChainTarget = 0x2000;

  Expect(ResolveVisitorRoute(false, iedChainTarget, iedChainTarget) ==
             VisitorRoute::CurrentTargetUnfiltered,
         "IED must retain its normal visitor path when SFS has nothing to filter");
  Expect(ResolveVisitorRoute(true, engineTarget, 0) ==
             VisitorRoute::CurrentTargetWithSfsFilter,
         "An unknown IED chain during early-game startup must stay on the ordinary engine path");
  Expect(ResolveVisitorRoute(true, engineTarget, iedChainTarget) ==
             VisitorRoute::CurrentTargetWithSfsFilter,
         "An unrelated visitor target must not be mistaken for IED");
  Expect(ResolveVisitorRoute(true, iedChainTarget, iedChainTarget) ==
             VisitorRoute::OriginalEngineWithSfsFilterThenIedEvaluate,
         "The exact IED chain must be bypassed before passing SFS's visitor and refreshed through IED afterward");
}

void TestHookTrampolineChainDecoding() {
  using namespace sfs::native::branch_chain::rules;
  constexpr std::uintptr_t base = 0x100000;

  std::array<std::uint8_t, 16> commonLibVeneer{};
  commonLibVeneer[0] = 0xFF;
  commonLibVeneer[1] = 0x25;
  constexpr std::uintptr_t iedTarget = 0x7FFA12345678;
  std::memcpy(commonLibVeneer.data() + 6, &iedTarget, sizeof(iedTarget));
  auto transfer = DecodeTransfer(base, commonLibVeneer);
  Expect(transfer.has_value() &&
             transfer->kind == TransferKind::IndirectTargetSlot &&
             transfer->address == base + 6,
         "CommonLib's rel32 branch-pool veneer must expose its indirect target slot for IED ownership resolution");

  std::array<std::uint8_t, 16> relativeJump{};
  relativeJump[0] = 0xE9;
  const std::int32_t relativeDisplacement = 0x120;
  std::memcpy(relativeJump.data() + 1, &relativeDisplacement,
              sizeof(relativeDisplacement));
  transfer = DecodeTransfer(base, relativeJump);
  Expect(transfer.has_value() &&
             transfer->kind == TransferKind::DirectTarget &&
             transfer->address == base + 5 + relativeDisplacement,
         "a rel32 trampoline hop must resolve to its next executable target");

  std::array<std::uint8_t, 16> shortJump{};
  shortJump[0] = 0xEB;
  shortJump[1] = 0xF0;
  transfer = DecodeTransfer(base, shortJump);
  Expect(transfer.has_value() &&
             transfer->kind == TransferKind::DirectTarget &&
             transfer->address == base - 14,
         "a negative rel8 trampoline hop must use signed displacement");

  std::array<std::uint8_t, 16> absoluteJump{};
  absoluteJump[0] = 0x48;
  absoluteJump[1] = 0xB8;
  std::memcpy(absoluteJump.data() + 2, &iedTarget, sizeof(iedTarget));
  absoluteJump[10] = 0xFF;
  absoluteJump[11] = 0xE0;
  transfer = DecodeTransfer(base, absoluteJump);
  Expect(transfer.has_value() && transfer->address == iedTarget,
         "a mov-rax/jmp-rax veneer must resolve to its module-owned target");

  std::array<std::uint8_t, 20> endbrR11Jump{};
  endbrR11Jump[0] = 0xF3;
  endbrR11Jump[1] = 0x0F;
  endbrR11Jump[2] = 0x1E;
  endbrR11Jump[3] = 0xFA;
  endbrR11Jump[4] = 0x49;
  endbrR11Jump[5] = 0xBB;
  std::memcpy(endbrR11Jump.data() + 6, &iedTarget, sizeof(iedTarget));
  endbrR11Jump[14] = 0x41;
  endbrR11Jump[15] = 0xFF;
  endbrR11Jump[16] = 0xE3;
  transfer = DecodeTransfer(base, endbrR11Jump);
  Expect(transfer.has_value() && transfer->address == iedTarget,
         "an ENDBR64-prefixed mov-r11/jmp-r11 veneer must resolve exactly");

  std::array<std::uint8_t, 16> underflowJump{};
  underflowJump[0] = 0xE9;
  const std::int32_t underflowDisplacement = -16;
  std::memcpy(underflowJump.data() + 1, &underflowDisplacement,
              sizeof(underflowDisplacement));
  Expect(!DecodeTransfer(0, underflowJump).has_value(),
         "an overflowing relative veneer must fail closed");

  std::array<std::uint8_t, 16> executableBody{};
  executableBody[0] = 0x48;
  executableBody[1] = 0x89;
  Expect(!DecodeTransfer(base, executableBody).has_value(),
         "ordinary hook bodies must not be guessed through as transparent trampolines");
}

void TestFinalRenderedNudityContract() {
  using sfs::native::final_outfit::rules::IsVisuallyNakedForSlots;
  using sfs::native::final_outfit::rules::ResolveDisplayedFootwear;
  constexpr auto body = std::uint32_t{1} << 2;
  constexpr auto hands = std::uint32_t{1} << 3;
  constexpr auto feet = std::uint32_t{1} << 7;

  Expect(!IsVisuallyNakedForSlots(body, 0, body),
         "visible actual body armor must prevent a nude result");
  Expect(!IsVisuallyNakedForSlots(0, body, body),
         "visible registered body appearance must prevent a nude result");
  Expect(!ResolveDisplayedFootwear(false, 0, 0x100).has_value(),
         "an unmanaged actor must preserve the consumer's actual-footwear fallback");
  Expect(ResolveDisplayedFootwear(true, 0x200U, 0x100U) == 0x200U,
         "visible registered footwear must win the managed final result");
  Expect(ResolveDisplayedFootwear(true, 0, 0x100U) == 0x100U,
         "visible actual footwear must remain available in a managed final result");
  Expect(ResolveDisplayedFootwear(true, 0, 0) == 0U,
         "a managed actor with every feet source hidden must produce an explicit barefoot result");
  Expect(!IsVisuallyNakedForSlots(hands, feet, hands | feet),
         "actual and registered coverage must combine in the final rendered outfit");
  Expect(IsVisuallyNakedForSlots(0, hands | feet, body),
         "a technically equipped but SFS-hidden body armor must not count as visible coverage");
  Expect(IsVisuallyNakedForSlots(hands, feet, body),
         "visible unrelated slots must not prevent body nudity");
  Expect(!IsVisuallyNakedForSlots(0, 0, 0),
         "an empty query mask must fail closed instead of reporting nudity");
}

void TestTngGenitalCoverIsolation() {
  using namespace sfs::armor::rules;
  constexpr auto slot52 = kGenitalSlotMask;
  constexpr auto slot32 = std::uint64_t{1} << 2;

  Expect(IsTngGenitalCoverIdentity(slot52, "thenewgentleman.esp",
                                   "tng_genitalcover", "") &&
             IsTngGenitalCoverIdentity(slot52, "tofu merged.esp",
                                       "tng_genitalcover", ""),
         "The exact TNG cover token must be excluded even after a plugin merge");
  Expect(!IsTngGenitalCoverIdentity(slot32, "thenewgentleman.esp",
                                    "tng_genitalcover", "") &&
             !IsTngGenitalCoverIdentity(slot52, "userarmor.esp",
                                        "custom_genitalcover", "Genital Cover"),
         "TNG cover isolation must require slot 52 and must not hide user armors by name");

  auto projection = ResolveGenitalCoverProjection(false, false);
  Expect(!projection.hideEquippedCover && !projection.occupyGenitalSlot,
         "Without SFS genital correction, TNG must retain sole control of its blocker");
  projection = ResolveGenitalCoverProjection(true, true);
  Expect(!projection.hideEquippedCover && projection.occupyGenitalSlot,
         "A concealing registered appearance must occupy slot 52 without exposing the internal token");
  projection = ResolveGenitalCoverProjection(true, false);
  Expect(projection.hideEquippedCover && !projection.occupyGenitalSlot,
         "A revealing registered appearance must hide only TNG's equipped blocker");

  Expect(!ShouldApplyGenitalCoverProjection(false, true, false) &&
             !ShouldApplyGenitalCoverProjection(false, false, true),
         "A custom or SOS slot-52 skin must never enable TNG projection when TNG is absent");
  Expect(ShouldApplyGenitalCoverProjection(true, true, false) &&
             ShouldApplyGenitalCoverProjection(true, false, true) &&
             !ShouldApplyGenitalCoverProjection(true, false, false),
         "TNG projection must require its installation and actor-local skin or cover evidence");

  Expect(ShouldRefreshForGenitalCoverEvent(true, true, false, false) &&
             ShouldRefreshForGenitalCoverEvent(true, false, true, false) &&
             ShouldRefreshForGenitalCoverEvent(true, false, false, true),
         "TNG cover changes must refresh the player and only state-owning NPCs");
  Expect(!ShouldRefreshForGenitalCoverEvent(true, false, false, false) &&
             !ShouldRefreshForGenitalCoverEvent(false, true, true, true),
         "TNG cover changes must not scan unrelated NPCs or run without TNG");
}

void TestGenitalCompatibilityEnvironmentIsolation() {
  using namespace sfs::native::genital_compatibility::rules;

  constexpr Environment none{};
  constexpr Environment sosOnly{.sosInstalled = true,
                                .tngInstalled = false};
  constexpr Environment tngOnly{.sosInstalled = false,
                                .tngInstalled = true};
  constexpr Environment both{.sosInstalled = true, .tngInstalled = true};

  Expect(!IsSosCompatibilityAvailable(none, true, true, true) &&
             !IsTngCompatibilityAvailable(none, true, true),
         "Genital compatibility must remain disabled when neither runtime is installed");

  Expect(IsSosCompatibilityAvailable(sosOnly, true, false, false) &&
             IsSosCompatibilityAvailable(sosOnly, false, true, false) &&
             IsSosCompatibilityAvailable(sosOnly, false, false, true) &&
             !IsTngCompatibilityAvailable(sosOnly, true, true),
         "An SOS-only environment must keep every established SOS evidence path without enabling TNG");

  Expect(!IsSosCompatibilityAvailable(tngOnly, true, true, true) &&
             IsTngCompatibilityAvailable(tngOnly, true, false) &&
             IsTngCompatibilityAvailable(tngOnly, false, true) &&
             !IsTngCompatibilityAvailable(tngOnly, false, false),
         "A TNG-only environment must use only TNG skin or cover evidence and never inherit SOS state");

  Expect(IsSosCompatibilityAvailable(both, true, false, false) &&
             IsTngCompatibilityAvailable(both, false, true),
         "When both runtimes are installed their decisions must remain independent and available");
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

void TestCatalogScrollbarReleaseIsolation() {
  using namespace sfs::ui::menu_interaction;
  CatalogReleaseState state{
      .hasSelection = true,
      .mouseReleased = true,
      .pointerOverWindow = true,
  };
  Expect(ShouldClearCatalogSelection(state),
         "A plain release on unused catalog space may clear selection");

  state.mouseDragPastThreshold = true;
  Expect(!ShouldClearCatalogSelection(state),
         "A scrollbar drag release must retain selection and scroll position");

  state.mouseDragPastThreshold = false;
  state.pointerOverItem = true;
  Expect(!ShouldClearCatalogSelection(state),
         "A scrollbar-track or other item release must retain selection");

  state.pointerOverItem = false;
  state.rowHandledRelease = true;
  Expect(!ShouldClearCatalogSelection(state),
         "A catalog row must own its release without a second global clear");
}

void TestMenuCameraZoomSynchronization() {
  using sfs::ui::menu_interaction::CameraZoomUpdate;
  using sfs::ui::menu_interaction::CameraZoomValues;
  using sfs::ui::menu_interaction::ResolveCameraZoomUpdate;

  constexpr CameraZoomValues live{32.0f, 7.0f};
  constexpr CameraZoomValues saved{-4.0f, 11.0f};
  const auto snapped = ResolveCameraZoomUpdate(
      CameraZoomUpdate::SnapCurrentToTarget, live, saved);
  Expect(snapped.target == 32.0f && snapped.current == 32.0f,
         "Menu camera activation must snap current zoom to Skyrim's target");

  const auto restored = ResolveCameraZoomUpdate(
      CameraZoomUpdate::RestoreSaved, live, saved);
  Expect(restored.target == -4.0f && restored.current == 11.0f,
         "Menu camera close must restore target and current zoom independently");

  const auto refreshed =
      ResolveCameraZoomUpdate(CameraZoomUpdate::Refresh, live, saved);
  Expect(refreshed.target == live.target && refreshed.current == live.current,
         "Rotation-only camera refresh must not change zoom state");
}

} // namespace

int main() {
  TestStripLinkPolicies();
  TestPapyrusPostLinkInspectionBoundary();
  TestVanillaAnchorResolution();
  TestActorLocalModSettingsState();
  TestHelmetToggleAppearanceBoundary();
  TestActorLocalActualEquipmentTransactions();
  TestSexLabPPlusStripContract();
  TestBodyMorphActivity();
  TestRefreshBackends();
  TestIedVisitorRoutingIsolation();
  TestHookTrampolineChainDecoding();
  TestFinalRenderedNudityContract();
  TestTngGenitalCoverIsolation();
  TestGenitalCompatibilityEnvironmentIsolation();
  TestPausedCharacterRotationIsolation();
  TestCatalogScrollbarReleaseIsolation();
  TestMenuCameraZoomSynchronization();

  if (g_failures != 0) {
    std::cerr << g_failures << " core behavior regression test(s) failed\n";
    return 1;
  }
  std::cout << "Core behavior regression tests passed\n";
  return 0;
}
