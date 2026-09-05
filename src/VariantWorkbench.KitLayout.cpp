#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "workbench/ItemFactory.h"
#include "workbench/AppearanceSlotProtection.h"

#include <algorithm>
#include <unordered_map>

namespace sfs::workbench {
namespace {
bool IsValidRowIndex(const int a_rowIndex, const std::size_t a_rowCount) {
  return a_rowIndex >= 0 && a_rowIndex < static_cast<int>(a_rowCount);
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

std::uint64_t SelectPrimarySlotMask(const std::uint64_t a_slotMask) {
  for (const auto singleSlotMask : armor::GetAllArmorSlotMasks()) {
    if ((a_slotMask & singleSlotMask) != 0) {
      return singleSlotMask;
    }
  }

  return 0;
}

std::uint64_t SelectArmorDisplaySlotMask(const RE::TESObjectARMO *a_armor) {
  if (a_armor == nullptr) {
    return 0;
  }

  return SelectPrimarySlotMask(armor::GetArmorWorkbenchSlotMask(a_armor));
}
} // namespace

bool VariantWorkbench::AppendOverrideItem(
    std::vector<EquipmentWidgetItem> &a_overrides,
    const RE::TESObjectARMO *a_overrideArmor,
    const bool a_allowRestrictedPreview) {
  if (a_overrideArmor == nullptr ||
      (!a_allowRestrictedPreview &&
       (armor::IsSosTngInternalArmor(a_overrideArmor) ||
        IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(a_overrideArmor))))) {
    return false;
  }
  if (std::ranges::find(a_overrides, a_overrideArmor->GetFormID(),
                        &EquipmentWidgetItem::formID) != a_overrides.end()) {
    return false;
  }

  EquipmentWidgetItem item{};
  if (!workbench::BuildCatalogItem(a_overrideArmor->GetFormID(), item) ||
      (!item.SupportsArmorReplacement() && item.slotMask == 0) ||
      armor::GetFormIdentifier(a_overrideArmor).empty()) {
    return false;
  }

  a_overrides.push_back(std::move(item));
  return true;
}

std::vector<const RE::TESObjectARMO *>
VariantWorkbench::ResolveKitLayoutOverrideArmors(
    const KitEntry::LayoutRow &a_layoutRow,
    const bool a_allowRestrictedPreview) {
  std::vector<const RE::TESObjectARMO *> overrideArmors;
  overrideArmors.reserve(a_layoutRow.overrideIdentifiers.size());
  for (const auto &identifier : a_layoutRow.overrideIdentifiers) {
    if (const auto *overrideArmor =
            armor::LookupByIdentifier<RE::TESObjectARMO>(identifier);
        overrideArmor != nullptr &&
        (a_allowRestrictedPreview ||
         (!armor::IsSosTngInternalArmor(overrideArmor) &&
          !IsAppearanceRegistrationProtectedSlotMask(
              armor::GetArmorDisplaySlotMask(overrideArmor))))) {
      overrideArmors.push_back(overrideArmor);
    }
  }
  return overrideArmors;
}

int VariantWorkbench::FindKitLayoutTargetRowIndex(
    const KitEntry::LayoutRow &a_layoutRow,
    const std::vector<int> &a_candidateRowIndices) const {
  if (a_layoutRow.targetKind == KitEntry::LayoutTargetKind::Slot) {
    const auto equippedRowIndex = FindBestItemTargetRowIndexBySlotMask(
        a_layoutRow.targetSlotMask, false, nullptr, nullptr,
        &a_candidateRowIndices);
    if (equippedRowIndex >= 0) {
      return equippedRowIndex;
    }

    for (const auto rowIndex : a_candidateRowIndices) {
      if (!IsValidRowIndex(rowIndex, rows_.size())) {
        continue;
      }

      const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
      if (row.IsSlotRow() &&
          row.equipped.slotMask == a_layoutRow.targetSlotMask) {
        return rowIndex;
      }
    }
  }

  return FindBestItemTargetRowIndexBySlotMask(
      a_layoutRow.targetSlotMask, false, nullptr, nullptr,
      &a_candidateRowIndices);
}

VariantWorkbench::KitLayoutProjection VariantWorkbench::ProjectKitLayoutRows(
    const KitEntry::Layout &a_layout,
    const std::vector<int> *a_candidateRowIndices,
    const KitLayoutFallbackMode a_fallbackMode,
    std::optional<std::string> a_fallbackConditionId,
    const RE::FormID a_fallbackOwnerActorFormID,
    const bool a_replaceExisting,
    const bool a_allowRestrictedPreview) const {
  KitLayoutProjection projection;
  const auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());

