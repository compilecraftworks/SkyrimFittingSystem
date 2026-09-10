#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "EquipmentCatalog.h"
#include "conditions/Status.h"
#include "native/ArmorSkinning.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/FittingSlotState.h"
#include "native/HelmetToggle2Rules.h"
#include "features/devious_devices/DeviousDevicesIntegration.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include "ui/Menu.h"
#include "workbench/AppearanceSlotProtection.h"
#include "workbench/AutomaticEquipmentVisibility.h"
#include "workbench/ItemFactory.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace sfs::workbench {
std::uint64_t AllocateVariantWorkbenchUiIdentity() {
  static std::atomic_uint64_t nextIdentity{1};
  return nextIdentity.fetch_add(1, std::memory_order_relaxed);
}

namespace {
std::atomic_uint64_t &NextVariantWorkbenchRegistrationOrder() {
  static std::atomic_uint64_t nextOrder{1};
  return nextOrder;
}
} // namespace

std::uint64_t AllocateVariantWorkbenchRegistrationOrder() {
  return NextVariantWorkbenchRegistrationOrder().fetch_add(
      1, std::memory_order_relaxed);
}

void ObserveVariantWorkbenchRegistrationOrder(const std::uint64_t a_order) {
  auto &nextOrder = NextVariantWorkbenchRegistrationOrder();
  auto expected = nextOrder.load(std::memory_order_relaxed);
  const auto desired = a_order + 1;
  while (expected < desired &&
         !nextOrder.compare_exchange_weak(expected, desired,
                                          std::memory_order_relaxed,
                                          std::memory_order_relaxed)) {
  }
}

namespace {
using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

constexpr std::array kTrackedWornSlots{BipedSlot::kHead,
                                       BipedSlot::kHair,
                                       BipedSlot::kBody,
                                       BipedSlot::kHands,
                                       BipedSlot::kForearms,
                                       BipedSlot::kAmulet,
                                       BipedSlot::kRing,
                                       BipedSlot::kFeet,
                                       BipedSlot::kCalves,
                                       BipedSlot::kShield,
                                       BipedSlot::kTail,
                                       BipedSlot::kLongHair,
                                       BipedSlot::kCirclet,
                                       BipedSlot::kEars,
                                       BipedSlot::kModMouth,
                                       BipedSlot::kModNeck,
                                       BipedSlot::kModChestPrimary,
                                       BipedSlot::kModBack,
                                       BipedSlot::kModMisc1,
                                       BipedSlot::kModPelvisPrimary,
                                       BipedSlot::kDecapitateHead,
                                       BipedSlot::kDecapitate,
                                       BipedSlot::kModPelvisSecondary,
                                       BipedSlot::kModLegRight,
                                       BipedSlot::kModLegLeft,
                                       BipedSlot::kModFaceJewelry,
                                       BipedSlot::kModChestSecondary,
                                       BipedSlot::kModShoulder,
                                       BipedSlot::kModArmLeft,
                                       BipedSlot::kModArmRight,
                                       BipedSlot::kModMisc2,
                                       BipedSlot::kFX01};

constexpr std::uint64_t SlotBit(const BipedSlot a_slot) {
  return static_cast<std::uint64_t>(std::to_underlying(a_slot));
}

std::string BuildSlotKey(const std::uint64_t a_slotMask) {
  const auto slotNumber = sfs::armor::GetArmorSlotNumber(a_slotMask);
  if (slotNumber == 0) {
    return {};
  }

  return "slot:" + std::to_string(slotNumber);
}

std::string BuildArmorSourceKey(const RE::FormID a_formID) {
  return "armor:" + sfs::armor::FormatFormID(a_formID);
}

std::string BuildRowKey(const std::string_view a_sourceKey,
                        const std::optional<std::string> &a_conditionId,
                        const RE::FormID a_ownerActorFormID = 0) {
  std::string key(a_sourceKey);
  key.append("|condition:");
  if (a_conditionId.has_value()) {
    key.append(*a_conditionId);
  } else {
    key.append("null");
  }
  if (a_ownerActorFormID != 0) {
    key.append("|actor:");
    key.append(sfs::armor::FormatFormID(a_ownerActorFormID));
  }
  return key;
}

void UpdateRowIdentity(VariantWorkbenchRow &a_row) {
  a_row.key =
      BuildRowKey(a_row.sourceKey, a_row.conditionId, a_row.ownerActorFormID);
  a_row.equipped.key = a_row.key;
}

int ScorePreferredTargetSlots(
    const std::uint64_t a_itemMask, const std::uint64_t a_targetMask,
    const std::initializer_list<BipedSlot> &a_sourceSlots,
    const std::initializer_list<BipedSlot> &a_preferredTargetSlots) {
  for (const auto sourceSlot : a_sourceSlots) {
    if ((a_itemMask & SlotBit(sourceSlot)) == 0) {
      continue;
    }

    int score = static_cast<int>(a_preferredTargetSlots.size());
    for (const auto targetSlot : a_preferredTargetSlots) {
      if ((a_targetMask & SlotBit(targetSlot)) != 0) {
        return score;
      }
      --score;
    }
  }

  return -1;
}

int ScoreFallbackTargetRow(const std::uint64_t a_itemMask,
                           const std::uint64_t a_targetMask) {
  int bestScore = -1;

  const std::array scores{
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kCirclet},
                                {BipedSlot::kHead, BipedSlot::kHair,
                                 BipedSlot::kLongHair, BipedSlot::kEars}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kHead},
                                {BipedSlot::kCirclet, BipedSlot::kHair,
                                 BipedSlot::kLongHair, BipedSlot::kEars}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kHair, BipedSlot::kLongHair, BipedSlot::kEars},
          {BipedSlot::kHead, BipedSlot::kCirclet}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kAmulet, BipedSlot::kModNeck},
          {BipedSlot::kModNeck, BipedSlot::kAmulet, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kRing},
          {BipedSlot::kModFaceJewelry, BipedSlot::kCirclet, BipedSlot::kEars}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kHands},
                                {BipedSlot::kForearms, BipedSlot::kBody}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask,
                                {BipedSlot::kForearms},
                                {BipedSlot::kHands, BipedSlot::kBody}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kFeet},
                                {BipedSlot::kCalves, BipedSlot::kBody}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kCalves},
                                {BipedSlot::kFeet, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModFaceJewelry, BipedSlot::kModMouth},
          {BipedSlot::kHead, BipedSlot::kCirclet, BipedSlot::kEars}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModChestPrimary, BipedSlot::kModChestSecondary,
           BipedSlot::kModBack, BipedSlot::kModShoulder,
           BipedSlot::kModPelvisPrimary, BipedSlot::kModPelvisSecondary},
          {BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModArmLeft, BipedSlot::kModArmRight},
          {BipedSlot::kHands, BipedSlot::kForearms, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModLegLeft, BipedSlot::kModLegRight},
          {BipedSlot::kFeet, BipedSlot::kCalves, BipedSlot::kBody}),
  };

  for (const auto score : scores) {
    bestScore = (std::max)(bestScore, score);
  }

  return bestScore;
}

std::uint64_t GetPlacementSlotMask(const EquipmentWidgetItem &a_item) {
  if (a_item.IsSlot()) {
    return a_item.slotMask;
  }

  const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_item.formID);
  const auto displaySlotMask = armor::GetArmorWorkbenchSlotMask(armor);
  return displaySlotMask != 0 ? displaySlotMask : a_item.slotMask;
}

std::uint64_t GetRepresentativeSlotMask(const EquipmentWidgetItem &a_item) {
  const auto slotMask = GetPlacementSlotMask(a_item);
  if (armor::GetArmorSlotNumber(slotMask) != 0) {
    return slotMask;
  }

  for (const auto slot : kTrackedWornSlots) {
    const auto candidate = SlotBit(slot);
    if ((slotMask & candidate) != 0) {
      return candidate;
    }
  }

  return 0;
}
bool IsValidRowIndex(const int a_rowIndex, const std::size_t a_rowCount) {
  return a_rowIndex >= 0 && a_rowIndex < static_cast<int>(a_rowCount);
}

[[nodiscard]] bool IsPlayerActor(const RE::Actor *a_actor) {
  const auto *player = RE::PlayerCharacter::GetSingleton();
  return player != nullptr && a_actor != nullptr &&
         a_actor->GetFormID() == player->GetFormID();
}

[[nodiscard]] bool
IsConditionActiveForActor(const std::optional<std::string> &a_conditionId,
                          RE::Actor *a_actor) {
  if (!a_conditionId.has_value() || !a_actor) {
    return false;
  }

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return false;
  }

  auto conditionStateLock = menu->AcquireConditionStateLock();
  auto &conditions = menu->GetConditions();
  const auto *definition =
      sfs::conditions::FindDefinitionById(conditions, *a_conditionId);
  if (!definition || !sfs::conditions::IsWorkbenchSelectable(*definition)) {
    return false;
  }

  if (!sfs::conditions::EvaluateDefinitionStatus(*definition, conditions)
           .IsActive()) {
    return false;
  }

  auto materialized =
      sfs::conditions::MaterializeConditionById(*a_conditionId, conditions);
  if (!materialized || !materialized->condition) {
    return false;
  }

  return materialized->condition->IsTrue(a_actor, a_actor);
}

[[nodiscard]] bool IsRowActiveForActor(const VariantWorkbenchRow &a_row,
                                       RE::Actor *a_actor) {
  if (!a_row.IsOwnedByActor(a_actor)) {
    return false;
  }
  if (a_row.conditionId.has_value()) {
    return IsConditionActiveForActor(a_row.conditionId, a_actor);
  }

  return a_row.HasOwnerActor() || IsPlayerActor(a_actor);
}

template <class F>
void VisitDistinctWornArmorItems(RE::Actor *a_actor, F &&a_visit) {
  if (a_actor == nullptr) {
    return;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  for (const auto slot : kTrackedWornSlots) {
    const auto *armor = a_actor->GetWornArmor(slot);
    if (armor == nullptr || armor::IsSosTngInternalArmor(armor) ||
        !seenArmorForms.insert(armor->GetFormID()).second) {
      continue;
    }

    EquipmentWidgetItem equipped{};
    if (!workbench::BuildCatalogItem(armor->GetFormID(), equipped)) {
      continue;
    }

    a_visit(armor, equipped);
  }
}

struct WornArmorState {
  RE::FormID formID{0};
  std::uint64_t normalizedSlotMask{0};
};

void ClearAutomaticEquipmentBinding(EquipmentWidgetItem &a_item) {
  a_item.automaticEquipmentBindingMode = 0;
  a_item.automaticEquipmentAnchorSlotMask = 0;
  a_item.automaticEquipmentAnchorFormID = 0;
  a_item.automaticEquipmentSuppressed = false;
  a_item.automaticEquipmentUserVisible = false;
}

[[nodiscard]] bool UpdateAutomaticEquipmentVisibility(
    const RE::Actor *a_actor, std::vector<VariantWorkbenchRow> &a_rows,
    const std::vector<WornArmorState> &a_wornArmors) {
  if (!a_actor) {
    return false;
  }

  const auto effectiveMode = GetEffectiveExternalModStripLinkMode();
  const bool customPolicy =
      GetExternalModStripLinkMode() == ExternalModStripLinkMode::Custom;
  const bool directMode =
      effectiveMode == ExternalModStripLinkMode::DirectSlots;
  const bool enabled =
      effectiveMode == ExternalModStripLinkMode::VanillaSlots || directMode;
  const auto externallySuppressedSlotMask =
      sfs::native::external_equipment::GetSuppressedActualSlotMask(
          a_actor->GetFormID());
  const auto protectedSlotMask = GetEffectiveAppearanceProtectedSlotMask();
  std::uint64_t wornSlotMask = 0;
  for (const auto &worn : a_wornArmors) {
    wornSlotMask |= worn.normalizedSlotMask;
  }
  const auto resolveVanillaAnchorSlotMask =
      [&](const RE::TESObjectARMO *a_appearance) -> std::uint64_t {
    const auto priority = GetVanillaAnchorPriority(a_appearance);
    return ResolveVanillaAnchorSlotMask(
        priority, protectedSlotMask, externallySuppressedSlotMask,
        wornSlotMask);
  };
  bool changed = false;
  for (auto &row : a_rows) {
    if (row.ownerActorFormID != a_actor->GetFormID()) {
      continue;
    }

    for (auto &item : row.overrides) {
      const auto oldBindingMode = item.automaticEquipmentBindingMode;
      const auto oldAnchorSlotMask = item.automaticEquipmentAnchorSlotMask;
      const auto oldAnchorFormID = item.automaticEquipmentAnchorFormID;
      const auto oldSuppressed = item.automaticEquipmentSuppressed;
      const auto oldUserVisible = item.automaticEquipmentUserVisible;

      item.automaticEquipmentSuppressed = false;
      const auto *appearance =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(item.formID);
      const auto appearanceSlotMask = row.GetOverrideDisplaySlotMask(item);
      if (!appearance || !enabled ||
          !IsExternalModStripLinkAppearanceEnabled(appearanceSlotMask)) {
        ClearAutomaticEquipmentBinding(item);
        changed = changed ||
                  oldBindingMode != item.automaticEquipmentBindingMode ||
                  oldAnchorSlotMask != item.automaticEquipmentAnchorSlotMask ||
                  oldAnchorFormID != item.automaticEquipmentAnchorFormID ||
                  oldSuppressed != item.automaticEquipmentSuppressed ||
                  oldUserVisible != item.automaticEquipmentUserVisible;
        continue;
      }

      std::uint8_t bindingMode = static_cast<std::uint8_t>(
          AutomaticEquipmentVisibilityMode::VanillaSlots);
      if (directMode) {
        std::uint64_t anchorSlotMask = 0;
        if (GetCustomDirectStripLinkAutomaticBaseMode() ==
            ExternalModStripLinkMode::ModSettingsSlots) {
          // Direct editing under a mod-settings base only remaps the queried
          // slot. The existing mod-settings transaction decides per actor
          // whether that target currently owns real worn armor or needs a
          // virtual token, so it must never acquire a parallel actual observer
          // anchor here.
          ClearAutomaticEquipmentBinding(item);
        } else {
          const auto explicitAnchor =
              ResolveCustomDirectStripLinkAnchorSlotMask(appearanceSlotMask);
          if (explicitAnchor.has_value()) {
            anchorSlotMask = *explicitAnchor;
            bindingMode = static_cast<std::uint8_t>(
                AutomaticEquipmentVisibilityMode::CustomSlots);
          } else {
            // Untouched rows under the vanilla base remain dynamically
            // classified. Only slots explicitly edited in the popup are fixed.
            anchorSlotMask = resolveVanillaAnchorSlotMask(appearance);
          }
          if (anchorSlotMask == 0) {
            ClearAutomaticEquipmentBinding(item);
          } else {
            item.automaticEquipmentBindingMode = bindingMode;
            item.automaticEquipmentAnchorSlotMask = anchorSlotMask;
            item.automaticEquipmentAnchorFormID = 0;
            item.automaticEquipmentSuppressed =
                (anchorSlotMask & externallySuppressedSlotMask) != 0;
          }
        }
      } else {
        const auto explicitAnchor =
            customPolicy
                ? ResolveCustomDirectStripLinkAnchorSlotMask(
                      appearanceSlotMask)
                : std::optional<std::uint64_t>{};
        const auto anchorSlotMask = explicitAnchor.has_value()
                                        ? *explicitAnchor
                                        : resolveVanillaAnchorSlotMask(
                                              appearance);
        if (explicitAnchor.has_value()) {
          bindingMode = static_cast<std::uint8_t>(
              AutomaticEquipmentVisibilityMode::CustomSlots);
        }
        if (anchorSlotMask == 0) {
          ClearAutomaticEquipmentBinding(item);
        } else {
          item.automaticEquipmentBindingMode = bindingMode;
          item.automaticEquipmentAnchorSlotMask = anchorSlotMask;
          item.automaticEquipmentAnchorFormID = 0;
          item.automaticEquipmentSuppressed =
              (anchorSlotMask & externallySuppressedSlotMask) != 0;
        }
      }

      if (!item.automaticEquipmentSuppressed) {
        item.automaticEquipmentUserVisible = false;
      }

      const bool itemChanged =
          oldBindingMode != item.automaticEquipmentBindingMode ||
          oldAnchorSlotMask != item.automaticEquipmentAnchorSlotMask ||
          oldAnchorFormID != item.automaticEquipmentAnchorFormID ||
          oldSuppressed != item.automaticEquipmentSuppressed ||
          oldUserVisible != item.automaticEquipmentUserVisible;
      changed |= itemChanged;
      if (itemChanged) {
        logger::info(
            "Actual-equipment appearance link actor={:08X} appearance={:08X} "
            "globalMode={} bindingMode={} anchorSlot={:016X} anchorForm={:08X} "
            "suppressed={} userVisible={}",
            a_actor->GetFormID(), item.formID,
            static_cast<std::uint8_t>(GetExternalModStripLinkMode()),
            bindingMode, item.automaticEquipmentAnchorSlotMask,
            item.automaticEquipmentAnchorFormID,
            item.automaticEquipmentSuppressed,
            item.automaticEquipmentUserVisible);
      }
    }
  }
  return changed;
}

} // namespace

