#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "workbench/ItemFactory.h"
#include "workbench/AppearanceSlotProtection.h"

#include <algorithm>

namespace sfs::workbench {
bool VariantWorkbench::AddConditionalVisibilityRule(
    const std::string_view a_conditionId,
    const RE::FormID a_ownerActorFormID,
    const ConditionalVisibilityTargetKind a_targetKind,
    const RE::FormID a_formID, const bool a_visibleWhenTrue,
    const std::uint64_t a_uiIdentity,
    const std::uint64_t a_registrationOrder) {
  // A condition slot is deliberately allowed to have only one side filled.
  // This keeps a dropped condition or target visible and editable after the
  // workbench is closed and opened again.
  if (a_conditionId.empty() && a_formID == 0) {
    return false;
  }

  EquipmentWidgetItem target{};
  if (a_formID != 0) {
    const auto *targetArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(a_formID);
    if (!targetArmor || !BuildCatalogItem(a_formID, target) ||
        IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(targetArmor))) {
      return false;
    }
  }

  const auto duplicate = std::ranges::find_if(
      conditionalVisibilityRules_, [&](const auto &a_rule) {
        return a_rule.conditionId == a_conditionId &&
               a_rule.ownerActorFormID == a_ownerActorFormID &&
               a_rule.targetKind == a_targetKind &&
               a_rule.target.formID == a_formID;
      });
  if (duplicate != conditionalVisibilityRules_.end()) {
    if (duplicate->visibleWhenTrue == a_visibleWhenTrue) {
      return false;
    }
    duplicate->visibleWhenTrue = a_visibleWhenTrue;
    MarkChanged();
    return true;
  }

  ConditionalVisibilityRule rule{
      .conditionId = std::string(a_conditionId),
      .ownerActorFormID = a_ownerActorFormID,
      .targetKind = a_targetKind,
      .target = std::move(target),
      .visibleWhenTrue = a_visibleWhenTrue};
  if (a_uiIdentity != 0) {
    rule.uiIdentity = a_uiIdentity;
  }
  if (a_registrationOrder != 0) {
    rule.registrationOrder = a_registrationOrder;
    ObserveVariantWorkbenchRegistrationOrder(a_registrationOrder);
  }
  conditionalVisibilityRules_.push_back(std::move(rule));
  MarkChanged();
  return true;
}

bool VariantWorkbench::ConvertConditionalVisibilityRuleToFittingRow(
    const std::size_t a_ruleIndex, const RE::FormID a_formID) {
  if (a_ruleIndex >= conditionalVisibilityRules_.size() || a_formID == 0) {
    return false;
  }

  const auto preservedRule = conditionalVisibilityRules_[a_ruleIndex];
  EquipmentWidgetItem item{};
  const auto *appearanceArmor =
      RE::TESForm::LookupByID<RE::TESObjectARMO>(a_formID);
  if (appearanceArmor == nullptr || !BuildCatalogItem(a_formID, item) ||
      !item.SupportsArmorReplacement() ||
      armor::IsSosTngInternalArmor(appearanceArmor)) {
    return false;
  }
  const auto appearanceSlotMask = armor::GetArmorDisplaySlotMask(appearanceArmor);
  if (appearanceSlotMask == 0 ||
      IsAppearanceRegistrationProtectedSlotMask(appearanceSlotMask)) {
    return false;
  }
  const auto ownerActorFormID = preservedRule.ownerActorFormID;
  if ((appearanceSlotMask &
       GetLockedAppearanceSlotMaskForActor(ownerActorFormID)) != 0) {
    // Converting a visibility-only action into a registered fitting action is
    // still an appearance assignment.  Apply the same actor-local, atomic
    // locked-slot rule as Gear/Outfits/Kits: an incoming multi-slot armor is
    // rejected as a whole when any displayed slot overlaps a locked card.
    return false;
  }

  // Use the same workbench representative-slot rule as ordinary appearance
  // registration. The row model still retains every original occupied slot on
  // the item itself.
  std::uint64_t representativeSlotMask = 0;
  const auto workbenchSlotMask = armor::GetArmorWorkbenchSlotMask(appearanceArmor);
  const auto placementSlotMask =
      workbenchSlotMask != 0 ? workbenchSlotMask : item.slotMask;
  if (armor::GetArmorSlotNumber(placementSlotMask) != 0) {
    representativeSlotMask = placementSlotMask;
  } else {
    for (std::uint32_t slot = 30; slot <= 61; ++slot) {
      const auto candidate = armor::GetArmorSlotMask(slot);
      if ((placementSlotMask & candidate) != 0) {
        representativeSlotMask = candidate;
        break;
      }
    }
  }
  if (representativeSlotMask == 0) {
    return false;
  }
  auto row = BuildSlotRow(representativeSlotMask, preservedRule.conditionId,
                          preservedRule.ownerActorFormID, nullptr);
  if (!row.has_value()) {
    const auto targetKey = std::string("slot:") +
        std::to_string(armor::GetArmorSlotNumber(representativeSlotMask)) +
        "|condition:" + preservedRule.conditionId +
        (preservedRule.ownerActorFormID != 0
             ? "|actor:" + armor::FormatFormID(preservedRule.ownerActorFormID)
             : std::string{});
    const auto existing =
        std::ranges::find(rows_, targetKey, &VariantWorkbenchRow::key);
    if (existing == rows_.end()) {
      return false;
    }
    item.hidden = false;
    existing->uiIdentity = preservedRule.uiIdentity;
    existing->registrationOrder = preservedRule.registrationOrder;
    std::erase_if(existing->overrides,
                  [](const EquipmentWidgetItem &a_item) {
                    return !a_item.locked;
                  });
    existing->overrides.push_back(std::move(item));
    conditionalVisibilityRules_.erase(conditionalVisibilityRules_.begin() +
                                      static_cast<std::ptrdiff_t>(a_ruleIndex));
    RebuildRowOrder();
    MarkChanged();
    return true;
  }
  item.hidden = false;
  row->uiIdentity = preservedRule.uiIdentity;
  row->registrationOrder = preservedRule.registrationOrder;
  row->overrides.push_back(std::move(item));
  conditionalVisibilityRules_.erase(conditionalVisibilityRules_.begin() +
                                    static_cast<std::ptrdiff_t>(a_ruleIndex));
  rowOrder_.push_back(row->key);
  rows_.push_back(std::move(*row));
  MarkChanged();
  return true;
}

