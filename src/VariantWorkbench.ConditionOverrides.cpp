#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "EquipmentCatalog.h"
#include "workbench/ItemFactory.h"
#include "workbench/AppearanceSlotProtection.h"

#include <algorithm>
#include <unordered_set>

namespace sfs::workbench {
namespace {
bool IsValidRowIndex(const int a_rowIndex, const std::size_t a_rowCount) {
  return a_rowIndex >= 0 && a_rowIndex < static_cast<int>(a_rowCount);
}

std::uint64_t SelectPrimarySlotMask(const RE::TESObjectARMO *a_armor,
                                    const EquipmentWidgetItem &a_item) {
  if (a_armor == nullptr) {
    return 0;
  }

  auto slotMask = armor::GetArmorWorkbenchSlotMask(a_armor);
  if (slotMask == 0) {
    slotMask = a_item.slotMask;
  }

  for (const auto singleSlotMask : armor::GetAllArmorSlotMasks()) {
    if ((slotMask & singleSlotMask) != 0) {
      return singleSlotMask;
    }
  }

  return 0;
}

std::optional<VariantWorkbenchRow> FindExistingSlotRow(
    const std::vector<VariantWorkbenchRow> &a_rows, std::uint64_t a_slotMask,
    const std::optional<std::string> &a_conditionId,
    const RE::FormID a_ownerActorFormID) {
  const auto rowIt = std::ranges::find_if(
      a_rows, [&](const VariantWorkbenchRow &a_row) {
        return a_row.IsSlotRow() && a_row.equipped.slotMask == a_slotMask &&
               a_row.conditionId == a_conditionId &&
               a_row.ownerActorFormID == a_ownerActorFormID;
      });
  if (rowIt == a_rows.end()) {
    return std::nullopt;
  }

  return *rowIt;
}

} // namespace

bool VariantWorkbench::PlanSlotFallbackAssignments(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<PlannedSlotFallbackAssignment> &a_assignments,
    int &a_skippedCount) const {
  a_assignments.clear();
  a_skippedCount = 0;

  std::unordered_set<RE::FormID> seenArmorForms;
  for (const auto formID :
       EquipmentCatalog::Get().ResolveArmorFormIDs(a_formIDs)) {
    const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (armor == nullptr ||
        !seenArmorForms.insert(armor->GetFormID()).second) {
      continue;
    }

    EquipmentWidgetItem item{};
    if (!workbench::BuildCatalogItem(armor->GetFormID(), item) ||
        (!item.SupportsArmorReplacement() && item.slotMask == 0) ||
        IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(armor))) {
      ++a_skippedCount;
      continue;
    }

    const auto slotMask = SelectPrimarySlotMask(armor, item);
    if (slotMask == 0) {
      ++a_skippedCount;
      continue;
    }

    a_assignments.push_back({slotMask, armor->GetFormID()});
  }

  return !a_assignments.empty();
}

VariantWorkbench::ConditionOverrideApplicationPlan
VariantWorkbench::PlanKitLayoutConditionOverrideApplication(
    const KitEntry::Layout &a_layout, const std::string_view a_conditionId,
    const std::vector<int> *a_candidateRowIndices,
    const bool a_allowSlotFallback,
    const RE::FormID a_ownerActorFormID) const {
  ConditionOverrideApplicationPlan plan;
  const auto projection = ProjectKitLayoutRows(
      a_layout, a_candidateRowIndices,
      a_allowSlotFallback ? KitLayoutFallbackMode::AnyTargetSlot
                          : KitLayoutFallbackMode::None,
      std::string(a_conditionId), a_ownerActorFormID, true);
  plan.previewRows.reserve(projection.rows.size());
  for (const auto &projectedRow : projection.rows) {
    auto row = projectedRow.row;
    row.hideEquipped = false;
    plan.previewRows.push_back(std::move(row));
  }

  return plan;
}

VariantWorkbench::ConditionOverrideApplicationPlan
VariantWorkbench::PlanConditionOverrideApplication(
    const std::vector<RE::FormID> &a_formIDs,
    const std::string_view a_conditionId, RE::Actor *a_sourceActor) const {
  ConditionOverrideApplicationPlan plan;

  std::vector<PlannedSlotFallbackAssignment> assignments;
  int skippedCount = 0;
  if (!PlanSlotFallbackAssignments(a_formIDs, assignments, skippedCount)) {
    assignments.clear();
  }

  const auto ownerActorFormID =
      a_sourceActor != nullptr ? a_sourceActor->GetFormID() : RE::FormID{0};
  const auto lockedSlotMask =
      GetLockedAppearanceSlotMaskForActor(ownerActorFormID);
  const auto assignmentCountBeforeLockFilter = assignments.size();
  std::erase_if(assignments, [&](const auto &a_assignment) {
    const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(
        a_assignment.armorFormID);
    return armor != nullptr &&
           (armor::GetArmorDisplaySlotMask(armor) & lockedSlotMask) != 0;
  });
  skippedCount += static_cast<int>(assignmentCountBeforeLockFilter -
                                   assignments.size());

  std::vector<std::uint64_t> slotMasks;
  std::unordered_set<std::uint64_t> seenSlotMasks;
  for (const auto &assignment : assignments) {
    if (seenSlotMasks.insert(assignment.slotMask).second) {
      slotMasks.push_back(assignment.slotMask);
    }
  }

  plan.skippedCount = skippedCount;

  for (const auto slotMask : slotMasks) {
    auto row = BuildSlotRow(slotMask, std::string(a_conditionId),
                            ownerActorFormID, nullptr);
    if (!row.has_value()) {
      row = FindExistingSlotRow(rows_, slotMask, std::string(a_conditionId),
                                ownerActorFormID);
      if (row.has_value()) {
        std::erase_if(row->overrides,
                      [](const EquipmentWidgetItem &a_item) {
                        return !a_item.locked;
                      });
        row->hideEquipped = false;
      }
    }

    if (row.has_value()) {
      plan.previewRows.push_back(std::move(*row));
    }
  }

  for (const auto &assignment : assignments) {
    const auto rowIt = std::ranges::find(
        plan.previewRows, assignment.slotMask,
        [](const VariantWorkbenchRow &a_row) { return a_row.equipped.slotMask; });
    if (rowIt == plan.previewRows.end()) {
      continue;
    }

    EquipmentWidgetItem item{};
    if (workbench::BuildCatalogItem(assignment.armorFormID, item)) {
      rowIt->overrides.push_back(std::move(item));
    }
  }

  std::erase_if(plan.previewRows, [](const VariantWorkbenchRow &a_row) {
    return !a_row.HasOverridesOrHideState();
  });
  return plan;
}