bool VariantWorkbench::RemoveProtectedAppearanceRegistrations() {
  const auto previousRows = rows_;
  bool changed = false;
  std::unordered_set<RE::FormID> ownerActorFormIDs;
  for (auto &row : rows_) {
    ownerActorFormIDs.insert(row.ownerActorFormID);
    const auto oldSize = row.overrides.size();
    std::erase_if(row.overrides, [&](const EquipmentWidgetItem &a_item) {
      return row.IsProtectedAppearance(a_item);
    });
    changed = changed || row.overrides.size() != oldSize;
  }

  // A conditional fitting target follows the same registration rule. Keep the
  // condition card and its order, but clear only the protected appearance.
  for (auto &rule : conditionalVisibilityRules_) {
    if (rule.targetKind != ConditionalVisibilityTargetKind::Fitting ||
        rule.target.formID == 0) {
      continue;
    }
    const auto *targetArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(rule.target.formID);
    if (targetArmor == nullptr ||
        !IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(targetArmor))) {
      continue;
    }
    rule.target = {};
    changed = true;
  }

  if (!changed) {
    return false;
  }

  // This removes newly empty base slot rows while deliberately retaining
  // condition rows and all real-equipment rows.
  for (const auto ownerActorFormID : ownerActorFormIDs) {
    static_cast<void>(NormalizeOverrideRowsForActor(ownerActorFormID));
  }
  static_cast<void>(PruneFullyEmptyConditionalRows());
  InvalidateRemovedAppearanceAutomation(previousRows);
  RebuildRowOrder();
  MarkChanged();
  return true;
}

VariantWorkbench::InitialEquippedState
VariantWorkbench::BuildInitialEquippedState(RE::Actor *a_actor) {
  InitialEquippedState initialState;
  VisitDistinctWornArmorItems(
      a_actor, [&](const RE::TESObjectARMO *a_armor,
                   const EquipmentWidgetItem &a_equipped) {
        initialState.wornArmorForms.insert(a_armor->GetFormID());
        const auto addonSlotMask = armor::GetArmorWorkbenchSlotMask(a_armor);
        initialState.occupiedSlotMask |=
            addonSlotMask != 0 ? addonSlotMask : a_equipped.slotMask;
      });
  return initialState;
}

std::uint64_t VariantWorkbenchRow::GetSelectionConflictSlotMask() const {
  if (equipped.IsSlot()) {
    return equipped.slotMask;
  }

  if (!isEquipped && !overrides.empty()) {
    const auto overrideSlotMask = GetOverrideVisualSlotMask();
    if (overrideSlotMask != 0) {
      return overrideSlotMask;
    }
  }

  const auto *armor =
      RE::TESForm::LookupByID<RE::TESObjectARMO>(equipped.formID);
  if (!armor) {
    return equipped.slotMask;
  }

  const auto displaySlotMask = armor::GetArmorWorkbenchSlotMask(armor);
  return displaySlotMask != 0 ? displaySlotMask : equipped.slotMask;
}

std::uint64_t VariantWorkbenchRow::GetSelectionDisplaySlotMask() const {
  if (equipped.IsSlot()) {
    return equipped.slotMask;
  }

  const auto *armor =
      RE::TESForm::LookupByID<RE::TESObjectARMO>(equipped.formID);
  if (armor != nullptr) {
    const auto displaySlotMask = armor::GetArmorDisplaySlotMask(armor);
    if (displaySlotMask != 0) {
      return displaySlotMask;
    }
  }
  return GetSelectionConflictSlotMask();
}

std::uint64_t VariantWorkbenchRow::GetOverrideDisplaySlotMask(
    const EquipmentWidgetItem &a_item) const {
  const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_item.formID);
  if (armor != nullptr) {
    const auto displaySlotMask = armor::GetArmorDisplaySlotMask(armor);
    if (displaySlotMask != 0) {
      return displaySlotMask;
    }
  }
  return GetOverrideVisualSlotMask(a_item);
}

bool VariantWorkbenchRow::IsProtectedAppearance(
    const EquipmentWidgetItem &a_item) const {
  return IsAppearanceRegistrationProtectedSlotMask(
      GetOverrideDisplaySlotMask(a_item));
}

bool VariantWorkbenchRow::IsAlwaysVisibleActualEquipment() const {
  return isEquipped && !IsSlotRow() && equipped.formID != 0 &&
         IsAppearanceRegistrationProtectedSlotMask(
             GetSelectionDisplaySlotMask());
}

bool VariantWorkbench::ResolveCatalogArmors(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<const RE::TESObjectARMO *> &a_armors) const {
  a_armors.clear();

  for (const auto formID :
       EquipmentCatalog::Get().ResolveArmorFormIDs(a_formIDs)) {
    const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (!armor || armor::IsSosTngInternalArmor(armor) ||
        !armor::HasArmorAddons(armor)) {
      continue;
    }
    a_armors.push_back(armor);
  }

  return !a_armors.empty();
}
bool VariantWorkbench::CanAcceptOverrideWithPendingAssignments(
    int a_targetRowIndex, const EquipmentWidgetItem &a_item,
    const std::vector<PlannedCatalogAssignment> &a_pendingAssignments) const {
  if (!CanAcceptOverride(a_targetRowIndex, a_item)) {
    return false;
  }

  for (const auto &assignment : a_pendingAssignments) {
    if (assignment.rowIndex == a_targetRowIndex &&
        assignment.armorFormID == a_item.formID) {
      return false;
    }
  }

  return true;
}

void VariantWorkbench::RebuildRowOrder() {
  rowOrder_.clear();
  rowOrder_.reserve(rows_.size());
  for (const auto &row : rows_) {
    rowOrder_.push_back(row.key);
  }
}

void VariantWorkbench::InvalidateAppearanceAutomation(
    const RE::FormID a_actorFormID, const RE::FormID a_appearanceFormID,
    const std::uint32_t a_visualSlotMask, const bool a_deleted) {
  if (deferRuntimeEffects_) {
    deferredInvalidations_.push_back({a_actorFormID, a_appearanceFormID,
                                      a_visualSlotMask, a_deleted});
    return;
  }
  sfs::virtual_tokens::InvalidateVirtualWornTokenAutomationForAppearance(
      a_actorFormID, a_appearanceFormID, a_visualSlotMask, a_deleted);
}

void VariantWorkbench::InvalidateRemovedAppearanceAutomation(
    const std::vector<VariantWorkbenchRow> &a_previousRows) {
  struct AppearanceIdentity {
    RE::FormID actorFormID{0};
    RE::FormID appearanceFormID{0};
    std::uint32_t visualSlotMask{0};

    [[nodiscard]] bool operator==(const AppearanceIdentity &) const = default;
  };

  std::vector<AppearanceIdentity> invalidated;
  for (const auto &previousRow : a_previousRows) {
    for (const auto &previousItem : previousRow.overrides) {
      if (previousItem.formID == 0) {
        continue;
      }
      const AppearanceIdentity identity{
          previousRow.ownerActorFormID, previousItem.formID,
          static_cast<std::uint32_t>(
              previousRow.GetOverrideVisualSlotMask(previousItem))};
      if (std::ranges::find(invalidated, identity) != invalidated.end()) {
        continue;
      }
      const bool retained = std::ranges::any_of(
          rows_, [&](const VariantWorkbenchRow &a_currentRow) {
            return a_currentRow.ownerActorFormID == identity.actorFormID &&
                   std::ranges::any_of(
                       a_currentRow.overrides,
                       [&](const EquipmentWidgetItem &a_currentItem) {
                         return a_currentItem.formID ==
                                    identity.appearanceFormID &&
                                static_cast<std::uint32_t>(
                                    a_currentRow.GetOverrideVisualSlotMask(
                                        a_currentItem)) ==
                                    identity.visualSlotMask;
                       });
          });
      if (retained) {
        continue;
      }
      invalidated.push_back(identity);
      InvalidateAppearanceAutomation(
          identity.actorFormID, identity.appearanceFormID,
          identity.visualSlotMask, true);
    }
  }
}

void VariantWorkbench::MarkChanged(const bool a_affectsNativeDisplay) {
  ++revision_;
  if (a_affectsNativeDisplay) {
    ++nativeDisplayRevision_;
  }
  if (!deferRuntimeEffects_) {
    sfs::virtual_tokens::UpdateVirtualWornTokenCache();
  }
}

std::optional<bool> VariantWorkbench::GetEquippedHiddenForActor(
    const RE::FormID a_actorFormID, const std::string_view a_rowKey) const {
  if (a_actorFormID == 0 || a_rowKey.empty()) {
    return std::nullopt;
  }

  const auto actorIt = equippedHiddenByActor_.find(a_actorFormID);
  if (actorIt == equippedHiddenByActor_.end()) {
    return std::nullopt;
  }

  const auto rowIt = actorIt->second.find(std::string(a_rowKey));
  if (rowIt == actorIt->second.end()) {
    return std::nullopt;
  }

  return rowIt->second;
}

void VariantWorkbench::PersistCurrentEquippedHiddenStateForActor(
    const RE::FormID a_actorFormID) {
  if (a_actorFormID == 0) {
    return;
  }

  auto &actorRows = equippedHiddenByActor_[a_actorFormID];
  bool storedAny = false;
  for (const auto &row : rows_) {
    if (row.ownerActorFormID != a_actorFormID || !row.isEquipped ||
        row.IsSlotRow() || row.key.empty()) {
      continue;
    }

    actorRows.insert_or_assign(row.key, row.hideEquipped);
    storedAny = true;
  }

  if (!storedAny && actorRows.empty()) {
    equippedHiddenByActor_.erase(a_actorFormID);
  }
}