  std::unordered_map<int, std::size_t> projectedIndexByTargetRow;
  std::unordered_map<std::uint64_t, std::size_t> fallbackIndexBySlotMask;
  std::size_t projectedFallbackCount = 0;
  auto lockedAppearanceSlotMask =
      GetLockedAppearanceSlotMaskForCandidateRows(a_candidateRowIndices);
  if (a_fallbackOwnerActorFormID != 0) {
    lockedAppearanceSlotMask |=
        GetLockedAppearanceSlotMaskForActor(a_fallbackOwnerActorFormID);
  }
  // Locked appearances claim their complete ARMO display mask before any new
  // kit/outfit item is considered. Multi-slot cards remain atomic: an
  // incoming item with even one overlapping slot is omitted in full.
  std::uint64_t claimedOverrideDisplaySlotMask = lockedAppearanceSlotMask;

  const auto canFallbackToSlot =
      [&](const std::uint64_t a_slotMask) {
        return a_fallbackMode != KitLayoutFallbackMode::None &&
               a_slotMask != 0;
      };

  const auto projectExistingRow = [&](const int a_targetRowIndex,
                                      const RE::TESObjectARMO *a_overrideArmor,
                                      const bool) {
    if (!IsValidRowIndex(a_targetRowIndex, rows_.size())) {
      return false;
    }

    const auto [rowIt, inserted] = projectedIndexByTargetRow.try_emplace(
        a_targetRowIndex, projection.rows.size());
    if (inserted) {
      auto row = rows_[static_cast<std::size_t>(a_targetRowIndex)];
      if (a_replaceExisting) {
        std::erase_if(row.overrides,
                      [](const EquipmentWidgetItem &a_item) {
                        return !a_item.locked;
                      });
        row.hideEquipped = false;
      }
      projection.rows.push_back(
          {.row = std::move(row),
           .priorityRowIndex = static_cast<std::size_t>(a_targetRowIndex)});
    }

    auto &projectedRow = projection.rows[rowIt->second].row;
    AppendOverrideItem(projectedRow.overrides, a_overrideArmor,
                       a_allowRestrictedPreview);
    projectedRow.hideEquipped = false;
    return true;
  };

  const auto findEquippedRowForSlot =
      [&](const std::uint64_t a_slotMask) -> int {
    for (const auto rowIndex : candidateRowIndices) {
      if (!IsValidRowIndex(rowIndex, rows_.size())) {
        continue;
      }

      const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
      if (row.IsSlotRow() || !row.isEquipped) {
        continue;
      }

      if ((row.GetSelectionConflictSlotMask() & a_slotMask) != 0) {
        return rowIndex;
      }
    }

    return -1;
  };

  const auto projectSlotRow = [&](const std::uint64_t a_slotMask,
                                  const RE::TESObjectARMO *a_overrideArmor,
                                  const bool a_hideEquipped) {
    const auto slotMask = SelectPrimarySlotMask(a_slotMask);
    if (slotMask == 0) {
      return;
    }

    const auto existingTargetRowIndex = findEquippedRowForSlot(slotMask);
    if (existingTargetRowIndex >= 0 &&
        projectExistingRow(existingTargetRowIndex, a_overrideArmor,
                           a_hideEquipped)) {
      return;
    }

    if (!canFallbackToSlot(slotMask)) {
      return;
    }

    auto slotIt = fallbackIndexBySlotMask.find(slotMask);
    if (slotIt == fallbackIndexBySlotMask.end()) {
      auto row = BuildSlotRow(slotMask, a_fallbackConditionId,
                              a_fallbackOwnerActorFormID, nullptr,
                              a_allowRestrictedPreview);
      if (!row.has_value()) {
        row = FindExistingSlotRow(rows_, slotMask, a_fallbackConditionId,
                                  a_fallbackOwnerActorFormID);
      }
      if (!row.has_value()) {
        return;
      }

      if (a_replaceExisting) {
        std::erase_if(row->overrides,
                      [](const EquipmentWidgetItem &a_item) {
                        return !a_item.locked;
                      });
        row->hideEquipped = false;
      }
      const auto projectedIndex = projection.rows.size();
      fallbackIndexBySlotMask.emplace(slotMask, projectedIndex);
      projection.rows.push_back(
          {.row = std::move(*row),
           .priorityRowIndex = rows_.size() + projectedFallbackCount});
      ++projectedFallbackCount;
      slotIt = fallbackIndexBySlotMask.find(slotMask);
    }

    auto &projectedRow = projection.rows[slotIt->second].row;
    AppendOverrideItem(projectedRow.overrides, a_overrideArmor,
                       a_allowRestrictedPreview);
    projectedRow.hideEquipped = false;
  };