bool VariantWorkbench::DeleteConditionalVisibilityRule(
    const std::size_t a_ruleIndex) {
  if (a_ruleIndex >= conditionalVisibilityRules_.size()) {
    return false;
  }
  conditionalVisibilityRules_.erase(conditionalVisibilityRules_.begin() +
                                    static_cast<std::ptrdiff_t>(a_ruleIndex));
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetConditionalVisibilityRuleConditionId(
    const std::size_t a_ruleIndex, const std::string_view a_conditionId) {
  if (a_ruleIndex >= conditionalVisibilityRules_.size()) {
    return false;
  }
  auto &rule = conditionalVisibilityRules_[a_ruleIndex];
  if (rule.conditionId == a_conditionId) {
    return false;
  }
  if (!a_conditionId.empty()) {
    const auto duplicate = std::ranges::find_if(
        conditionalVisibilityRules_, [&](const auto &a_candidate) {
          return std::addressof(a_candidate) != std::addressof(rule) &&
                 a_candidate.conditionId == a_conditionId &&
                 a_candidate.ownerActorFormID == rule.ownerActorFormID &&
                 a_candidate.targetKind == rule.targetKind &&
                 a_candidate.target.formID == rule.target.formID;
        });
    if (duplicate != conditionalVisibilityRules_.end()) {
      return false;
    }
  }
  rule.conditionId = std::string(a_conditionId);
  MarkChanged();
  return true;
}

std::size_t
VariantWorkbench::ClearConditionalVisibilityRuleConditionsByConditionId(
    const std::string_view a_conditionId) {
  if (a_conditionId.empty()) {
    return 0;
  }

  std::size_t clearedCount = 0;
  for (auto &rule : conditionalVisibilityRules_) {
    if (rule.conditionId != a_conditionId) {
      continue;
    }
    rule.conditionId.clear();
    ++clearedCount;
  }
  if (clearedCount != 0) {
    MarkChanged();
  }
  return clearedCount;
}

bool VariantWorkbench::SetConditionalVisibilityRuleTarget(
    const std::size_t a_ruleIndex,
    const ConditionalVisibilityTargetKind a_targetKind,
    const RE::FormID a_formID) {
  if (a_ruleIndex >= conditionalVisibilityRules_.size()) {
    return false;
  }

  EquipmentWidgetItem target{};
  if (a_formID != 0) {
    const auto *targetArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(a_formID);
    if (!targetArmor || !BuildCatalogItem(a_formID, target) ||
        IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(targetArmor))) {
      return false;
    }
  }

  auto &rule = conditionalVisibilityRules_[a_ruleIndex];
  if (rule.targetKind == a_targetKind && rule.target.formID == a_formID) {
    return false;
  }
  rule.targetKind = a_targetKind;
  rule.target = std::move(target);
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetConditionalVisibilityRuleVisible(
    const std::size_t a_ruleIndex, const bool a_visibleWhenTrue) {
  if (a_ruleIndex >= conditionalVisibilityRules_.size()) {
    return false;
  }
  auto &rule = conditionalVisibilityRules_[a_ruleIndex];
  if (rule.visibleWhenTrue == a_visibleWhenTrue) {
    return false;
  }
  rule.visibleWhenTrue = a_visibleWhenTrue;
  MarkChanged();
  return true;
}
} // namespace sfs::workbench