bool VariantWorkbench::ResolveEquippedHiddenForActor(
    RE::Actor *a_actor, const VariantWorkbenchRow &a_row) const {
  auto stateLock = AcquireStateLock();
  if (a_row.IsAlwaysVisibleActualEquipment()) {
    return false;
  }

  // Actual armor introduced by an accepted external scene receives one
  // explicit actor-row visibility value. Let that serialized value override
  // the broad "hide real equipment" switch for this item only: false keeps the
  // replacement outfit/restraint visible, while a later individual or global
  // eye action stores the user's latest choice. Never clear the actor-wide
  // switch just to reveal one new item; unrelated equipment (notably a 31/42
  // helmet) must retain its exact manual state.
  if (GetExternalModStripLinkMode() !=
          ExternalModStripLinkMode::Disabled &&
      a_actor != nullptr && !a_row.key.empty() && a_row.isEquipped &&
      !a_row.IsSlotRow()) {
    const auto *armor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(a_row.equipped.formID);
    const bool ddRenderedDevice =
        sfs::devious_devices::IsDeviousDevicesRenderedDevice(armor);
    const bool eventAddedActual =
        sfs::virtual_tokens::IsVirtualWornTokenEventAddedArmor(
            a_actor->GetFormID(), a_row.equipped.formID) ||
        sfs::native::external_equipment::IsEventAddedActualEquipment(
            a_actor->GetFormID(), a_row.equipped.formID);
    const bool externallyIntroduced = ddRenderedDevice || eventAddedActual;
    if (externallyIntroduced) {
      if (const auto hidden =
              GetEquippedHiddenForActor(a_actor->GetFormID(), a_row.key);
          hidden.has_value()) {
        return *hidden;
      }
      // Generic scene gear may reach the row model after its equip event. Its
      // actor-local event marker is sufficient to supply the ordinary visible
      // default until the first explicit eye value is stored.
      if (eventAddedActual) {
        return false;
      }
    }
  }

  if (a_actor != nullptr) {
    if (const auto *menu = sfs::Menu::GetSingleton();
        menu != nullptr &&
        menu->HideRealEquipmentWithFittingForActor(a_actor)) {
      return true;
    }
  }

  return ResolveEquippedIndividualHiddenForActor(a_actor, a_row);
}

bool VariantWorkbench::ResolveEquippedIndividualHiddenForActor(
    RE::Actor *a_actor, const VariantWorkbenchRow &a_row) const {
  auto stateLock = AcquireStateLock();
  if (a_actor != nullptr && !a_row.key.empty()) {
    if (const auto hidden =
            GetEquippedHiddenForActor(a_actor->GetFormID(), a_row.key);
        hidden.has_value()) {
      return *hidden;
    }
  }

  if (a_actor != nullptr && !a_row.HasOwnerActor() && !IsPlayerActor(a_actor)) {
    return false;
  }

  return a_row.hideEquipped;
}
std::vector<int> VariantWorkbench::BuildCandidateRowIndices(
    const std::vector<int> *a_candidateRowIndices,
    const std::size_t a_rowCount) {
  if (a_candidateRowIndices != nullptr) {
    return *a_candidateRowIndices;
  }

  std::vector<int> indices;
  indices.reserve(a_rowCount);
  for (int rowIndex = 0; rowIndex < static_cast<int>(a_rowCount); ++rowIndex) {
    indices.push_back(rowIndex);
  }
  return indices;
}

std::uint64_t VariantWorkbench::GetLockedAppearanceSlotMaskForCandidateRows(
    const std::vector<int> *a_candidateRowIndices) const {
  std::unordered_set<RE::FormID> ownerActorFormIDs;
  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (IsValidRowIndex(rowIndex, rows_.size())) {
        ownerActorFormIDs.insert(
            rows_[static_cast<std::size_t>(rowIndex)].ownerActorFormID);
      }
    }
  }

  std::uint64_t lockedSlotMask = 0;
  for (const auto &row : rows_) {
    if (a_candidateRowIndices != nullptr &&
        !ownerActorFormIDs.contains(row.ownerActorFormID)) {
      continue;
    }
    for (const auto &item : row.overrides) {
      if (item.locked) {
        lockedSlotMask |= row.GetOverrideDisplaySlotMask(item);
      }
    }
  }
  return lockedSlotMask;
}

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable,
    const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
    const std::vector<int> *a_candidateRowIndices) const {
  return FindBestItemTargetRowIndexBySlotMask(
      GetPlacementSlotMask(a_item), a_requireAcceptable, &a_item,
      a_pendingAssignments, a_candidateRowIndices);
}

int VariantWorkbench::FindBestItemTargetRowIndexBySlotMask(
    const std::uint64_t a_targetSlotMask, const bool a_requireAcceptable,
    const EquipmentWidgetItem *a_item,
    const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
    const std::vector<int> *a_candidateRowIndices) const {
  int fallbackRowIndex = -1;
  int bestPrecedenceRowIndex = -1;
  int bestPrecedenceScore = -1;

  const auto visitRow = [&](const int rowIndex) -> bool {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped && !row.HasOverridesOrHideState()) {
      return false;
    }

    if (a_requireAcceptable && a_item != nullptr &&
        ((a_pendingAssignments != nullptr &&
          !CanAcceptOverrideWithPendingAssignments(rowIndex, *a_item,
                                                   *a_pendingAssignments)) ||
         (a_pendingAssignments == nullptr &&
          !CanAcceptOverride(rowIndex, *a_item)))) {
      return false;
    }

    if (a_targetSlotMask == 0) {
      if (fallbackRowIndex < 0) {
        fallbackRowIndex = rowIndex;
      }
      return false;
    }

    const auto rowSlotMask = row.GetSelectionConflictSlotMask();
    if ((rowSlotMask & a_targetSlotMask) != 0) {
      fallbackRowIndex = rowIndex;
      bestPrecedenceRowIndex = rowIndex;
      bestPrecedenceScore = (std::numeric_limits<int>::max)();
      return true;
    }

    if (fallbackRowIndex < 0) {
      fallbackRowIndex = rowIndex;
    }

    const auto precedenceScore =
        ScoreFallbackTargetRow(a_targetSlotMask, rowSlotMask);
    if (precedenceScore > bestPrecedenceScore) {
      bestPrecedenceScore = precedenceScore;
      bestPrecedenceRowIndex = rowIndex;
    }
    return false;
  };

  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
        continue;
      }

      if (visitRow(rowIndex)) {
        return rowIndex;
      }
    }
  } else {
    for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
         ++rowIndex) {
      if (visitRow(rowIndex)) {
        return rowIndex;
      }
    }
  }

  if (bestPrecedenceRowIndex >= 0) {
    return bestPrecedenceRowIndex;
  }

  return fallbackRowIndex;
}

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable) const {
  return FindBestCatalogTargetRowIndex(a_item, a_requireAcceptable, nullptr,
                                       nullptr);
}

bool VariantWorkbench::PlanCatalogAssignments(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<PlannedCatalogAssignment> &a_assignments,
    const std::vector<int> *a_candidateRowIndices) const {
  a_assignments.clear();

  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formIDs, armors)) {
    return false;
  }

  const auto lockedSlotMask =
      GetLockedAppearanceSlotMaskForCandidateRows(a_candidateRowIndices);

  for (const auto *armor : armors) {
    EquipmentWidgetItem item{};
    if (!armor || !workbench::BuildCatalogItem(armor->GetFormID(), item) ||
        (armor::GetArmorDisplaySlotMask(armor) & lockedSlotMask) != 0) {
      continue;
    }

    const auto rowIndex = FindBestCatalogTargetRowIndex(
        item, true, &a_assignments, a_candidateRowIndices);
    if (rowIndex < 0) {
      continue;
    }

    a_assignments.push_back({rowIndex, armor->GetFormID()});
  }

  return !a_assignments.empty();
}