  for (const auto &layoutRow : a_layout.rows) {
    auto overrideArmors = ResolveKitLayoutOverrideArmors(
        layoutRow, a_allowRestrictedPreview);
    std::erase_if(overrideArmors, [&](const RE::TESObjectARMO *a_armor) {
      const auto displaySlotMask = armor::GetArmorDisplaySlotMask(a_armor);
      if (displaySlotMask == 0 ||
          (displaySlotMask & claimedOverrideDisplaySlotMask) != 0) {
        return true;
      }
      claimedOverrideDisplaySlotMask |= displaySlotMask;
      return false;
    });

    if (a_fallbackMode != KitLayoutFallbackMode::None) {
      if (overrideArmors.empty()) {
        continue;
      }

      const auto layoutSlotMask = SelectPrimarySlotMask(layoutRow.targetSlotMask);
      for (const auto *overrideArmor : overrideArmors) {
        const auto overrideSlotMask = SelectArmorDisplaySlotMask(overrideArmor);
        projectSlotRow(layoutSlotMask != 0 ? layoutSlotMask : overrideSlotMask,
                       overrideArmor, layoutRow.hideEquipped);
      }
      continue;
    }

    auto targetRowIndex =
        FindKitLayoutTargetRowIndex(layoutRow, candidateRowIndices);

    if (targetRowIndex < 0) {
      if (!canFallbackToSlot(layoutRow.targetSlotMask)) {
        continue;
      }

      auto slotIt = fallbackIndexBySlotMask.find(layoutRow.targetSlotMask);
      if (slotIt == fallbackIndexBySlotMask.end()) {
        auto row = BuildSlotRow(layoutRow.targetSlotMask,
                                a_fallbackConditionId,
                                a_fallbackOwnerActorFormID, nullptr,
                                a_allowRestrictedPreview);
        if (!row.has_value()) {
          row = FindExistingSlotRow(rows_, layoutRow.targetSlotMask,
                                    a_fallbackConditionId,
                                    a_fallbackOwnerActorFormID);
        }
        if (!row.has_value()) {
          continue;
        }

        if (a_replaceExisting) {
          std::erase_if(row->overrides,
                        [](const EquipmentWidgetItem &a_item) {
                          return !a_item.locked;
                        });
          row->hideEquipped = false;
        }
        const auto projectedIndex = projection.rows.size();
        fallbackIndexBySlotMask.emplace(layoutRow.targetSlotMask,
                                        projectedIndex);
        projection.rows.push_back(
            {.row = std::move(*row),
             .priorityRowIndex = rows_.size() + projectedFallbackCount});
        ++projectedFallbackCount;
        slotIt = fallbackIndexBySlotMask.find(layoutRow.targetSlotMask);
      }

      auto &projectedRow = projection.rows[slotIt->second].row;
      for (const auto *overrideArmor : overrideArmors) {
        AppendOverrideItem(projectedRow.overrides, overrideArmor,
                           a_allowRestrictedPreview);
      }
      projectedRow.hideEquipped = false;
      continue;
    }

    for (const auto *overrideArmor : overrideArmors) {
      projectExistingRow(targetRowIndex, overrideArmor,
                         layoutRow.hideEquipped);
    }
  }

  std::erase_if(projection.rows, [](const ProjectedKitLayoutRow &a_row) {
    return !a_row.row.HasOverridesOrHideState();
  });
  return projection;
}
} // namespace sfs::workbench
