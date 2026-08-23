#include "Menu.h"

#include "imgui_internal.h"
#include "ui/Localization.h"
#include "ui/catalog/Widgets.h"
#include "ui/components/PinnableTooltip.h"

#include <algorithm>
#include <format>

namespace {
enum class OutfitColumn : ImGuiID { Name = 1, Plugin, Pieces };

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
bool Menu::DrawOutfitTab() {
  auto *localization = ui::Localization::GetSingleton();
  auto &browser = CatalogBrowserState();
  const auto &rows = GetFilteredOutfitRows();
  const auto resultCount = rows.size();
  const auto resultsLabel = sfs::strings::SafeVFormat(
      std::string(localization->Get("catalog.results")),
      std::make_format_args(resultCount));
  ImGui::TextUnformatted(resultsLabel.c_str());
  bool rowClicked = false;

  const auto tableHeight = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginTable("##outfit-table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, tableHeight))) {
    const auto outfitLabel = localization->Get("common.outfit");
    const auto pluginLabel = localization->Get("common.plugin");
    const auto piecesLabel = localization->Get("common.pieces");
    const auto removeFavoriteLabel = localization->Get("favorites.remove");
    const auto addFavoriteLabel = localization->Get("favorites.add");
    const auto applyOutfitLabel =
        localization->Get("workbench.apply_outfit_set");
    ImGui::TableSetupColumn(outfitLabel.data(), ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Name));
    ImGui::TableSetupColumn(pluginLabel.data(), ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Plugin));
    ImGui::TableSetupColumn(piecesLabel.data(),
                            ImGuiTableColumnFlags_PreferSortDescending, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Pieces));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    const auto &sortedRows = GetSortedOutfitRows(ImGui::TableGetSortSpecs());
    const auto findRowIndex = [&](const std::string_view a_key) {
      if (a_key.empty()) {
        return -1;
      }
      for (std::size_t rowIndex = 0; rowIndex < sortedRows.size(); ++rowIndex) {
        if (sortedRows[rowIndex]->id == a_key) {
          return static_cast<int>(rowIndex);
        }
      }
      return -1;
    };
    int selectedRowIndex = findRowIndex(browser.selectedKey);
    int moveDelta = 0;
    bool applySelected = false;
    bool previewSelected = false;
    ConsumeKitListCommands(moveDelta, applySelected, previewSelected);
    int requestedScrollRowIndex = -1;
    // InputManager owns keyboard/gamepad list commands. Reading ImGui's
    // physical key state here as well made one arrow press move two rows.
    moveDelta = std::clamp(moveDelta, -1, 1);

    const auto previewSelectedOutfit = [&]() {
      if (!browser.previewSelected) {
        workbench_.ClearPreview();
        return;
      }
      if (selectedRowIndex < 0 ||
          selectedRowIndex >= static_cast<int>(sortedRows.size())) {
        return;
      }
      const auto &outfit = *sortedRows[static_cast<std::size_t>(selectedRowIndex)];
      PreviewOutfitEntry(outfit);
    };

    if (!sortedRows.empty()) {
      const auto previewRequested = previewSelected || moveDelta != 0;
      if (selectedRowIndex < 0) {
        selectedRowIndex = 0;
        browser.selectedKey = sortedRows.front()->id;
        requestedScrollRowIndex = 0;
        if (previewRequested) {
          previewSelectedOutfit();
        }
      }

      if (moveDelta != 0) {
        selectedRowIndex = std::clamp(
            selectedRowIndex + moveDelta, 0,
            static_cast<int>(sortedRows.size()) - 1);
        browser.selectedKey = sortedRows[static_cast<std::size_t>(selectedRowIndex)]->id;
        requestedScrollRowIndex = selectedRowIndex;
        if (previewRequested) {
          previewSelectedOutfit();
        }
      }

      if (selectedRowIndex >= 0 &&
          selectedRowIndex < static_cast<int>(sortedRows.size()) &&
          applySelected) {
        const auto &outfit = *sortedRows[static_cast<std::size_t>(selectedRowIndex)];
        rowClicked = true;
        AddOutfitEntryToWorkbench(outfit);
      }
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(sortedRows.size()));
    if (requestedScrollRowIndex >= 0) {
      clipper.IncludeItemByIndex(requestedScrollRowIndex);
    }
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &outfit = *sortedRows[static_cast<std::size_t>(rowIndex)];
        const auto favorite =
            IsFavorite(ui::catalog::BrowserTab::Outfits, outfit.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected = browser.selectedKey == outfit.id &&
                              (!browser.previewSelected ||
                               workbench_.IsPreviewingSelection(outfit.id));
        ImGui::Selectable(
            ("##outfit-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          const auto favoriteLabel =
              favorite ? removeFavoriteLabel.data() : addFavoriteLabel.data();
          if (ImGui::MenuItem(favoriteLabel)) {
            SetFavorite(ui::catalog::BrowserTab::Outfits, outfit.id, !favorite);
          }
          ImGui::Separator();
          if (ImGui::MenuItem(applyOutfitLabel.data())) {
            AddOutfitEntryToWorkbench(outfit);
          }
          DrawApplyWithConditionMenu(
              {.name = outfit.name,
               .formIDs = BuildOutfitFittingFormIDs(outfit),
               .replaceConditionSet = true});
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);

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
            rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const bool releasedOnRow =
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) && rowHovered;
        if (doubleClicked) {
          rowClicked = true;
          AddOutfitEntryToWorkbench(outfit);
        } else if (releasedOnRow) {
          rowClicked = true;
          if (selected) {
            ClearCatalogSelection();
          } else {
            CatalogBrowserState().selectedKey = outfit.id;
            if (browser.previewSelected) {
              PreviewOutfitEntry(outfit);
            } else {
              workbench_.ClearPreview();
            }
          }
        }
        if (!ImGui::IsDragDropActive() &&
            ui::components::ShouldDrawPinnableTooltip("outfit:" + outfit.id,
                                                      rowHovered)) {
          ui::catalog::DrawOutfitTooltip(outfit, rowHovered);
        }

        const auto displayName = BuildFavoriteLabel(outfit.name, favorite);
        ImGui::TextUnformatted(displayName.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(outfit.plugin.data());

        ImGui::TableSetColumnIndex(2);
        {
          const auto availableWidth = ImGui::GetContentRegionAvail().x;
          const auto displayText = ui::catalog::TruncateTextToWidth(
              outfit.GetPiecesText(), availableWidth);
          ImGui::TextUnformatted(displayText.c_str());
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