bool VariantWorkbench::NormalizeOverrideRowsForActor(
    const RE::FormID a_ownerActorFormID) {
  struct PendingOverride {
    EquipmentWidgetItem item;
    std::optional<std::string> conditionId;
  };

  const auto originalRows = rows_;
  std::vector<PendingOverride> pendingOverrides;
  std::vector<VariantWorkbenchRow> normalizedRows;
  std::unordered_map<std::string, std::size_t> actorRowIndices;
  std::vector<std::pair<std::string, std::string>> rowKeyRedirects;
  normalizedRows.reserve(rows_.size());

  const auto mergeRowState = [](VariantWorkbenchRow &a_target,
                                const VariantWorkbenchRow &a_source) {
    a_target.hideEquipped = a_target.hideEquipped || a_source.hideEquipped;
    if (a_source.isEquipped) {
      a_target.equipped = a_source.equipped;
      a_target.isEquipped = true;
    }
    for (const auto &overrideItem : a_source.overrides) {
      const auto duplicate =
          std::ranges::find(a_target.overrides, overrideItem.formID,
                            &EquipmentWidgetItem::formID);
      if (duplicate == a_target.overrides.end()) {
        a_target.overrides.push_back(overrideItem);
      } else {
        duplicate->hidden = duplicate->hidden && overrideItem.hidden;
        duplicate->locked = duplicate->locked || overrideItem.locked;
        duplicate->automaticEquipmentUserVisible =
            duplicate->automaticEquipmentUserVisible ||
            overrideItem.automaticEquipmentUserVisible;
      }
    }
  };

  for (const auto &originalRow : rows_) {
    if (originalRow.ownerActorFormID != a_ownerActorFormID) {
      normalizedRows.push_back(originalRow);
      continue;
    }

    auto row = originalRow;
    const auto overrideConditionId = row.conditionId;
    row.overrides.clear();
    for (const auto &overrideItem : originalRow.overrides) {
      auto normalizedOverrideItem = overrideItem;
      if (GetRepresentativeSlotMask(normalizedOverrideItem) == 0) {
        row.overrides.push_back(std::move(normalizedOverrideItem));
        continue;
      }
      pendingOverrides.push_back(
          {std::move(normalizedOverrideItem), overrideConditionId});
    }

    const auto oldKey = row.key;
    if (!row.IsSlotRow()) {
      row.conditionId.reset();
    } else {
      row.hideEquipped = false;
    }
    UpdateRowIdentity(row);
    if (oldKey != row.key) {
      rowKeyRedirects.emplace_back(oldKey, row.key);
    }

    if (const auto existing = actorRowIndices.find(row.key);
        existing != actorRowIndices.end()) {
      mergeRowState(normalizedRows[existing->second], row);
      continue;
    }

    actorRowIndices.emplace(row.key, normalizedRows.size());
    normalizedRows.push_back(std::move(row));
  }

  rows_ = std::move(normalizedRows);

  const auto findActualTarget = [&](const EquipmentWidgetItem &a_item) -> int {
    const auto placementSlotMask = GetPlacementSlotMask(a_item);
    if (placementSlotMask == 0) {
      return -1;
    }

    for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
         ++rowIndex) {
      const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
      if (row.ownerActorFormID != a_ownerActorFormID || row.IsSlotRow() ||
          row.conditionId.has_value() || !row.isEquipped) {
        continue;
      }
      if ((row.GetSelectionConflictSlotMask() & placementSlotMask) != 0) {
        return rowIndex;
      }
    }
    return -1;
  };

  const auto findOrCreateSlotTarget =
      [&](const EquipmentWidgetItem &a_item,
          const std::optional<std::string> &a_conditionId) -> int {
    const auto representativeSlotMask = GetRepresentativeSlotMask(a_item);
    const auto sourceKey = BuildSlotKey(representativeSlotMask);
    if (sourceKey.empty()) {
      return -1;
    }

    const auto rowKey =
        BuildRowKey(sourceKey, a_conditionId, a_ownerActorFormID);
    const auto existing =
        std::ranges::find(rows_, rowKey, &VariantWorkbenchRow::key);
    if (existing != rows_.end()) {
      return static_cast<int>(std::distance(rows_.begin(), existing));
    }

    auto slotRow = BuildSlotRow(representativeSlotMask, a_conditionId,
                                a_ownerActorFormID, nullptr);
    if (!slotRow.has_value()) {
      return -1;
    }
    rows_.push_back(std::move(*slotRow));
    return static_cast<int>(rows_.size() - 1);
  };

  for (auto &pendingOverride : pendingOverrides) {
    int targetRowIndex = -1;
    if (!pendingOverride.conditionId.has_value()) {
      targetRowIndex = findActualTarget(pendingOverride.item);
    }
    if (targetRowIndex < 0) {
      targetRowIndex = findOrCreateSlotTarget(pendingOverride.item,
                                              pendingOverride.conditionId);
    }
    if (!IsValidRowIndex(targetRowIndex, rows_.size())) {
      continue;
    }

    auto &targetOverrides =
        rows_[static_cast<std::size_t>(targetRowIndex)].overrides;
    const auto duplicate =
        std::ranges::find(targetOverrides, pendingOverride.item.formID,
                          &EquipmentWidgetItem::formID);
    if (duplicate == targetOverrides.end()) {
      targetOverrides.push_back(std::move(pendingOverride.item));
    } else {
      duplicate->hidden = duplicate->hidden && pendingOverride.item.hidden;
      duplicate->locked = duplicate->locked || pendingOverride.item.locked;
      duplicate->automaticEquipmentUserVisible =
          duplicate->automaticEquipmentUserVisible ||
          pendingOverride.item.automaticEquipmentUserVisible;
    }
  }

  // Repair legacy stacked entries with the fitting-system rule: within one
  // actor and one base/condition layer, the first item claiming an original
  // armor slot owns the whole item and later overlaps are discarded. This
  // does not merge or compare different condition layers.
  std::unordered_map<std::string, std::uint64_t> claimedSlotsByCondition;
  for (const auto &row : rows_) {
    if (row.ownerActorFormID != a_ownerActorFormID) {
      continue;
    }
    auto &claimedSlotMask =
        claimedSlotsByCondition[row.conditionId.value_or(std::string{})];
    for (const auto &item : row.overrides) {
      if (item.locked) {
        claimedSlotMask |= row.GetOverrideDisplaySlotMask(item);
      }
    }
  }
  for (auto &row : rows_) {
    if (row.ownerActorFormID != a_ownerActorFormID) {
      continue;
    }
    auto &claimedSlotMask =
        claimedSlotsByCondition[row.conditionId.value_or(std::string{})];
    std::erase_if(row.overrides, [&](const EquipmentWidgetItem &a_item) {
      const auto displaySlotMask = row.GetOverrideDisplaySlotMask(a_item);
      if (displaySlotMask == 0) {
        return false;
      }
      if (a_item.locked) {
        return false;
      }
      if ((displaySlotMask & claimedSlotMask) != 0) {
        return true;
      }
      claimedSlotMask |= displaySlotMask;
      return false;
    });
  }

  for (auto rowIndex = static_cast<int>(rows_.size()) - 1; rowIndex >= 0;
       --rowIndex) {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (row.ownerActorFormID != a_ownerActorFormID || row.isEquipped ||
        row.conditionId.has_value() || !row.overrides.empty()) {
      continue;
    }

    if (!row.IsSlotRow() && !row.key.empty()) {
      equippedHiddenByActor_[a_ownerActorFormID].insert_or_assign(
          row.key, row.hideEquipped);
    }
    rows_.erase(rows_.begin() + rowIndex);
  }

  const bool changed = rows_ != originalRows;
  if (!changed) {
    return false;
  }

  for (auto &actorState : equippedHiddenByActor_) {
    auto &rowStates = actorState.second;
    for (const auto &[oldKey, newKey] : rowKeyRedirects) {
      const auto oldState = rowStates.find(oldKey);
      if (oldState == rowStates.end()) {
        continue;
      }
      const auto hidden = oldState->second;
      rowStates.erase(oldState);
      const auto newState = rowStates.find(newKey);
      if (newState == rowStates.end()) {
        rowStates.emplace(newKey, hidden);
      } else {
        newState->second = newState->second || hidden;
      }
    }
  }

  RebuildRowOrder();
  InvalidateRemovedAppearanceAutomation(originalRows);
  return true;
}
void VariantWorkbench::SyncRowsFromActor(RE::Actor *a_actor) {
  auto stateLock = AcquireStateLock();
  if (!a_actor) {
    return;
  }

  const auto actorFormID = a_actor->GetFormID();
  const auto syncOwnerActorFormID = actorFormID;
  const bool actorChanged =
      lastSyncedActorFormID_ != 0 && lastSyncedActorFormID_ != actorFormID;
  if (actorChanged) {
    PersistCurrentEquippedHiddenStateForActor(lastSyncedActorFormID_);
  }
  lastSyncedActorFormID_ = actorFormID;

  std::unordered_map<std::string, bool> previousEquippedState;
  std::unordered_map<std::string, bool> previousHideEquippedState;
  std::unordered_map<std::string, bool> previousAutomaticSuppressedState;
  for (auto &row : rows_) {
    if (row.ownerActorFormID != syncOwnerActorFormID) {
      continue;
    }
    previousEquippedState[row.key] = row.isEquipped;
    previousHideEquippedState[row.key] = row.hideEquipped;
    for (const auto &item : row.overrides) {
      previousAutomaticSuppressedState[row.key + "|appearance:" +
                                       armor::FormatFormID(item.formID)] =
          item.automaticEquipmentSuppressed;
    }
    row.isEquipped = false;
  }

  const auto resolveEquippedHidden = [&](const std::string &a_rowKey) {
    if (const auto hidden = GetEquippedHiddenForActor(actorFormID, a_rowKey);
        hidden.has_value()) {
      return *hidden;
    }

    if (!actorChanged) {
      if (const auto previousIt = previousHideEquippedState.find(a_rowKey);
          previousIt != previousHideEquippedState.end()) {
        return previousIt->second;
      }
    }
    return false;
  };

  std::uint64_t occupiedSlotMask = 0;
  std::vector<VariantWorkbenchRow> newlyEquippedRows;
  std::vector<std::string> newlyEquippedRowKeys;
  std::vector<WornArmorState> wornArmors;

  VisitDistinctWornArmorItems(
      a_actor, [&](const RE::TESObjectARMO *a_armor,
                   const EquipmentWidgetItem &a_equipped) {
        const auto formID = a_armor->GetFormID();
        const auto addonSlotMask = armor::GetArmorWorkbenchSlotMask(a_armor);
        occupiedSlotMask |=
            addonSlotMask != 0 ? addonSlotMask : a_equipped.slotMask;
        const auto automaticControlSlotMask =
            GetAutomaticEquipmentControlSlotMask(a_armor);
        wornArmors.push_back(
            {formID, automaticControlSlotMask != 0
                         ? automaticControlSlotMask
                         : a_equipped.slotMask});
        const auto sourceKey = BuildArmorSourceKey(formID);
        bool hasActualRow = false;
        for (auto &row : rows_) {
          if (row.sourceKey != sourceKey ||
              row.ownerActorFormID != syncOwnerActorFormID ||
              row.conditionId.has_value()) {
            continue;
          }

          row.equipped = a_equipped;
          row.isEquipped = true;
          UpdateRowIdentity(row);
          row.hideEquipped = resolveEquippedHidden(row.key);
          hasActualRow = true;
        }
        if (hasActualRow) {
          return;
        }

        VariantWorkbenchRow row{};
        row.sourceKey = sourceKey;
        row.conditionId.reset();
        row.ownerActorFormID = syncOwnerActorFormID;
        row.equipped = a_equipped;
        row.isEquipped = true;
        UpdateRowIdentity(row);
        row.hideEquipped = resolveEquippedHidden(row.key);
        newlyEquippedRowKeys.push_back(row.key);
        newlyEquippedRows.push_back(std::move(row));
      });

  std::vector<RE::FormID> observedWornArmorFormIDs;
  observedWornArmorFormIDs.reserve(wornArmors.size());
  std::ranges::transform(wornArmors,
                         std::back_inserter(observedWornArmorFormIDs),
                         &WornArmorState::formID);
  const auto pendingEquipmentStates = ReconcileAutomaticEquipmentStateEvents(
      actorFormID, observedWornArmorFormIDs);
  for (const auto &pendingState : pendingEquipmentStates) {
    if (!pendingState.equipped) {
      std::erase_if(wornArmors, [&](const auto &worn) {
        return worn.formID == pendingState.armorFormID;
      });
      continue;
    }

    if (std::ranges::find(wornArmors, pendingState.armorFormID,
                          &WornArmorState::formID) != wornArmors.end()) {
      continue;
    }
    const auto *pendingArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(pendingState.armorFormID);
    if (!pendingArmor || armor::IsSosTngInternalArmor(pendingArmor)) {
      continue;
    }
    const auto pendingSlotMask =
        GetAutomaticEquipmentControlSlotMask(pendingArmor);
    if (pendingSlotMask != 0) {
      wornArmors.push_back({pendingState.armorFormID, pendingSlotMask});
    }
  }

  if (!newlyEquippedRows.empty()) {
    rows_.insert(rows_.begin(),
                 std::make_move_iterator(newlyEquippedRows.begin()),
                 std::make_move_iterator(newlyEquippedRows.end()));
    rowOrder_.insert(rowOrder_.begin(),
                     std::make_move_iterator(newlyEquippedRowKeys.begin()),
                     std::make_move_iterator(newlyEquippedRowKeys.end()));
  }

  bool changed = !newlyEquippedRows.empty();
  changed |= UpdateAutomaticEquipmentVisibility(a_actor, rows_, wornArmors);
  for (const auto &row : rows_) {
    if (row.ownerActorFormID != syncOwnerActorFormID) {
      continue;
    }
    const auto previousIt = previousEquippedState.find(row.key);
    const bool wasEquipped =
        previousIt != previousEquippedState.end() && previousIt->second;
    changed = changed || wasEquipped != row.isEquipped;

    const auto previousHideIt = previousHideEquippedState.find(row.key);
    const bool wasHidden = previousHideIt != previousHideEquippedState.end() &&
                           previousHideIt->second;
    changed = changed || wasHidden != row.hideEquipped;

    for (const auto &item : row.overrides) {
      const auto automaticStateKey =
          row.key + "|appearance:" + armor::FormatFormID(item.formID);
      const auto previousAutomaticIt =
          previousAutomaticSuppressedState.find(automaticStateKey);
      const bool wasAutomaticallySuppressed =
          previousAutomaticIt != previousAutomaticSuppressedState.end() &&
          previousAutomaticIt->second;
      changed = changed ||
                wasAutomaticallySuppressed != item.automaticEquipmentSuppressed;
    }
  }
  changed |= NormalizeOverrideRowsForActor(syncOwnerActorFormID);

  bool prunedRows = false;
  for (auto rowIndex = static_cast<int>(rows_.size()) - 1; rowIndex >= 0;
       --rowIndex) {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (row.ownerActorFormID != syncOwnerActorFormID) {
      continue;
    }
    if ((row.isEquipped && !row.IsSlotRow()) || row.HasOverridesOrHideState() ||
        row.conditionId.has_value()) {
      continue;
    }

    rows_.erase(rows_.begin() + rowIndex);
    prunedRows = true;
  }

  if (prunedRows) {
    RebuildRowOrder();
    changed = true;
  }

  if (changed) {
    MarkChanged();
  }
}

void VariantWorkbench::SyncRowsFromPlayer() {
  SyncRowsFromActor(RE::PlayerCharacter::GetSingleton());
}

void VariantWorkbench::ClearAutomaticEquipmentVisibilityBindings() {
  ClearAutomaticEquipmentStateEvents();
  bool changed = false;
  const auto clearRows = [&](auto &a_rows) {
    for (auto &row : a_rows) {
      for (auto &item : row.overrides) {
        changed = changed || item.automaticEquipmentBindingMode != 0 ||
                  item.automaticEquipmentAnchorSlotMask != 0 ||
                  item.automaticEquipmentAnchorFormID != 0 ||
                  item.automaticEquipmentSuppressed ||
                  item.automaticEquipmentUserVisible;
        ClearAutomaticEquipmentBinding(item);
      }
    }
  };
  clearRows(rows_);
  clearRows(previewNativeRows_);
  if (changed) {
    // Worn-item rows are a workbench view of the selected actor. Equipment
    // events and explicit editing operations already queue that actor's native
    // refresh. Treating this read-side synchronization as a global display
    // mutation made simply viewing a nearby actor refresh every managed actor.
    MarkChanged(false);
  }
}

bool VariantWorkbench::HasRegisteredAppearancesForActor(
    const RE::FormID a_actorFormID) const {
  auto stateLock = AcquireStateLock();
  return a_actorFormID != 0 && std::ranges::any_of(rows_, [&](const auto &row) {
           return row.ownerActorFormID == a_actorFormID &&
                  !row.overrides.empty();
         });
}

bool VariantWorkbench::HasDisplayStateForActor(
    const RE::FormID a_actorFormID) const {
  if (a_actorFormID == 0) {
    return false;
  }

  auto stateLock = AcquireStateLock();
  if (previewActorFormID_ == a_actorFormID &&
      !previewSelectionKey_.empty() && !previewNativeRows_.empty()) {
    return true;
  }
  if (std::ranges::any_of(rows_, [&](const auto &a_row) {
        return a_row.ownerActorFormID == a_actorFormID &&
               a_row.HasOverridesOrHideState();
      })) {
    return true;
  }
  return std::ranges::any_of(
      conditionalVisibilityRules_, [&](const auto &a_rule) {
        return a_rule.ownerActorFormID == a_actorFormID;
      });
}

bool VariantWorkbench::HasActualEquipmentLinkedAppearancesForActor(
    const RE::FormID a_actorFormID) const {
  return GetActualEquipmentLinkedSlotMaskForActor(a_actorFormID) != 0;
}

std::uint64_t VariantWorkbench::GetActualEquipmentLinkedSlotMaskForActor(
    const RE::FormID a_actorFormID) const {
  if (!IsActualEquipmentStripLinkPolicyActive()) {
    return 0;
  }
  auto stateLock = AcquireStateLock();
  if (a_actorFormID == 0) {
    return 0;
  }
  std::uint64_t linkedSlotMask = 0;
  for (const auto &row : rows_) {
    if (row.ownerActorFormID != a_actorFormID) {
      continue;
    }
    for (const auto &item : row.overrides) {
      if (!item.locked && !row.IsProtectedAppearance(item)) {
        linkedSlotMask |= item.automaticEquipmentAnchorSlotMask;
      }
    }
  }
  return linkedSlotMask;
}

std::uint64_t VariantWorkbench::GetLockedAppearanceSlotMaskForActor(
    const RE::FormID a_actorFormID) const {
  auto stateLock = AcquireStateLock();
  if (a_actorFormID == 0) {
    return 0;
  }
  const auto *player = RE::PlayerCharacter::GetSingleton();
  const bool resolvingPlayer =
      player != nullptr && player->GetFormID() == a_actorFormID;
  std::uint64_t lockedSlotMask = 0;
  for (const auto &row : rows_) {
    if (row.ownerActorFormID != a_actorFormID &&
        !(resolvingPlayer && row.ownerActorFormID == 0)) {
      continue;
    }
    for (const auto &item : row.overrides) {
      if (item.locked) {
        lockedSlotMask |= row.GetOverrideDisplaySlotMask(item);
      }
    }
  }
  return lockedSlotMask;
}

bool VariantWorkbench::IsRegisteredAppearanceLockedForActor(
    const RE::FormID a_actorFormID, const RE::FormID a_appearanceFormID,
    const std::uint64_t a_visualSlotMask) const {
  auto stateLock = AcquireStateLock();
  if (a_actorFormID == 0 || a_appearanceFormID == 0) {
    return false;
  }
  const auto *player = RE::PlayerCharacter::GetSingleton();
  const bool resolvingPlayer =
      player != nullptr && player->GetFormID() == a_actorFormID;
  return std::ranges::any_of(rows_, [&](const auto &row) {
    if (row.ownerActorFormID != a_actorFormID &&
        !(resolvingPlayer && row.ownerActorFormID == 0)) {
      return false;
    }
    return std::ranges::any_of(row.overrides, [&](const auto &item) {
      return item.locked && item.formID == a_appearanceFormID &&
             (a_visualSlotMask == 0 ||
              (row.GetOverrideDisplaySlotMask(item) & a_visualSlotMask) != 0);
    });
  });
}

std::uint32_t VariantWorkbench::GetHeadgearToggleFittingSlotMaskForActor(
    const RE::FormID a_actorFormID,
    const std::uint32_t a_controllerSlotMask) const {
  if (a_actorFormID == 0 || a_controllerSlotMask == 0) {
    return 0;
  }

  const auto *player = RE::PlayerCharacter::GetSingleton();
  const bool resolvingPlayer =
      player != nullptr && player->GetFormID() == a_actorFormID;
  std::uint32_t fittingSlotMask = 0;
  auto stateLock = AcquireStateLock();
  for (const auto &row : rows_) {
    // Ownership migration makes current rows actor-local.  A pre-migration
    // owner=0 row was player-only at runtime, though, so retain that exact
    // fallback for the player without ever sharing it with NPCs.
    if (row.ownerActorFormID != a_actorFormID &&
        !(resolvingPlayer && row.ownerActorFormID == 0)) {
      continue;
    }
    for (const auto &item : row.overrides) {
      const auto visualSlotMask = static_cast<std::uint32_t>(
          row.GetOverrideVisualSlotMask(item));
      fittingSlotMask |= sfs::native::helmet_toggle::rules::
          ResolveRegisteredAppearanceSuppressionSlots(
              visualSlotMask, a_controllerSlotMask, item.locked);
    }
  }
  return fittingSlotMask;
}