VariantWorkbench::ConditionOverrideApplicationPlan
VariantWorkbench::PlanConditionOverrideApplication(
    const KitEntry::Layout &a_layout, const std::string_view a_conditionId,
    RE::Actor *a_sourceActor) const {
  ConditionOverrideApplicationPlan plan;

  const std::vector<int> noCandidateRows;
  const auto ownerActorFormID =
      a_sourceActor != nullptr ? a_sourceActor->GetFormID() : RE::FormID{0};
  plan = PlanKitLayoutConditionOverrideApplication(
      a_layout, a_conditionId, &noCandidateRows, true, ownerActorFormID);
  return plan;
}

bool VariantWorkbench::ApplyConditionOverridePlan(
    const ConditionOverrideApplicationPlan &a_plan,
    const std::string_view a_conditionId, const bool a_replaceConditionSet,
    const InitialEquippedState *a_initialEquippedState) {
  if (!a_plan.CanApply()) {
    return false;
  }

  const auto previousRows = rows_;
  bool changed = false;
  bool directlyChanged = false;
  std::unordered_set<RE::FormID> affectedOwnerActorFormIDs;
  for (const auto &previewRow : a_plan.previewRows) {
    affectedOwnerActorFormIDs.insert(previewRow.ownerActorFormID);
  }

  for (const auto ownerActorFormID : affectedOwnerActorFormIDs) {
    std::uint64_t incomingSlotMask = 0;
    for (const auto &previewRow : a_plan.previewRows) {
      if (previewRow.ownerActorFormID == ownerActorFormID) {
        incomingSlotMask |= previewRow.GetOverrideDisplaySlotMask();
      }
    }

    for (auto &row : rows_) {
      if (row.ownerActorFormID != ownerActorFormID ||
          row.conditionId != std::optional<std::string>(a_conditionId)) {
        continue;
      }

      if (a_replaceConditionSet) {
        const auto oldSize = row.overrides.size();
        std::erase_if(row.overrides,
                      [](const EquipmentWidgetItem &a_item) {
                        return !a_item.locked;
                      });
        if (row.overrides.size() != oldSize) {
          directlyChanged = true;
        }
        continue;
      }

      const auto oldSize = row.overrides.size();
      std::erase_if(row.overrides, [&](const EquipmentWidgetItem &a_item) {
        return !a_item.locked &&
               (row.GetOverrideDisplaySlotMask(a_item) & incomingSlotMask) !=
                   0;
      });
      directlyChanged |= row.overrides.size() != oldSize;
    }
  }

  for (const auto &previewRow : a_plan.previewRows) {
    if (!previewRow.HasOverridesOrHideState() || !previewRow.IsSlotRow()) {
      continue;
    }

    auto rowIt = std::ranges::find(rows_, previewRow.key,
                                   &VariantWorkbenchRow::key);
    if (rowIt == rows_.end()) {
      const auto added =
          AddSlotRow(previewRow.equipped.slotMask, std::string(a_conditionId),
                     previewRow.ownerActorFormID,
                     a_initialEquippedState);
      changed |= added;
      rowIt = std::ranges::find(rows_, previewRow.key,
                                &VariantWorkbenchRow::key);
    }
    if (rowIt == rows_.end()) {
      continue;
    }

    auto desiredOverrides = rowIt->overrides;
    std::erase_if(desiredOverrides,
                  [](const EquipmentWidgetItem &a_item) {
                    return !a_item.locked;
                  });
    std::uint64_t lockedSlotMask = 0;
    for (const auto &item : desiredOverrides) {
      lockedSlotMask |= rowIt->GetOverrideDisplaySlotMask(item);
    }
    for (const auto &item : previewRow.overrides) {
      if ((previewRow.GetOverrideDisplaySlotMask(item) & lockedSlotMask) == 0 &&
          std::ranges::find(desiredOverrides, item.formID,
                            &EquipmentWidgetItem::formID) ==
              desiredOverrides.end()) {
        desiredOverrides.push_back(item);
      }
    }

    if (rowIt->overrides == desiredOverrides && !rowIt->hideEquipped) {
      continue;
    }

    rowIt->overrides = std::move(desiredOverrides);
    rowIt->hideEquipped = false;
    directlyChanged = true;
  }

  // An empty conditional slot is intentional UI state: it is the remaining
  // condition card after its equipment/appearance target is removed.  Do not
  // prune it when a condition set is replaced; it must survive save/load and
  // remain available as a drop target until the user explicitly fills it.

  for (const auto ownerActorFormID : affectedOwnerActorFormIDs) {
    directlyChanged |= NormalizeOverrideRowsForActor(ownerActorFormID);
  }

  if (directlyChanged) {
    InvalidateRemovedAppearanceAutomation(previousRows);
    MarkChanged();
  }
  return changed || directlyChanged;
}
} // namespace sfs::workbench
