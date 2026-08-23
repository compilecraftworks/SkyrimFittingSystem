#include "Menu.h"

#include "ArmorUtils.h"
#include "imgui_internal.h"
#include "ui/Localization.h"
#include "workbench/ItemFactory.h"
#include "workbench/AppearanceSlotProtection.h"

#include <algorithm>
#include <format>
#include <vector>

namespace {
enum class GearColumn : ImGuiID { Name = 1, Plugin };

void ScrollCurrentTableRowIntoView(const float a_centerRatio = 0.45F) {
  const auto *table = ImGui::GetCurrentTable();
  if (table == nullptr || table->InnerWindow == nullptr ||
      table->RowPosY2 <= table->RowPosY1) {
    return;
  }
  const auto rowCenter =
      ImLerp(table->RowPosY1, table->RowPosY2, a_centerRatio);
  ImGui::SetScrollFromPosY(table->InnerWindow,
                            rowCenter - table->InnerWindow->Pos.y,
                            a_centerRatio);
}
}

namespace sfs {
bool Menu::DrawGearTab() {
  auto *localization = ui::Localization::GetSingleton();
  const auto &rows = GetFilteredGearRows();
  const auto resultCount = rows.size();
  const auto equippedRowCount = workbench_.GetRowCount();
  const auto resultsLabel = sfs::strings::SafeVFormat(
      std::string(localization->Get("catalog.results")),
      std::make_format_args(resultCount));
  const auto equippedRowsLabel = sfs::strings::SafeVFormat(
      std::string(localization->Get("catalog.equipped_rows")),
      std::make_format_args(equippedRowCount));
  ImGui::TextUnformatted(resultsLabel.c_str());
  ImGui::SameLine();
  ImGui::TextUnformatted(equippedRowsLabel.c_str());
  return DrawGearCatalogTable();
}

bool Menu::DrawGearCatalogTable() {
  auto *localization = ui::Localization::GetSingleton();
  auto &browser = CatalogBrowserState();
  bool rowClicked = false;
  if (ImGui::BeginTable("##gear-table", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    const auto nameLabel = localization->Get("common.name");
    const auto pluginLabel = localization->Get("common.plugin");
    const auto removeFavoriteLabel = localization->Get("favorites.remove");
    const auto addFavoriteLabel = localization->Get("favorites.add");
    const auto applyGearLabel = localization->Get("workbench.apply_gear");
    ImGui::TableSetupColumn(nameLabel.data(), ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Name));
    ImGui::TableSetupColumn(pluginLabel.data(), ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Plugin));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    const auto &rows = GetSortedGearRows(ImGui::TableGetSortSpecs());
    const auto findRowIndex = [&](const std::string_view a_key) {
      if (a_key.empty()) {
        return -1;
      }
      for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        if (rows[rowIndex]->id == a_key) {
          return static_cast<int>(rowIndex);
        }
      }
      return -1;
    };

    int selectedRowIndex = findRowIndex(browser.selectedKey);
    if (selectedRowIndex < 0 && !browser.selectedGearKeys.empty()) {
      selectedRowIndex = findRowIndex(browser.selectedGearKeys.back());
    }
    int moveDelta = 0;
    bool applySelected = false;
    bool previewSelected = false;
    ConsumeKitListCommands(moveDelta, applySelected, previewSelected);
    int requestedScrollRowIndex = -1;

    // InputManager owns keyboard/gamepad list commands. Reading ImGui's
    // physical key state here as well made one arrow press move two rows.
    moveDelta = std::clamp(moveDelta, -1, 1);

    const auto isSelectedGearKey = [&](const std::string_view a_key) {
      return std::ranges::find(browser.selectedGearKeys, a_key) !=
             browser.selectedGearKeys.end();
    };
    const auto buildSelectedGearEntries = [&]() {
      std::vector<const GearEntry *> selectedEntries;
      selectedEntries.reserve(browser.selectedGearKeys.size());
      const auto &catalogEntries = EquipmentCatalog::Get().GetGear();
      for (const auto &selectedKey : browser.selectedGearKeys) {
        const auto selectedIt =
            std::ranges::find(catalogEntries, selectedKey, &GearEntry::id);
        if (selectedIt != catalogEntries.end()) {
          selectedEntries.push_back(&*selectedIt);
        }
      }
      return selectedEntries;
    };
    const auto buildSelectedGearFormIDs = [&]() {
      std::vector<RE::FormID> formIDs;
      const auto selectedEntries = buildSelectedGearEntries();
      formIDs.reserve(selectedEntries.size());
      for (const auto *selectedEntry : selectedEntries) {
        formIDs.push_back(selectedEntry->formID);
      }
      return formIDs;
    };
    const auto previewSelectedGear = [&]() {
      if (!browser.previewSelected || browser.selectedGearKeys.empty()) {
        workbench_.ClearPreview();
        return;
      }

      const auto selectedEntries = buildSelectedGearEntries();
      if (selectedEntries.size() == 1) {
        PreviewGearEntry(*selectedEntries.front());
        return;
      }

      workbench_.ClearPreview();
      (void)PreviewCatalogSelectionAsFittingOverrides(
          "gear:multi-selection", buildSelectedGearFormIDs());
    };
    const auto selectedGearSupportsAppearance = [&]() {
      const auto selectedEntries = buildSelectedGearEntries();
      return std::ranges::any_of(selectedEntries, [](const auto *a_entry) {
               workbench::EquipmentWidgetItem selectedItem{};
               const auto *armorForm =
                   RE::TESForm::LookupByID<RE::TESObjectARMO>(a_entry->formID);
               return armorForm != nullptr &&
                      workbench::BuildCatalogItem(a_entry->formID,
                                                  selectedItem) &&
                      selectedItem.SupportsArmorReplacement() &&
                      !workbench::IsAppearanceRegistrationProtectedSlotMask(
                          armor::GetArmorDisplaySlotMask(armorForm));
             });
    };