bool VariantWorkbench::IsPreviewingSelection(
    std::string_view a_selectionKey) const {
  return previewSelectionKey_ == a_selectionKey && !previewNativeRows_.empty();
}

bool VariantWorkbench::CanAcceptOverride(int a_targetRowIndex,
                                         const EquipmentWidgetItem &a_item,
                                         int a_sourceRowIndex,
                                         int a_sourceItemIndex) const {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  if (a_sourceRowIndex == a_targetRowIndex && a_sourceItemIndex >= 0) {
    return false;
  }

  if (!a_item.SupportsArmorReplacement()) {
    return false;
  }

  const auto *appearanceArmor =
      a_item.IsSlot()
          ? nullptr
          : RE::TESForm::LookupByID<RE::TESObjectARMO>(a_item.formID);
  const auto appearanceSlotMask =
      appearanceArmor != nullptr
          ? armor::GetArmorDisplaySlotMask(appearanceArmor)
          : a_item.slotMask;
  if (IsAppearanceRegistrationProtectedSlotMask(appearanceSlotMask)) {
    return false;
  }

  if (!a_item.IsSlot()) {
    const auto *armorForm = appearanceArmor;
    if (!armorForm || armor::IsSosTngInternalArmor(armorForm) ||
        armor::GetFormIdentifier(armorForm).empty()) {
      return false;
    }
  }

  const auto &row = rows_[static_cast<std::size_t>(a_targetRowIndex)];
  if (row.conditionId.has_value()) {
    return false;
  }

  // Match against the same normalized placement mask used before catalog
  // items began retaining their complete visual slot identity. This preserves
  // row-selection behavior while the stored appearance keeps every ARMO bit.
  const auto overrideSlotMask = GetPlacementSlotMask(a_item);
  const auto targetSlotMask = row.GetSelectionConflictSlotMask();
  if (overrideSlotMask == 0 || targetSlotMask == 0 ||
      (overrideSlotMask & targetSlotMask) == 0) {
    return false;
  }

  std::uint64_t lockedSlotMask = 0;
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
       ++rowIndex) {
    const auto &candidateRow = rows_[static_cast<std::size_t>(rowIndex)];
    if (candidateRow.ownerActorFormID != row.ownerActorFormID) {
      continue;
    }
    for (int itemIndex = 0;
         itemIndex < static_cast<int>(candidateRow.overrides.size());
         ++itemIndex) {
      const auto &candidateItem =
          candidateRow.overrides[static_cast<std::size_t>(itemIndex)];
      if (!candidateItem.locked ||
          (rowIndex == a_sourceRowIndex && itemIndex == a_sourceItemIndex)) {
        continue;
      }
      lockedSlotMask |= candidateRow.GetOverrideDisplaySlotMask(candidateItem);
    }
  }
  if ((appearanceSlotMask & lockedSlotMask) != 0) {
    return false;
  }

  for (int itemIndex = 0; itemIndex < static_cast<int>(row.overrides.size());
       ++itemIndex) {
    if (a_targetRowIndex == a_sourceRowIndex &&
        itemIndex == a_sourceItemIndex) {
      continue;
    }

    if (row.overrides[static_cast<std::size_t>(itemIndex)].formID ==
        a_item.formID) {
      return false;
    }
  }

  return true;
}

bool VariantWorkbench::AddCatalogOverride(int a_targetRowIndex,
                                          RE::FormID a_formID) {
  EquipmentWidgetItem item{};
  if (!workbench::BuildCatalogItem(a_formID, item) ||
      !CanAcceptOverride(a_targetRowIndex, item)) {
    return false;
  }

  rows_[static_cast<std::size_t>(a_targetRowIndex)].overrides.push_back(
      std::move(item));
  MarkChanged();
  return true;
}

bool VariantWorkbench::AddCatalogSelectionToWorkbench(
    const std::vector<RE::FormID> &a_formIDs,
    const std::vector<int> *a_candidateRowIndices) {
  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formIDs, assignments, a_candidateRowIndices)) {
    return false;
  }

  bool addedAny = false;
  for (const auto &assignment : assignments) {
    addedAny |= AddCatalogOverride(assignment.rowIndex, assignment.armorFormID);
  }

  return addedAny;
}

bool VariantWorkbench::ReplaceCatalogSelectionInWorkbench(
    const std::vector<RE::FormID> &a_formIDs,
    const std::vector<int> *a_candidateRowIndices) {
  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formIDs, assignments, a_candidateRowIndices)) {
    return false;
  }
  const auto previousRows = rows_;

  std::unordered_set<int> targetRows;
  targetRows.reserve(assignments.size());
  for (const auto &assignment : assignments) {
    targetRows.insert(assignment.rowIndex);
  }

  for (const auto rowIndex : targetRows) {
    auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    std::erase_if(row.overrides,
                  [](const EquipmentWidgetItem &a_item) {
                    return !a_item.locked;
                  });
  }

  bool addedAny = false;
  for (const auto &assignment : assignments) {
    addedAny |= AddCatalogOverride(assignment.rowIndex, assignment.armorFormID);
  }

  if (addedAny) {
    InvalidateRemovedAppearanceAutomation(previousRows);
  }

  return addedAny;
}

bool VariantWorkbench::RemoveOverridesOverlappingCatalogSelection(
    const std::vector<RE::FormID> &a_formIDs,
    const std::vector<int> *a_candidateRowIndices) {
  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formIDs, armors)) {
    return false;
  }

  std::uint64_t incomingSlotMask = 0;
  for (const auto *armor : armors) {
    incomingSlotMask |= armor::GetArmorDisplaySlotMask(armor);
  }
  if (incomingSlotMask == 0) {
    return false;
  }

  const auto previousRows = rows_;
  bool changed = false;
  const auto removeOverlaps = [&](VariantWorkbenchRow &a_row) {
    const auto oldSize = a_row.overrides.size();
    std::erase_if(a_row.overrides, [&](const EquipmentWidgetItem &a_item) {
      return !a_item.locked &&
             (a_row.GetOverrideDisplaySlotMask(a_item) & incomingSlotMask) != 0;
    });
    changed |= a_row.overrides.size() != oldSize;
  };

  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (!IsValidRowIndex(rowIndex, rows_.size())) {
        continue;
      }
      removeOverlaps(rows_[static_cast<std::size_t>(rowIndex)]);
    }
  } else {
    for (auto &row : rows_) {
      removeOverlaps(row);
    }
  }

  if (changed) {
    InvalidateRemovedAppearanceAutomation(previousRows);
    MarkChanged();
  }
  return changed;
}

bool VariantWorkbench::AddCatalogSelectionAsRows(
    const std::vector<RE::FormID> &a_formIDs,
    std::optional<std::string> a_conditionId,
    const RE::FormID a_ownerActorFormID,
    const InitialEquippedState *a_initialEquippedState) {
  auto newRows = BuildCatalogRows(a_formIDs, std::move(a_conditionId),
                                  a_ownerActorFormID, a_initialEquippedState);
  bool addedAny = false;
  for (auto &row : newRows) {
    rowOrder_.push_back(row.key);
    rows_.push_back(std::move(row));
    addedAny = true;
  }

  if (addedAny) {
    MarkChanged();
  }

  return addedAny;
}

bool VariantWorkbench::AddSlotRow(
    const std::uint64_t a_slotMask, std::optional<std::string> a_conditionId,
    const RE::FormID a_ownerActorFormID,
    const InitialEquippedState *a_initialEquippedState) {
  auto row = BuildSlotRow(a_slotMask, std::move(a_conditionId),
                          a_ownerActorFormID, a_initialEquippedState);
  if (!row) {
    return false;
  }

  rowOrder_.push_back(row->key);
  rows_.push_back(std::move(*row));
  MarkChanged();
  return true;
}

std::vector<VariantWorkbenchRow> VariantWorkbench::BuildCatalogRows(
    const std::vector<RE::FormID> &a_formIDs,
    std::optional<std::string> a_conditionId,
    const RE::FormID a_ownerActorFormID,
    const InitialEquippedState *a_initialEquippedState) const {
  std::vector<VariantWorkbenchRow> newRows;
  const auto resolvedConditionId = std::move(a_conditionId);

  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formIDs, armors)) {
    return newRows;
  }

  std::unordered_set<std::string> seenRowKeys;
  for (const auto &row : rows_) {
    seenRowKeys.insert(row.key);
  }

  std::uint64_t lockedSlotMask = 0;
  for (const auto &row : rows_) {
    if (row.ownerActorFormID != a_ownerActorFormID) {
      continue;
    }
    for (const auto &item : row.overrides) {
      if (item.locked) {
        lockedSlotMask |= row.GetOverrideDisplaySlotMask(item);
      }
    }
  }

  for (const auto *armor : armors) {
    if (!armor) {
      continue;
    }
    const auto displaySlotMask = armor::GetArmorDisplaySlotMask(armor);
    if (IsAppearanceRegistrationProtectedSlotMask(displaySlotMask) ||
        (displaySlotMask & lockedSlotMask) != 0) {
      continue;
    }

    const auto sourceKey = BuildArmorSourceKey(armor->GetFormID());
    const auto rowKey =
        BuildRowKey(sourceKey, resolvedConditionId, a_ownerActorFormID);
    if (!seenRowKeys.insert(rowKey).second) {
      continue;
    }

    EquipmentWidgetItem equipped{};
    if (!workbench::BuildCatalogItem(armor->GetFormID(), equipped)) {
      continue;
    }

    VariantWorkbenchRow row{};
    row.sourceKey = sourceKey;
    row.conditionId = resolvedConditionId;
    row.ownerActorFormID = a_ownerActorFormID;
    row.equipped = std::move(equipped);
    row.isEquipped =
        a_initialEquippedState != nullptr &&
        a_initialEquippedState->wornArmorForms.contains(armor->GetFormID());
    UpdateRowIdentity(row);
    newRows.push_back(std::move(row));
  }

  return newRows;
}

std::optional<VariantWorkbenchRow> VariantWorkbench::BuildSlotRow(
    const std::uint64_t a_slotMask, std::optional<std::string> a_conditionId,
    const RE::FormID a_ownerActorFormID, const InitialEquippedState *,
    const bool a_allowRestrictedPreview) const {
  if (!a_allowRestrictedPreview &&
      IsAppearanceRegistrationProtectedSlotMask(a_slotMask)) {
    return std::nullopt;
  }
  const auto sourceKey = BuildSlotKey(a_slotMask);
  if (sourceKey.empty()) {
    return std::nullopt;
  }

  const auto resolvedConditionId = std::move(a_conditionId);
  const auto rowKey =
      BuildRowKey(sourceKey, resolvedConditionId, a_ownerActorFormID);
  if (std::ranges::find(rows_, rowKey, &VariantWorkbenchRow::key) !=
      rows_.end()) {
    return std::nullopt;
  }

  EquipmentWidgetItem slotItem{};
  if (!workbench::BuildSlotItem(a_slotMask, slotItem)) {
    return std::nullopt;
  }

  VariantWorkbenchRow row{};
  row.sourceKey = sourceKey;
  row.conditionId = resolvedConditionId;
  row.ownerActorFormID = a_ownerActorFormID;
  row.equipped = std::move(slotItem);
  row.isEquipped = false;
  UpdateRowIdentity(row);
  return row;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool VariantWorkbench::DeleteOverride(int a_rowIndex, int a_itemIndex) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];
  auto &overrides = row.overrides;
  if (a_itemIndex < 0 || a_itemIndex >= static_cast<int>(overrides.size())) {
    return false;
  }

  const auto previousRows = rows_;
  overrides.erase(overrides.begin() + a_itemIndex);
  // The same actor/form/visual-slot identity may still be registered by a
  // different condition row. Release automation only when the committed
  // model no longer owns that identity.
  InvalidateRemovedAppearanceAutomation(previousRows);
  // Keep conditional rows alive after their last appearance is removed.  The
  // remaining row is an empty drop target until a new appearance/equipment is
  // assigned, matching the condition-card persistence contract.
  MarkChanged();
  return true;
}

