#include "Menu.h"

#include "ArmorUtils.h"
#include "imgui_internal.h"
#include "native/FittingSlotState.h"
#include "features/devious_devices/DeviousDevicesIntegration.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include "ui/Localization.h"
#include "ui/components/EditableCombo.h"
#include "ui/workbench/Common.h"
#include "workbench/EquipmentRefreshEventSink.h"
#include "workbench/AutomaticEquipmentVisibility.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>

namespace {
struct WorkbenchToolbarAction {
  std::string label;
  bool enabled{true};
  std::function<void()> callback;
  std::function<void()> tooltip;
};
} // namespace

namespace sfs {
bool Menu::DrawWorkbenchFilterBar() {
  auto *localization = ui::Localization::GetSingleton();
  EnsureWorkbenchDerivedState();
  const auto &filterOptions = workbenchDerived_.filterOptions;

  std::vector<ui::components::EditableDropdownItem<WorkbenchFilterOption>>
      filterItems;
  filterItems.reserve(filterOptions.size());

  std::string selectedFilterLabel(
      localization->Get("workbench.filter.placeholder"));
  int selectedFilterIndex = -1;
  if (const auto it =
          std::ranges::find(filterOptions, workbenchFilter_.actorFormID,
                            &WorkbenchFilterOption::actorFormID);
      it != filterOptions.end()) {
    selectedFilterLabel = it->label;
    selectedFilterIndex =
        static_cast<int>(std::distance(filterOptions.begin(), it));
  }

  for (const auto &option : filterOptions) {
    filterItems.push_back({.label = option.label, .value = option});
  }

  std::optional<WorkbenchFilterOption> selectedFilterOption;
  const std::function<void(
      const ui::components::EditableDropdownItem<WorkbenchFilterOption> &)>
      drawFilterTooltip = [&](const auto &item) {
        if (item.value.has_value()) {
          ui::workbench::DrawWorkbenchFilterOptionTooltip(*item.value);
        }
      };
  const std::string filterPlaceholder(
      localization->Get("workbench.filter.placeholder"));
  const bool filterSelectionChanged = ui::components::DrawSearchableDropdown(
      "##workbench-filter", filterPlaceholder.c_str(), selectedFilterLabel,
      std::span<
          const ui::components::EditableDropdownItem<WorkbenchFilterOption>>(
          filterItems),
      ImGui::GetContentRegionAvail().x, &selectedFilterIndex,
      &selectedFilterOption, drawFilterTooltip,
      static_cast<int>(filterItems.size()));
  if (filterSelectionChanged && selectedFilterOption.has_value()) {
    workbenchFilter_.actorFormID = selectedFilterOption->actorFormID;
    pendingSlotCreations_.clear();
    workbench_.ClearPreview();
    SyncWorkbenchRowsForCurrentFilter();
    workbench::EquipmentRefreshEventSink::GetSingleton()->QueueActorRefresh(
        workbenchFilter_.actorFormID);
    ui::MenuCharacterPresentation::GetSingleton()->Apply(
        menuCharacterSide_, ResolveWorkbenchPreviewActor());
  }
  return filterSelectionChanged;
}
void Menu::DrawWorkbenchToolbar() {
  auto *localization = ui::Localization::GetSingleton();
  const auto visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  std::vector<int> baseRowIndices;
  std::ranges::copy_if(visibleRowIndices, std::back_inserter(baseRowIndices),
                       [&](const int a_rowIndex) {
                         const auto &rows = workbench_.GetRows();
                         return a_rowIndex >= 0 &&
                                a_rowIndex < static_cast<int>(rows.size()) &&
                                !rows[static_cast<std::size_t>(a_rowIndex)]
                                     .conditionId.has_value();
                       });
  const auto overrideKitFormIDs =
      workbench_.CollectOverrideArmorFormIDsFromEquippedRows(&baseRowIndices);
  const bool canCreateOverrideKit = !overrideKitFormIDs.empty();
  const bool canDeleteAppearances =
      std::ranges::any_of(baseRowIndices, [&](const int a_rowIndex) {
        const auto &rows = workbench_.GetRows();
        return a_rowIndex >= 0 && a_rowIndex < static_cast<int>(rows.size()) &&
               !rows[static_cast<std::size_t>(a_rowIndex)]
                    .conditionId.has_value() &&
               !rows[static_cast<std::size_t>(a_rowIndex)].overrides.empty();
      });

  const std::string kitFromOverridesLabel(
      localization->Get("workbench.toolbar.kit_from_overrides"));
  const std::string kitFromOverridesTooltip(
      localization->Get("workbench.toolbar.kit_from_overrides.tooltip"));
  const std::string deleteAppearancesLabel(
      localization->Get("workbench.toolbar.reset_all"));
  const std::string deleteAppearancesTooltip(
      localization->Get("workbench.toolbar.reset_all.tooltip"));
  const std::string hideRealEquipmentLabel(
      localization->Get("options.hide_real_equipment_with_fitting"));
  const std::string hideFittingAppearancesLabel(
      localization->Get("options.hide_fitting_appearances"));
  std::vector<int> controllableActualRows;
  std::vector<std::pair<int, int>> controllableOverrides;
  int individuallyHiddenActualCount = 0;
  int individuallyHiddenOverrideCount = 0;
  auto *previewActor = ResolveWorkbenchPreviewActor();
  const auto virtualTokenSuppressedFittingSlotMask =
      previewActor != nullptr &&
              sfs::workbench::IsModSettingsStripLinkPolicyActive()
          ? static_cast<std::uint64_t>(
                sfs::native::GetVirtualTokenSuppressedFittingSlotMask(
                    previewActor))
          : 0;
  const auto headgearToggleSuppressedFittingSlotMask =
      previewActor != nullptr
          ? static_cast<std::uint64_t>(
                sfs::native::GetHeadgearToggleSuppressedFittingSlotMask(
                    previewActor))
          : 0;
  const auto deviousDevicesHiderSuppressedFittingSlotMask =
      previewActor != nullptr
          ? static_cast<std::uint64_t>(
                sfs::devious_devices::GetDeviousDevicesHiderSuppressedFittingSlotMask(
                    previewActor))
          : 0;
  for (const auto rowIndex : baseRowIndices) {
    const auto &rows = workbench_.GetRows();
    if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size())) {
      continue;
    }
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    if (row.isEquipped && !row.IsSlotRow()) {
      const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
      if (!sfs::armor::IsSosTngInternalArmor(armor) &&
          !row.IsAlwaysVisibleActualEquipment()) {
        controllableActualRows.push_back(rowIndex);
        const bool ddOrdinaryVisible =
            previewActor != nullptr &&
            sfs::devious_devices::IsDeviousDevicesRenderedDeviceOrdinaryVisible(
                previewActor->GetFormID(), row.equipped.formID);
        if (!ddOrdinaryVisible &&
            workbench_.ResolveEquippedIndividualHiddenForActor(previewActor,
                                                               row)) {
          ++individuallyHiddenActualCount;
        }
      }
    }
    for (int itemIndex = 0; itemIndex < static_cast<int>(row.overrides.size());
         ++itemIndex) {
      const auto &overrideItem =
          row.overrides[static_cast<std::size_t>(itemIndex)];
      controllableOverrides.emplace_back(rowIndex, itemIndex);
      const auto overrideSlotMask = row.GetOverrideVisualSlotMask(overrideItem);
      const bool virtualTokenAppearanceSuppressed =
          !overrideItem.locked && previewActor != nullptr &&
          (overrideSlotMask & virtualTokenSuppressedFittingSlotMask) == 0 &&
          sfs::virtual_tokens::IsVirtualWornTokenAppearanceSuppressed(
              previewActor->GetFormID(), overrideItem.formID,
              static_cast<std::uint32_t>(overrideSlotMask));
      const bool automaticallyHidden =
          !overrideItem.locked &&
          (row.IsOverrideAutomaticallySuppressed(overrideItem) ||
           virtualTokenAppearanceSuppressed ||
           (overrideSlotMask & virtualTokenSuppressedFittingSlotMask) != 0 ||
           (overrideSlotMask &
            deviousDevicesHiderSuppressedFittingSlotMask) != 0 ||
           (overrideSlotMask & headgearToggleSuppressedFittingSlotMask) != 0);
      if (overrideItem.hidden || automaticallyHidden) {
        ++individuallyHiddenOverrideCount;
      }
    }
  }
  const std::vector<WorkbenchToolbarAction> actions = {
      WorkbenchToolbarAction{
          .label = kitFromOverridesLabel,
          .enabled = canCreateOverrideKit,
          .callback =
              [&]() {
                OpenCreateKitDialog(KitCreationSource::Overrides,
                                    &baseRowIndices);
              },
          .tooltip =
              [kitFromOverridesTooltip]() {
                ImGui::TextUnformatted(kitFromOverridesTooltip.data());
              },
      },
      WorkbenchToolbarAction{
          .label = deleteAppearancesLabel,
          .enabled = canDeleteAppearances,
          .callback =
              [&]() {
                std::vector<int> targetRows;
                std::ranges::copy_if(
                    visibleRowIndices, std::back_inserter(targetRows),
                    [&](const int a_rowIndex) {
                      const auto &rows = workbench_.GetRows();
                      return a_rowIndex >= 0 &&
                             a_rowIndex < static_cast<int>(rows.size()) &&
                             !rows[static_cast<std::size_t>(a_rowIndex)]
                                  .conditionId.has_value();
                    });
                bool changed = workbench_.ResetAllRows(&targetRows);

                std::ranges::sort(targetRows, std::greater<>{});
                for (const auto rowIndex : targetRows) {
                  const auto &rows = workbench_.GetRows();
                  if (rowIndex < 0 ||
                      rowIndex >= static_cast<int>(rows.size())) {
                    continue;
                  }

                  const auto &row = rows[static_cast<std::size_t>(rowIndex)];
                  if (!row.IsSlotRow() || !row.overrides.empty()) {
                    continue;
                  }

                  changed |= workbench_.DeleteRow(rowIndex);
                }

                if (changed) {
                  SyncWorkbenchRowsForCurrentFilter();
                  workbench_.RefreshNativeArmorOverridesForActor(
                      previewActor != nullptr ? previewActor->GetFormID()
                                              : RE::FormID{0},
                      conditionStore_.revision);
                }
              },
          .tooltip =
              [deleteAppearancesTooltip]() {
                ImGui::TextUnformatted(deleteAppearancesTooltip.data());
              },
      },
  };

  const auto &style = ImGui::GetStyle();
  const auto spacingX = style.ItemSpacing.x;
  const auto buttonWidth = [](std::string_view label) {
    return ImGui::CalcTextSize(label.data(), label.data() + label.size()).x +
           ImGui::GetStyle().FramePadding.x * 2.0f;
  };
  const auto checkboxWidth = [&](const std::string &a_label) {
    return ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
           ImGui::CalcTextSize(a_label.data()).x;
  };

  std::vector<float> widths;
  widths.reserve(actions.size());
  float totalWidth = 0.0f;
  for (const auto &action : actions) {
    const auto width = buttonWidth(action.label);
    widths.push_back(width);
    if (widths.size() > 1) {
      totalWidth += spacingX;
    }
    totalWidth += width;
  }

  const auto availableWidth = ImGui::GetContentRegionAvail().x;
  const auto toggleReserveWidth = checkboxWidth(hideRealEquipmentLabel) +
                                  spacingX +
                                  checkboxWidth(hideFittingAppearancesLabel) +
                                  (actions.empty() ? 0.0f : spacingX);
  const auto availableActionWidth =
      (std::max)(0.0f, availableWidth - toggleReserveWidth);
  const auto moreButtonWidth = buttonWidth(ui::workbench::kIconEllipsis);
  std::size_t visibleCount = actions.size();
  if (totalWidth > availableActionWidth) {
    visibleCount = 0;
    float usedWidth = 0.0f;
    for (std::size_t index = 0; index < actions.size(); ++index) {
      const auto remainingActions = actions.size() - (index + 1);
      const auto width = widths[index];
      const auto leadingSpacing = visibleCount > 0 ? spacingX : 0.0f;
      const auto overflowReserve =
          remainingActions > 0
              ? (visibleCount > 0 || index > 0 ? spacingX : 0.0f) +
                    moreButtonWidth
              : 0.0f;

      if (usedWidth + leadingSpacing + width + overflowReserve <=
          availableActionWidth) {
        usedWidth += leadingSpacing + width;
        ++visibleCount;
        continue;
      }

      break;
    }
  }

  const auto drawToolbarAction = [&](const WorkbenchToolbarAction &a_action,
                                     const std::string_view a_tooltipId) {
    if (!a_action.enabled) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(a_action.label.data())) {
      a_action.callback();
    }
    if (!a_action.enabled) {
      ImGui::EndDisabled();
    }
    if (a_action.tooltip) {
      ui::workbench::DrawSimplePinnableTooltip(
          a_tooltipId,
          ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                               ImGuiHoveredFlags_AllowWhenDisabled),
          a_action.tooltip);
    }
  };

  for (std::size_t index = 0; index < visibleCount; ++index) {
    if (index > 0) {
      ImGui::SameLine();
    }
    drawToolbarAction(actions[index],
                      "workbench:toolbar:" + std::to_string(index));
  }

  if (visibleCount < actions.size()) {
    if (visibleCount > 0) {
      ImGui::SameLine();
    }
    if (ImGui::Button(ui::workbench::kIconEllipsis)) {
      ImGui::OpenPopup(ui::workbench::kWorkbenchOverflowPopupId);
    }
    if (ImGui::BeginPopup(ui::workbench::kWorkbenchOverflowPopupId)) {
      for (std::size_t index = visibleCount; index < actions.size(); ++index) {
        const auto &action = actions[index];
        if (ImGui::MenuItem(action.label.data(), nullptr, false,
                            action.enabled)) {
          action.callback();
          ImGui::CloseCurrentPopup();
        }
        if (action.tooltip) {
          ui::workbench::DrawSimplePinnableTooltip(
              "workbench:toolbar:overflow:" + std::to_string(index),
              ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                                   ImGuiHoveredFlags_AllowWhenDisabled),
              action.tooltip);
        }
      }
      ImGui::EndPopup();
    }
  }
  if (!actions.empty()) {
    ImGui::SameLine();
  }
  const bool globallyHideRealEquipment =
      previewActor != nullptr &&
      HideRealEquipmentWithFittingForActor(previewActor);
  const bool allActualIndividuallyHidden =
      !controllableActualRows.empty() &&
      individuallyHiddenActualCount ==
          static_cast<int>(controllableActualRows.size());
  const bool mixedActualVisibility = !globallyHideRealEquipment &&
                                     individuallyHiddenActualCount > 0 &&
                                     !allActualIndividuallyHidden;
  bool hideRealEquipment =
      globallyHideRealEquipment || allActualIndividuallyHidden;
  ImGui::BeginDisabled(controllableActualRows.empty());
  if (mixedActualVisibility) {
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
  }
  if (ImGui::Checkbox(hideRealEquipmentLabel.c_str(), &hideRealEquipment)) {
    if (previewActor != nullptr) {
      sfs::devious_devices::ClearDeviousDevicesRenderedDeviceOrdinaryVisibility(
          previewActor->GetFormID());
      SetHideRealEquipmentWithFittingForActor(previewActor, hideRealEquipment);
      const auto actorFormID = previewActor->GetFormID();
      for (const auto rowIndex : controllableActualRows) {
        static_cast<void>(workbench_.SetEquippedHiddenForActor(
            actorFormID, rowIndex, hideRealEquipment));
      }
      workbench_.RefreshNativeArmorOverridesForActor(actorFormID,
                                                     conditionStore_.revision);
    }
  }
  if (mixedActualVisibility) {
    ImGui::PopItemFlag();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();

  const bool globallyHideFittingAppearances =
      previewActor != nullptr && HideFittingOverridesForActor(previewActor);
  const bool allOverridesIndividuallyHidden =
      !controllableOverrides.empty() &&
      individuallyHiddenOverrideCount ==
          static_cast<int>(controllableOverrides.size());
  const bool mixedOverrideVisibility = !globallyHideFittingAppearances &&
                                       individuallyHiddenOverrideCount > 0 &&
                                       !allOverridesIndividuallyHidden;
  bool hideFittingAppearances =
      globallyHideFittingAppearances || allOverridesIndividuallyHidden;
  ImGui::BeginDisabled(controllableOverrides.empty());
  if (mixedOverrideVisibility) {
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
  }
  if (ImGui::Checkbox(hideFittingAppearancesLabel.c_str(),
                      &hideFittingAppearances)) {
    if (previewActor != nullptr) {
      SetHideFittingOverridesForActor(previewActor, hideFittingAppearances);
      for (const auto &[rowIndex, itemIndex] : controllableOverrides) {
        const auto &rows = workbench_.GetRows();
        if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size()) ||
            itemIndex < 0 ||
            itemIndex >= static_cast<int>(
                             rows[static_cast<std::size_t>(rowIndex)]
                                 .overrides.size())) {
          continue;
        }
        const auto &row = rows[static_cast<std::size_t>(rowIndex)];
        const auto &item = row.overrides[static_cast<std::size_t>(itemIndex)];
        const auto overrideSlotMask = row.GetOverrideVisualSlotMask(item);
        const bool headgearToggleHidden =
            !item.locked &&
            (overrideSlotMask & headgearToggleSuppressedFittingSlotMask) != 0;
        if (!hideFittingAppearances && headgearToggleHidden) {
          // A global show is a temporary user override for Helmet Toggle.
          // It is also an explicit request to make every saved appearance
          // visible. Clear a stale saved eye-hide before returning; otherwise
          // the HT2 show transition removes the temporary override and the
          // same card becomes permanently hidden again.
          static_cast<void>(
              workbench_.SetOverrideHeadgearToggleManualVisible(
                  rowIndex, itemIndex, true));
          if (item.hidden) {
            static_cast<void>(
                workbench_.SetOverrideHidden(rowIndex, itemIndex, false));
          }
          continue;
        }
        if (hideFittingAppearances) {
          static_cast<void>(
              workbench_.SetOverrideHeadgearToggleManualVisible(
                  rowIndex, itemIndex, false));
        }
        // A global show request has the same authority as clicking every eye
        // individually.  In Vanilla/Direct-Vanilla mode this records the
        // per-item user-visible override while an external strip is active;
        // SetOverrideHidden also releases ModSettings tickets and DD latches.
        static_cast<void>(
            workbench_.SetOverrideAutomaticEquipmentUserVisible(
                rowIndex, itemIndex, !hideFittingAppearances));
        static_cast<void>(workbench_.SetOverrideHidden(rowIndex, itemIndex,
                                                       hideFittingAppearances));
      }
      workbench_.RefreshNativeArmorOverridesForActor(previewActor->GetFormID(),
                                                     conditionStore_.revision);
    }
  }
  if (mixedOverrideVisibility) {
    ImGui::PopItemFlag();
  }
  ImGui::EndDisabled();
  ImGui::Spacing();
}

} // namespace sfs