    if (!rows.empty()) {
      const auto previewRequested =
          previewSelected || moveDelta != 0;
      if (selectedRowIndex < 0) {
        selectedRowIndex = 0;
        browser.selectedKey = rows.front()->id;
        browser.selectedGearKeys = {rows.front()->id};
        if (previewRequested) {
          previewSelectedGear();
        }
        requestedScrollRowIndex = 0;
      }

      if (moveDelta != 0) {
        selectedRowIndex = std::clamp(
            selectedRowIndex + moveDelta, 0, static_cast<int>(rows.size()) - 1);
        const auto &entry = *rows[static_cast<std::size_t>(selectedRowIndex)];
        browser.selectedGearKeys = {entry.id};
        browser.selectedKey = entry.id;
        requestedScrollRowIndex = selectedRowIndex;
        if (previewRequested) {
          previewSelectedGear();
        }
      }

      if (selectedRowIndex >= 0 &&
          selectedRowIndex < static_cast<int>(rows.size())) {
        const auto &entry = *rows[static_cast<std::size_t>(selectedRowIndex)];
        if (applySelected) {
          rowClicked = true;
          AddGearEntryToWorkbench(entry);
        }
      }
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    if (requestedScrollRowIndex >= 0) {
      clipper.IncludeItemByIndex(requestedScrollRowIndex);
    }
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &entry = *rows[static_cast<std::size_t>(rowIndex)];
        const auto favorite =
            IsFavorite(ui::catalog::BrowserTab::Gear, entry.id);
        workbench::EquipmentWidgetItem item{};
        if (!workbench::BuildCatalogItem(entry.formID, item)) {
          item.formID = entry.formID;
          item.key = "catalog:" + entry.id;
          item.name = entry.name;
        }
        const auto *armorForm =
            RE::TESForm::LookupByID<RE::TESObjectARMO>(entry.formID);
        const auto supportsArmorReplacement =
            item.SupportsArmorReplacement() && armorForm != nullptr &&
            !workbench::IsAppearanceRegistrationProtectedSlotMask(
                armor::GetArmorDisplaySlotMask(armorForm));
        item.name = BuildFavoriteLabel(item.name, favorite);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected = isSelectedGearKey(entry.id);
        ImGui::Selectable(
            ("##catalog-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          if (!isSelectedGearKey(entry.id)) {
            browser.selectedGearKeys.assign(1, entry.id);
            browser.selectedKey = entry.id;
            previewSelectedGear();
          }
          const auto selectedFormIDs = buildSelectedGearFormIDs();
          const auto selectedEntries = buildSelectedGearEntries();
          const bool canApplySelectedGear = selectedGearSupportsAppearance();
          const auto favoriteLabel =
              favorite ? removeFavoriteLabel.data() : addFavoriteLabel.data();
          if (ImGui::MenuItem(favoriteLabel)) {
            SetFavorite(ui::catalog::BrowserTab::Gear, entry.id, !favorite);
          }
          ImGui::Separator();
          ImGui::BeginDisabled(!canApplySelectedGear);
          if (ImGui::MenuItem(applyGearLabel.data())) {
            (void)ApplyCatalogSelectionAsFittingOverrides(selectedFormIDs,
                                                          false);
          }
          const auto selectionName =
              selectedEntries.size() > 1
                  ? std::format("{} (+{})", selectedEntries.front()->name,
                                selectedEntries.size() - 1)
                  : entry.name;
          DrawApplyWithConditionMenu(
              {.name = selectionName, .formIDs = selectedFormIDs,
               .replaceConditionSet = false});
          ImGui::EndDisabled();
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);
        const auto widgetResult = supportsArmorReplacement
                                      ? DrawCatalogDragWidget(
                                            item, DragSourceKind::Catalog)
                                      : ui::components::DrawEquipmentWidget(
                                            item.key.c_str(), item,
                                            {.disabledAppearance = true});

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry.plugin.data());

        if (selected) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.40f));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("TABLE_HOVER", 0.12f));
        }

        const bool doubleClicked =
            (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ||
            widgetResult.doubleClicked;
        const bool releasedOnRow =
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            (rowHovered || widgetResult.hovered);
        if (doubleClicked) {
          rowClicked = true;
          if (supportsArmorReplacement) {
            AddGearEntryToWorkbench(entry);
          }
        } else if (releasedOnRow) {
          rowClicked = true;
          if (ImGui::GetIO().KeyCtrl) {
            const auto selectedIt = std::ranges::find(
                browser.selectedGearKeys, entry.id);
            if (selectedIt != browser.selectedGearKeys.end()) {
              browser.selectedGearKeys.erase(selectedIt);
            } else {
              browser.selectedGearKeys.push_back(entry.id);
            }

            if (browser.selectedGearKeys.empty()) {
              ClearCatalogSelection();
            } else {
              browser.selectedKey = browser.selectedGearKeys.back();
              previewSelectedGear();
            }
          } else if (selected && browser.selectedGearKeys.size() == 1) {
            ClearCatalogSelection();
          } else {
            browser.selectedGearKeys.assign(1, entry.id);
            CatalogBrowserState().selectedKey = entry.id;
            if (browser.previewSelected) {
              PreviewGearEntry(entry);
            } else {
              workbench_.ClearPreview();
            }
          }
        }
        if (rowIndex == requestedScrollRowIndex) {
          ScrollCurrentTableRowIntoView();
        }
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}
} // namespace sfs