bool VariantWorkbench::ReplaceConditionalFittingTarget(
    const int a_targetRowIndex, const RE::FormID a_formID) {
  if (!IsValidRowIndex(a_targetRowIndex, rows_.size()) || a_formID == 0) {
    return false;
  }

  auto &row = rows_[static_cast<std::size_t>(a_targetRowIndex)];
  if (!row.IsSlotRow() || !row.conditionId.has_value()) {
    return false;
  }

  EquipmentWidgetItem item{};
  const auto *appearanceArmor =
      RE::TESForm::LookupByID<RE::TESObjectARMO>(a_formID);
  if (appearanceArmor == nullptr ||
      !workbench::BuildCatalogItem(a_formID, item) ||
      !item.SupportsArmorReplacement() ||
      armor::IsSosTngInternalArmor(appearanceArmor) ||
      armor::GetFormIdentifier(appearanceArmor).empty()) {
    return false;
  }

  const auto appearanceSlotMask = armor::GetArmorDisplaySlotMask(appearanceArmor);
  const auto representativeSlotMask = GetRepresentativeSlotMask(item);
  if (appearanceSlotMask == 0 || representativeSlotMask == 0 ||
      IsAppearanceRegistrationProtectedSlotMask(appearanceSlotMask)) {
    return false;
  }

  std::uint64_t lockedSlotMask = 0;
  for (const auto &candidateRow : rows_) {
    if (candidateRow.ownerActorFormID != row.ownerActorFormID) {
      continue;
    }
    for (const auto &candidateItem : candidateRow.overrides) {
      if (candidateItem.locked) {
        lockedSlotMask |=
            candidateRow.GetOverrideDisplaySlotMask(candidateItem);
      }
    }
  }
  if ((appearanceSlotMask & lockedSlotMask) != 0) {
    return false;
  }

  EquipmentWidgetItem replacementSlot{};
  if (!workbench::BuildSlotItem(representativeSlotMask, replacementSlot)) {
    return false;
  }
  const auto previousRows = rows_;

  const auto newSourceKey = BuildSlotKey(representativeSlotMask);
  const auto newRowKey =
      BuildRowKey(newSourceKey, row.conditionId, row.ownerActorFormID);
  if (const auto duplicate = std::ranges::find(rows_, newRowKey,
                                                &VariantWorkbenchRow::key);
      duplicate != rows_.end() && std::addressof(*duplicate) !=
                                      std::addressof(row)) {
    // The model deliberately keeps one fitting row per actor/condition/slot.
    // Reuse that row, but transfer the retained condition row's identity/order
    // so the drop fills the row the user actually targeted.
    duplicate->uiIdentity = row.uiIdentity;
    duplicate->registrationOrder = row.registrationOrder;
    std::erase_if(duplicate->overrides,
                  [](const EquipmentWidgetItem &a_item) {
                    return !a_item.locked;
                  });
    item.hidden = false;
    duplicate->overrides.push_back(std::move(item));
    const auto sourceIndex = static_cast<std::size_t>(a_targetRowIndex);
    rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(sourceIndex));
    RebuildRowOrder();
    InvalidateRemovedAppearanceAutomation(previousRows);
    MarkChanged();
    return true;
  }

  item.hidden = false;
  // A condition-only row is type-neutral. Re-key it to the newly dropped
  // appearance instead of requiring the obsolete slot left by the deleted
  // action card.
  row.sourceKey = newSourceKey;
  row.equipped = std::move(replacementSlot);
  row.isEquipped = false;
  std::erase_if(row.overrides, [](const EquipmentWidgetItem &a_item) {
    return !a_item.locked;
  });
  row.overrides.push_back(std::move(item));
  UpdateRowIdentity(row);
  RebuildRowOrder();
  InvalidateRemovedAppearanceAutomation(previousRows);
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetOverrideHidden(int a_rowIndex, int a_itemIndex,
                                          const bool a_hidden) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];

  auto &overrides = row.overrides;
  if (a_itemIndex < 0 || a_itemIndex >= static_cast<int>(overrides.size())) {
    return false;
  }

  auto &overrideItem = overrides[static_cast<std::size_t>(a_itemIndex)];
  if (overrideItem.hidden == a_hidden) {
    return false;
  }
  InvalidateAppearanceAutomation(
      row.ownerActorFormID, overrideItem.formID,
      static_cast<std::uint32_t>(row.GetOverrideVisualSlotMask(overrideItem)),
      false);

  overrideItem.hidden = a_hidden;
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetOverrideLocked(const int a_rowIndex,
                                         const int a_itemIndex,
                                         const bool a_locked) {
  auto stateLock = AcquireStateLock();
  if (!IsValidRowIndex(a_rowIndex, rows_.size())) {
    return false;
  }
  auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];
  if (!IsValidRowIndex(a_itemIndex, row.overrides.size())) {
    return false;
  }

  auto &item = row.overrides[static_cast<std::size_t>(a_itemIndex)];
  if (item.locked == a_locked) {
    return false;
  }

  if (a_locked) {
    // Release only this appearance's current automation tickets/latches. The
    // actor transaction and every other appearance remain untouched.
    InvalidateAppearanceAutomation(
        row.ownerActorFormID, item.formID,
        static_cast<std::uint32_t>(row.GetOverrideVisualSlotMask(item)),
        false);
    item.automaticEquipmentUserVisible = false;
  }
  item.locked = a_locked;
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetOverrideHeadgearToggleManualVisible(
    const int a_rowIndex, const int a_itemIndex, const bool a_visible) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }
  const auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];
  if (a_itemIndex < 0 ||
      a_itemIndex >= static_cast<int>(row.overrides.size())) {
    return false;
  }
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(row.ownerActorFormID);
  if (actor == nullptr && row.ownerActorFormID == 0) {
    // Legacy owner=0 rows are player-only.  Do not fall back to a global
    // actor for an NPC row: the runtime exception must remain actor-local.
    actor = RE::PlayerCharacter::GetSingleton();
  }
  return sfs::native::SetHeadgearToggleFittingSlotsManualVisible(
      actor, static_cast<std::uint32_t>(
                 row.GetOverrideVisualSlotMask(
                     row.overrides[static_cast<std::size_t>(a_itemIndex)])),
      a_visible);
}

bool VariantWorkbench::PersistOverrideHiddenFromExternalSuppression(
    const int a_rowIndex, const int a_itemIndex) {
  auto stateLock = AcquireStateLock();
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }
  auto &overrides = rows_[static_cast<std::size_t>(a_rowIndex)].overrides;
  if (a_itemIndex < 0 ||
      a_itemIndex >= static_cast<int>(overrides.size())) {
    return false;
  }
  auto &overrideItem = overrides[static_cast<std::size_t>(a_itemIndex)];
  if (overrideItem.locked || overrideItem.hidden) {
    return false;
  }
  overrideItem.hidden = true;
  overrideItem.automaticEquipmentUserVisible = false;
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetOverrideAutomaticEquipmentUserVisible(
    const int a_rowIndex, const int a_itemIndex, const bool a_visible) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto &overrides = rows_[static_cast<std::size_t>(a_rowIndex)].overrides;
  if (a_itemIndex < 0 || a_itemIndex >= static_cast<int>(overrides.size())) {
    return false;
  }

  auto &overrideItem = overrides[static_cast<std::size_t>(a_itemIndex)];
  const bool visible = a_visible && overrideItem.automaticEquipmentSuppressed;
  if (overrideItem.automaticEquipmentUserVisible == visible) {
    return false;
  }

  overrideItem.automaticEquipmentUserVisible = visible;
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetOverrideConditionId(
    const int a_rowIndex, const int a_itemIndex,
    std::optional<std::string> a_conditionId) {
  if (!IsValidRowIndex(a_rowIndex, rows_.size())) {
    return false;
  }

  const auto sourceIndex = static_cast<std::size_t>(a_rowIndex);
  if (!IsValidRowIndex(a_itemIndex, rows_[sourceIndex].overrides.size())) {
    return false;
  }

  const auto &sourceRow = rows_[sourceIndex];
  if (sourceRow.conditionId == a_conditionId) {
    return false;
  }

  // Clearing the condition from a condition-area card must retain the card in
  // place with an empty condition target.  Moving it back to the base layer
  // here made the opposite half of the row appear to disappear.
  if (!a_conditionId.has_value() && sourceRow.conditionId.has_value()) {
    rows_[sourceIndex].conditionId = std::string{};
    UpdateRowIdentity(rows_[sourceIndex]);
    RebuildRowOrder();
    MarkChanged();
    return true;
  }

  const auto ownerActorFormID = sourceRow.ownerActorFormID;
  auto overrideItem =
      sourceRow.overrides[static_cast<std::size_t>(a_itemIndex)];
  if (a_conditionId.has_value()) {
    overrideItem.hidden = false;
  }
  const auto representativeSlotMask = GetRepresentativeSlotMask(overrideItem);
  const auto targetSourceKey = BuildSlotKey(representativeSlotMask);
  if (targetSourceKey.empty()) {
    return false;
  }

  rows_[sourceIndex].overrides.erase(rows_[sourceIndex].overrides.begin() +
                                     a_itemIndex);

  const auto targetKey =
      BuildRowKey(targetSourceKey, a_conditionId, ownerActorFormID);
  auto target = std::ranges::find(rows_, targetKey, &VariantWorkbenchRow::key);
  if (target == rows_.end() && a_conditionId.has_value()) {
    // A condition card represents one action row even when the fitting and
    // actual-equipment sides are both empty. Reuse that card before creating
    // another row for the newly assigned fitting item. Rebase its slot-row
    // identity to the incoming item while preserving its visual/order
    // identity so the card does not jump during the edit.
    target = std::ranges::find_if(rows_, [&](const VariantWorkbenchRow &a_row) {
      return a_row.ownerActorFormID == ownerActorFormID && a_row.IsSlotRow() &&
             a_row.conditionId == a_conditionId &&
             !a_row.HasOverridesOrHideState();
    });
    if (target != rows_.end() && target->key != targetKey) {
      auto rebased = BuildSlotRow(representativeSlotMask, a_conditionId,
                                  ownerActorFormID, nullptr);
      if (rebased.has_value()) {
        rebased->uiIdentity = target->uiIdentity;
        rebased->registrationOrder = target->registrationOrder;
        *target = std::move(*rebased);
      } else {
        target = rows_.end();
      }
    }
  }
  if (target == rows_.end()) {
    auto slotRow = BuildSlotRow(representativeSlotMask, a_conditionId,
                                ownerActorFormID, nullptr);
    if (!slotRow.has_value()) {
      rows_[sourceIndex].overrides.insert(rows_[sourceIndex].overrides.begin() +
                                              a_itemIndex,
                                          std::move(overrideItem));
      return false;
    }
    rows_.push_back(std::move(*slotRow));
    target = std::prev(rows_.end());
  }

  const auto duplicate = std::ranges::find(
      target->overrides, overrideItem.formID, &EquipmentWidgetItem::formID);
  if (duplicate == target->overrides.end()) {
    target->overrides.push_back(std::move(overrideItem));
  } else {
    duplicate->hidden = duplicate->hidden && overrideItem.hidden;
    duplicate->locked = duplicate->locked || overrideItem.locked;
  }

  static_cast<void>(NormalizeOverrideRowsForActor(ownerActorFormID));
  RebuildRowOrder();
  MarkChanged();
  return true;
}
bool VariantWorkbench::SetEquippedHidden(const int a_rowIndex,
                                         const bool a_hidden) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];
  if (a_hidden && row.IsAlwaysVisibleActualEquipment()) {
    return false;
  }
  if (row.hideEquipped == a_hidden) {
    return false;
  }

  row.hideEquipped = a_hidden;
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetEquippedHiddenForActor(const RE::FormID a_actorFormID,
                                                 const int a_rowIndex,
                                                 const bool a_hidden) {
  auto stateLock = AcquireStateLock();
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  bool changed = false;
  auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];
  if (row.ownerActorFormID != 0 && row.ownerActorFormID != a_actorFormID) {
    return false;
  }
  if (a_hidden && row.IsAlwaysVisibleActualEquipment()) {
    return false;
  }
  if (a_actorFormID != 0 && !row.key.empty()) {
    auto &actorRows = equippedHiddenByActor_[a_actorFormID];
    const auto hiddenIt = actorRows.find(row.key);
    if (hiddenIt == actorRows.end()) {
      actorRows.emplace(row.key, a_hidden);
      changed = true;
    } else if (hiddenIt->second != a_hidden) {
      hiddenIt->second = a_hidden;
      changed = true;
    }
  }

  const bool updateRowDefaultHidden = [&]() {
    if (row.ownerActorFormID != 0 || a_actorFormID == 0) {
      return true;
    }
    const auto *player = RE::PlayerCharacter::GetSingleton();
    return player != nullptr && player->GetFormID() == a_actorFormID;
  }();
  if (updateRowDefaultHidden && row.hideEquipped != a_hidden) {
    row.hideEquipped = a_hidden;
    changed = true;
  }

  if (changed) {
    MarkChanged();
  }
  return changed;
}

bool VariantWorkbench::DeleteRow(int a_rowIndex) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  const auto previousRows = rows_;
  const auto &deletedRow = rows_[static_cast<std::size_t>(a_rowIndex)];

  const auto rowKey = deletedRow.key;
  if (!rowKey.empty()) {
    for (auto &[actorFormID, rowStates] : equippedHiddenByActor_) {
      rowStates.erase(rowKey);
    }
  }

  rows_.erase(rows_.begin() + a_rowIndex);
  InvalidateRemovedAppearanceAutomation(previousRows);
  RebuildRowOrder();
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetEquippedConditionId(
    const int a_rowIndex, std::optional<std::string> a_conditionId) {
  if (!IsValidRowIndex(a_rowIndex, rows_.size())) {
    return false;
  }
  auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];
  if (!row.isEquipped || row.IsSlotRow() || row.conditionId == a_conditionId) {
    return false;
  }
  row.conditionId = std::move(a_conditionId);
  row.key = BuildRowKey(row.sourceKey, row.conditionId, row.ownerActorFormID);
  RebuildRowOrder();
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetConditionAssignmentKeepRow(
    const int a_rowIndex, const std::string_view a_conditionId) {
  if (!IsValidRowIndex(a_rowIndex, rows_.size())) {
    return false;
  }
  auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];
  if (!row.IsSlotRow() || row.conditionId.value_or(std::string{}) ==
                          a_conditionId) {
    return false;
  }
  row.conditionId = std::string(a_conditionId);
  UpdateRowIdentity(row);
  // Editing a condition card is an in-place UI operation. Normalizing here
  // can merge the edited row with another active row and make it jump or
  // disappear before the workbench is closed. Native synchronization still
  // resolves the effective condition state without requiring that merge.
  RebuildRowOrder();
  MarkChanged();
  return true;
}

