#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <cstdint>
#include <optional>
#include <string>

namespace RE {
class ActorWeightModel;
}

namespace sfs::native {
struct ActiveFittingAppearance {
  RE::TESObjectARMO *armor{nullptr};
  std::uint32_t slotMask{0};
  std::string rowKey;
  std::string itemKey;
};

enum class ArmorGenitalKeywordDisposition : std::uint8_t {
  kInherit,
  kReveal,
  kConceal,
};

// The final SFS 32-body / 49-lower classification overlay.  kInherit leaves
// plugin and KID keywords untouched.  An active reveal/conceal result takes
// precedence for SFS rendering and for the contextual virtual-token view,
// without deleting keywords owned by another mod from the source ARMO.
struct ArmorGenitalKeywordOverride {
  ArmorGenitalKeywordDisposition disposition{
      ArmorGenitalKeywordDisposition::kInherit};
  bool underwear{false};
  // A full-body slot-32 item already conceals through ordinary SOS/TNG slot
  // behavior. SFS can therefore override the rendered result without adding
  // redundant Concealing/Covering keywords to the source ARMO.
  bool materializeCoveringKeywords{true};

  [[nodiscard]] bool IsActive() const {
    return disposition != ArmorGenitalKeywordDisposition::kInherit;
  }
};

enum class ArmorRefreshReason : std::uint8_t {
  kDisplayState,
  kEquipmentChange,
};

void InstallArmorSkinningHooks();
void RefreshArmorFor(RE::Actor *a_actor, ArmorRefreshReason a_reason =
                                             ArmorRefreshReason::kDisplayState);
void RefreshPlayerArmor();
void QueueArmorRefreshFor(RE::Actor *a_actor,
                          ArmorRefreshReason a_reason =
                              ArmorRefreshReason::kDisplayState);
void QueuePlayerArmorRefresh();
void InvalidateQueuedArmorRefreshes();
void CaptureArmorClassificationKeywordBaseline();
void ScheduleLegacyArmorClassificationKeywordMigration();
void SerializeArmorClassificationMigrationState(
    SKSE::SerializationInterface *a_skse);
void DeserializeArmorClassificationMigrationState(
    SKSE::SerializationInterface *a_skse);
void RevertArmorClassificationMigrationState();
void SynchronizeArmorClassificationKeywords(RE::TESObjectARMO *a_armor);
[[nodiscard]] bool IsSFSOwnedRuntimeKeyword(
    const RE::TESObjectARMO *a_armor, const RE::BGSKeyword *a_keyword);
// Removes only runtime SOS/TNG keywords owned by SFS. Keywords supplied by
// ESP/KID or other mods are never touched.
void ClearSFSOwnedRuntimeKeywords();
[[nodiscard]] ArmorGenitalKeywordOverride
GetArmorGenitalKeywordOverride(const RE::TESObjectARMO *a_armor);
void BeginSOSUserArmorListSync();
void AddSOSUserRevealingArmor(RE::TESForm *a_form);
void AddSOSUserConcealingArmor(RE::TESForm *a_form);
[[nodiscard]] bool EndSOSUserArmorListSync();
void ClearSOSUserArmorLists();

[[nodiscard]] std::uint32_t GetActiveFittingSlotMask(RE::Actor *a_actor);
[[nodiscard]] std::uint32_t GetDisplayedFittingSlotMask(RE::Actor *a_actor);
// Returns the final rendered WornHasKeyword answer for the vanilla body
// coverage keywords (ArmorCuirass and ClothingBody). A missing value means
// that SFS is not managing this actor/display or the query is outside that
// narrow compatibility surface, so the caller must preserve Skyrim's result.
// This is read-only and never equips a proxy item.
[[nodiscard]] std::optional<bool>
GetDisplayedBodyKeywordState(RE::Actor *a_actor,
                             const RE::BGSKeyword *a_keyword);
[[nodiscard]] std::optional<ActiveFittingAppearance>
GetActiveFittingAppearanceForSlot(
    RE::Actor *a_actor, std::uint32_t a_slotMask,
    bool a_ignoreVirtualTokenSuppression = false);
[[nodiscard]] RE::TESObjectARMO *
GetActiveFittingArmorForSlot(RE::Actor *a_actor, std::uint32_t a_slotMask,
                             bool a_ignoreVirtualTokenSuppression = false);
[[nodiscard]] std::uint32_t
GetActiveFittingArmorSlotMaskForSlot(RE::Actor *a_actor,
                                     std::uint32_t a_slotMask,
                                     bool a_ignoreVirtualTokenSuppression =
                                         false);
[[nodiscard]] std::uint32_t GetHiddenRealEquipmentSlotMask(RE::Actor *a_actor);
[[nodiscard]] bool IsRealEquipmentHiddenForActorSlots(RE::Actor *a_actor,
                                                      std::uint32_t a_slotMask);
[[nodiscard]] bool ShouldOverrideSkinning(RE::TESObjectREFR *a_target);
[[nodiscard]] bool IsDisplayedFittingArmor(RE::Actor *a_actor,
                                           const RE::TESObjectARMO *a_armor);
// Read-only final-rendered equipment queries.  These include visible real
// armor and active registered appearances, while excluding real equipment
// that SFS is currently hiding for this actor.
[[nodiscard]] bool IsArmorShownForActor(RE::Actor *a_actor,
                                         const RE::TESObjectARMO *a_armor);
[[nodiscard]] bool IsShownArmorKeywordForActor(
    RE::Actor *a_actor, const RE::BGSKeyword *a_keyword);
using ShownArmorPredicate = bool (*)(const RE::TESObjectARMO *, void *);
[[nodiscard]] bool AnyShownArmorForActor(RE::Actor *a_actor,
                                          ShownArmorPredicate a_predicate,
                                          void *a_context = nullptr);
[[nodiscard]] bool IsActorVisuallyNakedForSlots(RE::Actor *a_actor,
                                                 std::uint32_t a_slotMask);
[[nodiscard]] bool ShouldBlockVanillaArmor(RE::TESObjectARMO *a_armor,
                                           RE::TESObjectREFR *a_target);
[[nodiscard]] bool ShouldBlockDavInitWornArmor(RE::TESObjectARMO *a_armor,
                                               RE::TESObjectREFR *a_target);
[[nodiscard]] std::uint32_t
GetDisplayWornMask(RE::InventoryChanges *a_inventory,
                   RE::TESObjectREFR *a_target, std::uint32_t a_baseWornMask);
void ApplyAdditionalDisplayArmors(RE::Actor *a_actor,
                                  RE::ActorWeightModel *a_actorWeightModel);
void ApplyDisplaySkinning(RE::Actor *a_actor,
                          RE::ActorWeightModel *a_actorWeightModel);
// IED's custom-skin hook expects its concrete visitor layout. The native hook
// records that ABI boundary here so the filtering path can use the original
// engine visitor and request an actor-level IED refresh afterward.
void SetIedVisitWornItemsChainTarget(std::uintptr_t a_chainTarget);
void VisitWornItemsWithHiddenRealEquipmentFilter(
    RE::InventoryChanges *a_inventory,
    RE::InventoryChanges::IItemChangeVisitor *a_visitor,
    RE::TESObjectREFR *a_target, std::uintptr_t a_visitWornItems);
} // namespace sfs::native