condition_drop::Status VariantWorkbench::ApplyConditionDropTransaction(
    const condition_drop::Request &a_request) {
  const auto lock = AcquireStateLock();

  std::vector<condition_drop::Entry> entries;
  entries.reserve(rows_.size() + conditionalVisibilityRules_.size());
  for (const auto &row : rows_) {
    if (!row.IsSlotRow()) {
      continue;
    }
    entries.push_back({
        .target = {condition_drop::TargetKind::ConditionalRow,
                   row.uiIdentity},
        .conditionId = row.conditionId.value_or(std::string{}),
    });
  }
  for (const auto &rule : conditionalVisibilityRules_) {
    entries.push_back({
        .target = {condition_drop::TargetKind::ConditionalVisibilityRule,
                   rule.uiIdentity},
        .conditionId = rule.conditionId,
    });
  }

  const auto plan = condition_drop::BuildPlan(entries, a_request);
  if (!plan.Changed()) {
    return plan.status;
  }

  auto plannedRows = rows_;
  auto plannedRules = conditionalVisibilityRules_;
  for (const auto &change : plan.changes) {
    if (change.target.kind == condition_drop::TargetKind::ConditionalRow) {
      const auto row = std::ranges::find(
          plannedRows, change.target.uiIdentity,
          &VariantWorkbenchRow::uiIdentity);
      if (row == plannedRows.end() || !row->IsSlotRow()) {
        return condition_drop::Status::TargetNotFound;
      }
      row->conditionId = change.conditionId;
      UpdateRowIdentity(*row);
      continue;
    }
    if (change.target.kind ==
        condition_drop::TargetKind::ConditionalVisibilityRule) {
      const auto rule = std::ranges::find(
          plannedRules, change.target.uiIdentity,
          &ConditionalVisibilityRule::uiIdentity);
      if (rule == plannedRules.end()) {
        return condition_drop::Status::TargetNotFound;
      }
      rule->conditionId = change.conditionId;
      continue;
    }
    return condition_drop::Status::InvalidRequest;
  }

  // A visibility rule is unique by condition, owner, target kind, and target
  // form. Validate the complete post-drop state so a cross-row swap never
  // commits only one side or relies on a temporarily invalid intermediate
  // state.
  for (std::size_t left = 0; left < plannedRules.size(); ++left) {
    const auto &leftRule = plannedRules[left];
    if (leftRule.conditionId.empty()) {
      continue;
    }
    for (std::size_t right = left + 1; right < plannedRules.size(); ++right) {
      const auto &rightRule = plannedRules[right];
      if (leftRule.conditionId == rightRule.conditionId &&
          leftRule.ownerActorFormID == rightRule.ownerActorFormID &&
          leftRule.targetKind == rightRule.targetKind &&
          leftRule.target.formID == rightRule.target.formID) {
        return condition_drop::Status::Conflict;
      }
    }
  }

  rows_ = std::move(plannedRows);
  conditionalVisibilityRules_ = std::move(plannedRules);
  RebuildRowOrder();
  MarkChanged();
  return condition_drop::Status::Applied;
}

condition_drop::Status VariantWorkbench::ApplyConditionalActionDropTransaction(
    const ConditionalActionDropRequest &a_request) {
  const auto lock = AcquireStateLock();

  std::vector<action_drop::Entry> entries;
  entries.reserve(rows_.size() + conditionalVisibilityRules_.size());
  for (const auto &row : rows_) {
    if (!row.IsSlotRow()) {
      continue;
    }
    action_drop::Entry entry{
        .target = {condition_drop::TargetKind::ConditionalRow, row.uiIdentity}};
    entry.formIDs.reserve(row.overrides.size());
    for (const auto &item : row.overrides) {
      if (item.formID != 0) {
        entry.formIDs.push_back(item.formID);
      }
    }
    entries.push_back(std::move(entry));
  }
  for (const auto &rule : conditionalVisibilityRules_) {
    action_drop::Entry entry{
        .target = {condition_drop::TargetKind::ConditionalVisibilityRule,
                   rule.uiIdentity}};
    if (rule.target.formID != 0) {
      entry.formIDs.push_back(rule.target.formID);
    }
    entries.push_back(std::move(entry));
  }

  const auto validation = action_drop::ValidateRequest(
      entries, {.target = a_request.target,
                .formID = a_request.formID,
                .source = a_request.source});
  if (validation != condition_drop::Status::Applied) {
    return validation;
  }

  // Apply every domain operation to an isolated model. Runtime invalidations
  // are recorded rather than published until the complete target/source state
  // has passed validation and is ready to commit.
  VariantWorkbench staged;
  staged.rows_ = rows_;
  staged.conditionalVisibilityRules_ = conditionalVisibilityRules_;
  staged.rowOrder_ = rowOrder_;
  staged.equippedHiddenByActor_ = equippedHiddenByActor_;
  staged.lastSyncedActorFormID_ = lastSyncedActorFormID_;
  staged.deferRuntimeEffects_ = true;

  const auto findRowIndex = [&](const std::uint64_t a_identity) {
    const auto it = std::ranges::find(staged.rows_, a_identity,
                                      &VariantWorkbenchRow::uiIdentity);
    return it == staged.rows_.end()
               ? -1
               : static_cast<int>(std::distance(staged.rows_.begin(), it));
  };
  const auto findRuleIndex = [&](const std::uint64_t a_identity)
      -> std::optional<std::size_t> {
    const auto it = std::ranges::find(
        staged.conditionalVisibilityRules_, a_identity,
        &ConditionalVisibilityRule::uiIdentity);
    if (it == staged.conditionalVisibilityRules_.end()) {
      return std::nullopt;
    }
    return static_cast<std::size_t>(
        std::distance(staged.conditionalVisibilityRules_.begin(), it));
  };

  bool targetChanged = false;
  if (a_request.target.kind == condition_drop::TargetKind::ConditionalRow) {
    const auto targetRowIndex = findRowIndex(a_request.target.uiIdentity);
    if (targetRowIndex < 0) {
      return condition_drop::Status::TargetNotFound;
    }

    if (a_request.targetKind == ConditionalVisibilityTargetKind::Fitting) {
      targetChanged = staged.ReplaceConditionalFittingTarget(
          targetRowIndex, a_request.formID);
    } else {
      const auto targetRow =
          staged.rows_[static_cast<std::size_t>(targetRowIndex)];
      if (!targetRow.conditionId.has_value()) {
        return condition_drop::Status::InvalidRequest;
      }
      EquipmentWidgetItem targetItem{};
      const auto *targetArmor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(a_request.formID);
      if (!targetArmor || !BuildCatalogItem(a_request.formID, targetItem) ||
          IsAppearanceRegistrationProtectedSlotMask(
              armor::GetArmorDisplaySlotMask(targetArmor))) {
        return condition_drop::Status::InvalidRequest;
      }
      ConditionalVisibilityRule targetRule{
          .conditionId = *targetRow.conditionId,
          .ownerActorFormID = targetRow.ownerActorFormID,
          .targetKind = a_request.targetKind,
          .target = std::move(targetItem),
          .visibleWhenTrue = a_request.visibleWhenTrue};
      targetRule.uiIdentity = targetRow.uiIdentity;
      targetRule.registrationOrder = targetRow.registrationOrder;
      staged.conditionalVisibilityRules_.push_back(std::move(targetRule));
      targetChanged = staged.DeleteRow(targetRowIndex);
    }
  } else if (a_request.target.kind ==
             condition_drop::TargetKind::ConditionalVisibilityRule) {
    const auto targetRuleIndex = findRuleIndex(a_request.target.uiIdentity);
    if (!targetRuleIndex.has_value()) {
      return condition_drop::Status::TargetNotFound;
    }
    targetChanged = a_request.registerFittingTarget
        ? staged.ConvertConditionalVisibilityRuleToFittingRow(
              *targetRuleIndex, a_request.formID)
        : [&]() {
            const bool targetReplaced =
                staged.SetConditionalVisibilityRuleTarget(
                    *targetRuleIndex, a_request.targetKind,
                    a_request.formID);
            const bool polarityChanged =
                staged.SetConditionalVisibilityRuleVisible(
                    *targetRuleIndex, a_request.visibleWhenTrue);
            return targetReplaced || polarityChanged;
          }();
  } else {
    return condition_drop::Status::InvalidRequest;
  }

  if (!targetChanged) {
    return condition_drop::Status::NoChange;
  }

  if (a_request.source.has_value()) {
    if (a_request.source->kind ==
        condition_drop::TargetKind::ConditionalRow) {
      const auto sourceRowIndex = findRowIndex(a_request.source->uiIdentity);
      if (sourceRowIndex >= 0) {
        const auto &sourceRow =
            staged.rows_[static_cast<std::size_t>(sourceRowIndex)];
        const auto sourceItem = std::ranges::find(
            sourceRow.overrides, a_request.formID,
            &EquipmentWidgetItem::formID);
        if (sourceItem == sourceRow.overrides.end()) {
          return condition_drop::Status::StaleSource;
        }
        const auto sourceItemIndex = static_cast<int>(
            std::distance(sourceRow.overrides.begin(), sourceItem));
        if (!staged.DeleteOverride(sourceRowIndex, sourceItemIndex)) {
          return condition_drop::Status::StaleSource;
        }
      } else {
        const bool sourceConsumedByFittingMerge =
            a_request.targetKind ==
                ConditionalVisibilityTargetKind::Fitting &&
            (a_request.target.kind ==
                 condition_drop::TargetKind::ConditionalRow ||
             a_request.registerFittingTarget) &&
            findRowIndex(a_request.target.uiIdentity) >= 0;
        if (!sourceConsumedByFittingMerge) {
          return condition_drop::Status::StaleSource;
        }
      }
      // ReplaceConditionalFittingTarget may merge the target into the source
      // row and transfer the target identity. In that one case the original
      // source identity is intentionally consumed by the target operation.
    } else if (a_request.source->kind ==
               condition_drop::TargetKind::ConditionalVisibilityRule) {
      const auto sourceRuleIndex = findRuleIndex(a_request.source->uiIdentity);
      if (!sourceRuleIndex.has_value() ||
          !staged.SetConditionalVisibilityRuleTarget(
              *sourceRuleIndex,
              staged.conditionalVisibilityRules_[*sourceRuleIndex].targetKind,
              0)) {
        return condition_drop::Status::StaleSource;
      }
    } else {
      return condition_drop::Status::InvalidRequest;
    }
  }

  // The setters used by legacy UI paths validate only their local endpoint.
  // Validate the complete staged state so a move cannot create a duplicate
  // visibility rule after clearing its source.
  for (std::size_t left = 0;
       left < staged.conditionalVisibilityRules_.size(); ++left) {
    const auto &leftRule = staged.conditionalVisibilityRules_[left];
    if (leftRule.conditionId.empty() || leftRule.target.formID == 0) {
      continue;
    }
    for (std::size_t right = left + 1;
         right < staged.conditionalVisibilityRules_.size(); ++right) {
      const auto &rightRule = staged.conditionalVisibilityRules_[right];
      if (leftRule.conditionId == rightRule.conditionId &&
          leftRule.ownerActorFormID == rightRule.ownerActorFormID &&
          leftRule.targetKind == rightRule.targetKind &&
          leftRule.target.formID == rightRule.target.formID) {
        return condition_drop::Status::Conflict;
      }
    }
  }

  rows_ = std::move(staged.rows_);
  conditionalVisibilityRules_ =
      std::move(staged.conditionalVisibilityRules_);
  rowOrder_ = std::move(staged.rowOrder_);
  equippedHiddenByActor_ = std::move(staged.equippedHiddenByActor_);
  for (const auto &invalidation : staged.deferredInvalidations_) {
    InvalidateAppearanceAutomation(
        invalidation.actorFormID, invalidation.appearanceFormID,
        invalidation.visualSlotMask, invalidation.deleted);
  }
  MarkChanged();
  return condition_drop::Status::Applied;
}

bool VariantWorkbench::ClearConditionAssignmentKeepRow(const int a_rowIndex) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }
  auto &row = rows_[static_cast<std::size_t>(a_rowIndex)];
  if (!row.conditionId.has_value() || row.conditionId->empty()) {
    return false;
  }
  row.conditionId = std::string{};
  UpdateRowIdentity(row);
  RebuildRowOrder();
  MarkChanged();
  return true;
}

std::size_t
VariantWorkbench::DeleteRowsByConditionId(const std::string_view a_conditionId,
                                          const bool a_onlyIfEmpty) {
  std::size_t removedCount = 0;

  for (auto index = static_cast<int>(rows_.size()) - 1; index >= 0; --index) {
    if (index < 0 || index >= static_cast<int>(rows_.size())) {
      continue;
    }
    const auto &row = rows_[static_cast<std::size_t>(index)];
    if (!row.conditionId.has_value() || *row.conditionId != a_conditionId) {
      continue;
    }
    if (a_onlyIfEmpty && row.HasOverridesOrHideState()) {
      continue;
    }

    // Keep the row and all cards in the condition section. An empty condition
    // assignment is rendered as an empty condition card and is inactive.
    rows_[static_cast<std::size_t>(index)].conditionId = std::string{};
    UpdateRowIdentity(rows_[static_cast<std::size_t>(index)]);
    ++removedCount;
  }

  if (removedCount != 0) {
    RebuildRowOrder();
    MarkChanged();
  }

  return removedCount;
}

std::size_t VariantWorkbench::PruneFullyEmptyConditionalRows() {
  auto stateLock = AcquireStateLock();
  std::vector<std::string> removedRowKeys;
  const auto oldRowCount = rows_.size();
  std::erase_if(rows_, [&](const VariantWorkbenchRow &a_row) {
    const bool remove = a_row.IsSlotRow() && a_row.conditionId.has_value() &&
                        a_row.conditionId->empty() &&
                        !a_row.HasOverridesOrHideState();
    if (remove && !a_row.key.empty()) {
      removedRowKeys.push_back(a_row.key);
    }
    return remove;
  });

  const auto oldRuleCount = conditionalVisibilityRules_.size();
  std::erase_if(conditionalVisibilityRules_,
                [](const ConditionalVisibilityRule &a_rule) {
                  return a_rule.conditionId.empty() &&
                         a_rule.target.formID == 0;
                });

  const auto removedRows = oldRowCount - rows_.size();
  const auto removedRules = oldRuleCount - conditionalVisibilityRules_.size();
  if (removedRows == 0 && removedRules == 0) {
    return 0;
  }

  for (auto &[actorFormID, rowStates] : equippedHiddenByActor_) {
    static_cast<void>(actorFormID);
    for (const auto &rowKey : removedRowKeys) {
      rowStates.erase(rowKey);
    }
  }
  std::erase_if(equippedHiddenByActor_,
                [](const auto &a_entry) { return a_entry.second.empty(); });
  RebuildRowOrder();
  MarkChanged();
  return removedRows + removedRules;
}

bool VariantWorkbench::ResetAllRows(
    const std::vector<int> *a_candidateRowIndices) {
  if (a_candidateRowIndices == nullptr) {
    Revert();
    return true;
  }

  const auto previousRows = rows_;
  bool changed = false;
  for (const auto rowIndex : *a_candidateRowIndices) {
    if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
      continue;
    }

    auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    const auto oldSize = row.overrides.size();
    std::erase_if(row.overrides,
                  [](const EquipmentWidgetItem &a_item) {
                    return !a_item.locked;
                  });
    if (row.overrides.size() != oldSize) {
      changed = true;
    }
  }

  if (changed) {
    InvalidateRemovedAppearanceAutomation(previousRows);
    MarkChanged();
  }

  return changed;
}

std::vector<RE::FormID> VariantWorkbench::CollectEquippedArmorFormIDs(
    const std::vector<int> *a_candidateRowIndices) const {
  std::vector<RE::FormID> formIDs;
  std::unordered_set<RE::FormID> seen;

  const auto collectRow = [&](const VariantWorkbenchRow &a_row) {
    if (!a_row.isEquipped || a_row.equipped.formID == 0) {
      return;
    }
    const auto *equippedArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(a_row.equipped.formID);
    if (equippedArmor == nullptr ||
        IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(equippedArmor))) {
      return;
    }
    if (seen.insert(a_row.equipped.formID).second) {
      formIDs.push_back(a_row.equipped.formID);
    }
  };

  formIDs.reserve(a_candidateRowIndices != nullptr
                      ? a_candidateRowIndices->size()
                      : rows_.size());
  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
        continue;
      }
      collectRow(rows_[static_cast<std::size_t>(rowIndex)]);
    }
  } else {
    for (const auto &row : rows_) {
      collectRow(row);
    }
  }

  return formIDs;
}

std::vector<RE::FormID>
VariantWorkbench::CollectOverrideArmorFormIDsFromEquippedRows(
    const std::vector<int> *a_candidateRowIndices) const {
  std::vector<RE::FormID> formIDs;
  std::unordered_set<RE::FormID> seen;

  const auto collectRow = [&](const VariantWorkbenchRow &a_row) {
    if (a_row.conditionId.has_value()) {
      return;
    }
    for (const auto &item : a_row.overrides) {
      if (item.formID == 0 || a_row.IsProtectedAppearance(item)) {
        continue;
      }
      if (seen.insert(item.formID).second) {
        formIDs.push_back(item.formID);
      }
    }
  };

  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
        continue;
      }
      collectRow(rows_[static_cast<std::size_t>(rowIndex)]);
    }
  } else {
    for (const auto &row : rows_) {
      collectRow(row);
    }
  }

  return formIDs;
}

std::optional<KitEntry::Layout> VariantWorkbench::CaptureKitLayout(
    const std::vector<int> *a_candidateRowIndices, RE::Actor *a_actor) const {
  KitEntry::Layout layout;
  const auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());
  layout.rows.reserve(candidateRowIndices.size());

  for (const auto rowIndex : candidateRowIndices) {
    if (!IsValidRowIndex(rowIndex, rows_.size())) {
      continue;
    }

    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (row.conditionId.has_value() || row.overrides.empty()) {
      continue;
    }

    KitEntry::LayoutRow layoutRow;
    layoutRow.targetKind = row.IsSlotRow() ? KitEntry::LayoutTargetKind::Slot
                                           : KitEntry::LayoutTargetKind::Item;
    layoutRow.targetSlotMask = row.IsSlotRow()
                                   ? row.equipped.slotMask
                                   : row.GetSelectionConflictSlotMask();

    std::uint64_t actualVisibilitySlotMask = 0;
    std::uint64_t hiddenActualSlotMask = 0;
    for (const auto actualRowIndex : candidateRowIndices) {
      if (!IsValidRowIndex(actualRowIndex, rows_.size())) {
        continue;
      }

      const auto &actualRow = rows_[static_cast<std::size_t>(actualRowIndex)];
      if (actualRow.ownerActorFormID != row.ownerActorFormID ||
          actualRow.conditionId.has_value() || !actualRow.isEquipped ||
          actualRow.IsSlotRow() || actualRow.IsAlwaysVisibleActualEquipment()) {
        continue;
      }

      const auto *actualArmor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(actualRow.equipped.formID);
      if (armor::IsSosTngInternalArmor(actualArmor)) {
        continue;
      }

      const auto overlappingSlotMask =
          actualRow.GetSelectionConflictSlotMask() & layoutRow.targetSlotMask;
      if (overlappingSlotMask == 0) {
        continue;
      }

      actualVisibilitySlotMask |= overlappingSlotMask;
      if (ResolveEquippedIndividualHiddenForActor(a_actor, actualRow)) {
        hiddenActualSlotMask |= overlappingSlotMask;
      }
    }
    layoutRow.actualVisibilitySlotMask = actualVisibilitySlotMask;
    layoutRow.hiddenActualSlotMask = hiddenActualSlotMask;
    layoutRow.hideEquipped =
        actualVisibilitySlotMask != 0 &&
        (hiddenActualSlotMask & actualVisibilitySlotMask) ==
            actualVisibilitySlotMask;

    for (const auto &overrideItem : row.overrides) {
      if (!overrideItem.HasForm() || row.IsProtectedAppearance(overrideItem)) {
        continue;
      }
      if (const auto *overrideArmor =
              RE::TESForm::LookupByID<RE::TESObjectARMO>(overrideItem.formID);
          overrideArmor != nullptr) {
        const auto identifier = armor::GetFormIdentifier(overrideArmor);
        if (!identifier.empty()) {
          layoutRow.overrideIdentifiers.push_back(identifier);
        }
      }
    }

    if (layoutRow.targetSlotMask == 0 ||
        layoutRow.overrideIdentifiers.empty()) {
      continue;
    }

    layout.rows.push_back(std::move(layoutRow));
  }

  if (layout.rows.empty()) {
    return std::nullopt;
  }

  return layout;
}

std::optional<KitEntry::Layout> VariantWorkbench::CaptureEquippedKitLayout(
    const std::vector<int> *a_candidateRowIndices) const {
  KitEntry::Layout layout;
  const auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());
  layout.rows.reserve(candidateRowIndices.size());

  for (const auto rowIndex : candidateRowIndices) {
    if (!IsValidRowIndex(rowIndex, rows_.size())) {
      continue;
    }

    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped || row.IsSlotRow()) {
      continue;
    }

    const auto *equippedArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
    if (!equippedArmor ||
        IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(equippedArmor))) {
      continue;
    }

    const auto targetSlotMask = row.GetSelectionConflictSlotMask();
    const auto identifier = armor::GetFormIdentifier(equippedArmor);
    if (targetSlotMask == 0 || identifier.empty()) {
      continue;
    }

    KitEntry::LayoutRow layoutRow;
    layoutRow.targetKind = KitEntry::LayoutTargetKind::Item;
    layoutRow.targetSlotMask = targetSlotMask;
    layoutRow.overrideIdentifiers.push_back(identifier);
    layout.rows.push_back(std::move(layoutRow));
  }

  if (layout.rows.empty()) {
    return std::nullopt;
  }

  return layout;
}

bool VariantWorkbench::ApplyKitLayout(
    const KitEntry::Layout &a_layout, const bool a_replaceExisting,
    std::optional<std::string> a_newSlotRowConditionId,
    const RE::FormID a_newSlotRowOwnerActorFormID,
    const InitialEquippedState *a_initialEquippedState,
    const std::vector<int> *a_candidateRowIndices,
    const bool a_allowSlotFallback) {
  auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());

  std::uint64_t actualVisibilitySlotMask = 0;
  std::uint64_t hiddenActualSlotMask = 0;
  if (!a_newSlotRowConditionId.has_value()) {
    for (const auto &layoutRow : a_layout.rows) {
      if (!layoutRow.actualVisibilitySlotMask.has_value() ||
          ResolveKitLayoutOverrideArmors(layoutRow).empty()) {
        continue;
      }

      const auto rowVisibilitySlotMask =
          *layoutRow.actualVisibilitySlotMask & layoutRow.targetSlotMask &
          ~GetEffectiveAppearanceProtectedSlotMask();
      actualVisibilitySlotMask |= rowVisibilitySlotMask;
      hiddenActualSlotMask |=
          layoutRow.hiddenActualSlotMask & rowVisibilitySlotMask;
    }
  }

  const auto fallbackMode = a_allowSlotFallback
                                ? KitLayoutFallbackMode::AnyTargetSlot
                                : (a_newSlotRowConditionId.has_value()
                                       ? KitLayoutFallbackMode::SlotTargetsOnly
                                       : KitLayoutFallbackMode::None);
  const auto projection = ProjectKitLayoutRows(
      a_layout, &candidateRowIndices, fallbackMode, a_newSlotRowConditionId,
      a_newSlotRowOwnerActorFormID, a_replaceExisting);

  // A kit/outfit can contain pieces that became protected after it was saved.
  // Treat an all-protected (or otherwise unusable) layout as a no-op instead
  // of clearing the actor's existing registered appearances. Mixed layouts
  // still project every valid piece and silently skip only protected pieces.
  if (projection.rows.empty() && actualVisibilitySlotMask == 0) {
    return false;
  }

  bool changed = false;
  bool directlyChanged = false;
  if (a_replaceExisting) {
    changed |= ResetAllRows(&candidateRowIndices);
  }

  for (const auto &projectedRow : projection.rows) {
    const auto &row = projectedRow.row;
    auto rowIt = std::ranges::find(rows_, row.key, &VariantWorkbenchRow::key);
    if (rowIt == rows_.end() && row.IsSlotRow() &&
        (a_allowSlotFallback || a_newSlotRowConditionId.has_value())) {
      const auto added =
          AddSlotRow(row.equipped.slotMask, a_newSlotRowConditionId,
                     a_newSlotRowOwnerActorFormID, a_initialEquippedState);
      changed |= added;
      rowIt = std::ranges::find(rows_, row.key, &VariantWorkbenchRow::key);
    }
    if (rowIt == rows_.end()) {
      continue;
    }

    if (rowIt->overrides == row.overrides) {
      continue;
    }

    rowIt->overrides = row.overrides;
    directlyChanged = true;
  }

  bool prunedRows = false;
  for (auto rowIndex = static_cast<int>(rows_.size()) - 1; rowIndex >= 0;
       --rowIndex) {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (row.ownerActorFormID != a_newSlotRowOwnerActorFormID ||
        row.conditionId != a_newSlotRowConditionId || !row.IsSlotRow() ||
        row.conditionId.has_value() || !row.overrides.empty()) {
      continue;
    }

    rows_.erase(rows_.begin() + rowIndex);
    prunedRows = true;
  }

  if (prunedRows) {
    RebuildRowOrder();
    directlyChanged = true;
  }

  directlyChanged |=
      NormalizeOverrideRowsForActor(a_newSlotRowOwnerActorFormID);

  bool actualVisibilityChanged = false;
  if (actualVisibilitySlotMask != 0) {
    for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
         ++rowIndex) {
      const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
      if (row.ownerActorFormID != a_newSlotRowOwnerActorFormID ||
          row.conditionId.has_value() || !row.isEquipped || row.IsSlotRow() ||
          row.IsAlwaysVisibleActualEquipment()) {
        continue;
      }

      const auto *actualArmor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
      if (armor::IsSosTngInternalArmor(actualArmor)) {
        continue;
      }

      const auto affectedSlotMask =
          row.GetSelectionConflictSlotMask() & actualVisibilitySlotMask;
      if (affectedSlotMask == 0) {
        continue;
      }

      const bool hideActual = (affectedSlotMask & hiddenActualSlotMask) != 0;
      actualVisibilityChanged |=
          a_newSlotRowOwnerActorFormID != 0
              ? SetEquippedHiddenForActor(a_newSlotRowOwnerActorFormID,
                                          rowIndex, hideActual)
              : SetEquippedHidden(rowIndex, hideActual);
    }
  }

  if (directlyChanged) {
    MarkChanged();
  }
  return changed || directlyChanged || actualVisibilityChanged;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool VariantWorkbench::InsertCatalogRow(
    RE::FormID a_formID, int a_targetRowIndex, bool a_insertAfter,
    std::optional<std::string> a_conditionId,
    const RE::FormID a_ownerActorFormID,
    const InitialEquippedState *a_initialEquippedState) {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto newRows = BuildCatalogRows(std::vector<RE::FormID>{a_formID},
                                  std::move(a_conditionId), a_ownerActorFormID,
                                  a_initialEquippedState);
  if (newRows.empty()) {
    return false;
  }

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  insertIndex = std::clamp(insertIndex, 0, static_cast<int>(rows_.size()));
  rows_.insert(rows_.begin() + insertIndex, std::move(newRows.front()));

  RebuildRowOrder();
  MarkChanged();
  return true;
}

bool VariantWorkbench::InsertSlotRow(
    const std::uint64_t a_slotMask, int a_targetRowIndex, bool a_insertAfter,
    std::optional<std::string> a_conditionId,
    const RE::FormID a_ownerActorFormID,
    const InitialEquippedState *a_initialEquippedState) {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto row = BuildSlotRow(a_slotMask, std::move(a_conditionId),
                          a_ownerActorFormID, a_initialEquippedState);
  if (!row) {
    return false;
  }

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  insertIndex = std::clamp(insertIndex, 0, static_cast<int>(rows_.size()));
  rows_.insert(rows_.begin() + insertIndex, std::move(*row));

  RebuildRowOrder();
  MarkChanged();
  return true;
}

} // namespace sfs::workbench
