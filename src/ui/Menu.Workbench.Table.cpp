#include "Menu.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "StringUtils.h"
#include "conditions/Defaults.h"
#include "conditions/Status.h"
#include "conditions/Validation.h"
#include "imgui_internal.h"
#include "native/ArmorSkinning.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/FittingSlotState.h"
#include "poc/DeviousDevicesHiderPoC.h"
#include "poc/VirtualWornTokenPoC.h"
#include "ui/InputWidgets.h"
#include "ui/Localization.h"
#include "ui/WorkbenchConflicts.h"
#include "ui/catalog/Widgets.h"
#include "ui/components/EquipmentWidget.h"
#include "ui/workbench/Common.h"
#include "ui/workbench/Tooltips.h"
#include "workbench/AppearanceSlotProtection.h"
#include "workbench/ItemFactory.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace sfs {
namespace {
constexpr std::string_view kWorkbenchTrashIcon = "\xee\x86\x8c";
constexpr std::string_view kWorkbenchActualSlotIcon =
    "\xee\x85\x9b"; // ICON_LC_SHIELD
constexpr std::string_view kWorkbenchFittingSlotIcon =
    "\xee\x87\x89"; // ICON_LC_SHIRT

ImU32 ConditionSurfaceColor(const int a_red, const int a_green,
                            const int a_blue, const int a_alpha = 255) {
  constexpr float kChannelScale = 1.0f / 255.0f;
  return ImGui::GetColorU32(
      ImVec4(static_cast<float>(a_red) * kChannelScale,
             static_cast<float>(a_green) * kChannelScale,
             static_cast<float>(a_blue) * kChannelScale,
             static_cast<float>(a_alpha) * kChannelScale));
}

constexpr float kEmptyConditionCardRounding = 8.0f;

ImU32 EmptyConditionCardSurfaceColor(const bool a_hovered) {
  return a_hovered ? ConditionSurfaceColor(18, 18, 20)
                   : ConditionSurfaceColor(10, 10, 12);
}

ImU32 EmptyConditionCardBorderColor(const ThemeConfig *a_theme) {
  return a_theme->GetColorU32("TEXT_DISABLED", 0.55f);
}

std::vector<std::uint64_t>
SplitWorkbenchSlotMask(const std::uint64_t a_slotMask) {
  std::vector<std::uint64_t> slotMasks;
  for (const auto slotMask : armor::GetAllArmorSlotMasks()) {
    if ((a_slotMask & slotMask) != 0) {
      slotMasks.push_back(slotMask);
    }
  }
  return slotMasks;
}

std::string BuildMultilineWorkbenchSlotText(const std::uint64_t a_slotMask) {
  std::string text;
  for (const auto slotMask : SplitWorkbenchSlotMask(a_slotMask)) {
    const auto labels = armor::GetArmorSlotLabels(slotMask);
    if (labels.empty()) {
      continue;
    }
    if (!text.empty()) {
      text.push_back('\n');
    }
    text.append(labels.front());
  }
  return text;
}

std::uint64_t ResolveWorkbenchItemDisplaySlotMask(
    const workbench::EquipmentWidgetItem &a_item) {
  const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_item.formID);
  if (armor != nullptr) {
    const auto displaySlotMask = armor::GetArmorDisplaySlotMask(armor);
    if (displaySlotMask != 0) {
      return displaySlotMask;
    }
  }
  return a_item.slotMask;
}

int FindWorkbenchSlotLineIndex(const std::uint64_t a_slotMask,
                               const std::uint64_t a_targetSlotMask) {
  int lineIndex = 0;
  for (const auto slotMask : SplitWorkbenchSlotMask(a_slotMask)) {
    if ((slotMask & a_targetSlotMask) != 0) {
      return lineIndex;
    }
    ++lineIndex;
  }
  return -1;
}
} // namespace

void Menu::DrawWorkbenchSortableHeader(const char *a_label, const char *a_id,
                                       WorkbenchSortState &a_state,
                                       const WorkbenchSortColumn a_column) {
  std::string headerLabel(a_label != nullptr ? a_label : "");
  if (a_state.column == a_column) {
    headerLabel.append(a_state.ascending ? "  \xe2\x96\xb2" : "  \xe2\x96\xbc");
  }
  headerLabel.append("###");
  headerLabel.append(a_id != nullptr ? a_id : "workbench-sort-header");

  ImGui::TableHeader(headerLabel.c_str());
  if (ImGui::IsDragDropActive() ||
      !ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    return;
  }

  if (a_state.column != a_column) {
    a_state.column = a_column;
    a_state.ascending = true;
  } else if (a_state.ascending) {
    a_state.ascending = false;
  } else {
    a_state = {};
  }
}

void Menu::OpenSlotCreationRow() {
  PendingSlotCreationState state{};
  state.active = true;
  state.conditionId.reset();
  state.ownerActorFormID = ResolveNewWorkbenchRowOwnerActorFormID();

  pendingSlotCreations_.push_back(std::move(state));
}

bool Menu::TryCommitSlotCreationRow(const std::size_t a_index) {
  if (a_index >= pendingSlotCreations_.size()) {
    return false;
  }

  const auto pending = pendingSlotCreations_[a_index];
  if (!pending.active ||
      (!pending.conditionId.has_value() && pending.overrideFormID == 0 &&
       pending.visibilityTargetFormID == 0)) {
    return false;
  }

  const auto conditionId = pending.conditionId.value_or(std::string{});
  const auto *condition = conditionId.empty()
                              ? nullptr
                              : conditions::FindDefinitionById(
                                    ConditionDefinitions(), conditionId);
  if (!conditionId.empty() &&
      (condition == nullptr || !IsWorkbenchSelectableCondition(*condition))) {
    return false;
  }

  if (pending.visibilityTargetKind.has_value() ||
      pending.visibilityTargetFormID != 0 || pending.overrideFormID == 0) {
    const bool committed = workbench_.AddConditionalVisibilityRule(
        conditionId, pending.ownerActorFormID,
        pending.visibilityTargetKind.value_or(
            workbench::ConditionalVisibilityTargetKind::Fitting),
        pending.visibilityTargetFormID,
        pending.visibleWhenTrue);
    if (committed) {
      pendingSlotCreations_.erase(pendingSlotCreations_.begin() +
                                  static_cast<std::ptrdiff_t>(a_index));
    }
    return committed;
  }

  const auto layout =
      BuildSlotFallbackLayoutFromArmorForms({pending.overrideFormID});
  if (!layout.has_value()) {
    return false;
  }

  const auto rowConditionId = conditionId.empty()
                                  ? std::optional<std::string>{std::string{}}
                                  : pending.conditionId;
  const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
  workbench_.ClearPreview();

  std::vector<int> candidateRows;
  candidateRows.reserve(layout->rows.size());
  for (const auto &layoutRow : layout->rows) {
    if (layoutRow.targetSlotMask == 0) {
      continue;
    }

    (void)workbench_.AddSlotRow(layoutRow.targetSlotMask, rowConditionId,
                                pending.ownerActorFormID,
                                &initialEquippedState);
    const auto &rows = workbench_.GetRows();
    const auto rowIt = std::ranges::find_if(rows, [&](const auto &a_row) {
      return a_row.IsSlotRow() &&
             a_row.equipped.slotMask == layoutRow.targetSlotMask &&
             a_row.conditionId == rowConditionId &&
             a_row.ownerActorFormID == pending.ownerActorFormID;
    });
    if (rowIt == rows.end()) {
      continue;
    }

    candidateRows.push_back(
        static_cast<int>(std::distance(rows.begin(), rowIt)));
  }

  bool committed = false;
  if (!candidateRows.empty()) {
    // A registered appearance is one atomic item even when it occupies more
    // than one original armor slot. Remove every overlapping item from this
    // actor/condition layer before placing the replacement into its logical
    // anchor row. Rows from other conditions and the base layer are untouched.
    std::vector<int> layerRows;
    const auto &currentRows = workbench_.GetRows();
    layerRows.reserve(currentRows.size());
    for (int rowIndex = 0; rowIndex < static_cast<int>(currentRows.size());
         ++rowIndex) {
      const auto &row = currentRows[static_cast<std::size_t>(rowIndex)];
      if (row.ownerActorFormID == pending.ownerActorFormID &&
          row.conditionId == rowConditionId) {
        layerRows.push_back(rowIndex);
      }
    }
    static_cast<void>(workbench_.RemoveOverridesOverlappingCatalogSelection(
        {pending.overrideFormID}, &layerRows));
    committed = workbench_.ApplyKitLayout(
        *layout, true, rowConditionId, pending.ownerActorFormID,
        &initialEquippedState, &candidateRows);
  }

  if (!committed) {
    return false;
  }

  if (!pending.visibleWhenTrue) {
    const auto &rows = workbench_.GetRows();
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
      const auto &row = rows[rowIndex];
      if (row.conditionId != rowConditionId ||
          row.ownerActorFormID != pending.ownerActorFormID) {
        continue;
      }
      for (std::size_t itemIndex = 0; itemIndex < row.overrides.size();
           ++itemIndex) {
        if (row.overrides[itemIndex].formID == pending.overrideFormID) {
          static_cast<void>(workbench_.SetOverrideHidden(
              static_cast<int>(rowIndex), static_cast<int>(itemIndex), true));
        }
      }
    }
  }

  pendingSlotCreations_.erase(pendingSlotCreations_.begin() +
                              static_cast<std::ptrdiff_t>(a_index));
  return committed;
}

bool Menu::DrawSlotCreationRow(const bool a_drawConditionSectionHeader,
                               const float a_stickyHeaderBottomY) {
  auto *localization = ui::Localization::GetSingleton();
  const auto *table = ImGui::GetCurrentTable();
  if (table == nullptr) {
    return false;
  }

  constexpr bool applicationLocked = false;
  const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
  const auto rowHeight = widgetHeight + ui::workbench::kWorkbenchRowGapY;
  const auto cellPadding = ImGui::GetStyle().CellPadding;
  const auto *theme = ThemeConfig::GetSingleton();
  bool conditionHeaderReachedStickyRow = false;

  if (a_drawConditionSectionHeader) {
    ImGui::PushID("slot-creation-condition-header");
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers,
                        ImGui::TableGetHeaderRowHeight());
    ImGui::TableSetColumnIndex(0);
    DrawWorkbenchSortableHeader(
        localization->GetCStr("workbench.condition_setup"),
        "slot-creation-condition-sort", workbenchConditionalSort_,
        WorkbenchSortColumn::Left);
    ImGui::TableSetColumnIndex(1);
    DrawWorkbenchSortableHeader(
        localization->GetCStr("workbench.action_setup"),
        "slot-creation-action-sort", workbenchConditionalSort_,
        WorkbenchSortColumn::Right);
    conditionHeaderReachedStickyRow =
        ImGui::TableGetCellBgRect(table, 0).Min.y <=
        a_stickyHeaderBottomY + 0.5f;
    ImGui::PopID();
  }

  const auto drawPrompt = [&](const ImRect &a_cardRect,
                              const std::string_view a_text) {
    const std::string text(a_text);
    constexpr float textPaddingX = 10.0f;
    const auto availableWidth =
        (std::max)(1.0f, a_cardRect.GetWidth() - (textPaddingX * 2.0f));
    const auto textSize =
        ImGui::CalcTextSize(text.c_str(), nullptr, false, availableWidth);
    const auto textPosition = ImVec2(
        a_cardRect.Min.x + textPaddingX,
        a_cardRect.Min.y + ((a_cardRect.GetHeight() - textSize.y) * 0.5f));
    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(a_cardRect.Min, a_cardRect.Max, true);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPosition,
                      theme->GetColorU32("TEXT_DISABLED"), text.c_str(),
                      nullptr, availableWidth);
    drawList->PopClipRect();
  };

  const auto drawConditionCell = [&](PendingSlotCreationState &a_state,
                                     const ImRect &a_cellRect) {
    bool changed = false;
    const auto contentMin = ImVec2(a_cellRect.Min.x + cellPadding.x,
                                   a_cellRect.Min.y + cellPadding.y);
    const auto contentMax = ImVec2(a_cellRect.Max.x - cellPadding.x,
                                   a_cellRect.Max.y - cellPadding.y);
    const auto contentSize =
        ImVec2((std::max)(1.0f, contentMax.x - contentMin.x),
               (std::max)(1.0f, contentMax.y - contentMin.y));
    ImGui::SetCursorScreenPos(contentMin);
    ImGui::InvisibleButton("##slot-create-condition-cell", contentSize);
    const ImRect cardRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

    const auto conditionIt =
        a_state.conditionId.has_value()
            ? std::ranges::find_if(ConditionDefinitions(),
                                   [&](const auto &a_condition) {
                                     return a_condition.id ==
                                            *a_state.conditionId;
                                   })
            : ConditionDefinitions().end();
    const bool hasCondition = conditionIt != ConditionDefinitions().end();
    const auto *themeConfig = ThemeConfig::GetSingleton();
    auto *drawList = ImGui::GetWindowDrawList();
    const auto bodyColor =
        hasCondition
            ? (ImGui::IsItemHovered() ? ConditionSurfaceColor(42, 42, 44)
                                      : ConditionSurfaceColor(34, 34, 36))
            : EmptyConditionCardSurfaceColor(ImGui::IsItemHovered());
    const auto borderColor =
        hasCondition ? ImGui::GetColorU32(ImGuiCol_Border)
                     : EmptyConditionCardBorderColor(themeConfig);
    const auto rounding =
        hasCondition ? 4.0f : kEmptyConditionCardRounding;
    drawList->AddRectFilled(cardRect.Min, cardRect.Max, bodyColor, rounding);
    drawList->AddRect(cardRect.Min, cardRect.Max, borderColor, rounding);

    constexpr float deletePaneWidth = 34.0f;
    const auto deleteMin =
        ImVec2(cardRect.Max.x - deletePaneWidth, cardRect.Min.y);
    const auto deleteMax = cardRect.Max;
    ui::input_widgets::RectClickTargetState deleteState{};
    if (hasCondition) {
      deleteState = ui::input_widgets::EvaluateRectClickTarget(
          ImGui::GetID("##slot-create-condition-delete"), deleteMin, deleteMax);
      const auto deleteFill =
          applicationLocked  ? themeConfig->GetColorU32("TEXT_DISABLED", 0.26f)
          : deleteState.held ? themeConfig->GetColorU32("DECLINE")
          : deleteState.hovered ? themeConfig->GetColorU32("DECLINE", 0.95f)
                                : themeConfig->GetColorU32("DECLINE", 0.78f);
      drawList->AddRectFilled(deleteMin, deleteMax, deleteFill, 4.0f,
                              ImDrawFlags_RoundCornersRight);
      drawList->AddLine(ImVec2(deleteMin.x, cardRect.Min.y + 1.0f),
                        ImVec2(deleteMin.x, cardRect.Max.y - 1.0f),
                        themeConfig->GetColorU32("BORDER"));
      const auto iconSize = ImGui::CalcTextSize(kWorkbenchTrashIcon.data());
      drawList->AddText(
          ImVec2(deleteMin.x +
                     (((deleteMax.x - deleteMin.x) - iconSize.x) * 0.5f),
                 cardRect.Min.y + ((cardRect.GetHeight() - iconSize.y) * 0.5f)),
          applicationLocked ? themeConfig->GetColorU32("TEXT_DISABLED")
                            : themeConfig->GetColorU32("TEXT"),
          kWorkbenchTrashIcon.data());
      if (!applicationLocked && deleteState.pressed) {
        a_state.conditionId.reset();
        changed = true;
      }
      if (deleteState.hovered && !ImGui::IsDragDropActive()) {
        ImGui::SetTooltip("%s", localization->GetCStr("common.delete"));
      }
    }

    const auto lineHeight = ImGui::GetTextLineHeight();
    const auto textBlockHeight = (lineHeight * 2.0f) + 4.0f;
    const auto textY =
        cardRect.Min.y + ((cardRect.GetHeight() - textBlockHeight) * 0.5f);
    auto textX = cardRect.Min.x + 13.0f;
    const auto textMaxX = (hasCondition ? deleteMin.x : cardRect.Max.x) - 10.0f;

    if (hasCondition) {
      if (conditionIt->GetCatalog() != nullptr) {
        const ImVec4 conditionColor{conditionIt->GetCatalog()->color.x,
                                    conditionIt->GetCatalog()->color.y,
                                    conditionIt->GetCatalog()->color.z,
                                    conditionIt->GetCatalog()->color.w};
        constexpr float accentWidth = 5.0f;
        const auto accentMin = ImVec2(cardRect.Min.x, cardRect.Min.y + 2.0f);
        const auto accentMax =
            ImVec2(accentMin.x + accentWidth, cardRect.Max.y - 2.0f);
        drawList->AddRectFilled(accentMin, accentMax,
                                ImGui::GetColorU32(conditionColor), 3.0f);
        textX = accentMax.x + 8.0f;
      }
      const bool builtIn = conditions::IsBuiltInCondition(conditionIt->id);
      std::string summary(localization->Get(
          builtIn ? "workbench.condition.basic_summary"
                  : "workbench.condition.user_defined_summary"));
      summary.push_back(' ');
      summary.append(std::to_string(conditionIt->clauses.size()));
      const auto compactName = ui::catalog::TruncateTextToWidth(
          conditionIt->name, (std::max)(1.0f, textMaxX - textX));
      const auto compactSummary = ui::catalog::TruncateTextToWidth(
          summary, (std::max)(1.0f, textMaxX - textX));
      drawList->PushClipRect(ImVec2(textX, cardRect.Min.y),
                             ImVec2(textMaxX, cardRect.Max.y), true);
      drawList->AddText(ImVec2(textX, textY), themeConfig->GetColorU32("TEXT"),
                        compactName.c_str());
      drawList->AddText(ImVec2(textX, textY + lineHeight + 4.0f),
                        themeConfig->GetColorU32("TEXT_DISABLED"),
                        compactSummary.c_str());
      drawList->PopClipRect();
    } else {
      const auto prompt =
          localization->Get("workbench.slot_create.condition_prompt");
      drawPrompt(cardRect, prompt);
    }

    const auto *conditionCellDragPayload = ImGui::GetDragDropPayload();
    if (!applicationLocked && conditionCellDragPayload != nullptr &&
        conditionCellDragPayload->IsDataType(
            ui::workbench::kConditionPayloadType) &&
        ImGui::BeginDragDropTargetCustom(
            cardRect, ImGui::GetID("##slot-create-condition-target"))) {
      if (const auto *payload = ImGui::AcceptDragDropPayload(
              ui::workbench::kConditionPayloadType);
          payload && payload->Data != nullptr &&
          payload->DataSize == sizeof(DraggedConditionPayload)) {
        DraggedConditionPayload conditionPayload{};
        std::memcpy(&conditionPayload, payload->Data, sizeof(conditionPayload));
        const std::string conditionId(conditionPayload.conditionId.data());
        if (const auto *definition = conditions::FindDefinitionById(
                ConditionDefinitions(), conditionId);
            definition != nullptr &&
            IsWorkbenchSelectableCondition(*definition)) {
          a_state.conditionId = definition->id;
          changed = true;
        }
      }
      ImGui::EndDragDropTarget();
    }

    return changed;
  };

  const auto drawAddButtonRow = [&]() {
    ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, theme->GetColorU32("BG"));

    ImGui::TableSetColumnIndex(0);
    const auto leftCellRect = ImGui::TableGetCellBgRect(table, 0);
    const auto rightCellRect = ImGui::TableGetCellBgRect(table, 1);
    ImRect buttonRect = leftCellRect;
    buttonRect.Max.x = rightCellRect.Max.x;
    buttonRect.Min.x =
        (std::max)(buttonRect.Min.x, table->InnerClipRect.Min.x) +
        cellPadding.x;
    buttonRect.Min.y += cellPadding.y;
    buttonRect.Max.x =
        (std::min)(buttonRect.Max.x, table->InnerClipRect.Max.x) -
        cellPadding.x;
    buttonRect.Max.y -= cellPadding.y;
    ImGui::SetCursorScreenPos(buttonRect.Min);
    const auto buttonSize =
        ImVec2((std::max)(1.0f, buttonRect.Max.x - buttonRect.Min.x),
               (std::max)(1.0f, buttonRect.Max.y - buttonRect.Min.y));
    const std::string addLabel(localization->Get("workbench.slot_create.add"));
    if (applicationLocked) {
      ImGui::BeginDisabled();
    }
    const bool addClicked =
        ImGui::Selectable("##workbench-condition-slot-add", false,
                          ImGuiSelectableFlags_SpanAllColumns, buttonSize);
    if (applicationLocked) {
      ImGui::EndDisabled();
    }
    const auto labelSize = ImGui::CalcTextSize(addLabel.c_str());
    const auto labelPosition = ImVec2(
        buttonRect.Min.x + ((buttonRect.GetWidth() - labelSize.x) * 0.5f),
        buttonRect.Min.y + ((buttonRect.GetHeight() - labelSize.y) * 0.5f));
    auto *drawList = ImGui::GetWindowDrawList();
    const auto labelColor =
        applicationLocked ? theme->GetColorU32("TEXT_DISABLED")
                           : theme->GetColorU32("TEXT");
    const auto drawLabelPart = [&](const ImRect &a_clipRect) {
      auto visibleClipRect = a_clipRect;
      visibleClipRect.ClipWithFull(table->InnerClipRect);
      if (visibleClipRect.GetWidth() <= 0.0f ||
          visibleClipRect.GetHeight() <= 0.0f) {
        return;
      }
      // Keep the table/window clip active. Replacing it here lets an
      // off-screen final row draw its label below the workbench window.
      drawList->PushClipRect(visibleClipRect.Min, visibleClipRect.Max, true);
      drawList->AddText(labelPosition, labelColor, addLabel.c_str());
      drawList->PopClipRect();
    };
    // ImGui tables submit each column through a separate draw channel.  A
    // label drawn only in column 0 is subsequently covered by column 1's dark
    // layer even if its clip rectangle spans both columns. Draw the two halves
    // in their owning channels so the centered label remains visually whole.
    drawLabelPart(ImRect(buttonRect.Min,
                         ImVec2((std::min)(buttonRect.Max.x,
                                           leftCellRect.Max.x),
                                buttonRect.Max.y)));
    ImGui::TableSetColumnIndex(1);
    drawLabelPart(ImRect(ImVec2((std::max)(buttonRect.Min.x,
                                           rightCellRect.Min.x),
                                buttonRect.Min.y),
                         buttonRect.Max));
    if (addClicked && !applicationLocked) {
      OpenSlotCreationRow();
    }
  };

  bool refreshWorkbench = false;
  for (std::size_t index = 0; index < pendingSlotCreations_.size(); ++index) {
    auto &pending = pendingSlotCreations_[index];
    workbench::EquipmentWidgetItem overrideItem{};
    const bool hasOverrideItem =
        pending.overrideFormID != 0 &&
        workbench::BuildCatalogItem(pending.overrideFormID, overrideItem);
    workbench::EquipmentWidgetItem visibilityItem{};
    const bool hasVisibilityItem =
        pending.visibilityTargetKind.has_value() &&
        pending.visibilityTargetFormID != 0 &&
        workbench::BuildCatalogItem(pending.visibilityTargetFormID,
                                    visibilityItem);
    auto *pendingItem = hasVisibilityItem ? &visibilityItem : &overrideItem;
    const bool hasPendingItem = hasVisibilityItem || hasOverrideItem;
    const auto pendingDisplaySlotMask =
        hasPendingItem ? ResolveWorkbenchItemDisplaySlotMask(*pendingItem) : 0;
    if (hasPendingItem) {
      pendingItem->slotText =
          BuildMultilineWorkbenchSlotText(pendingDisplaySlotMask);
    }
    const auto pendingSlotLineCount =
        hasPendingItem
            ? (std::max)(
                  1, static_cast<int>(
                         SplitWorkbenchSlotMask(pendingDisplaySlotMask).size()))
            : 1;
    const auto pendingCardHeight =
        hasPendingItem ? 18.0f + (ImGui::GetTextLineHeight() *
                                  static_cast<float>(1 + pendingSlotLineCount))
                       : widgetHeight;
    const auto pendingRowHeight =
        pendingCardHeight + ui::workbench::kWorkbenchRowGapY;
    ImGui::TableNextRow(ImGuiTableRowFlags_None, pendingRowHeight);
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, theme->GetColorU32("BG"));
    ImGui::PushID(static_cast<int>(index));

    ImGui::TableSetColumnIndex(0);
    const auto leftCellRect = ImGui::TableGetCellBgRect(table, 0);
    const auto rightCellRect = ImGui::TableGetCellBgRect(table, 1);
    const bool conditionChanged = drawConditionCell(pending, leftCellRect);
    if (index >= pendingSlotCreations_.size()) {
      ImGui::PopID();
      break;
    }

    ImGui::TableSetColumnIndex(1);
    bool actionChanged = false;
    if (hasPendingItem) {
      ImGui::SetCursorScreenPos(
          ImVec2(rightCellRect.Min.x + cellPadding.x,
                 rightCellRect.Min.y + cellPadding.y +
                     ((rightCellRect.Max.y - rightCellRect.Min.y -
                       (cellPadding.y * 2.0f) - pendingCardHeight) *
                      0.5f)));
      const auto actionWidget = ui::components::DrawEquipmentWidget(
          "slot-create-action", *pendingItem,
          {.showDeleteButton = true,
           .deleteButtonEnabled = !applicationLocked,
           .showHideButton = true,
           .hideButtonEnabled = !applicationLocked,
           .hideButtonTooltipKey = pending.visibleWhenTrue
                                       ? "workbench.condition.action_hide"
                                       : "workbench.condition.action_show",
           .hidden = !pending.visibleWhenTrue,
           .preserveContentTextColors = true,
           .interactive = true,
           .allowContextMenu = false,
           .minimumHeight = pendingCardHeight,
           .statusText = localization->GetCStr("workbench.fitting.not_applied"),
           .statusColor = theme->GetColor("TEXT_DISABLED"),
           .slotIconText =
               pending.visibilityTargetKind ==
                       workbench::ConditionalVisibilityTargetKind::Actual
                   ? kWorkbenchActualSlotIcon.data()
                   : kWorkbenchFittingSlotIcon.data(),
           .slotIconScale = 0.72f});
      if (!applicationLocked && actionWidget.hideClicked) {
        pending.visibleWhenTrue = !pending.visibleWhenTrue;
        actionChanged = true;
      }
      if (!applicationLocked && actionWidget.deleteClicked) {
        pending.overrideFormID = 0;
        pending.visibilityTargetKind.reset();
        pending.visibilityTargetFormID = 0;
        actionChanged = true;
      }
    } else {
      const auto contentMin = ImVec2(rightCellRect.Min.x + cellPadding.x,
                                     rightCellRect.Min.y + cellPadding.y);
      const auto contentMax = ImVec2(rightCellRect.Max.x - cellPadding.x,
                                     rightCellRect.Max.y - cellPadding.y);
      const ImRect contentRect(contentMin, contentMax);
      const auto hovered =
          ImGui::IsMouseHoveringRect(contentMin, contentMax, true);
      const auto bodyColor = theme->GetColorU32(hovered ? "BG_LIGHT" : "BG",
                                                hovered ? 1.0f : 0.92f);
      ImGui::GetWindowDrawList()->AddRectFilled(contentMin, contentMax,
                                                bodyColor, 8.0f);
      ImGui::GetWindowDrawList()->AddRect(
          contentMin, contentMax, theme->GetColorU32("TEXT_DISABLED", 0.55f),
          8.0f);
      drawPrompt(contentRect,
                 localization->Get("workbench.slot_create.action_prompt"));
    }

    if (!applicationLocked &&
        ImGui::BeginDragDropTargetCustom(
            rightCellRect, ImGui::GetID("##slot-create-override-target"))) {
      if (const auto *payload = ImGui::AcceptDragDropPayload(
              ui::workbench::kVariantItemPayloadType);
          payload && payload->Data != nullptr &&
          payload->DataSize == sizeof(DraggedEquipmentPayload)) {
        DraggedEquipmentPayload dragPayload{};
        std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
        const auto sourceKind =
            static_cast<DragSourceKind>(dragPayload.sourceKind);
        if (sourceKind == DragSourceKind::Catalog && dragPayload.formID != 0) {
          pending.overrideFormID = dragPayload.formID;
          pending.visibilityTargetKind.reset();
          pending.visibilityTargetFormID = 0;
          actionChanged = true;
        } else if (sourceKind == DragSourceKind::Row &&
                   dragPayload.formID != 0) {
          const auto &rows = workbench_.GetRows();
          auto sourceRowIndex = dragPayload.rowIndex;
          if (dragPayload.sourceUiIdentity != 0) {
            const auto sourceIt = std::ranges::find(
                rows, dragPayload.sourceUiIdentity,
                &workbench::VariantWorkbenchRow::uiIdentity);
            sourceRowIndex = sourceIt == rows.end()
                                 ? -1
                                 : static_cast<int>(
                                       std::distance(rows.begin(), sourceIt));
          }
          if (sourceRowIndex >= 0 &&
              sourceRowIndex < static_cast<int>(rows.size())) {
            const auto &sourceRow =
                rows[static_cast<std::size_t>(sourceRowIndex)];
            const bool actualTarget = dragPayload.itemIndex < 0;
            const auto sourceOverride = std::ranges::find(
                sourceRow.overrides, dragPayload.formID,
                &workbench::EquipmentWidgetItem::formID);
            const bool validTarget =
                !sourceRow.conditionId.has_value() &&
                ((actualTarget && sourceRow.isEquipped &&
                  !sourceRow.IsSlotRow() &&
                  sourceRow.equipped.formID == dragPayload.formID &&
                  !sourceRow.IsAlwaysVisibleActualEquipment()) ||
                 (!actualTarget &&
                  sourceOverride != sourceRow.overrides.end()));
            if (validTarget) {
              pending.overrideFormID = 0;
              pending.visibilityTargetKind =
                  actualTarget
                      ? workbench::ConditionalVisibilityTargetKind::Actual
                      : workbench::ConditionalVisibilityTargetKind::Fitting;
              pending.visibilityTargetFormID = dragPayload.formID;
              if (actualTarget) {
                pending.visibleWhenTrue =
                    !workbench_.ResolveEquippedHiddenForActor(
                        ResolveWorkbenchPreviewActor(), sourceRow);
              } else {
                pending.visibleWhenTrue =
                    !sourceOverride->hidden && !HideFittingOverridesForActor(
                                               ResolveWorkbenchPreviewActor());
              }
              actionChanged = true;
            }
          }
        }
      }
      if (const auto *payload = ImGui::AcceptDragDropPayload(
              ui::workbench::kConditionPayloadType);
          payload && payload->Data != nullptr &&
          payload->DataSize == sizeof(DraggedConditionPayload)) {
        DraggedConditionPayload conditionPayload{};
        std::memcpy(&conditionPayload, payload->Data, sizeof(conditionPayload));
        const std::string conditionId(conditionPayload.conditionId.data());
        if (const auto *definition = conditions::FindDefinitionById(
                ConditionDefinitions(), conditionId);
            definition != nullptr &&
            IsWorkbenchSelectableCondition(*definition)) {
          pending.conditionId = definition->id;
        }
      }
      ImGui::EndDragDropTarget();
    }
    if ((conditionChanged || actionChanged || pending.conditionId.has_value() ||
         pending.overrideFormID != 0 || pending.visibilityTargetFormID != 0) &&
        TryCommitSlotCreationRow(index)) {
      refreshWorkbench = true;
      workbenchSortDeferredUntilClose_ = true;
    }
    ImGui::PopID();
    if (refreshWorkbench) {
      break;
    }
  }

  drawAddButtonRow();
  if (refreshWorkbench) {
    workbench_.RefreshNativeArmorOverridesForActorWithoutEquipmentSync(
        ResolveNewWorkbenchRowOwnerActorFormID(), conditionStore_.revision);
  }
  return conditionHeaderReachedStickyRow;
}

void Menu::DrawGeneratedKitPreviewWorkbenchTable(
    const std::vector<workbench::VariantWorkbenchRow> &a_previewRows) {
  auto *localization = ui::Localization::GetSingleton();
  const auto tableHeight = (std::max)(0.0f, ImGui::GetContentRegionAvail().y);
  if (!ImGui::BeginTable("##generated-kit-workbench-preview", 2,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_ScrollY,
                         ImVec2(0.0f, tableHeight))) {
    return;
  }

  const std::string equippedHeader =
      std::string(localization->Get("workbench.equipped")) +
      "###generated-kit-preview-equipped";
  const std::string overridesHeader =
      std::string(localization->Get("workbench.overrides")) +
      "###generated-kit-preview-overrides";
  ImGui::TableSetupColumn(equippedHeader.c_str(),
                          ImGuiTableColumnFlags_WidthStretch, 1.0f);
  ImGui::TableSetupColumn(overridesHeader.c_str(),
                          ImGuiTableColumnFlags_WidthStretch, 1.0f);
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::PushID("generated-kit-preview-headers");
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers,
                      ImGui::TableGetHeaderRowHeight());
  ImGui::TableSetColumnIndex(0);
  DrawWorkbenchSortableHeader(
      localization->Get("workbench.equipped").data(),
      "generated-kit-preview-actual-sort", workbenchBaseSort_,
      WorkbenchSortColumn::Left);
  ImGui::TableSetColumnIndex(1);
  DrawWorkbenchSortableHeader(
      localization->Get("workbench.overrides").data(),
      "generated-kit-preview-fitting-sort", workbenchBaseSort_,
      WorkbenchSortColumn::Right);
  ImGui::PopID();

  const auto slotMaskRank = [](const std::uint64_t a_slotMask) {
    std::size_t rank = 62;
    std::size_t slot = 30;
    for (const auto slotMask : armor::GetAllArmorSlotMasks()) {
      if ((a_slotMask & slotMask) != 0) {
        rank = slot;
        break;
      }
      ++slot;
    }
    return rank;
  };
  const auto resolveActualSlotMask = [](
                                         const workbench::VariantWorkbenchRow
                                             &a_row) {
    if (a_row.isEquipped && !a_row.IsSlotRow() && a_row.equipped.HasForm()) {
      const auto displayMask = a_row.GetSelectionDisplaySlotMask();
      return displayMask != 0 ? displayMask : a_row.equipped.slotMask;
    }
    return a_row.GetOverrideDisplaySlotMask();
  };
  const auto fittingSlotMask = [](const workbench::VariantWorkbenchRow &a_row) {
    return a_row.GetOverrideDisplaySlotMask();
  };
  const auto rankLess = [&](const std::size_t a_left,
                            const std::size_t a_right,
                            const bool a_ascending) {
    if (a_left == a_right) {
      return false;
    }
    if (a_left == 62) {
      return false;
    }
    if (a_right == 62) {
      return true;
    }
    return a_ascending ? a_left < a_right : a_left > a_right;
  };

  std::vector<std::size_t> rowOrder(a_previewRows.size());
  std::iota(rowOrder.begin(), rowOrder.end(), 0);
  const bool sortActual =
      workbenchBaseSort_.column == WorkbenchSortColumn::Left;
  const bool ascending = workbenchBaseSort_.column == WorkbenchSortColumn::None
                             ? true
                             : workbenchBaseSort_.ascending;
  std::ranges::stable_sort(
      rowOrder, [&](const std::size_t a_left, const std::size_t a_right) {
        const auto &left = a_previewRows[a_left];
        const auto &right = a_previewRows[a_right];
        const auto leftPrimary = slotMaskRank(
            sortActual ? resolveActualSlotMask(left) : fittingSlotMask(left));
        const auto rightPrimary = slotMaskRank(
            sortActual ? resolveActualSlotMask(right)
                       : fittingSlotMask(right));
        if (leftPrimary != rightPrimary) {
          return rankLess(leftPrimary, rightPrimary, ascending);
        }
        const auto leftSecondary = slotMaskRank(
            sortActual ? fittingSlotMask(left)
                       : resolveActualSlotMask(left));
        const auto rightSecondary = slotMaskRank(
            sortActual ? fittingSlotMask(right)
                       : resolveActualSlotMask(right));
        if (leftSecondary != rightSecondary) {
          return rankLess(leftSecondary, rightSecondary, true);
        }
        return left.key < right.key;
      });

  for (const auto rowIndex : rowOrder) {
    const auto &row = a_previewRows[rowIndex];
    if (row.overrides.empty()) {
      continue;
    }

    std::vector<std::size_t> overrideOrder(row.overrides.size());
    std::iota(overrideOrder.begin(), overrideOrder.end(), 0);
    std::ranges::stable_sort(
        overrideOrder, [&](const std::size_t a_left,
                           const std::size_t a_right) {
          const auto leftRank = slotMaskRank(
              row.GetOverrideDisplaySlotMask(row.overrides[a_left]));
          const auto rightRank = slotMaskRank(
              row.GetOverrideDisplaySlotMask(row.overrides[a_right]));
          if (leftRank != rightRank) {
            return rankLess(leftRank, rightRank,
                            !sortActual ? ascending : true);
          }
          return row.overrides[a_left].formID < row.overrides[a_right].formID;
        });

    for (std::size_t overrideOrderIndex = 0;
         overrideOrderIndex < overrideOrder.size(); ++overrideOrderIndex) {
      const auto overrideIndex = overrideOrder[overrideOrderIndex];
      const auto &overrideItem = row.overrides[overrideIndex];
      const auto displaySlotMask = row.GetOverrideDisplaySlotMask(overrideItem);
      const auto widgetHeight =
          18.0f + ImGui::GetTextLineHeight() *
                      static_cast<float>((std::max)(
                          std::size_t{2},
                          SplitWorkbenchSlotMask(displaySlotMask).size()));
      ImGui::PushID(static_cast<int>(rowIndex));
      ImGui::PushID(static_cast<int>(overrideIndex));
      ImGui::TableNextRow(ImGuiTableRowFlags_None,
                          widgetHeight + ui::workbench::kWorkbenchRowGapY);
      ImGui::TableSetColumnIndex(0);

      if (overrideOrderIndex == 0) {
        auto actualDisplayItem = row.equipped;
        if (row.isEquipped && !row.IsSlotRow() &&
            actualDisplayItem.HasForm()) {
          auto actualSlotMask = row.GetSelectionDisplaySlotMask();
          if (actualSlotMask == 0) {
            actualSlotMask = actualDisplayItem.slotMask;
          }
          actualDisplayItem.slotText =
              BuildMultilineWorkbenchSlotText(actualSlotMask);
          (void)ui::components::DrawEquipmentWidget(
              "##generated-kit-preview-actual", actualDisplayItem,
              {.showDeleteButton = false,
               .showHideButton = false,
               .interactive = false,
               .allowContextMenu = false,
               .minimumHeight = widgetHeight,
               .slotIconText = kWorkbenchActualSlotIcon.data(),
               .slotIconScale = 0.72f});
        } else {
          auto emptyActual = actualDisplayItem;
          emptyActual.kind = workbench::EquipmentWidgetItemKind::Slot;
          emptyActual.formID = 0;
          emptyActual.key = std::format("generated-kit-preview-empty:{}:{}",
                                       rowIndex, overrideIndex);
          emptyActual.name =
              std::string(localization->Get("workbench.real_clothing.none"));
          emptyActual.slotMask = displaySlotMask;
          emptyActual.slotText =
              BuildMultilineWorkbenchSlotText(displaySlotMask);
          emptyActual.hasArmorAddons = true;
          (void)ui::components::DrawEquipmentWidget(
              "##generated-kit-preview-empty-actual", emptyActual,
              {.showDeleteButton = false,
               .showHideButton = false,
               .disabledAppearance = true,
               .interactive = false,
               .allowContextMenu = false,
               .minimumHeight = widgetHeight,
               .slotIconText = kWorkbenchActualSlotIcon.data(),
               .slotIconScale = 0.72f});
        }
      }

      ImGui::TableSetColumnIndex(1);
      auto overrideDisplayItem = overrideItem;
      overrideDisplayItem.slotText =
          BuildMultilineWorkbenchSlotText(displaySlotMask);
      (void)ui::components::DrawEquipmentWidget(
          "##generated-kit-preview-override", overrideDisplayItem,
          {.showDeleteButton = false,
           .showHideButton = false,
           .interactive = false,
           .allowContextMenu = false,
           .minimumHeight = widgetHeight,
           .slotIconText = kWorkbenchFittingSlotIcon.data(),
           .slotIconScale = 0.72f});
      ImGui::PopID();
      ImGui::PopID();
    }
  }
  ImGui::EndTable();
}

void Menu::DrawWorkbenchTable(const std::vector<int> &a_visibleRowIndices) {
  if (ImGui::IsDragDropActive()) {
    workbenchSortDeferredUntilClose_ = true;
  }
  auto *localization = ui::Localization::GetSingleton();
  EnsureWorkbenchDerivedState();
  const auto &rows = workbench_.GetRows();
  const auto &rowConditionStates = workbenchDerived_.rowConditionStates;
  const auto &rowConflicts = workbenchDerived_.conflictState.rowConflicts;
  const auto &overrideConflicts =
      workbenchDerived_.conflictState.overrideConflicts;

  const auto tableHeight = (std::max)(0.0f, ImGui::GetContentRegionAvail().y);
  auto *previewActor = ResolveWorkbenchPreviewActor();
  const bool globallyHideFittingOverrides =
      previewActor != nullptr && HideFittingOverridesForActor(previewActor);
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
                sfs::poc::GetDeviousDevicesHiderSuppressedFittingSlotMask(
                    previewActor))
          : 0;
  const auto selectPrimaryDisplaySlot = [](const std::uint64_t a_slotMask) {
    for (const auto slotMask : sfs::armor::GetAllArmorSlotMasks()) {
      if ((a_slotMask & slotMask) != 0) {
        return slotMask;
      }
    }
    return std::uint64_t{0};
  };
  struct DisplayOverrideRef {
    int rowIndex{-1};
    int overrideIndex{-1};
  };
  struct DisplayRowRoles {
    int realEquipmentRowIndex{-1};
    int baseFittingRowIndex{-1};
    int activeConditionalFittingRowIndex{-1};
    int activeFittingRowIndex{-1};
    int emptySlotRowIndex{-1};
    std::vector<int> conditionalFittingRowIndices;
  };
  struct DisplayRowGroup {
    int displayRowIndex{-1};
    int actualRowIndex{-1};
    std::vector<int> actualRowIndices;
    int dropTargetRowIndex{-1};
    int conditionRowIndex{-1};
    bool conditionalSection{false};
    std::size_t firstVisiblePosition{0};
    std::uint64_t slotMask{0};
    std::uint64_t spanSlotMask{0};
    std::uint64_t actualSpanSlotMask{0};
    std::uint64_t overrideSpanSlotMask{0};
    std::vector<int> memberRowIndices;
    std::vector<DisplayOverrideRef> overrides;
    DisplayRowRoles roles;
  };

  std::unordered_map<int, std::size_t> visiblePositions;
  visiblePositions.reserve(a_visibleRowIndices.size());
  for (std::size_t position = 0; position < a_visibleRowIndices.size();
       ++position) {
    visiblePositions.emplace(a_visibleRowIndices[position], position);
  }

  std::vector<DisplayRowGroup> displayRowGroups;
  displayRowGroups.reserve(a_visibleRowIndices.size());

  const auto findDisplayGroup = [&](const std::uint64_t a_slotMask) {
    if (a_slotMask == 0) {
      return displayRowGroups.end();
    }
    return std::ranges::find(displayRowGroups, a_slotMask,
                             &DisplayRowGroup::slotMask);
  };
  const auto addMemberRow = [&](DisplayRowGroup &a_group,
                                const int a_rowIndex) {
    a_group.firstVisiblePosition = (std::min)(a_group.firstVisiblePosition,
                                              visiblePositions.at(a_rowIndex));
    if (std::ranges::find(a_group.memberRowIndices, a_rowIndex) ==
        a_group.memberRowIndices.end()) {
      a_group.memberRowIndices.push_back(a_rowIndex);
    }
  };
  const auto createDisplayGroup =
      [&](const int a_rowIndex, const std::uint64_t a_slotMask,
          const bool a_actualRow) -> DisplayRowGroup & {
    DisplayRowGroup group{};
    group.displayRowIndex = a_rowIndex;
    group.actualRowIndex = a_actualRow ? a_rowIndex : -1;
    if (a_actualRow) {
      group.actualRowIndices.push_back(a_rowIndex);
    }
    group.dropTargetRowIndex = a_actualRow ? a_rowIndex : -1;
    group.firstVisiblePosition = visiblePositions.at(a_rowIndex);
    group.slotMask = a_slotMask;
    group.memberRowIndices.push_back(a_rowIndex);
    displayRowGroups.push_back(std::move(group));
    return displayRowGroups.back();
  };

  // Real clothing establishes the left-hand card for a display slot.
  for (const auto rowIndex : a_visibleRowIndices) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped || row.IsSlotRow()) {
      continue;
    }

    auto rowSlotMask =
        selectPrimaryDisplaySlot(row.GetSelectionDisplaySlotMask());
    if (rowSlotMask == 0) {
      rowSlotMask = selectPrimaryDisplaySlot(row.equipped.slotMask);
    }
    auto groupIt = findDisplayGroup(rowSlotMask);
    if (groupIt == displayRowGroups.end()) {
      static_cast<void>(createDisplayGroup(rowIndex, rowSlotMask, true));
      continue;
    }

    addMemberRow(*groupIt, rowIndex);
    if (std::ranges::find(groupIt->actualRowIndices, rowIndex) ==
        groupIt->actualRowIndices.end()) {
      groupIt->actualRowIndices.push_back(rowIndex);
    }
    if (groupIt->actualRowIndex < 0) {
      groupIt->actualRowIndex = rowIndex;
      groupIt->displayRowIndex = rowIndex;
      groupIt->dropTargetRowIndex = rowIndex;
    }
  }

  // Group every override by its own visual slot. This also repairs the UI for
  // legacy rows that accidentally contain overrides from several slots.
  for (const auto rowIndex : a_visibleRowIndices) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];

    if (row.overrides.empty()) {
      auto rowSlotMask =
          selectPrimaryDisplaySlot(row.GetSelectionDisplaySlotMask());
      if (rowSlotMask == 0) {
        rowSlotMask = selectPrimaryDisplaySlot(row.equipped.slotMask);
      }
      auto groupIt = findDisplayGroup(rowSlotMask);
      if (groupIt == displayRowGroups.end()) {
        static_cast<void>(createDisplayGroup(rowIndex, rowSlotMask, false));
      } else {
        addMemberRow(*groupIt, rowIndex);
      }
      continue;
    }

    for (int overrideIndex = 0;
         overrideIndex < static_cast<int>(row.overrides.size());
         ++overrideIndex) {
      const auto &overrideItem =
          row.overrides[static_cast<std::size_t>(overrideIndex)];
      auto overrideSlotMask = selectPrimaryDisplaySlot(
          row.GetOverrideDisplaySlotMask(overrideItem));
      if (overrideSlotMask == 0) {
        overrideSlotMask =
            selectPrimaryDisplaySlot(row.GetSelectionConflictSlotMask());
      }

      auto groupIt = findDisplayGroup(overrideSlotMask);
      if (groupIt == displayRowGroups.end()) {
        static_cast<void>(
            createDisplayGroup(rowIndex, overrideSlotMask, false));
        groupIt = std::prev(displayRowGroups.end());
      } else {
        addMemberRow(*groupIt, rowIndex);
      }
      groupIt->overrides.push_back(
          {.rowIndex = rowIndex, .overrideIndex = overrideIndex});
    }
  }

  const auto getBaseCoverageSlotMask = [&](const DisplayRowGroup &a_group) {
    std::uint64_t coverage = 0;
    for (const auto actualIndex : a_group.actualRowIndices) {
      if (actualIndex >= 0 && actualIndex < static_cast<int>(rows.size())) {
        coverage |= rows[static_cast<std::size_t>(actualIndex)]
                        .GetSelectionDisplaySlotMask();
      }
    }
    for (const auto &overrideRef : a_group.overrides) {
      if (overrideRef.rowIndex < 0 ||
          overrideRef.rowIndex >= static_cast<int>(rows.size())) {
        continue;
      }
      const auto &overrideRow =
          rows[static_cast<std::size_t>(overrideRef.rowIndex)];
      if (overrideRow.conditionId.has_value() ||
          overrideRef.overrideIndex < 0 ||
          overrideRef.overrideIndex >=
              static_cast<int>(overrideRow.overrides.size())) {
        continue;
      }
      coverage |= overrideRow.GetOverrideDisplaySlotMask(
          overrideRow
              .overrides[static_cast<std::size_t>(overrideRef.overrideIndex)]);
    }
    for (const auto memberRowIndex : a_group.memberRowIndices) {
      if (memberRowIndex < 0 ||
          memberRowIndex >= static_cast<int>(rows.size())) {
        continue;
      }
      const auto &memberRow = rows[static_cast<std::size_t>(memberRowIndex)];
      if (memberRow.IsSlotRow() && !memberRow.conditionId.has_value()) {
        coverage |= memberRow.GetSelectionConflictSlotMask();
      }
    }
    return coverage != 0 ? coverage : a_group.slotMask;
  };

  // A multi-slot card owns one vertical display group. Merge only through
  // real clothing and base fitting coverage; conditional rows are split into
  // their own section later and must not collapse unrelated base rows.
  for (std::size_t groupIndex = 0; groupIndex < displayRowGroups.size();
       ++groupIndex) {
    bool mergedAnotherGroup = true;
    while (mergedAnotherGroup) {
      mergedAnotherGroup = false;
      const auto groupCoverage =
          getBaseCoverageSlotMask(displayRowGroups[groupIndex]);
      for (std::size_t otherIndex = groupIndex + 1;
           otherIndex < displayRowGroups.size(); ++otherIndex) {
        const auto otherCoverage =
            getBaseCoverageSlotMask(displayRowGroups[otherIndex]);
        if ((groupCoverage & otherCoverage) == 0) {
          continue;
        }

        auto &target = displayRowGroups[groupIndex];
        auto &source = displayRowGroups[otherIndex];
        target.firstVisiblePosition = (std::min)(target.firstVisiblePosition,
                                                 source.firstVisiblePosition);
        for (const auto rowIndex : source.memberRowIndices) {
          addMemberRow(target, rowIndex);
        }
        for (const auto rowIndex : source.actualRowIndices) {
          if (std::ranges::find(target.actualRowIndices, rowIndex) ==
              target.actualRowIndices.end()) {
            target.actualRowIndices.push_back(rowIndex);
          }
        }
        for (const auto &overrideRef : source.overrides) {
          const auto duplicate = std::ranges::any_of(
              target.overrides, [&](const DisplayOverrideRef &a_existing) {
                return a_existing.rowIndex == overrideRef.rowIndex &&
                       a_existing.overrideIndex == overrideRef.overrideIndex;
              });
          if (!duplicate) {
            target.overrides.push_back(overrideRef);
          }
        }
        if (target.actualRowIndex < 0) {
          target.actualRowIndex = source.actualRowIndex;
        }
        target.slotMask =
            selectPrimaryDisplaySlot(groupCoverage | otherCoverage);
        displayRowGroups.erase(displayRowGroups.begin() +
                               static_cast<std::ptrdiff_t>(otherIndex));
        mergedAnotherGroup = true;
        break;
      }
    }
  }

  const auto isConditionalRowActive = [&](const int a_rowIndex) {
    if (previewActor == nullptr || a_rowIndex < 0 ||
        a_rowIndex >= static_cast<int>(rows.size())) {
      return false;
    }

    const auto &candidateRow = rows[static_cast<std::size_t>(a_rowIndex)];
    if (!candidateRow.conditionId.has_value()) {
      return false;
    }

    auto &conditionDefinitions = ConditionDefinitions();
    const auto *definition = conditions::FindDefinitionById(
        conditionDefinitions, *candidateRow.conditionId);
    if (definition == nullptr || !IsWorkbenchSelectableCondition(*definition) ||
        !conditions::EvaluateDefinitionStatus(*definition, conditionDefinitions)
             .IsActive()) {
      return false;
    }

    const auto materialized = conditions::MaterializeConditionById(
        *candidateRow.conditionId, conditionDefinitions);
    return materialized.has_value() && materialized->condition != nullptr &&
           materialized->condition->IsTrue(previewActor, previewActor);
  };
  const auto isConditionIdActive = [&](const std::string_view a_conditionId) {
    if (previewActor == nullptr || a_conditionId.empty()) {
      return false;
    }
    auto &conditionDefinitions = ConditionDefinitions();
    const auto *definition =
        conditions::FindDefinitionById(conditionDefinitions, a_conditionId);
    if (definition == nullptr || !IsWorkbenchSelectableCondition(*definition) ||
        !conditions::EvaluateDefinitionStatus(*definition, conditionDefinitions)
             .IsActive()) {
      return false;
    }
    const auto materialized = conditions::MaterializeConditionById(
        a_conditionId, conditionDefinitions);
    return materialized.has_value() && materialized->condition != nullptr &&
           materialized->condition->IsTrue(previewActor, previewActor);
  };
  const auto &conditionalVisibilityRules =
      workbench_.GetConditionalVisibilityRules();
  std::unordered_map<RE::FormID, std::size_t> activeActualVisibilityRules;
  std::unordered_map<RE::FormID, std::size_t> activeFittingVisibilityRules;
  for (std::size_t ruleIndex = 0; ruleIndex < conditionalVisibilityRules.size();
       ++ruleIndex) {
    const auto &rule = conditionalVisibilityRules[ruleIndex];
    if (!rule.IsOwnedByActor(previewActor) ||
        !isConditionIdActive(rule.conditionId)) {
      continue;
    }
    const auto *targetArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(rule.target.formID);
    if (targetArmor != nullptr &&
        workbench::IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(targetArmor))) {
      continue;
    }
    auto &activeRules =
        rule.targetKind == workbench::ConditionalVisibilityTargetKind::Actual
            ? activeActualVisibilityRules
            : activeFittingVisibilityRules;
    static_cast<void>(activeRules.emplace(rule.target.formID, ruleIndex));
  }
  const auto resolveVisibilityTargetPayload =
      [&](const DraggedEquipmentPayload &a_payload,
          workbench::ConditionalVisibilityTargetKind &a_targetKind,
          bool &a_visibleWhenTrue) {
        const auto sourceKind =
            static_cast<DragSourceKind>(a_payload.sourceKind);
        if (a_payload.formID == 0) {
          return false;
        }
        if (sourceKind == DragSourceKind::ConditionalVisibilityRule) {
          const auto ruleIt = std::ranges::find(
              conditionalVisibilityRules, a_payload.sourceUiIdentity,
              &workbench::ConditionalVisibilityRule::uiIdentity);
          if (ruleIt == conditionalVisibilityRules.end() ||
              ruleIt->target.formID != a_payload.formID) {
            return false;
          }
          a_targetKind = ruleIt->targetKind;
          a_visibleWhenTrue = ruleIt->visibleWhenTrue;
          return true;
        }
        if (sourceKind != DragSourceKind::Row &&
            sourceKind != DragSourceKind::ConditionalRow) {
          return false;
        }
        auto sourceRowIndex = a_payload.rowIndex;
        if (a_payload.sourceUiIdentity != 0) {
          const auto sourceIt = std::ranges::find(
              rows, a_payload.sourceUiIdentity,
              &workbench::VariantWorkbenchRow::uiIdentity);
          if (sourceIt == rows.end()) {
            return false;
          }
          sourceRowIndex =
              static_cast<int>(std::distance(rows.begin(), sourceIt));
        }
        if (sourceRowIndex < 0 ||
            sourceRowIndex >= static_cast<int>(rows.size())) {
          return false;
        }
        const auto &sourceRow =
            rows[static_cast<std::size_t>(sourceRowIndex)];
        if (sourceKind == DragSourceKind::ConditionalRow) {
          if (!sourceRow.conditionId.has_value()) {
            return false;
          }
          const auto sourceItem = std::ranges::find(
              sourceRow.overrides, a_payload.formID,
              &workbench::EquipmentWidgetItem::formID);
          if (sourceItem == sourceRow.overrides.end()) {
            return false;
          }
          a_targetKind = workbench::ConditionalVisibilityTargetKind::Fitting;
          a_visibleWhenTrue = !sourceItem->hidden;
          return true;
        }
        if (sourceRow.conditionId.has_value()) {
          return false;
        }
        if (a_payload.itemIndex < 0) {
          if (!sourceRow.isEquipped || sourceRow.IsSlotRow() ||
              sourceRow.equipped.formID != a_payload.formID ||
              sourceRow.IsAlwaysVisibleActualEquipment()) {
            return false;
          }
          a_targetKind = workbench::ConditionalVisibilityTargetKind::Actual;
          a_visibleWhenTrue = !workbench_.ResolveEquippedHiddenForActor(
              previewActor, sourceRow);
          return true;
        }
        const auto sourceItem = std::ranges::find(
            sourceRow.overrides, a_payload.formID,
            &workbench::EquipmentWidgetItem::formID);
        if (sourceItem == sourceRow.overrides.end()) {
          return false;
        }
        a_targetKind = workbench::ConditionalVisibilityTargetKind::Fitting;
        a_visibleWhenTrue = !sourceItem->hidden && !globallyHideFittingOverrides;
        return true;
      };
  const auto resolveActiveFittingRowForSlotMask =
      [&](const std::uint64_t a_slotMask) {
        int baseRowIndex = -1;
        int activeConditionalRowIndex = -1;
        for (const auto rowIndex : a_visibleRowIndices) {
          if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size())) {
            continue;
          }
          const auto &candidateRow = rows[static_cast<std::size_t>(rowIndex)];
          const bool overlapsSlot = std::ranges::any_of(
              candidateRow.overrides, [&](const auto &a_item) {
                return !candidateRow.IsProtectedAppearance(a_item) &&
                       (candidateRow.GetOverrideDisplaySlotMask(a_item) &
                        a_slotMask) != 0;
              });
          if (!overlapsSlot) {
            continue;
          }
          if (!candidateRow.conditionId.has_value()) {
            if (baseRowIndex < 0) {
              baseRowIndex = rowIndex;
            }
          } else if (activeConditionalRowIndex < 0 &&
                     isConditionalRowActive(rowIndex)) {
            activeConditionalRowIndex = rowIndex;
          }
        }
        return activeConditionalRowIndex >= 0 ? activeConditionalRowIndex
                                              : baseRowIndex;
      };

  const auto groupContainsOverrideFromRow = [](const DisplayRowGroup &a_group,
                                               const int a_rowIndex) {
    return std::ranges::any_of(a_group.overrides,
                               [a_rowIndex](const DisplayOverrideRef &a_ref) {
                                 return a_ref.rowIndex == a_rowIndex;
                               });
  };
  const auto resolveDisplayRowRoles = [&](DisplayRowGroup &a_group) {
    auto &roles = a_group.roles;
    roles = {};
    roles.realEquipmentRowIndex = a_group.actualRowIndex;

    for (const auto memberRowIndex : a_group.memberRowIndices) {
      const auto &memberRow = rows[static_cast<std::size_t>(memberRowIndex)];
      const bool hasGroupedOverride =
          groupContainsOverrideFromRow(a_group, memberRowIndex);
      if (!hasGroupedOverride && memberRow.IsSlotRow() &&
          !memberRow.conditionId.has_value() && roles.emptySlotRowIndex < 0) {
        roles.emptySlotRowIndex = memberRowIndex;
      }
      if (!hasGroupedOverride && !memberRow.conditionId.has_value()) {
        continue;
      }

      if (!memberRow.conditionId.has_value()) {
        if (roles.baseFittingRowIndex < 0) {
          roles.baseFittingRowIndex = memberRowIndex;
        }
        continue;
      }

      roles.conditionalFittingRowIndices.push_back(memberRowIndex);
      if (roles.activeConditionalFittingRowIndex < 0 &&
          isConditionalRowActive(memberRowIndex)) {
        roles.activeConditionalFittingRowIndex = memberRowIndex;
      }
    }

    roles.activeFittingRowIndex = roles.activeConditionalFittingRowIndex >= 0
                                      ? roles.activeConditionalFittingRowIndex
                                      : roles.baseFittingRowIndex;
    if (roles.realEquipmentRowIndex >= 0) {
      a_group.displayRowIndex = roles.realEquipmentRowIndex;
    } else if (roles.baseFittingRowIndex >= 0) {
      a_group.displayRowIndex = roles.baseFittingRowIndex;
    } else if (roles.activeConditionalFittingRowIndex >= 0) {
      a_group.displayRowIndex = roles.activeConditionalFittingRowIndex;
    } else if (roles.emptySlotRowIndex >= 0) {
      a_group.displayRowIndex = roles.emptySlotRowIndex;
    } else if (!a_group.memberRowIndices.empty()) {
      a_group.displayRowIndex = a_group.memberRowIndices.front();
    }

    if (roles.baseFittingRowIndex >= 0) {
      a_group.dropTargetRowIndex = roles.baseFittingRowIndex;
    } else if (roles.realEquipmentRowIndex >= 0 &&
               !rows[static_cast<std::size_t>(roles.realEquipmentRowIndex)]
                    .conditionId.has_value()) {
      a_group.dropTargetRowIndex = roles.realEquipmentRowIndex;
    } else {
      a_group.dropTargetRowIndex = roles.emptySlotRowIndex;
    }
  };

  for (auto &group : displayRowGroups) {
    std::ranges::sort(
        group.memberRowIndices, [&](const int a_left, const int a_right) {
          return visiblePositions.at(a_left) < visiblePositions.at(a_right);
        });
    std::ranges::sort(
        group.actualRowIndices, [&](const int a_left, const int a_right) {
          return visiblePositions.at(a_left) < visiblePositions.at(a_right);
        });
    resolveDisplayRowRoles(group);
  }
  std::ranges::sort(
      displayRowGroups, [](const auto &a_left, const auto &a_right) {
        return a_left.firstVisiblePosition < a_right.firstVisiblePosition;
      });

  const auto computeActualSpanSlotMask = [&](const DisplayRowGroup &a_group) {
    std::uint64_t spanSlotMask = 0;
    for (const auto actualRowIndex : a_group.actualRowIndices) {
      if (actualRowIndex < 0 ||
          actualRowIndex >= static_cast<int>(rows.size())) {
        continue;
      }
      spanSlotMask |= rows[static_cast<std::size_t>(actualRowIndex)]
                          .GetSelectionDisplaySlotMask();
    }
    return spanSlotMask;
  };

  const auto computeOverrideSpanSlotMask = [&](const DisplayRowGroup &a_group) {
    std::uint64_t spanSlotMask = 0;
    for (const auto &overrideRef : a_group.overrides) {
      if (overrideRef.rowIndex < 0 ||
          overrideRef.rowIndex >= static_cast<int>(rows.size())) {
        continue;
      }
      const auto &overrideRow =
          rows[static_cast<std::size_t>(overrideRef.rowIndex)];
      if (overrideRef.overrideIndex < 0 ||
          overrideRef.overrideIndex >=
              static_cast<int>(overrideRow.overrides.size())) {
        continue;
      }
      spanSlotMask |= overrideRow.GetOverrideDisplaySlotMask(
          overrideRow
              .overrides[static_cast<std::size_t>(overrideRef.overrideIndex)]);
    }
    return spanSlotMask;
  };

  std::vector<DisplayRowGroup> baseDisplayRowGroups;
  std::vector<DisplayRowGroup> conditionalDisplayRowGroups;
  baseDisplayRowGroups.reserve(displayRowGroups.size());
  for (const auto &sourceGroup : displayRowGroups) {
    auto baseGroup = sourceGroup;
    std::erase_if(baseGroup.overrides, [&](const DisplayOverrideRef &a_ref) {
      return a_ref.rowIndex < 0 ||
             a_ref.rowIndex >= static_cast<int>(rows.size()) ||
             rows[static_cast<std::size_t>(a_ref.rowIndex)]
                 .conditionId.has_value();
    });
    baseGroup.actualSpanSlotMask = computeActualSpanSlotMask(baseGroup);
    baseGroup.overrideSpanSlotMask = computeOverrideSpanSlotMask(baseGroup);
    baseGroup.spanSlotMask =
        baseGroup.actualSpanSlotMask | baseGroup.overrideSpanSlotMask;
    if (baseGroup.spanSlotMask == 0) {
      baseGroup.spanSlotMask = baseGroup.slotMask;
    }
    if (baseGroup.actualRowIndex >= 0 || !baseGroup.overrides.empty() ||
        baseGroup.roles.emptySlotRowIndex >= 0) {
      baseDisplayRowGroups.push_back(std::move(baseGroup));
    }

    for (const auto conditionRowIndex :
         sourceGroup.roles.conditionalFittingRowIndices) {
      auto conditionGroup = sourceGroup;
      conditionGroup.conditionalSection = true;
      conditionGroup.conditionRowIndex = conditionRowIndex;
      conditionGroup.actualRowIndex = -1;
      conditionGroup.actualRowIndices.clear();
      conditionGroup.displayRowIndex = conditionRowIndex;
      conditionGroup.dropTargetRowIndex = conditionRowIndex;
      conditionGroup.memberRowIndices = {conditionRowIndex};
      std::erase_if(conditionGroup.overrides,
                    [&](const DisplayOverrideRef &a_ref) {
                      return a_ref.rowIndex != conditionRowIndex;
                    });
      conditionGroup.actualSpanSlotMask = 0;
      conditionGroup.overrideSpanSlotMask =
          computeOverrideSpanSlotMask(conditionGroup);
      conditionGroup.spanSlotMask = conditionGroup.overrideSpanSlotMask;
      if (conditionGroup.spanSlotMask == 0) {
        conditionGroup.spanSlotMask = selectPrimaryDisplaySlot(
            rows[static_cast<std::size_t>(conditionRowIndex)]
                .GetSelectionDisplaySlotMask());
      }
      if (conditionGroup.spanSlotMask == 0) {
        conditionGroup.spanSlotMask = conditionGroup.slotMask;
      }
      conditionalDisplayRowGroups.push_back(std::move(conditionGroup));
    }
  }

  const auto baseDisplayCount = baseDisplayRowGroups.size();
  const auto conditionalDisplayCount = conditionalDisplayRowGroups.size();
  displayRowGroups.clear();
  displayRowGroups.reserve(baseDisplayRowGroups.size() +
                           conditionalDisplayRowGroups.size());
  std::ranges::move(baseDisplayRowGroups, std::back_inserter(displayRowGroups));
  std::ranges::move(conditionalDisplayRowGroups,
                    std::back_inserter(displayRowGroups));

  // Keep the condition section deterministic without splitting a single
  // condition across the list: condition card first, then ascending slot,
  // then the original registration order (older rows first).  Sorting is
  // deferred while this workbench session receives drag/drop edits so a card
  // stays where the user placed it until the next open.
  const auto slotMaskRank = [](const std::uint64_t a_slotMask) {
    for (std::uint32_t slot = 30; slot <= 61; ++slot) {
      if ((a_slotMask & sfs::armor::GetArmorSlotMask(slot)) != 0) {
        return static_cast<std::size_t>(slot);
      }
    }
    return std::size_t{62};
  };
  const auto conditionSlotRank = [&](const DisplayRowGroup &a_group) {
    // Once the action card is deleted, its former slot must not keep affecting
    // the next-session ordering. Empty action sides sort after concrete slots.
    if (a_group.overrides.empty()) {
      return std::size_t{62};
    }
    return slotMaskRank(a_group.slotMask);
  };
  const auto conditionGroupKey = [&](const DisplayRowGroup &a_group) {
    if (a_group.conditionRowIndex >= 0 &&
        a_group.conditionRowIndex < static_cast<int>(rows.size())) {
      const auto &row =
          rows[static_cast<std::size_t>(a_group.conditionRowIndex)];
      if (row.conditionId.has_value()) {
        return *row.conditionId;
      }
    }
    return std::string{};
  };
  // The unified fitting/actual condition-entry list below owns the final
  // ordering. Sorting only fitting rows here would make their relative order
  // disagree with conditional actual-equipment rules.

  const auto buildConditionExpression = [&](const int a_rowIndex) {
    std::string expression;
    if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows.size())) {
      return expression;
    }
    const auto &row = rows[static_cast<std::size_t>(a_rowIndex)];
    if (!row.conditionId.has_value()) {
      return expression;
    }
    const auto materialized = conditions::MaterializeConditionById(
        *row.conditionId, ConditionDefinitions());
    if (!materialized.has_value()) {
      return expression;
    }
    for (const auto &group : materialized->displayCnf) {
      if (group.empty()) {
        continue;
      }
      if (!expression.empty()) {
        expression.append(" AND ");
      }
      if (group.size() > 1) {
        expression.push_back('(');
      }
      for (std::size_t index = 0; index < group.size(); ++index) {
        if (index > 0) {
          expression.append(" OR ");
        }
        expression.append(group[index]);
      }
      if (group.size() > 1) {
        expression.push_back(')');
      }
    }
    return expression;
  };

  if (ImGui::BeginChild("##variant-workbench-scroll", ImVec2(0.0f, tableHeight),
                        ImGuiChildFlags_None)) {
    if (ImGui::BeginTable("##variant-workbench", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 0.0f))) {
      const bool showStickyConditionalHeader =
          baseDisplayCount != 0 && workbenchStickyConditionalHeader_;
      const auto stickyLeftHeaderLabel = localization->Get(
          showStickyConditionalHeader ? "workbench.condition_setup"
                                      : "workbench.equipped");
      const auto stickyRightHeaderLabel = localization->Get(
          showStickyConditionalHeader ? "workbench.action_setup"
                                      : "workbench.overrides");
      const std::string stickyLeftHeader =
          std::string(stickyLeftHeaderLabel) + "###workbench-left-header";
      const std::string stickyRightHeader =
          std::string(stickyRightHeaderLabel) + "###workbench-right-header";
      ImGui::TableSetupColumn(stickyLeftHeader.c_str(),
                              ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableSetupColumn(stickyRightHeader.c_str(),
                              ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::PushID("workbench-sticky-header");
      ImGui::TableNextRow(ImGuiTableRowFlags_Headers,
                          ImGui::TableGetHeaderRowHeight());
      ImGui::TableSetColumnIndex(0);
      if (showStickyConditionalHeader) {
        DrawWorkbenchSortableHeader(
            stickyLeftHeaderLabel.data(), "sticky-condition-sort",
            workbenchConditionalSort_, WorkbenchSortColumn::Left);
      } else {
        DrawWorkbenchSortableHeader(stickyLeftHeaderLabel.data(),
                                    "sticky-actual-sort",
                                    workbenchBaseSort_,
                                    WorkbenchSortColumn::Left);
      }
      ImGui::TableSetColumnIndex(1);
      if (showStickyConditionalHeader) {
        DrawWorkbenchSortableHeader(
            stickyRightHeaderLabel.data(), "sticky-action-sort",
            workbenchConditionalSort_, WorkbenchSortColumn::Right);
      } else {
        DrawWorkbenchSortableHeader(stickyRightHeaderLabel.data(),
                                    "sticky-fitting-sort",
                                    workbenchBaseSort_,
                                    WorkbenchSortColumn::Right);
      }
      ImGui::PopID();
      const auto *workbenchTable = ImGui::GetCurrentTable();
      const float stickyHeaderBottomY =
          workbenchTable != nullptr
              ? ImGui::TableGetCellBgRect(workbenchTable, 0).Max.y
              : 0.0f;
      bool stickyConditionalHeaderNextFrame = false;
      const auto drawEmptyBaseRow = [&]() {
        const auto widgetHeight =
            18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto rowHeight =
            widgetHeight + ui::workbench::kWorkbenchRowGapY;
        const auto cellPadding = ImGui::GetStyle().CellPadding;
        const auto *theme = ThemeConfig::GetSingleton();
        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               theme->GetColorU32("BG"));
        ImGui::TableSetColumnIndex(0);

        const auto *table = ImGui::GetCurrentTable();
        if (table == nullptr) {
          return;
        }
        const auto leftCellRect = ImGui::TableGetCellBgRect(table, 0);
        const auto rightCellRect = ImGui::TableGetCellBgRect(table, 1);
        ImRect contentRect = leftCellRect;
        contentRect.Max.x = rightCellRect.Max.x;
        contentRect.Min.x =
            (std::max)(contentRect.Min.x, table->InnerClipRect.Min.x) +
            cellPadding.x;
        contentRect.Min.y += cellPadding.y;
        contentRect.Max.x =
            (std::min)(contentRect.Max.x, table->InnerClipRect.Max.x) -
            cellPadding.x;
        contentRect.Max.y -= cellPadding.y;

        const std::string label(
            localization->Get("workbench.empty.no_rows"));
        const auto labelSize = ImGui::CalcTextSize(label.c_str());
        const auto labelPosition = ImVec2(
            contentRect.Min.x +
                ((contentRect.GetWidth() - labelSize.x) * 0.5f),
            contentRect.Min.y +
                ((contentRect.GetHeight() - labelSize.y) * 0.5f));
        auto *drawList = ImGui::GetWindowDrawList();
        const auto drawLabelPart = [&](const ImRect &a_clipRect) {
          auto visibleClipRect = a_clipRect;
          visibleClipRect.ClipWithFull(table->InnerClipRect);
          if (visibleClipRect.GetWidth() <= 0.0f ||
              visibleClipRect.GetHeight() <= 0.0f) {
            return;
          }
          drawList->PushClipRect(visibleClipRect.Min, visibleClipRect.Max,
                                 true);
          drawList->AddText(labelPosition,
                            theme->GetColorU32("TEXT_DISABLED"),
                            label.c_str());
          drawList->PopClipRect();
        };
        drawLabelPart(ImRect(
            contentRect.Min,
            ImVec2((std::min)(contentRect.Max.x, leftCellRect.Max.x),
                   contentRect.Max.y)));
        ImGui::TableSetColumnIndex(1);
        drawLabelPart(ImRect(
            ImVec2((std::max)(contentRect.Min.x, rightCellRect.Min.x),
                   contentRect.Min.y),
            contentRect.Max));
      };
      if (baseDisplayCount == 0) {
        drawEmptyBaseRow();
      }
      const auto drawConditionSectionHeader = [&]() {
        ImGui::PushID("workbench-condition-section-header");
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers,
                            ImGui::TableGetHeaderRowHeight());
        ImGui::TableSetColumnIndex(0);
        DrawWorkbenchSortableHeader(
            localization->GetCStr("workbench.condition_setup"),
            "condition-section-sort", workbenchConditionalSort_,
            WorkbenchSortColumn::Left);
        ImGui::TableSetColumnIndex(1);
        DrawWorkbenchSortableHeader(
            localization->GetCStr("workbench.action_setup"),
            "action-section-sort", workbenchConditionalSort_,
            WorkbenchSortColumn::Right);
        if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
          stickyConditionalHeaderNextFrame =
              ImGui::TableGetCellBgRect(table, 0).Min.y <=
              stickyHeaderBottomY + 0.5f;
        }
        ImGui::PopID();
      };

      struct PendingConditionalRowAction {
        int rowIndex{-1};
        std::uint64_t targetUiIdentity{0};
        std::optional<std::string> conditionId;
        std::optional<DraggedConditionPayload> draggedCondition;
      };
      struct PendingCardConditionAssignment {
        int rowIndex{-1};
        int overrideIndex{-1};
        std::optional<std::string> conditionId;
      };
      std::optional<PendingCardConditionAssignment>
          pendingCardConditionAssignment;
      struct PendingConditionalFittingReplacement {
        int rowIndex{-1};
        std::uint64_t targetUiIdentity{0};
        RE::FormID formID{0};
        DraggedEquipmentPayload draggedItem{};
      };
      std::optional<PendingConditionalRowAction> pendingConditionalRowAction;
      std::optional<PendingConditionalFittingReplacement>
          pendingConditionalFittingReplacement;
      struct PendingVisibilityRuleDelete {
        std::size_t ruleIndex{0};
        bool preserveTarget{false};
      };
      std::optional<PendingVisibilityRuleDelete> pendingVisibilityRuleDelete;
      struct PendingVisibilityRuleReplacement {
        std::size_t ruleIndex{0};
        std::uint64_t targetUiIdentity{0};
        workbench::ConditionalVisibilityTargetKind targetKind{
            workbench::ConditionalVisibilityTargetKind::Fitting};
        RE::FormID formID{0};
        bool visibleWhenTrue{true};
        DraggedEquipmentPayload draggedItem{};
      };
      std::optional<PendingVisibilityRuleReplacement>
          pendingVisibilityRuleReplacement;
      struct PendingVisibilityRuleFittingConversion {
        std::size_t ruleIndex{0};
        std::uint64_t targetUiIdentity{0};
        RE::FormID formID{0};
        DraggedEquipmentPayload draggedItem{};
      };
      std::optional<PendingVisibilityRuleFittingConversion>
          pendingVisibilityRuleFittingConversion;
      struct PendingVisibilityRuleConditionAssignment {
        std::size_t ruleIndex{0};
        std::uint64_t targetUiIdentity{0};
        std::string conditionId;
        std::optional<DraggedConditionPayload> draggedCondition;
      };
      std::optional<PendingVisibilityRuleConditionAssignment>
          pendingVisibilityRuleConditionAssignment;
      struct PendingConditionalRowVisibilityConversion {
        int rowIndex{-1};
        std::uint64_t targetUiIdentity{0};
        workbench::ConditionalVisibilityTargetKind targetKind{
            workbench::ConditionalVisibilityTargetKind::Fitting};
        RE::FormID formID{0};
        bool visibleWhenTrue{true};
        DraggedEquipmentPayload draggedItem{};
      };
      std::optional<PendingConditionalRowVisibilityConversion>
          pendingConditionalRowVisibilityConversion;
      bool refreshNativeAfterTable = false;
      bool syncRowsAfterTable = false;
      const auto freezeWorkbenchOrderForEdit = [&]() {
        if (workbenchSortDeferredUntilClose_) {
          return;
        }
        workbenchBaseSessionOrder_.clear();
        std::size_t baseOrder = 0;
        for (const auto &group : displayRowGroups) {
          if (group.conditionalSection || group.displayRowIndex < 0 ||
              group.displayRowIndex >= static_cast<int>(rows.size())) {
            continue;
          }
          workbenchBaseSessionOrder_.try_emplace(
              rows[static_cast<std::size_t>(group.displayRowIndex)].uiIdentity,
              baseOrder++);
        }
        workbenchSortDeferredUntilClose_ = true;
      };
      const auto drawConditionAssignmentMenu =
          [&](const int a_rowIndex, const int a_overrideIndex = -1) {
            if (ImGui::BeginMenu(
                    localization->GetCStr("workbench.condition_setup"))) {
              if (ImGui::MenuItem(
                      localization->GetCStr("common.none"))) {
                pendingCardConditionAssignment =
                    PendingCardConditionAssignment{a_rowIndex, a_overrideIndex,
                                                   std::nullopt};
              }
              ImGui::Separator();
              for (const auto &definition : ConditionDefinitions()) {
                if (!IsWorkbenchSelectableCondition(definition)) {
                  continue;
                }
                if (ImGui::MenuItem(definition.name.c_str())) {
                  pendingCardConditionAssignment =
                      PendingCardConditionAssignment{a_rowIndex, a_overrideIndex,
                                                     definition.id};
                }
              }
              ImGui::EndMenu();
            }
          };
      const auto drawVisibilityRuleConditionAssignmentMenu =
          [&](const std::size_t a_ruleIndex,
              const std::string_view a_currentConditionId) {
            if (!ImGui::BeginMenu(
                    localization->GetCStr("workbench.condition_setup"))) {
              return;
            }
            if (ImGui::MenuItem(localization->GetCStr("common.none"), nullptr,
                                a_currentConditionId.empty())) {
              pendingVisibilityRuleConditionAssignment =
                  PendingVisibilityRuleConditionAssignment{
                      a_ruleIndex,
                      conditionalVisibilityRules[a_ruleIndex].uiIdentity, {}};
            }
            ImGui::Separator();
            for (const auto &definition : ConditionDefinitions()) {
              if (!IsWorkbenchSelectableCondition(definition)) {
                continue;
              }
              if (ImGui::MenuItem(definition.name.c_str(), nullptr,
                                  definition.id == a_currentConditionId)) {
                pendingVisibilityRuleConditionAssignment =
                    PendingVisibilityRuleConditionAssignment{
                        a_ruleIndex,
                        conditionalVisibilityRules[a_ruleIndex].uiIdentity,
                        definition.id};
              }
            }
            ImGui::EndMenu();
          };
      std::unordered_map<std::string, ImRect> widgetRects;
      widgetRects.reserve(displayRowGroups.size() * 3);
      std::vector<std::string> hoveredConflictWidgetIds;
      bool conditionSectionHeaderDrawn = false;
      const auto drawDisplayGroup = [&](const DisplayRowGroup &displayGroup) {
        if (displayGroup.conditionalSection && !conditionSectionHeaderDrawn) {
          drawConditionSectionHeader();
          conditionSectionHeaderDrawn = true;
        }
        const int displayRowIndex = displayGroup.displayRowIndex;
        const auto &displayRow =
            rows[static_cast<std::size_t>(displayRowIndex)];
        const auto overrideCount = displayGroup.overrides.size();
        const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto cardHeightForSlotMask = [&](const std::uint64_t a_slotMask) {
          const auto span =
              (std::max)(1, static_cast<int>(std::popcount(a_slotMask)));
          return (widgetHeight * static_cast<float>(span)) +
                 (ui::workbench::kWorkbenchOverrideGapY *
                  static_cast<float>(span - 1));
        };
        const auto missingActualSlotMasks = SplitWorkbenchSlotMask(
            displayGroup.spanSlotMask & ~displayGroup.actualSpanSlotMask);
        float actualGridHeight = 0.0f;
        std::size_t actualGridCardCount = 0;
        const auto appendActualGridCardHeight = [&](const float a_cardHeight) {
          if (actualGridCardCount > 0) {
            actualGridHeight += ui::workbench::kWorkbenchOverrideGapY;
          }
          actualGridHeight += a_cardHeight;
          ++actualGridCardCount;
        };
        if (!displayGroup.conditionalSection) {
          for (const auto actualRowIndex : displayGroup.actualRowIndices) {
            if (actualRowIndex < 0 ||
                actualRowIndex >= static_cast<int>(rows.size())) {
              continue;
            }
            const auto &actualRow =
                rows[static_cast<std::size_t>(actualRowIndex)];
            if (!actualRow.isEquipped || actualRow.IsSlotRow()) {
              continue;
            }
            const auto actualSlotMask = actualRow.GetSelectionDisplaySlotMask();
            if (actualSlotMask == 0) {
              continue;
            }
            appendActualGridCardHeight(cardHeightForSlotMask(actualSlotMask));
          }
        }
        for ([[maybe_unused]] const auto slotMask : missingActualSlotMasks) {
          appendActualGridCardHeight(widgetHeight);
        }
        if (actualGridCardCount == 0) {
          actualGridHeight = widgetHeight;
        }

        float dropZoneHeight = 0.0f;
        std::size_t overrideGridCardCount = 0;
        for (const auto &overrideRef : displayGroup.overrides) {
          if (overrideRef.rowIndex < 0 ||
              overrideRef.rowIndex >= static_cast<int>(rows.size())) {
            continue;
          }
          const auto &overrideRow =
              rows[static_cast<std::size_t>(overrideRef.rowIndex)];
          if (overrideRef.overrideIndex < 0 ||
              overrideRef.overrideIndex >=
                  static_cast<int>(overrideRow.overrides.size())) {
            continue;
          }
          if (overrideGridCardCount > 0) {
            dropZoneHeight += ui::workbench::kWorkbenchOverrideGapY;
          }
          const auto &overrideItem =
              overrideRow.overrides[static_cast<std::size_t>(
                  overrideRef.overrideIndex)];
          const auto overrideSlotMask =
              overrideRow.GetOverrideDisplaySlotMask(overrideItem);
          dropZoneHeight += cardHeightForSlotMask(overrideSlotMask);
          ++overrideGridCardCount;
        }
        const auto missingOverrideSlotMasks = SplitWorkbenchSlotMask(
            displayGroup.spanSlotMask & ~displayGroup.overrideSpanSlotMask);
        for (const auto unusedSlotMask : missingOverrideSlotMasks) {
          static_cast<void>(unusedSlotMask);
          if (overrideGridCardCount > 0) {
            dropZoneHeight += ui::workbench::kWorkbenchOverrideGapY;
          }
          dropZoneHeight += widgetHeight;
          ++overrideGridCardCount;
        }
        if (overrideGridCardCount == 0) {
          dropZoneHeight = widgetHeight;
        }
        const auto actualContentHeight = actualGridHeight;
        const auto contentHeight =
            (std::max)(actualContentHeight, dropZoneHeight);
        const auto rowHeight = contentHeight + ui::workbench::kWorkbenchRowGapY;
        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               ThemeConfig::GetSingleton()->GetColorU32("BG"));

        ImGui::TableSetColumnIndex(0);
        const auto *table = ImGui::GetCurrentTable();
        const ImRect leftCellRect = table ? ImGui::TableGetCellBgRect(table, 0)
                                          : ImRect(ImGui::GetCursorScreenPos(),
                                                   ImGui::GetCursorScreenPos());
        if (table) {
          const auto leftCellContentHeight =
              (leftCellRect.Max.y - leftCellRect.Min.y) -
              (ImGui::GetStyle().CellPadding.y * 2.0f);
          const auto leftCellContentOffsetY =
              (std::max)(0.0f, (leftCellContentHeight - contentHeight) * 0.5f);
          ImGui::SetCursorScreenPos(
              ImVec2(leftCellRect.Min.x + ImGui::GetStyle().CellPadding.x,
                     leftCellRect.Min.y + ImGui::GetStyle().CellPadding.y +
                         leftCellContentOffsetY));
        }

        ui::components::EquipmentWidgetResult equippedWidget{};
        const auto drawMissingActualSlotCards = [&]() {
          for (const auto slotMask : missingActualSlotMasks) {
            auto slotItem = displayRow.equipped;
            slotItem.kind = workbench::EquipmentWidgetItemKind::Slot;
            slotItem.formID = 0;
            slotItem.key =
                displayRow.key + ":missing-real:" + std::to_string(slotMask);
            slotItem.name =
                std::string(localization->Get("workbench.real_clothing.none"));
            slotItem.slotText = BuildMultilineWorkbenchSlotText(slotMask);
            slotItem.slotMask = slotMask;
            slotItem.hasArmorAddons = true;
            slotItem.hidden = false;
            const auto slotWidget = ui::components::DrawEquipmentWidget(
                slotItem.key.c_str(), slotItem,
                {.showDeleteButton = false,
                 .deleteButtonEnabled = false,
                 .disabledAppearance = true,
                 .interactive = false,
                 .minimumHeight = widgetHeight,
                 .slotIconText = kWorkbenchActualSlotIcon.data(),
                 .slotIconScale = 0.72f});
            equippedWidget.hovered =
                equippedWidget.hovered || slotWidget.hovered;
            widgetRects.insert_or_assign(
                slotItem.key,
                ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
          }
        };
        if (displayGroup.conditionalSection) {
          const bool hasConditionState =
              displayGroup.conditionRowIndex >= 0 &&
              displayGroup.conditionRowIndex <
                  static_cast<int>(rowConditionStates.size());
          const auto *conditionState =
              hasConditionState ? &rowConditionStates[static_cast<std::size_t>(
                                      displayGroup.conditionRowIndex)]
                                : nullptr;
          const bool conditionMet =
              isConditionalRowActive(displayGroup.conditionRowIndex);
          const std::string conditionName =
              conditionState != nullptr && !conditionState->name.empty()
                  ? conditionState->name
                  : std::string(localization->Get("common.unknown"));
          const auto &conditionRow =
              rows[static_cast<std::size_t>(displayGroup.conditionRowIndex)];
          const auto *conditionDefinition =
              conditionRow.conditionId.has_value()
                  ? conditions::FindDefinitionById(ConditionDefinitions(),
                                                   *conditionRow.conditionId)
                  : nullptr;
          // A row may intentionally remain after its condition definition was
          // deleted.  Treat that row as an empty drop target instead of
          // displaying a misleading "missing condition" card.
          const bool hasConditionDefinition = conditionDefinition != nullptr;
          const bool builtInCondition =
              conditionRow.conditionId.has_value() &&
              conditions::IsBuiltInCondition(*conditionRow.conditionId);
          std::string conditionTypeSummary(localization->Get(
              builtInCondition ? "workbench.condition.basic_summary"
                               : "workbench.condition.user_defined_summary"));
          conditionTypeSummary.push_back(' ');
          conditionTypeSummary.append(
              std::to_string(conditionDefinition != nullptr
                                 ? conditionDefinition->clauses.size()
                                 : 0));
          const auto *conditionStatus = localization->GetCStr(
              conditionMet ? "workbench.condition.met"
                           : "workbench.condition.not_met");
          const auto cellPadding = ImGui::GetStyle().CellPadding;
          const auto conditionCardMin =
              ImVec2(leftCellRect.Min.x + cellPadding.x,
                     ImGui::GetCursorScreenPos().y);
          const auto conditionCardMax =
              ImVec2(leftCellRect.Max.x - cellPadding.x,
                     conditionCardMin.y + contentHeight);
          ImGui::PushID(displayGroup.conditionRowIndex);
          ImGui::SetCursorScreenPos(conditionCardMin);
          ImGui::InvisibleButton(
              "##condition-summary-card",
              ImVec2(
                  (std::max)(1.0f, conditionCardMax.x - conditionCardMin.x),
                  (std::max)(1.0f, conditionCardMax.y - conditionCardMin.y)));
          const bool conditionCardHovered = ImGui::IsItemHovered();
          const ImRect conditionCardRect(ImGui::GetItemRectMin(),
                                         ImGui::GetItemRectMax());
          if (hasConditionDefinition && ImGui::BeginDragDropSource()) {
            DraggedConditionPayload payload{};
            std::snprintf(payload.conditionId.data(),
                          payload.conditionId.size(), "%s",
                          conditionDefinition->id.c_str());
            payload.sourceKind = static_cast<std::uint32_t>(
                ConditionDragSourceKind::ConditionalRow);
            payload.sourceIndex = displayGroup.conditionRowIndex;
            payload.sourceUiIdentity = conditionRow.uiIdentity;
            ImGui::SetDragDropPayload(ui::workbench::kConditionPayloadType,
                                      &payload, sizeof(payload));
            ImGui::TextUnformatted(conditionDefinition->name.c_str());
            ImGui::EndDragDropSource();
          }
          const auto *theme = ThemeConfig::GetSingleton();
          const auto accentColor =
              conditionState != nullptr && conditionState->color.has_value()
                  ? *conditionState->color
                  : theme->GetColor("PRIMARY");
          auto *drawList = ImGui::GetWindowDrawList();
          const auto cardRounding =
              hasConditionDefinition ? ImGui::GetStyle().FrameRounding
                                     : kEmptyConditionCardRounding;
          constexpr float conditionDeletePaneWidth = 34.0f;
          const auto conditionDeleteMin =
              ImVec2(conditionCardRect.Max.x - conditionDeletePaneWidth,
                     conditionCardRect.Min.y);
          const auto conditionDeleteMax = conditionCardRect.Max;
          const auto conditionDeleteState = hasConditionDefinition
              ? ui::input_widgets::EvaluateRectClickTarget(
                    ImGui::GetID("##condition-summary-delete"),
                    conditionDeleteMin, conditionDeleteMax)
              : ui::input_widgets::RectClickTargetState{};
          equippedWidget.hovered =
              conditionCardHovered || conditionDeleteState.hovered;
          const auto bodyColor =
              hasConditionDefinition
                  ? (conditionCardHovered ? ConditionSurfaceColor(42, 42, 44)
                                          : ConditionSurfaceColor(34, 34, 36))
                  : EmptyConditionCardSurfaceColor(conditionCardHovered);
          const auto borderColor =
              hasConditionDefinition
                  ? theme->GetColorU32("BORDER")
                  : EmptyConditionCardBorderColor(theme);
          drawList->AddRectFilled(conditionCardRect.Min, conditionCardRect.Max,
                                  bodyColor, cardRounding);
          drawList->AddRect(conditionCardRect.Min, conditionCardRect.Max,
                            borderColor, cardRounding);
          if (hasConditionDefinition) {
            drawList->AddRectFilled(
                conditionCardRect.Min,
                ImVec2(conditionCardRect.Min.x + 5.0f,
                       conditionCardRect.Max.y),
                ImGui::GetColorU32(accentColor), cardRounding,
                ImDrawFlags_RoundCornersTopLeft |
                    ImDrawFlags_RoundCornersBottomLeft);
          }
          constexpr bool conditionActionsLocked = false;
          const auto conditionDeleteFill =
              conditionActionsLocked
                  ? theme->GetColorU32("TEXT_DISABLED", 0.26f)
              : conditionDeleteState.held ? theme->GetColorU32("DECLINE")
              : conditionDeleteState.hovered
                  ? theme->GetColorU32("DECLINE", 0.95f)
                  : theme->GetColorU32("DECLINE", 0.78f);
          if (hasConditionDefinition) {
            drawList->AddRectFilled(conditionDeleteMin, conditionDeleteMax,
                                    conditionDeleteFill, cardRounding,
                                    ImDrawFlags_RoundCornersRight);
            drawList->AddLine(
                ImVec2(conditionDeleteMin.x, conditionCardRect.Min.y + 1.0f),
                ImVec2(conditionDeleteMin.x, conditionCardRect.Max.y - 1.0f),
                theme->GetColorU32("BORDER"));
            const auto conditionDeleteIconSize =
                ImGui::CalcTextSize(kWorkbenchTrashIcon.data());
            drawList->AddText(
                ImVec2(conditionDeleteMin.x +
                           (((conditionDeleteMax.x - conditionDeleteMin.x) -
                             conditionDeleteIconSize.x) *
                            0.5f),
                       conditionCardRect.Min.y +
                           ((conditionCardRect.GetHeight() -
                             conditionDeleteIconSize.y) * 0.5f)),
                conditionActionsLocked ? theme->GetColorU32("TEXT_DISABLED")
                                       : theme->GetColorU32("TEXT"),
                kWorkbenchTrashIcon.data());
            if (!conditionActionsLocked && conditionDeleteState.pressed) {
              pendingConditionalRowAction = PendingConditionalRowAction{
                  displayGroup.conditionRowIndex, conditionRow.uiIdentity,
                  std::nullopt};
            }
            if (conditionDeleteState.hovered && !ImGui::IsDragDropActive()) {
              ImGui::SetTooltip("%s",
                                localization->GetCStr("common.delete"));
            }
          }

          const auto lineHeight = ImGui::GetTextLineHeight();
          const auto textBlockHeight = (lineHeight * 2.0f) + 4.0f;
          const auto textY =
              conditionCardRect.Min.y +
              ((conditionCardRect.GetHeight() - textBlockHeight) * 0.5f);
          const auto textMinX = conditionCardRect.Min.x + 13.0f;
          const auto textMaxX = hasConditionDefinition
                                    ? conditionDeleteMin.x - 10.0f
                                    : conditionCardRect.Max.x - 10.0f;
          drawList->PushClipRect(ImVec2(textMinX, conditionCardRect.Min.y),
                                 ImVec2(textMaxX, conditionCardRect.Max.y),
                                 true);
          if (hasConditionDefinition) {
            const auto statusSize = ImGui::CalcTextSize(conditionStatus);
            const auto statusX = textMaxX - statusSize.x;
            const auto compactName = ui::catalog::TruncateTextToWidth(
                conditionName, (std::max)(1.0f, textMaxX - textMinX));
            const auto compactSummary = ui::catalog::TruncateTextToWidth(
                conditionTypeSummary,
                (std::max)(1.0f, statusX - textMinX - 10.0f));
            drawList->AddText(
                ImVec2(textMinX, textY),
                conditionState != nullptr && conditionState->disabled
                    ? theme->GetColorU32("TEXT_DISABLED")
                    : theme->GetColorU32("TEXT"),
                compactName.c_str());
            drawList->AddText(
                ImVec2(statusX, textY + lineHeight + 4.0f),
                conditionMet ? theme->GetColorU32("TEXT_HEADER", 0.92f)
                             : theme->GetColorU32("TEXT_DISABLED"),
                conditionStatus);
            drawList->AddText(ImVec2(textMinX, textY + lineHeight + 4.0f),
                              theme->GetColorU32("TEXT_DISABLED"),
                              compactSummary.c_str());
          } else {
            const auto prompt = localization->Get(
                "workbench.slot_create.condition_prompt");
            drawList->AddText(
                ImVec2(textMinX, textY + (lineHeight * 0.5f)),
                theme->GetColorU32("TEXT_DISABLED"), prompt.data());
          }
          drawList->PopClipRect();

          const auto *conditionCardDragPayload = ImGui::GetDragDropPayload();
          if (!conditionActionsLocked && conditionCardDragPayload != nullptr &&
              conditionCardDragPayload->IsDataType(
                  ui::workbench::kConditionPayloadType) &&
              ImGui::BeginDragDropTargetCustom(
                  conditionCardRect,
                  ImGui::GetID("##condition-summary-target"))) {
            if (const auto *payload = ImGui::AcceptDragDropPayload(
                    ui::workbench::kConditionPayloadType);
                payload && payload->Data != nullptr &&
                payload->DataSize == sizeof(DraggedConditionPayload)) {
              DraggedConditionPayload conditionPayload{};
              std::memcpy(&conditionPayload, payload->Data,
                          sizeof(conditionPayload));
              const std::string conditionId(
                  conditionPayload.conditionId.data());
              if (const auto *definition = conditions::FindDefinitionById(
                      ConditionDefinitions(), conditionId);
                  definition != nullptr &&
                  IsWorkbenchSelectableCondition(*definition) &&
                  (!conditionRow.conditionId.has_value() ||
                   *conditionRow.conditionId != definition->id)) {
                pendingConditionalRowAction = PendingConditionalRowAction{
                    displayGroup.conditionRowIndex, conditionRow.uiIdentity,
                    definition->id, conditionPayload};
              }
            }
            ImGui::EndDragDropTarget();
          }

          const auto conditionTooltipHovered = conditionCardHovered &&
                                               !conditionDeleteState.hovered &&
                                               !ImGui::IsDragDropActive();
          if (conditionDefinition != nullptr) {
            ui::workbench::DrawConditionDefinitionTooltip(
                "condition-summary:" +
                    std::to_string(displayGroup.conditionRowIndex),
                *conditionDefinition, conditionTooltipHovered,
                ConditionDefinitions());
          }
          ImGui::PopID();
        } else {
          const auto oldActualItemSpacing = ImGui::GetStyle().ItemSpacing;
          ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                              ImVec2(oldActualItemSpacing.x,
                                     ui::workbench::kWorkbenchOverrideGapY));
          for (const auto actualRowIndex : displayGroup.actualRowIndices) {
            if (actualRowIndex < 0 ||
                actualRowIndex >= static_cast<int>(rows.size())) {
              continue;
            }
            const auto &actualRow =
                rows[static_cast<std::size_t>(actualRowIndex)];
            if (!actualRow.isEquipped || actualRow.IsSlotRow()) {
              continue;
            }

            const auto actualSlotMask = actualRow.GetSelectionDisplaySlotMask();
            const auto *actualArmor =
                RE::TESForm::LookupByID<RE::TESObjectARMO>(
                    actualRow.equipped.formID);
            const bool sosTngControlledActual =
                armor::IsSosTngGenitalArmor(actualArmor);
            const bool ddRenderedDevice =
                sfs::poc::IsDeviousDevicesRenderedDevice(actualArmor);
            const bool alwaysVisibleActual =
                actualRow.IsAlwaysVisibleActualEquipment();
            const bool hideControlLocked = alwaysVisibleActual ||
                                           sosTngControlledActual;
            const bool baseActualHidden =
                workbench_.ResolveEquippedHiddenForActor(previewActor,
                                                         actualRow);
            const bool ddOrdinaryVisible =
                previewActor != nullptr &&
                sfs::poc::IsDeviousDevicesRenderedDeviceOrdinaryVisible(
                    previewActor->GetFormID(), actualRow.equipped.formID);
            const auto activeActualRule =
                activeActualVisibilityRules.find(actualRow.equipped.formID);
            const bool effectiveHidden =
                activeActualRule != activeActualVisibilityRules.end()
                    ? !conditionalVisibilityRules[activeActualRule->second]
                           .visibleWhenTrue
                : ddOrdinaryVisible
                    ? false
                    : baseActualHidden;
            const bool hasActualConflict = rowConflicts.contains(actualRow.key);
            auto actualDisplayItem = actualRow.equipped;
            actualDisplayItem.slotText =
                BuildMultilineWorkbenchSlotText(actualSlotMask);
            const bool activeConditionalActualTarget =
                activeActualVisibilityRules.contains(actualRow.equipped.formID);
            const auto actualWidget = ui::components::DrawEquipmentWidget(
                actualRow.key.c_str(), actualDisplayItem,
                {.showDeleteButton = false,
                 .deleteButtonEnabled = false,
                 .showHideButton = true,
                 .hideButtonEnabled =
                     !hideControlLocked && !activeConditionalActualTarget,
                 .hideButtonTooltipKey =
                     alwaysVisibleActual
                         ? "workbench.appearance_protected.tooltip"
                     : sosTngControlledActual
                         ? "workbench.external_control.sos_tng.tooltip"
                      : activeConditionalActualTarget
                          ? "workbench.condition.visibility_active"
                          : nullptr,
                 .hidden = effectiveHidden,
                 .preserveContentTextColors = true,
                 .interactive = true,
                 .minimumHeight = cardHeightForSlotMask(actualSlotMask),
                 .statusText =
                     activeConditionalActualTarget
                         ? localization->GetCStr(
                               "workbench.condition.visibility_active")
                         : nullptr,
                 .statusColor =
                     activeConditionalActualTarget
                         ? std::optional<ImVec4>{ThemeConfig::GetSingleton()
                                                     ->GetColor("TEXT_HEADER",
                                                                0.92f)}
                         : std::nullopt,
                 .slotIconText = kWorkbenchActualSlotIcon.data(),
                 .slotIconScale = 0.72f,
                 .conflictStyle =
                     hasActualConflict
                         ? ui::components::EquipmentWidgetConflictStyle::Warning
                         : ui::components::EquipmentWidgetConflictStyle::None,
                 .drawTooltipExtras =
                     hasActualConflict ? std::function<
                                             void()>{[&, actualRowIndex]() {
                       const auto &tooltipRow =
                           rows[static_cast<std::size_t>(actualRowIndex)];
                       const auto conflictIt =
                           rowConflicts.find(tooltipRow.key);
                       if (conflictIt == rowConflicts.end()) {
                         return;
                       }
                       ui::workbench::DrawConflictTooltipSection(
                           localization
                               ->Get("workbench.conflict.variant_selection")
                               .data(),
                           [&](const ThemeConfig *a_theme) {
                             for (const auto &description :
                                  conflictIt->second.targetDescriptions) {
                               ImGui::Bullet();
                               ImGui::SameLine(
                                   0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                               ImGui::BeginGroup();
                               ui::workbench::DrawWrappedColoredTextRuns(
                                   {{description.primaryName,
                                     a_theme->GetColorU32("TEXT")},
                                    {description.secondaryName.empty() ? ""
                                                                       : " (",
                                     a_theme->GetColorU32("TEXT_DISABLED")},
                                    {description.secondaryName,
                                     a_theme->GetColorU32("TEXT_DISABLED")},
                                    {description.secondaryName.empty() ? ""
                                                                       : ")",
                                     a_theme->GetColorU32("TEXT_DISABLED")},
                                    {description.targetLabel.empty() ? ""
                                                                     : "  [",
                                     a_theme->GetColorU32("TEXT")},
                                    {description.targetLabel,
                                     a_theme->GetColorU32("TEXT_HEADER",
                                                          0.92f)},
                                    {description.targetLabel.empty() ? "" : "]",
                                     a_theme->GetColorU32("TEXT")}});
                               ImGui::EndGroup();
                             }
                           });
                     }}
                                       : std::function<void()>{},
                 .drawContextMenuEntries =
                     [&, actualRowIndex]() {
                       drawConditionAssignmentMenu(actualRowIndex);
                     }});
            if (ImGui::BeginDragDropSource()) {
              DraggedEquipmentPayload payload{};
              payload.sourceKind =
                  static_cast<std::uint32_t>(DragSourceKind::Row);
            payload.rowIndex = actualRowIndex;
            payload.itemIndex = -1;
            payload.formID = actualRow.equipped.formID;
            payload.slotMask = actualSlotMask;
            payload.sourceUiIdentity = actualRow.uiIdentity;
              ImGui::SetDragDropPayload(ui::workbench::kVariantItemPayloadType,
                                        &payload, sizeof(payload));
              ImGui::TextUnformatted(actualRow.equipped.name.c_str());
              ImGui::TextUnformatted(actualDisplayItem.slotText.c_str());
              ImGui::EndDragDropSource();
            }
            equippedWidget.hovered =
                equippedWidget.hovered || actualWidget.hovered;
            const ImRect actualWidgetRect(ImGui::GetItemRectMin(),
                                          ImGui::GetItemRectMax());
            widgetRects.insert_or_assign(actualRow.key, actualWidgetRect);
            if (!alwaysVisibleActual && actualWidget.hideClicked) {
              const auto actorFormID = previewActor != nullptr
                                           ? previewActor->GetFormID()
                                           : RE::FormID{0};
              bool changed = false;
              const bool desiredHidden = !effectiveHidden;
              if (ddRenderedDevice) {
                sfs::poc::SetDeviousDevicesRenderedDeviceUserVisible(
                    actorFormID, actualRow.equipped.formID, !desiredHidden);
              } else if (baseActualHidden && previewActor != nullptr &&
                  HideRealEquipmentWithFittingForActor(previewActor)) {
                SetHideRealEquipmentWithFittingForActor(previewActor, false);
                changed = true;
              }
              changed |= workbench_.SetEquippedHiddenForActor(
                  actorFormID, actualRowIndex, desiredHidden);
              if (changed) {
                workbench_.RefreshNativeArmorOverridesForActorWithoutEquipmentSync(
                    actorFormID, conditionStore_.revision);
              }
            }
            if (hasActualConflict && actualWidget.hovered) {
              hoveredConflictWidgetIds =
                  rowConflicts.at(actualRow.key).targetWidgetIds;
            }
          }
          drawMissingActualSlotCards();
          if (displayGroup.actualRowIndices.empty() && equippedWidget.hovered &&
              rowConflicts.contains(displayRow.key)) {
            hoveredConflictWidgetIds =
                rowConflicts.at(displayRow.key).targetWidgetIds;
          }
          ImGui::PopStyleVar();
        }
        ImGui::TableSetColumnIndex(1);
        const auto *currentTable = ImGui::GetCurrentTable();
        const auto overrideCellRect =
            currentTable ? ImGui::TableGetCellBgRect(currentTable, 1)
                         : ImRect(ImGui::GetCursorScreenPos(),
                                  ImGui::GetCursorScreenPos());
        const auto cellPadding = ImGui::GetStyle().CellPadding;
        const auto overrideDropMin =
            ImVec2(overrideCellRect.Min.x + cellPadding.x,
                   overrideCellRect.Min.y + cellPadding.y);
        const auto overrideDropMaxX = overrideCellRect.Max.x - cellPadding.x;
        const auto overrideCellContentHeight =
            (overrideCellRect.Max.y - overrideCellRect.Min.y) -
            (cellPadding.y * 2.0f);
        const auto overrideCellContentOffsetY = (std::max)(
            0.0f, (overrideCellContentHeight - contentHeight) * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(overrideCellRect.Min.x + cellPadding.x,
                                         overrideCellRect.Min.y +
                                             cellPadding.y +
                                             overrideCellContentOffsetY));

        const auto drawMissingOverrideSlotCards = [&]() {
          for (const auto slotMask : missingOverrideSlotMasks) {
            auto slotItem = displayRow.equipped;
            slotItem.kind = workbench::EquipmentWidgetItemKind::Slot;
            slotItem.formID = 0;
            slotItem.key =
                displayRow.key + ":missing-fitting:" + std::to_string(slotMask);
            slotItem.name =
                std::string(localization->Get("workbench.no_fitting_override"));
            slotItem.slotText = BuildMultilineWorkbenchSlotText(slotMask);
            slotItem.slotMask = slotMask;
            slotItem.hasArmorAddons = true;
            slotItem.hidden = false;
            const auto activeFittingRowIndex =
                resolveActiveFittingRowForSlotMask(slotMask);
            const bool conditionActiveOnSlot =
                activeFittingRowIndex >= 0 &&
                activeFittingRowIndex < static_cast<int>(rows.size()) &&
                rows[static_cast<std::size_t>(activeFittingRowIndex)]
                    .conditionId.has_value();
            (void)ui::components::DrawEquipmentWidget(
                slotItem.key.c_str(), slotItem,
                {.showDeleteButton = false,
                 .deleteButtonEnabled = false,
                 .disabledAppearance = true,
                 .interactive = false,
                 .minimumHeight = widgetHeight,
                 .statusText =
                     conditionActiveOnSlot
                         ? localization->GetCStr(
                               "workbench.condition.visibility_active")
                         : nullptr,
                 .statusColor =
                     conditionActiveOnSlot
                         ? std::optional<ImVec4>{ThemeConfig::GetSingleton()
                                                     ->GetColor("TEXT_HEADER",
                                                                0.92f)}
                         : std::nullopt,
                 .slotIconText = kWorkbenchFittingSlotIcon.data(),
                 .slotIconScale = 0.72f});
            widgetRects.insert_or_assign(
                slotItem.key,
                ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
          }
        };

        if (overrideCount == 0 && displayGroup.conditionalSection) {
          // Deleting the registered appearance must leave the condition card
          // and a true empty action drop target, not a disabled slot card.
          const ImRect emptyActionRect(
              overrideDropMin,
              ImVec2(overrideDropMaxX,
                     overrideDropMin.y + (std::max)(widgetHeight,
                                                    contentHeight)));
          auto *drawList = ImGui::GetWindowDrawList();
          const auto hovered = ImGui::IsMouseHoveringRect(
              emptyActionRect.Min, emptyActionRect.Max, true);
          drawList->AddRectFilled(
              emptyActionRect.Min, emptyActionRect.Max,
              hovered ? ConditionSurfaceColor(18, 18, 20)
                      : ConditionSurfaceColor(10, 10, 12),
              8.0f);
          drawList->AddRect(
              emptyActionRect.Min, emptyActionRect.Max,
              ThemeConfig::GetSingleton()->GetColorU32("TEXT_DISABLED", 0.55f),
              8.0f);
          const auto *prompt =
              localization->GetCStr("workbench.slot_create.action_prompt");
          constexpr float promptPaddingX = 13.0f;
          const auto promptWidth = (std::max)(
              1.0f, emptyActionRect.GetWidth() - (promptPaddingX * 2.0f));
          const auto promptSize =
              ImGui::CalcTextSize(prompt, nullptr, false, promptWidth);
          const auto promptPosition = ImVec2(
              emptyActionRect.Min.x + promptPaddingX,
              emptyActionRect.Min.y +
                  ((emptyActionRect.GetHeight() - promptSize.y) * 0.5f));
          drawList->PushClipRect(emptyActionRect.Min, emptyActionRect.Max,
                                 true);
          drawList->AddText(
              ImGui::GetFont(), ImGui::GetFontSize(), promptPosition,
              ThemeConfig::GetSingleton()->GetColorU32("TEXT_DISABLED"),
              prompt, nullptr, promptWidth);
          drawList->PopClipRect();
        } else if (overrideCount == 0) {
          drawMissingOverrideSlotCards();
        } else {
          const auto oldItemSpacing = ImGui::GetStyle().ItemSpacing;
          ImGui::PushStyleVar(
              ImGuiStyleVar_ItemSpacing,
              ImVec2(oldItemSpacing.x, ui::workbench::kWorkbenchOverrideGapY));
          for (const auto &overrideRef : displayGroup.overrides) {
            const auto overrideRowIndex = overrideRef.rowIndex;
            const auto overrideIndex = overrideRef.overrideIndex;
            if (overrideRowIndex < 0 ||
                overrideRowIndex >= static_cast<int>(rows.size()) ||
                overrideRowIndex >=
                    static_cast<int>(rowConditionStates.size())) {
              continue;
            }
            const auto &overrideRow =
                rows[static_cast<std::size_t>(overrideRowIndex)];
            if (overrideIndex < 0 ||
                overrideIndex >=
                    static_cast<int>(overrideRow.overrides.size())) {
              continue;
            }
            const auto &overrideConditionState =
                rowConditionStates[static_cast<std::size_t>(overrideRowIndex)];
            const auto widgetId =
                "override:" + std::to_string(overrideRowIndex) + ":" +
                std::to_string(overrideIndex);
            const auto &overrideItem =
                overrideRow.overrides[static_cast<std::size_t>(overrideIndex)];
            const auto overrideSlotMask =
                overrideRow.GetOverrideDisplaySlotMask(overrideItem);
            const bool protectedAppearance =
                overrideRow.IsProtectedAppearance(overrideItem);
            auto overrideDisplayItem = overrideItem;
            overrideDisplayItem.slotText =
                BuildMultilineWorkbenchSlotText(overrideSlotMask);
            const bool isConditionalFitting =
                overrideRow.conditionId.has_value();
            const bool conditionalFittingConditionMet =
                isConditionalFitting &&
                isConditionalRowActive(overrideRowIndex);
            const auto activeFittingRowIndex =
                resolveActiveFittingRowForSlotMask(overrideSlotMask);
            const bool isActiveFittingRow =
                overrideRowIndex == activeFittingRowIndex;
            const bool conditionActiveOnBaseSlot =
                !isConditionalFitting && !isActiveFittingRow &&
                activeFittingRowIndex >= 0 &&
                activeFittingRowIndex < static_cast<int>(rows.size()) &&
                rows[static_cast<std::size_t>(activeFittingRowIndex)]
                    .conditionId.has_value();
            const bool manuallyHidden =
                !isConditionalFitting && overrideItem.hidden;
            const bool conditionalFittingHidden =
                isConditionalFitting && overrideItem.hidden;
            const bool baseFittingHidden =
                (!isConditionalFitting && globallyHideFittingOverrides) ||
                manuallyHidden;
            const bool virtualTokenAppearanceSuppressed =
                previewActor != nullptr &&
                (overrideSlotMask &
                 virtualTokenSuppressedFittingSlotMask) == 0 &&
                sfs::poc::IsVirtualWornTokenAppearanceSuppressed(
                    previewActor->GetFormID(), overrideItem.formID,
                    static_cast<std::uint32_t>(overrideSlotMask));
            const bool virtualTokenHidden =
                virtualTokenAppearanceSuppressed ||
                (overrideSlotMask &
                 virtualTokenSuppressedFittingSlotMask) != 0;
            const bool ddHiderHidden =
                (overrideSlotMask &
                 deviousDevicesHiderSuppressedFittingSlotMask) != 0;
            const bool headgearToggleHidden =
                (overrideSlotMask &
                 headgearToggleSuppressedFittingSlotMask) != 0;
            const bool automaticallyHidden =
                overrideRow.IsOverrideAutomaticallySuppressed(overrideItem) ||
                virtualTokenHidden || ddHiderHidden || headgearToggleHidden;
            const auto activeFittingRule =
                activeFittingVisibilityRules.find(overrideItem.formID);
            const bool conditionallyHidden =
                conditionActiveOnBaseSlot ? true
                : !isConditionalFitting &&
                        activeFittingRule != activeFittingVisibilityRules.end()
                    ? !conditionalVisibilityRules[activeFittingRule->second]
                           .visibleWhenTrue
                    : baseFittingHidden;
            const bool effectiveHidden =
                protectedAppearance ||
                (isConditionalFitting ? conditionalFittingHidden
                                      : conditionallyHidden) ||
                automaticallyHidden;
            const auto overrideCardHeight =
                cardHeightForSlotMask(overrideSlotMask);
            const bool activeConditionalFittingTarget =
                !isConditionalFitting &&
                (conditionActiveOnBaseSlot ||
                 activeFittingRule != activeFittingVisibilityRules.end());
            const auto *fittingStatusText =
                isConditionalFitting
                    ? localization->GetCStr(
                          isActiveFittingRow ? "workbench.fitting.applied"
                          : conditionalFittingConditionMet
                              ? "workbench.fitting.another_condition_applied"
                              : "workbench.fitting.not_applied")
                : activeConditionalFittingTarget
                    ? localization->GetCStr(
                          "workbench.condition.visibility_active")
                    : nullptr;
            const bool hasOverrideConflict =
                overrideConflicts.contains(widgetId);
            const std::string appliedConditionName =
                overrideRow.conditionId.has_value()
                    ? overrideConditionState.name
                    : std::string{};
            std::function<void()> overrideTooltipExtras{};
            if (hasOverrideConflict || !appliedConditionName.empty()) {
              overrideTooltipExtras = [&, widgetId, hasOverrideConflict,
                                       appliedConditionName]() {
                if (hasOverrideConflict) {
                  ui::workbench::DrawConflictTooltipSection(
                      localization->Get("workbench.conflict.visuals").data(),
                      [&](const ThemeConfig *a_theme) {
                        for (const auto &description :
                             overrideConflicts.at(widgetId)
                                 .targetDescriptions) {
                          ui::workbench::DrawConflictEntry(description,
                                                           a_theme);
                        }
                      });
                }
                if (!appliedConditionName.empty()) {
                  ImGui::Spacing();
                  ImGui::Separator();
                  ImGui::Spacing();
                  ImGui::TextWrapped(
                      "%s: %s",
                      localization->GetCStr("workbench.tooltip.condition"),
                      appliedConditionName.c_str());
                }
              };
            }
            ui::components::EquipmentWidgetResult overrideWidget{};
            overrideWidget = ui::components::DrawEquipmentWidget(
                widgetId.c_str(), overrideDisplayItem,
                {.showDeleteButton = true,
                 .deleteButtonEnabled = true,
                 .showHideButton = true,
                 .hideButtonEnabled = !protectedAppearance &&
                     !activeConditionalFittingTarget &&
                     (isConditionalFitting || isActiveFittingRow),
                 .hideButtonTooltipKey =
                     protectedAppearance
                         ? "workbench.appearance_protected.tooltip"
                     : isConditionalFitting
                          ? overrideItem.hidden
                                ? "workbench.condition.action_show"
                               : "workbench.condition.action_hide"
                     : activeConditionalFittingTarget
                         ? "workbench.condition.visibility_active"
                         : nullptr,
                 .showDyeButton = true,
                 .dyeButtonEnabled = previewActor != nullptr &&
                     previewActor->Is3DLoaded(),
                 .dyeButtonTooltip =
                     localization->GetCStr("dye.workbench.tooltip"),
                 .hidden = effectiveHidden,
                 .preserveContentTextColors = true,
                 .allowContextMenu = true,
                 .minimumHeight = overrideCardHeight,
                 .statusText = fittingStatusText,
                 .statusColor =
                     (isConditionalFitting && isActiveFittingRow) ||
                             activeConditionalFittingTarget
                         ? std::optional<ImVec4>{ThemeConfig::GetSingleton()
                                                     ->GetColor("TEXT_HEADER",
                                                                0.92f)}
                         : std::optional<ImVec4>{ThemeConfig::GetSingleton()
                                                     ->GetColor(
                                                         "TEXT_DISABLED")},
                 .slotIconText = kWorkbenchFittingSlotIcon.data(),
                 .slotIconScale = 0.72f,
                 .accentColor = overrideConditionState.color,
                 .conflictStyle =
                     hasOverrideConflict
                         ? ui::components::EquipmentWidgetConflictStyle::Error
                         : ui::components::EquipmentWidgetConflictStyle::None,
                 .drawTooltipExtras = std::move(overrideTooltipExtras),
                 .drawContextMenuEntries =
                     [&, overrideRowIndex, overrideIndex]() {
                       drawConditionAssignmentMenu(overrideRowIndex,
                                                   overrideIndex);
                     }});
            if (ImGui::BeginDragDropSource()) {
              DraggedEquipmentPayload payload{};
              payload.sourceKind =
                  static_cast<std::uint32_t>(
                      isConditionalFitting ? DragSourceKind::ConditionalRow
                                           : DragSourceKind::Row);
              payload.rowIndex = overrideRowIndex;
              payload.itemIndex = overrideIndex;
              payload.formID = overrideItem.formID;
              payload.slotMask = overrideSlotMask;
              payload.sourceUiIdentity = overrideRow.uiIdentity;
              ImGui::SetDragDropPayload(ui::workbench::kVariantItemPayloadType,
                                        &payload, sizeof(payload));
              ImGui::TextUnformatted(overrideItem.name.c_str());
              ImGui::TextUnformatted(overrideDisplayItem.slotText.c_str());
              ImGui::EndDragDropSource();
            }
            const ImRect overrideWidgetRect(ImGui::GetItemRectMin(),
                                            ImGui::GetItemRectMax());
            const auto *overrideCardDragPayload = ImGui::GetDragDropPayload();
            if (isConditionalFitting && overrideCardDragPayload != nullptr &&
                overrideCardDragPayload->IsDataType(
                    ui::workbench::kConditionPayloadType) &&
                ImGui::BeginDragDropTargetCustom(
                    overrideWidgetRect,
                    ImGui::GetID((widgetId + ":condition-target").c_str()))) {
              if (const auto *payload = ImGui::AcceptDragDropPayload(
                      ui::workbench::kConditionPayloadType);
                  payload && payload->Data != nullptr &&
                  payload->DataSize == sizeof(DraggedConditionPayload)) {
                DraggedConditionPayload conditionPayload{};
                std::memcpy(&conditionPayload, payload->Data,
                            sizeof(conditionPayload));
                const std::string conditionId(
                    conditionPayload.conditionId.data());
                if (const auto *definition = conditions::FindDefinitionById(
                        ConditionDefinitions(), conditionId);
                    definition != nullptr &&
                    IsWorkbenchSelectableCondition(*definition) &&
                    (!overrideRow.conditionId.has_value() ||
                     *overrideRow.conditionId != definition->id)) {
                  pendingConditionalRowAction = PendingConditionalRowAction{
                      overrideRowIndex, overrideRow.uiIdentity, definition->id,
                      conditionPayload};
                }
              }
              ImGui::EndDragDropTarget();
            }
            if (isConditionalFitting && overrideWidget.hideClicked) {
              bool changed = false;
              if (headgearToggleHidden) {
                changed |= workbench_.SetOverrideHeadgearToggleManualVisible(
                    overrideRowIndex, overrideIndex, true);
              } else if (virtualTokenHidden || ddHiderHidden) {
                changed |= workbench_.SetOverrideHidden(overrideRowIndex,
                                                        overrideIndex, false);
              } else if (overrideRow.IsOverrideAutomaticallySuppressed(
                             overrideItem)) {
                changed |= workbench_.SetOverrideHidden(overrideRowIndex,
                                                        overrideIndex, false);
                changed |= workbench_.SetOverrideAutomaticEquipmentUserVisible(
                    overrideRowIndex, overrideIndex, true);
              } else {
                changed |= workbench_.SetOverrideAutomaticEquipmentUserVisible(
                    overrideRowIndex, overrideIndex, false);
                changed |= workbench_.SetOverrideHidden(
                    overrideRowIndex, overrideIndex, !overrideItem.hidden);
              }
              if (changed) {
                workbench_.RefreshNativeArmorOverridesForActorWithoutEquipmentSync(
                    overrideRow.ownerActorFormID, conditionStore_.revision);
              }
            } else if (!isConditionalFitting && overrideWidget.hideClicked) {
              bool changed = false;
              if (baseFittingHidden && globallyHideFittingOverrides &&
                  previewActor != nullptr) {
                SetHideFittingOverridesForActor(previewActor, false);
                changed = true;
              }
              if (headgearToggleHidden) {
                changed |= workbench_.SetOverrideHeadgearToggleManualVisible(
                    overrideRowIndex, overrideIndex, true);
              } else if (virtualTokenHidden || ddHiderHidden) {
                changed |= workbench_.SetOverrideHidden(overrideRowIndex,
                                                        overrideIndex, false);
              } else if (overrideRow.IsOverrideAutomaticallySuppressed(
                             overrideItem)) {
                changed |= workbench_.SetOverrideHidden(overrideRowIndex,
                                                        overrideIndex, false);
                changed |= workbench_.SetOverrideAutomaticEquipmentUserVisible(
                    overrideRowIndex, overrideIndex, true);
              } else {
                changed |= workbench_.SetOverrideAutomaticEquipmentUserVisible(
                    overrideRowIndex, overrideIndex, false);
                changed |= workbench_.SetOverrideHidden(
                    overrideRowIndex, overrideIndex, !baseFittingHidden);
              }
              if (changed) {
                workbench_.RefreshNativeArmorOverridesForActorWithoutEquipmentSync(
                    overrideRow.ownerActorFormID, conditionStore_.revision);
              }
            }
            if (overrideWidget.dyeClicked) {
              OpenWorkbenchDyePopup(previewActor->GetFormID(),
                                    overrideDisplayItem, overrideSlotMask);
            }
            if (overrideWidget.deleteClicked) {
              if (isConditionalFitting) {
                // Freeze the already displayed order before the deleted action
                // changes this row's slot rank.
                workbenchSortDeferredUntilClose_ = true;
              }
              if (workbench_.DeleteOverride(overrideRowIndex, overrideIndex)) {
                syncRowsAfterTable = true;
                refreshNativeAfterTable = true;
              }
              break;
            }
            widgetRects.insert_or_assign(widgetId, overrideWidgetRect);
            if (hasOverrideConflict && overrideWidget.hovered) {
              hoveredConflictWidgetIds =
                  overrideConflicts.at(widgetId).targetWidgetIds;
            }
          }
          drawMissingOverrideSlotCards();
          ImGui::PopStyleVar();
        }

        const ImRect overrideDropRect(
            overrideDropMin,
            ImVec2(overrideDropMaxX, overrideCellRect.Max.y - cellPadding.y));
        ImGui::TableSetColumnIndex(1);
        const auto *overrideCellDragPayload = ImGui::GetDragDropPayload();
        if (const bool overrideTargetActive =
                displayGroup.conditionalSection &&
                overrideCellDragPayload != nullptr &&
                overrideCellDragPayload->IsDataType(
                    ui::workbench::kVariantItemPayloadType) &&
                displayGroup.dropTargetRowIndex >= 0 &&
                ImGui::BeginDragDropTargetCustom(
                    overrideDropRect,
                    ImGui::GetID(
                        ("##override-cell-target-" +
                         std::to_string(displayGroup.dropTargetRowIndex))
                            .c_str()));
            overrideTargetActive) {
          if (const auto *payload = ImGui::AcceptDragDropPayload(
                  ui::workbench::kVariantItemPayloadType);
              payload && payload->Data != nullptr &&
              payload->DataSize == sizeof(DraggedEquipmentPayload)) {
            DraggedEquipmentPayload dragPayload{};
            std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
            workbench::EquipmentWidgetItem droppedItem{};
            if (static_cast<DragSourceKind>(dragPayload.sourceKind) ==
                    DragSourceKind::Catalog &&
                dragPayload.formID != 0 &&
                workbench::BuildCatalogItem(dragPayload.formID, droppedItem)) {
              // A condition-only row has no action type or slot. Its previous
              // appearance slot must not prevent a different appearance from
              // being dropped into the same retained row.
              pendingConditionalFittingReplacement =
                  PendingConditionalFittingReplacement{
                      displayGroup.dropTargetRowIndex,
                      rows[static_cast<std::size_t>(
                               displayGroup.dropTargetRowIndex)]
                          .uiIdentity,
                      dragPayload.formID, dragPayload};
            } else {
              workbench::ConditionalVisibilityTargetKind targetKind{};
              bool visibleWhenTrue = true;
              if (resolveVisibilityTargetPayload(dragPayload, targetKind,
                                                 visibleWhenTrue)) {
                if (targetKind == workbench::
                                      ConditionalVisibilityTargetKind::Fitting) {
                   pendingConditionalFittingReplacement =
                       PendingConditionalFittingReplacement{
                           displayGroup.dropTargetRowIndex,
                           rows[static_cast<std::size_t>(
                                    displayGroup.dropTargetRowIndex)]
                               .uiIdentity,
                           dragPayload.formID,
                           dragPayload};
                } else {
                  pendingConditionalRowVisibilityConversion =
                       PendingConditionalRowVisibilityConversion{
                           displayGroup.dropTargetRowIndex,
                           rows[static_cast<std::size_t>(
                                    displayGroup.dropTargetRowIndex)]
                               .uiIdentity,
                           targetKind,
                           dragPayload.formID, visibleWhenTrue, dragPayload};
                }
              }
            }
          }
          ImGui::EndDragDropTarget();
        }
      };

      const auto drawConditionalVisibilityRule = [&](const std::size_t ruleIndex) {
        const auto &rule = conditionalVisibilityRules[ruleIndex];
        if (!rule.IsOwnedByActor(previewActor)) {
          return;
        }
        if (!conditionSectionHeaderDrawn) {
          drawConditionSectionHeader();
          conditionSectionHeaderDrawn = true;
        }

        const bool hasRuleCondition = !rule.conditionId.empty();
        const bool hasRuleTarget = rule.target.formID != 0;
        auto targetItem = rule.target;
        const auto targetDisplaySlotMask = hasRuleTarget
            ? ResolveWorkbenchItemDisplaySlotMask(targetItem)
            : std::uint64_t{0};
        const bool targetProtected =
            hasRuleTarget &&
            workbench::IsAppearanceRegistrationProtectedSlotMask(
                targetDisplaySlotMask);
        if (hasRuleTarget) {
          targetItem.slotText =
              BuildMultilineWorkbenchSlotText(targetDisplaySlotMask);
        }
        const auto slotLineCount = (std::max)(
            1, static_cast<int>(
                   SplitWorkbenchSlotMask(targetDisplaySlotMask).size()));
        const auto cardHeight = 18.0f + (ImGui::GetTextLineHeight() *
                                         static_cast<float>(1 + slotLineCount));
        const bool actualTarget =
            rule.targetKind ==
            workbench::ConditionalVisibilityTargetKind::Actual;
        const auto rowHeight = cardHeight + ui::workbench::kWorkbenchRowGapY;
        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               ThemeConfig::GetSingleton()->GetColorU32("BG"));
        ImGui::PushID(static_cast<int>(ruleIndex));

        const bool conditionMet = hasRuleCondition &&
                                  isConditionIdActive(rule.conditionId);
        const auto &activeVisibilityRules = actualTarget
                                                ? activeActualVisibilityRules
                                                : activeFittingVisibilityRules;
        const auto activeVisibilityRule = hasRuleTarget
            ? activeVisibilityRules.find(rule.target.formID)
            : activeVisibilityRules.end();
        const bool conditionActionApplied =
            conditionMet && !targetProtected &&
            activeVisibilityRule != activeVisibilityRules.end() &&
            activeVisibilityRule->second == ruleIndex;
        const auto *conditionActionStatusKey =
            !conditionMet || targetProtected ? "workbench.fitting.not_applied"
            : conditionActionApplied
                ? "workbench.fitting.applied"
                : "workbench.fitting.another_condition_applied";
        const auto *definition = hasRuleCondition
            ? conditions::FindDefinitionById(ConditionDefinitions(),
                                             rule.conditionId)
            : nullptr;
        const auto conditionName =
            definition != nullptr
                ? definition->name
                : std::string(localization->Get("common.unknown"));
        const bool builtIn = conditions::IsBuiltInCondition(rule.conditionId);
        std::string conditionSummary(localization->Get(
            builtIn ? "workbench.condition.basic_summary"
                    : "workbench.condition.user_defined_summary"));
        conditionSummary.push_back(' ');
        conditionSummary.append(std::to_string(
            definition != nullptr ? definition->clauses.size() : 0));

        ImGui::TableSetColumnIndex(0);
        const auto *ruleTable = ImGui::GetCurrentTable();
        const auto leftRect = ImGui::TableGetCellBgRect(ruleTable, 0);
        const auto padding = ImGui::GetStyle().CellPadding;
        const ImRect conditionRect(
            ImVec2(leftRect.Min.x + padding.x, leftRect.Min.y + padding.y),
            ImVec2(leftRect.Max.x - padding.x,
                   leftRect.Min.y + padding.y + cardHeight));
        ImGui::SetCursorScreenPos(conditionRect.Min);
        ImGui::InvisibleButton("##visibility-rule-condition",
                               conditionRect.GetSize());
        const bool conditionHovered = ImGui::IsItemHovered();
        if (hasRuleCondition && ImGui::BeginDragDropSource()) {
          DraggedConditionPayload payload{};
          std::snprintf(payload.conditionId.data(), payload.conditionId.size(),
                        "%s", rule.conditionId.c_str());
          payload.sourceKind = static_cast<std::uint32_t>(
              ConditionDragSourceKind::ConditionalVisibilityRule);
          payload.sourceIndex = static_cast<std::int32_t>(ruleIndex);
          payload.sourceUiIdentity = rule.uiIdentity;
          ImGui::SetDragDropPayload(ui::workbench::kConditionPayloadType,
                                    &payload, sizeof(payload));
          ImGui::TextUnformatted(conditionName.c_str());
          ImGui::EndDragDropSource();
        }
        auto *drawList = ImGui::GetWindowDrawList();
        const auto *theme = ThemeConfig::GetSingleton();
        const auto accentColor =
            definition != nullptr && definition->GetCatalog() != nullptr
                ? ImVec4{definition->GetCatalog()->color.x,
                         definition->GetCatalog()->color.y,
                         definition->GetCatalog()->color.z,
                         definition->GetCatalog()->color.w}
                : theme->GetColor("PRIMARY");
        drawList->AddRectFilled(
            conditionRect.Min, conditionRect.Max,
            hasRuleCondition
                ? (conditionHovered ? ConditionSurfaceColor(42, 42, 44)
                                    : ConditionSurfaceColor(34, 34, 36))
                : (conditionHovered ? ConditionSurfaceColor(18, 18, 20)
                                    : ConditionSurfaceColor(10, 10, 12)),
            4.0f);
        drawList->AddRect(conditionRect.Min, conditionRect.Max,
                          theme->GetColorU32("BORDER"), 4.0f);
        if (hasRuleCondition) {
          drawList->AddRectFilled(
              conditionRect.Min,
              ImVec2(conditionRect.Min.x + 5.0f, conditionRect.Max.y),
              ImGui::GetColorU32(accentColor), 4.0f,
              ImDrawFlags_RoundCornersTopLeft |
                  ImDrawFlags_RoundCornersBottomLeft);
        }
        constexpr float deleteWidth = 34.0f;
        const auto deleteMin =
            ImVec2(conditionRect.Max.x - deleteWidth, conditionRect.Min.y);
        const auto deleteState = hasRuleCondition
            ? ui::input_widgets::EvaluateRectClickTarget(
                  ImGui::GetID("##visibility-rule-condition-delete"),
                  deleteMin, conditionRect.Max)
            : ui::input_widgets::RectClickTargetState{};
        constexpr bool actionsLocked = false;
        if (hasRuleCondition) {
          drawList->AddRectFilled(
              deleteMin, conditionRect.Max,
              actionsLocked ? theme->GetColorU32("TEXT_DISABLED", 0.26f)
                            : theme->GetColorU32(
                                  "DECLINE", deleteState.hovered ? 0.95f : 0.78f),
              4.0f, ImDrawFlags_RoundCornersRight);
          const auto deleteIconSize =
              ImGui::CalcTextSize(kWorkbenchTrashIcon.data());
          drawList->AddText(
              ImVec2(deleteMin.x + ((deleteWidth - deleteIconSize.x) * 0.5f),
                     conditionRect.Min.y +
                         ((conditionRect.GetHeight() - deleteIconSize.y) * 0.5f)),
              actionsLocked ? theme->GetColorU32("TEXT_DISABLED")
                            : theme->GetColorU32("TEXT"),
              kWorkbenchTrashIcon.data());
          if (!actionsLocked && deleteState.pressed) {
            // Clearing the condition key must not regroup this row in the
            // current menu session.
            workbenchSortDeferredUntilClose_ = true;
            static_cast<void>(workbench_.SetConditionalVisibilityRuleConditionId(
                ruleIndex, {}));
            refreshNativeAfterTable = true;
          }
        }
        const auto lineHeight = ImGui::GetTextLineHeight();
        const auto textY =
            conditionRect.Min.y +
            ((conditionRect.GetHeight() - ((lineHeight * 2.0f) + 4.0f)) * 0.5f);
        const auto textMinX = conditionRect.Min.x + 13.0f;
        const auto textMaxX = hasRuleCondition ? deleteMin.x - 10.0f
                                                : conditionRect.Max.x - 10.0f;
        const auto statusText =
            localization->Get(conditionMet ? "workbench.condition.met"
                                           : "workbench.condition.not_met");
        const auto statusSize = ImGui::CalcTextSize(statusText.data());
        const auto statusX = textMaxX - statusSize.x;
        const auto compactName = ui::catalog::TruncateTextToWidth(
            conditionName, (std::max)(1.0f, textMaxX - textMinX));
        const auto compactSummary = ui::catalog::TruncateTextToWidth(
            conditionSummary, (std::max)(1.0f, statusX - textMinX - 8.0f));
        if (hasRuleCondition) {
          drawList->AddText(ImVec2(textMinX, textY), theme->GetColorU32("TEXT"),
                            compactName.c_str());
          drawList->AddText(ImVec2(statusX, textY + lineHeight + 4.0f),
                            conditionMet
                                ? theme->GetColorU32("TEXT_HEADER", 0.92f)
                                : theme->GetColorU32("TEXT_DISABLED"),
                            statusText.data());
          drawList->AddText(ImVec2(textMinX, textY + lineHeight + 4.0f),
                            theme->GetColorU32("TEXT_DISABLED"),
                            compactSummary.c_str());
        } else {
          drawList->AddText(
              ImVec2(textMinX, textY + (lineHeight * 0.5f)),
              theme->GetColorU32("TEXT_DISABLED"),
              localization->GetCStr("workbench.slot_create.condition_prompt"));
        }

        const auto *conditionPayload = ImGui::GetDragDropPayload();
        if (!actionsLocked && conditionPayload != nullptr &&
            conditionPayload->IsDataType(
                ui::workbench::kConditionPayloadType) &&
            ImGui::BeginDragDropTargetCustom(
                conditionRect,
                ImGui::GetID("##visibility-rule-condition-target"))) {
          if (const auto *payload = ImGui::AcceptDragDropPayload(
                  ui::workbench::kConditionPayloadType);
              payload != nullptr && payload->Data != nullptr &&
              payload->DataSize == sizeof(DraggedConditionPayload)) {
            DraggedConditionPayload draggedCondition{};
            std::memcpy(&draggedCondition, payload->Data,
                        sizeof(draggedCondition));
            const std::string conditionId(draggedCondition.conditionId.data());
            if (const auto *newDefinition = conditions::FindDefinitionById(
                    ConditionDefinitions(), conditionId);
                newDefinition != nullptr &&
                IsWorkbenchSelectableCondition(*newDefinition) &&
                newDefinition != nullptr) {
              pendingVisibilityRuleConditionAssignment =
                  PendingVisibilityRuleConditionAssignment{
                      ruleIndex, rule.uiIdentity, newDefinition->id,
                      draggedCondition};
            }
          }
          ImGui::EndDragDropTarget();
        }
        const auto conditionTooltipHovered = conditionHovered &&
                                             !deleteState.hovered &&
                                             !ImGui::IsDragDropActive();
        if (definition != nullptr) {
          ui::workbench::DrawConditionDefinitionTooltip(
              "visibility-rule-condition:" + std::to_string(ruleIndex),
              *definition, conditionTooltipHovered, ConditionDefinitions());
        } else {
          ui::workbench::DrawSimplePinnableTooltip(
              "visibility-rule-condition-missing:" + std::to_string(ruleIndex),
              conditionTooltipHovered,
              [&]() {
                ImGui::TextColored(theme->GetColor("TEXT"), "%s",
                                   conditionName.c_str());
              },
              460.0f);
        }

        ImGui::TableSetColumnIndex(1);
        const auto rightRect = ImGui::TableGetCellBgRect(ruleTable, 1);
        ImGui::SetCursorScreenPos(
            ImVec2(rightRect.Min.x + padding.x, rightRect.Min.y + padding.y));
        ui::components::EquipmentWidgetResult actionWidget{};
        ImRect actionWidgetRect{};
        if (hasRuleTarget) {
          actionWidget = ui::components::DrawEquipmentWidget(
              "visibility-rule-target", targetItem,
              {.showDeleteButton = true,
               .deleteButtonEnabled = !actionsLocked,
               .showHideButton = true,
               .hideButtonEnabled = !actionsLocked && !targetProtected,
               .hideButtonTooltipKey =
                   targetProtected
                       ? "workbench.appearance_protected.tooltip"
                   : rule.visibleWhenTrue ? "workbench.condition.action_hide"
                                          : "workbench.condition.action_show",
               .hidden = !rule.visibleWhenTrue,
               .preserveContentTextColors = true,
               .interactive = true,
               .allowContextMenu = true,
               .minimumHeight = cardHeight,
               .statusText = localization->GetCStr(conditionActionStatusKey),
               .statusColor =
                   conditionActionApplied
                       ? std::optional<ImVec4>{theme->GetColor("TEXT_HEADER",
                                                               0.92f)}
                       : std::optional<ImVec4>{theme->GetColor("TEXT_DISABLED")},
               .slotIconText = actualTarget ? kWorkbenchActualSlotIcon.data()
                                            : kWorkbenchFittingSlotIcon.data(),
               .slotIconScale = 0.72f,
               // An action without a condition is only a neutral drop target;
               // it must not inherit the default PRIMARY condition stripe.
               .accentColor = hasRuleCondition
                                  ? std::optional<ImVec4>{accentColor}
                                  : std::nullopt,
               .drawContextMenuEntries =
                   [&, ruleIndex, conditionId = rule.conditionId]() {
                     drawVisibilityRuleConditionAssignmentMenu(ruleIndex,
                                                               conditionId);
                   }});
          actionWidgetRect =
              ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
          if (ImGui::BeginDragDropSource()) {
            DraggedEquipmentPayload payload{};
            payload.sourceKind = static_cast<std::uint32_t>(
                DragSourceKind::ConditionalVisibilityRule);
            payload.rowIndex = static_cast<std::int32_t>(ruleIndex);
            payload.formID = rule.target.formID;
            payload.slotMask = targetDisplaySlotMask;
            payload.sourceUiIdentity = rule.uiIdentity;
            ImGui::SetDragDropPayload(ui::workbench::kVariantItemPayloadType,
                                      &payload, sizeof(payload));
            ImGui::TextUnformatted(targetItem.name.c_str());
            ImGui::TextUnformatted(targetItem.slotText.c_str());
            ImGui::EndDragDropSource();
          }
        } else {
          const ImVec2 actionMin(rightRect.Min.x + padding.x,
                                 rightRect.Min.y + padding.y);
          const ImVec2 actionMax(rightRect.Max.x - padding.x,
                                 actionMin.y + cardHeight);
          actionWidgetRect = ImRect(actionMin, actionMax);
          ImGui::SetCursorScreenPos(actionMin);
          ImGui::InvisibleButton("##visibility-rule-empty-target",
                                 actionWidgetRect.GetSize());
          const bool actionHovered = ImGui::IsItemHovered();
          drawList->AddRectFilled(
              actionWidgetRect.Min, actionWidgetRect.Max,
              theme->GetColorU32(actionHovered ? "BG_LIGHT" : "BG",
                                 actionHovered ? 1.0f : 0.92f),
              8.0f);
          drawList->AddRect(actionWidgetRect.Min, actionWidgetRect.Max,
                            theme->GetColorU32("TEXT_DISABLED", 0.55f), 8.0f);
          const auto *actionPrompt =
              localization->GetCStr("workbench.slot_create.action_prompt");
          constexpr float promptPaddingX = 13.0f;
          const auto promptWidth = (std::max)(
              1.0f, actionWidgetRect.GetWidth() - (promptPaddingX * 2.0f));
          const auto promptSize =
              ImGui::CalcTextSize(actionPrompt, nullptr, false, promptWidth);
          const auto promptPosition = ImVec2(
              actionWidgetRect.Min.x + promptPaddingX,
              actionWidgetRect.Min.y +
                  ((actionWidgetRect.GetHeight() - promptSize.y) * 0.5f));
          drawList->PushClipRect(actionWidgetRect.Min, actionWidgetRect.Max,
                                 true);
          drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                            promptPosition,
                            theme->GetColorU32("TEXT_DISABLED"), actionPrompt,
                            nullptr, promptWidth);
          drawList->PopClipRect();
        }
        const auto *actionDragPayload = ImGui::GetDragDropPayload();
        if (!actionsLocked && actionDragPayload != nullptr &&
            actionDragPayload->IsDataType(
                ui::workbench::kVariantItemPayloadType) &&
            ImGui::BeginDragDropTargetCustom(
                actionWidgetRect,
                ImGui::GetID("##visibility-rule-action-target"))) {
          if (const auto *payload = ImGui::AcceptDragDropPayload(
                  ui::workbench::kVariantItemPayloadType);
              payload != nullptr && payload->Data != nullptr &&
              payload->DataSize == sizeof(DraggedEquipmentPayload)) {
            DraggedEquipmentPayload draggedItem{};
            std::memcpy(&draggedItem, payload->Data, sizeof(draggedItem));
            if (static_cast<DragSourceKind>(draggedItem.sourceKind) ==
                    DragSourceKind::Catalog &&
                draggedItem.formID != 0) {
              pendingVisibilityRuleFittingConversion =
                  PendingVisibilityRuleFittingConversion{ruleIndex,
                                                         rule.uiIdentity,
                                                         draggedItem.formID,
                                                         draggedItem};
            } else {
              workbench::ConditionalVisibilityTargetKind targetKind{};
              bool visibleWhenTrue = true;
              if (resolveVisibilityTargetPayload(draggedItem, targetKind,
                                                 visibleWhenTrue)) {
                pendingVisibilityRuleReplacement =
                    PendingVisibilityRuleReplacement{ruleIndex,
                                                     rule.uiIdentity, targetKind,
                                                     draggedItem.formID,
                                                     visibleWhenTrue,
                                                     draggedItem};
              }
            }
          }
          ImGui::EndDragDropTarget();
        }
        if (hasRuleTarget && !actionsLocked && actionWidget.hideClicked &&
            workbench_.SetConditionalVisibilityRuleVisible(
                ruleIndex, !rule.visibleWhenTrue)) {
          refreshNativeAfterTable = true;
        }
        if (hasRuleTarget && !actionsLocked && actionWidget.deleteClicked) {
          // The target slot becomes empty (rank 62), but that new rank is only
          // applied after the workbench is closed and opened again.
          workbenchSortDeferredUntilClose_ = true;
          static_cast<void>(workbench_.SetConditionalVisibilityRuleTarget(
              ruleIndex, rule.targetKind, 0));
          refreshNativeAfterTable = true;
        }
        ImGui::PopID();
      };

      // Base rows remain in their normal workbench order. Conditional fitting
      // rows and conditional actual-equipment rules then share one ordering:
      // condition first, slot ascending, and original registration order for
      // equal slots. This prevents the same condition from being split merely
      // because one action targets actual equipment and another targets a
      // registered appearance.
      std::vector<std::size_t> baseDisplayIndices(baseDisplayCount);
      std::iota(baseDisplayIndices.begin(), baseDisplayIndices.end(), 0);
      const auto baseGroupIdentity = [&](const std::size_t a_index) {
        const auto &group = displayRowGroups[a_index];
        if (group.displayRowIndex >= 0 &&
            group.displayRowIndex < static_cast<int>(rows.size())) {
          return rows[static_cast<std::size_t>(group.displayRowIndex)]
              .uiIdentity;
        }
        return std::uint64_t{0};
      };
      if (!workbenchSortDeferredUntilClose_) {
        workbenchBaseSessionOrder_.clear();
        for (std::size_t order = 0; order < baseDisplayIndices.size(); ++order) {
          const auto identity = baseGroupIdentity(baseDisplayIndices[order]);
          if (identity != 0) {
            workbenchBaseSessionOrder_.insert_or_assign(identity, order);
          }
        }
      } else {
        std::size_t nextOrder = workbenchBaseSessionOrder_.size();
        for (const auto index : baseDisplayIndices) {
          const auto identity = baseGroupIdentity(index);
          if (identity != 0 && !workbenchBaseSessionOrder_.contains(identity)) {
            workbenchBaseSessionOrder_.emplace(identity, nextOrder++);
          }
        }
        std::ranges::stable_sort(
            baseDisplayIndices, [&](const auto a_left, const auto a_right) {
              const auto leftIdentity = baseGroupIdentity(a_left);
              const auto rightIdentity = baseGroupIdentity(a_right);
              const auto leftIt = workbenchBaseSessionOrder_.find(leftIdentity);
              const auto rightIt = workbenchBaseSessionOrder_.find(rightIdentity);
              if (leftIt == workbenchBaseSessionOrder_.end()) {
                return false;
              }
              if (rightIt == workbenchBaseSessionOrder_.end()) {
                return true;
              }
              return leftIt->second < rightIt->second;
            });
      }
      if (workbenchBaseSort_.column != WorkbenchSortColumn::None) {
        const bool sortActual =
            workbenchBaseSort_.column == WorkbenchSortColumn::Left;
        std::ranges::stable_sort(
            baseDisplayIndices,
            [&](const std::size_t a_left, const std::size_t a_right) {
              const auto &leftGroup = displayRowGroups[a_left];
              const auto &rightGroup = displayRowGroups[a_right];
              const auto leftPrimaryMask =
                  sortActual ? leftGroup.actualSpanSlotMask
                             : leftGroup.overrideSpanSlotMask;
              const auto rightPrimaryMask =
                  sortActual ? rightGroup.actualSpanSlotMask
                             : rightGroup.overrideSpanSlotMask;
              // An empty cell still belongs to the row's logical slot. Use
              // the complete row span as its fallback so sorting either
              // column moves every base row, including empty actual/fitting
              // drop targets.
              const auto leftRank = slotMaskRank(
                  leftPrimaryMask != 0 ? leftPrimaryMask
                                       : leftGroup.spanSlotMask);
              const auto rightRank = slotMaskRank(
                  rightPrimaryMask != 0 ? rightPrimaryMask
                                        : rightGroup.spanSlotMask);
              if (leftRank == rightRank) {
                return false;
              }
              return workbenchBaseSort_.ascending ? leftRank < rightRank
                                                  : leftRank > rightRank;
            });
      }
      for (const auto index : baseDisplayIndices) {
        drawDisplayGroup(displayRowGroups[index]);
      }

      struct ConditionalRenderEntry {
        bool visibilityRule{false};
        std::size_t index{0};
        std::uint64_t uiIdentity{0};
        std::string conditionKey;
        std::string conditionName;
        std::size_t slotRank{62};
        std::uint64_t registrationOrder{0};
      };
      std::vector<ConditionalRenderEntry> conditionalRenderEntries;
      conditionalRenderEntries.reserve(
          conditionalDisplayCount + conditionalVisibilityRules.size());
      for (std::size_t index = baseDisplayCount;
           index < displayRowGroups.size(); ++index) {
        const auto &group = displayRowGroups[index];
        const auto conditionRowIndex = group.conditionRowIndex;
        const auto &conditionRow =
            rows[static_cast<std::size_t>(conditionRowIndex)];
        std::string conditionName;
        if (conditionRowIndex >= 0 &&
            conditionRowIndex < static_cast<int>(rowConditionStates.size())) {
          conditionName =
              rowConditionStates[static_cast<std::size_t>(conditionRowIndex)]
                  .name;
        }
        if (conditionName.empty() && conditionRow.conditionId.has_value()) {
          conditionName = *conditionRow.conditionId;
        }
        conditionalRenderEntries.push_back(
            {.visibilityRule = false,
             .index = index,
             .uiIdentity = conditionRow.uiIdentity,
             .conditionKey = conditionGroupKey(group),
             .conditionName = std::move(conditionName),
             .slotRank = conditionSlotRank(group),
             .registrationOrder = conditionRow.registrationOrder});
      }
      for (std::size_t ruleIndex = 0;
           ruleIndex < conditionalVisibilityRules.size(); ++ruleIndex) {
        const auto &rule = conditionalVisibilityRules[ruleIndex];
        if (!rule.IsOwnedByActor(previewActor)) {
          continue;
        }
        const auto targetSlotMask = rule.target.formID != 0
            ? ResolveWorkbenchItemDisplaySlotMask(rule.target)
            : std::uint64_t{0};
        const auto *conditionDefinition =
            conditions::FindDefinitionById(ConditionDefinitions(),
                                            rule.conditionId);
        conditionalRenderEntries.push_back(
            {.visibilityRule = true,
             .index = ruleIndex,
             .uiIdentity = rule.uiIdentity,
             .conditionKey = rule.conditionId,
             .conditionName = conditionDefinition != nullptr
                                  ? conditionDefinition->name
                                  : rule.conditionId,
             .slotRank = slotMaskRank(targetSlotMask),
             .registrationOrder = rule.registrationOrder});
      }
      if (!workbenchSortDeferredUntilClose_) {
        struct ConditionGroupSortKey {
          std::size_t minimumSlotRank{62};
          std::uint64_t firstRegistrationOrder{
              (std::numeric_limits<std::uint64_t>::max)()};
        };
        std::unordered_map<std::string, ConditionGroupSortKey>
            conditionGroupSortKeys;
        for (const auto &entry : conditionalRenderEntries) {
          auto &key = conditionGroupSortKeys[entry.conditionKey];
          key.minimumSlotRank = (std::min)(key.minimumSlotRank, entry.slotRank);
          key.firstRegistrationOrder =
              (std::min)(key.firstRegistrationOrder, entry.registrationOrder);
        }
        std::ranges::stable_sort(
            conditionalRenderEntries,
            [&](const ConditionalRenderEntry &a_left,
                const ConditionalRenderEntry &a_right) {
              if (a_left.conditionKey != a_right.conditionKey) {
                // Empty condition targets remain below configured condition
                // groups. Configured groups are ordered by the lowest slot
                // currently assigned to that condition.
                if (a_left.conditionKey.empty() !=
                    a_right.conditionKey.empty()) {
                  return !a_left.conditionKey.empty();
                }
                const auto &leftGroup =
                    conditionGroupSortKeys.at(a_left.conditionKey);
                const auto &rightGroup =
                    conditionGroupSortKeys.at(a_right.conditionKey);
                if (leftGroup.minimumSlotRank != rightGroup.minimumSlotRank) {
                  return leftGroup.minimumSlotRank < rightGroup.minimumSlotRank;
                }
                if (leftGroup.firstRegistrationOrder !=
                    rightGroup.firstRegistrationOrder) {
                  return leftGroup.firstRegistrationOrder <
                         rightGroup.firstRegistrationOrder;
                }
                return a_left.conditionKey < a_right.conditionKey;
              }
              if (a_left.slotRank != a_right.slotRank) {
                return a_left.slotRank < a_right.slotRank;
              }
              if (a_left.registrationOrder != a_right.registrationOrder) {
                return a_left.registrationOrder < a_right.registrationOrder;
              }
              return a_left.visibilityRule < a_right.visibilityRule;
            });
        workbenchConditionalSessionOrder_.clear();
        for (std::size_t order = 0; order < conditionalRenderEntries.size();
             ++order) {
          workbenchConditionalSessionOrder_.insert_or_assign(
              conditionalRenderEntries[order].uiIdentity, order);
        }
      } else {
        std::size_t nextOrder = workbenchConditionalSessionOrder_.size();
        for (const auto &entry : conditionalRenderEntries) {
          if (!workbenchConditionalSessionOrder_.contains(entry.uiIdentity)) {
            workbenchConditionalSessionOrder_.emplace(entry.uiIdentity,
                                                       nextOrder++);
          }
        }
        std::ranges::stable_sort(
            conditionalRenderEntries,
            [&](const ConditionalRenderEntry &a_left,
                const ConditionalRenderEntry &a_right) {
              return workbenchConditionalSessionOrder_.at(a_left.uiIdentity) <
                     workbenchConditionalSessionOrder_.at(a_right.uiIdentity);
            });
      }
      if (workbenchConditionalSort_.column == WorkbenchSortColumn::Left) {
        std::ranges::stable_sort(
            conditionalRenderEntries,
            [&](const ConditionalRenderEntry &a_left,
                const ConditionalRenderEntry &a_right) {
              if (a_left.conditionKey == a_right.conditionKey) {
                return false;
              }
              if (a_left.conditionKey.empty() !=
                  a_right.conditionKey.empty()) {
                return !a_left.conditionKey.empty();
              }
              auto compare = strings::CompareTextInsensitive(
                  a_left.conditionName, a_right.conditionName);
              if (compare == 0) {
                compare = strings::CompareTextInsensitive(a_left.conditionKey,
                                                          a_right.conditionKey);
              }
              return workbenchConditionalSort_.ascending ? compare < 0
                                                         : compare > 0;
            });
      } else if (workbenchConditionalSort_.column ==
                 WorkbenchSortColumn::Right) {
        std::unordered_map<std::string, std::size_t> conditionGroupOrder;
        struct ConditionActionSortKey {
          std::size_t minimumSlotRank{62};
          std::size_t maximumSlotRank{0};
          bool hasOccupiedAction{false};
        };
        std::unordered_map<std::string, ConditionActionSortKey>
            conditionActionSortKeys;
        for (const auto &entry : conditionalRenderEntries) {
          conditionGroupOrder.try_emplace(entry.conditionKey,
                                          conditionGroupOrder.size());
          auto &key = conditionActionSortKeys[entry.conditionKey];
          // Empty action cells are drop targets rather than slot-62
          // equipment. Keep them inside their condition group, but do not let
          // one empty cell make the whole group sort as slot 62.
          if (entry.slotRank < 62) {
            key.minimumSlotRank =
                (std::min)(key.minimumSlotRank, entry.slotRank);
            key.maximumSlotRank =
                (std::max)(key.maximumSlotRank, entry.slotRank);
            key.hasOccupiedAction = true;
          }
        }
        std::ranges::stable_sort(
            conditionalRenderEntries,
            [&](const ConditionalRenderEntry &a_left,
                const ConditionalRenderEntry &a_right) {
              if (a_left.conditionKey != a_right.conditionKey) {
                const auto &leftKey =
                    conditionActionSortKeys.at(a_left.conditionKey);
                const auto &rightKey =
                    conditionActionSortKeys.at(a_right.conditionKey);
                if (leftKey.hasOccupiedAction !=
                    rightKey.hasOccupiedAction) {
                  return leftKey.hasOccupiedAction;
                }
                const auto leftRank = workbenchConditionalSort_.ascending
                                          ? leftKey.minimumSlotRank
                                          : leftKey.maximumSlotRank;
                const auto rightRank = workbenchConditionalSort_.ascending
                                           ? rightKey.minimumSlotRank
                                           : rightKey.maximumSlotRank;
                if (leftRank != rightRank) {
                  return workbenchConditionalSort_.ascending
                             ? leftRank < rightRank
                             : leftRank > rightRank;
                }
                return conditionGroupOrder.at(a_left.conditionKey) <
                       conditionGroupOrder.at(a_right.conditionKey);
              }
              if (a_left.slotRank == a_right.slotRank) {
                return false;
              }
              // An empty action is a drop target, not slot 62 equipment.
              if (a_left.slotRank == 62) {
                return false;
              }
              if (a_right.slotRank == 62) {
                return true;
              }
              return workbenchConditionalSort_.ascending
                         ? a_left.slotRank < a_right.slotRank
                         : a_left.slotRank > a_right.slotRank;
            });
      }
      for (const auto &entry : conditionalRenderEntries) {
        if (entry.visibilityRule) {
          drawConditionalVisibilityRule(entry.index);
        } else {
          drawDisplayGroup(displayRowGroups[entry.index]);
        }
      }

      stickyConditionalHeaderNextFrame |= DrawSlotCreationRow(
          !conditionSectionHeaderDrawn, stickyHeaderBottomY);

      workbenchStickyConditionalHeader_ =
          baseDisplayCount != 0 && stickyConditionalHeaderNextFrame;

      if (!hoveredConflictWidgetIds.empty()) {
        auto *drawList = ImGui::GetWindowDrawList();
        if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
          drawList->PushClipRect(table->OuterRect.Min, table->OuterRect.Max,
                                 false);
        }
        for (const auto &targetWidgetId : hoveredConflictWidgetIds) {
          if (const auto rectIt = widgetRects.find(targetWidgetId);
              rectIt != widgetRects.end()) {
            drawList->AddRectFilled(
                rectIt->second.Min, rectIt->second.Max,
                ThemeConfig::GetSingleton()->GetColorU32("WARN", 0.18f), 8.0f);
            drawList->AddRect(rectIt->second.Min, rectIt->second.Max,
                              ThemeConfig::GetSingleton()->GetColorU32("WARN"),
                              8.0f, 0, 3.0f);
          }
        }
        if (ImGui::GetCurrentTable() != nullptr) {
          drawList->PopClipRect();
        }
      }

      ImGui::EndTable();

      const auto findRowIndexByUiIdentity = [&](const std::uint64_t a_identity) {
        const auto &currentRows = workbench_.GetRows();
        const auto it = std::ranges::find(
            currentRows, a_identity, &workbench::VariantWorkbenchRow::uiIdentity);
        return it == currentRows.end()
                   ? -1
                   : static_cast<int>(std::distance(currentRows.begin(), it));
      };
      const auto findRuleIndexByUiIdentity =
          [&](const std::uint64_t a_identity) -> std::optional<std::size_t> {
        const auto &currentRules = workbench_.GetConditionalVisibilityRules();
        const auto it = std::ranges::find(
            currentRules, a_identity,
            &workbench::ConditionalVisibilityRule::uiIdentity);
        if (it == currentRules.end()) {
          return std::nullopt;
        }
        return static_cast<std::size_t>(
            std::distance(currentRules.begin(), it));
      };
      const auto readConditionByIdentity =
          [&](const ConditionDragSourceKind a_kind,
              const std::uint64_t a_identity) -> std::string {
        if (a_kind == ConditionDragSourceKind::ConditionalRow) {
          const auto rowIndex = findRowIndexByUiIdentity(a_identity);
          if (rowIndex >= 0) {
            return workbench_.GetRows()[static_cast<std::size_t>(rowIndex)]
                .conditionId.value_or(std::string{});
          }
        } else if (a_kind ==
                   ConditionDragSourceKind::ConditionalVisibilityRule) {
          if (const auto ruleIndex = findRuleIndexByUiIdentity(a_identity);
              ruleIndex.has_value()) {
            return workbench_.GetConditionalVisibilityRules()[*ruleIndex]
                .conditionId;
          }
        }
        return {};
      };
      const auto writeConditionByIdentity =
          [&](const ConditionDragSourceKind a_kind,
              const std::uint64_t a_identity,
              const std::string_view a_conditionId) {
        if (a_kind == ConditionDragSourceKind::ConditionalRow) {
          const auto rowIndex = findRowIndexByUiIdentity(a_identity);
          if (rowIndex < 0) {
            return false;
          }
          return a_conditionId.empty()
                     ? workbench_.ClearConditionAssignmentKeepRow(rowIndex)
                     : workbench_.SetConditionAssignmentKeepRow(rowIndex,
                                                                a_conditionId);
        }
        if (a_kind ==
            ConditionDragSourceKind::ConditionalVisibilityRule) {
          const auto ruleIndex = findRuleIndexByUiIdentity(a_identity);
          return ruleIndex.has_value() &&
                 workbench_.SetConditionalVisibilityRuleConditionId(
                     *ruleIndex, a_conditionId);
        }
        return false;
      };
      const auto applyConditionDrop =
          [&](const ConditionDragSourceKind a_targetKind,
              const std::uint64_t a_targetIdentity,
              const std::string_view a_conditionId,
              const std::optional<DraggedConditionPayload> &a_dragged) {
        if (!a_dragged.has_value() ||
            a_dragged->sourceUiIdentity == 0) {
          return writeConditionByIdentity(a_targetKind, a_targetIdentity,
                                          a_conditionId);
        }
        const auto sourceKind = static_cast<ConditionDragSourceKind>(
            a_dragged->sourceKind);
        if (sourceKind == a_targetKind &&
            a_dragged->sourceUiIdentity == a_targetIdentity) {
          return false;
        }
        const auto oldTargetCondition =
            readConditionByIdentity(a_targetKind, a_targetIdentity);
        const auto oldSourceCondition = readConditionByIdentity(
            sourceKind, a_dragged->sourceUiIdentity);
        // Move between occupied condition cells as a swap, so editing never
        // silently discards the destination card. Moving to an empty cell
        // simply leaves the source cell empty.
        // Clear the destination first. Otherwise two visibility-rule cards
        // with the same target can temporarily collide during a swap and the
        // model correctly rejects the duplicate before the source is moved.
        const bool targetCleared = writeConditionByIdentity(
            a_targetKind, a_targetIdentity, {});
        if (!targetCleared &&
            !readConditionByIdentity(a_targetKind, a_targetIdentity).empty()) {
          return false;
        }
        const bool sourceMoved = writeConditionByIdentity(
            sourceKind, a_dragged->sourceUiIdentity, oldTargetCondition);
        if (!sourceMoved &&
            readConditionByIdentity(sourceKind,
                                    a_dragged->sourceUiIdentity) !=
                oldTargetCondition) {
          static_cast<void>(writeConditionByIdentity(
              a_targetKind, a_targetIdentity, oldTargetCondition));
          return false;
        }
        const bool targetMoved = writeConditionByIdentity(
            a_targetKind, a_targetIdentity, a_conditionId);
        if (!targetMoved &&
            readConditionByIdentity(a_targetKind, a_targetIdentity) !=
                a_conditionId) {
          static_cast<void>(writeConditionByIdentity(
              sourceKind, a_dragged->sourceUiIdentity, oldSourceCondition));
          static_cast<void>(writeConditionByIdentity(
              a_targetKind, a_targetIdentity, oldTargetCondition));
          return false;
        }
        return targetCleared || sourceMoved || targetMoved;
      };
      const auto clearMovedActionSource =
          [&](const DraggedEquipmentPayload &a_payload,
              const std::uint64_t a_targetIdentity) {
        const auto sourceKind =
            static_cast<DragSourceKind>(a_payload.sourceKind);
        if (a_payload.sourceUiIdentity == 0 ||
            a_payload.sourceUiIdentity == a_targetIdentity) {
          return false;
        }
        if (sourceKind == DragSourceKind::ConditionalRow) {
          const auto rowIndex =
              findRowIndexByUiIdentity(a_payload.sourceUiIdentity);
          if (rowIndex < 0) {
            return false;
          }
          const auto &sourceRow =
              workbench_.GetRows()[static_cast<std::size_t>(rowIndex)];
          const auto itemIt = std::ranges::find(
              sourceRow.overrides, a_payload.formID,
              &workbench::EquipmentWidgetItem::formID);
          if (itemIt == sourceRow.overrides.end()) {
            return false;
          }
          return workbench_.DeleteOverride(
              rowIndex, static_cast<int>(
                            std::distance(sourceRow.overrides.begin(), itemIt)));
        }
        if (sourceKind == DragSourceKind::ConditionalVisibilityRule) {
          const auto ruleIndex =
              findRuleIndexByUiIdentity(a_payload.sourceUiIdentity);
          if (!ruleIndex.has_value()) {
            return false;
          }
          const auto targetKind =
              workbench_.GetConditionalVisibilityRules()[*ruleIndex].targetKind;
          return workbench_.SetConditionalVisibilityRuleTarget(
              *ruleIndex, targetKind, 0);
        }
        return false;
      };

      if (pendingConditionalRowVisibilityConversion.has_value()) {
        freezeWorkbenchOrderForEdit();
        const auto conversion = *pendingConditionalRowVisibilityConversion;
        const auto &currentRows = workbench_.GetRows();
        const auto conversionRowIndex =
            findRowIndexByUiIdentity(conversion.targetUiIdentity);
        if (conversionRowIndex >= 0 &&
            conversionRowIndex < static_cast<int>(currentRows.size())) {
          const auto sourceRow =
              currentRows[static_cast<std::size_t>(conversionRowIndex)];
          if (sourceRow.conditionId.has_value()) {
            bool targetReady = workbench_.AddConditionalVisibilityRule(
                *sourceRow.conditionId, sourceRow.ownerActorFormID,
                conversion.targetKind, conversion.formID,
                conversion.visibleWhenTrue, sourceRow.uiIdentity,
                sourceRow.registrationOrder);
            if (!targetReady) {
              targetReady = std::ranges::any_of(
                  workbench_.GetConditionalVisibilityRules(),
                  [&](const auto &a_rule) {
                    return a_rule.conditionId == *sourceRow.conditionId &&
                           a_rule.ownerActorFormID ==
                               sourceRow.ownerActorFormID &&
                           a_rule.targetKind == conversion.targetKind &&
                           a_rule.target.formID == conversion.formID;
                  });
            }
            // The drop fills this row's action side. Conditional actual gear
            // uses a visibility-rule representation, so consume the former
            // empty fitting row instead of leaving it beside a new row.
            if (targetReady && workbench_.DeleteRow(conversionRowIndex)) {
              syncRowsAfterTable = true;
              refreshNativeAfterTable = true;
              static_cast<void>(clearMovedActionSource(
                  conversion.draggedItem, conversion.targetUiIdentity));
            }
          }
        }
      }

      if (pendingCardConditionAssignment.has_value()) {
        freezeWorkbenchOrderForEdit();
        const auto assignment = *pendingCardConditionAssignment;
        bool changed = false;
        if (assignment.overrideIndex >= 0) {
          changed = workbench_.SetOverrideConditionId(
              assignment.rowIndex, assignment.overrideIndex,
              assignment.conditionId);
        } else if (assignment.conditionId.has_value()) {
          const auto &currentRows = workbench_.GetRows();
          if (assignment.rowIndex >= 0 &&
              assignment.rowIndex < static_cast<int>(currentRows.size())) {
            const auto &actualRow = currentRows[static_cast<std::size_t>(
                assignment.rowIndex)];
            if (actualRow.isEquipped && !actualRow.IsSlotRow() &&
                actualRow.equipped.formID != 0) {
              changed = workbench_.AddConditionalVisibilityRule(
                  *assignment.conditionId, actualRow.ownerActorFormID,
                  workbench::ConditionalVisibilityTargetKind::Actual,
                  actualRow.equipped.formID,
                  !workbench_.ResolveEquippedHiddenForActor(
                      previewActor, actualRow));
            }
          }
        }
        if (changed) {
          syncRowsAfterTable = true;
          refreshNativeAfterTable = true;
        }
      }

      if (pendingVisibilityRuleConditionAssignment.has_value()) {
        freezeWorkbenchOrderForEdit();
        const auto assignment =
            *pendingVisibilityRuleConditionAssignment;
        // Keep the row where it was for the rest of this open menu; the new
        // condition grouping takes effect after close/reopen.
        workbenchSortDeferredUntilClose_ = true;
        if (applyConditionDrop(
                ConditionDragSourceKind::ConditionalVisibilityRule,
                assignment.targetUiIdentity, assignment.conditionId,
                assignment.draggedCondition)) {
          refreshNativeAfterTable = true;
        }
      }

      if (pendingVisibilityRuleReplacement.has_value()) {
        freezeWorkbenchOrderForEdit();
        const auto replacement = *pendingVisibilityRuleReplacement;
        const auto targetRuleIndex =
            findRuleIndexByUiIdentity(replacement.targetUiIdentity);
        if (targetRuleIndex.has_value()) {
          if (workbench_.SetConditionalVisibilityRuleTarget(
                  *targetRuleIndex, replacement.targetKind,
                  replacement.formID)) {
            static_cast<void>(clearMovedActionSource(
                replacement.draggedItem, replacement.targetUiIdentity));
            refreshNativeAfterTable = true;
          }
        }
      }

      if (pendingVisibilityRuleFittingConversion.has_value()) {
        freezeWorkbenchOrderForEdit();
        const auto conversion = *pendingVisibilityRuleFittingConversion;
        const auto targetRuleIndex =
            findRuleIndexByUiIdentity(conversion.targetUiIdentity);
        if (targetRuleIndex.has_value() &&
            workbench_.ConvertConditionalVisibilityRuleToFittingRow(
                *targetRuleIndex, conversion.formID)) {
          static_cast<void>(clearMovedActionSource(
              conversion.draggedItem, conversion.targetUiIdentity));
          syncRowsAfterTable = true;
          refreshNativeAfterTable = true;
        }
      }

      if (pendingVisibilityRuleDelete.has_value()) {
        freezeWorkbenchOrderForEdit();
        const auto action = *pendingVisibilityRuleDelete;
        const auto &currentRules = workbench_.GetConditionalVisibilityRules();
        if (action.ruleIndex < currentRules.size()) {
          const auto &rule = currentRules[action.ruleIndex];
          const bool changed = action.preserveTarget
              ? workbench_.SetConditionalVisibilityRuleConditionId(
                    action.ruleIndex, {})
              : workbench_.SetConditionalVisibilityRuleTarget(
                    action.ruleIndex, rule.targetKind, 0);
          if (changed) {
            refreshNativeAfterTable = true;
          }
        }
      }

      if (pendingConditionalRowAction.has_value()) {
        freezeWorkbenchOrderForEdit();
        const auto action = *pendingConditionalRowAction;
        const auto targetRowIndex =
            findRowIndexByUiIdentity(action.targetUiIdentity);
        const auto &currentRows = workbench_.GetRows();
        if (targetRowIndex >= 0 &&
            targetRowIndex < static_cast<int>(currentRows.size())) {
          const auto &sourceRow =
              currentRows[static_cast<std::size_t>(targetRowIndex)];
          // Removing a condition card must not remove the registered
          // equipment/appearance. Keep the row in the condition section with
          // an empty, inactive condition assignment instead.
          if (!action.conditionId.has_value() &&
              sourceRow.conditionId.has_value()) {
            if (workbench_.ClearConditionAssignmentKeepRow(
                    targetRowIndex)) {
              syncRowsAfterTable = true;
              refreshNativeAfterTable = true;
            }
          } else if (action.conditionId.has_value() &&
                     applyConditionDrop(
                         ConditionDragSourceKind::ConditionalRow,
                         action.targetUiIdentity, *action.conditionId,
                         action.draggedCondition)) {
            syncRowsAfterTable = true;
            refreshNativeAfterTable = true;
          }
        }
      } else if (pendingConditionalFittingReplacement.has_value()) {
        freezeWorkbenchOrderForEdit();
        const auto replacement = *pendingConditionalFittingReplacement;
        const auto targetRowIndex =
            findRowIndexByUiIdentity(replacement.targetUiIdentity);
        const auto &currentRows = workbench_.GetRows();
        if (targetRowIndex >= 0 &&
            targetRowIndex < static_cast<int>(currentRows.size())) {
          if (workbench_.ReplaceConditionalFittingTarget(
                  targetRowIndex, replacement.formID)) {
            static_cast<void>(clearMovedActionSource(
                replacement.draggedItem, replacement.targetUiIdentity));
            syncRowsAfterTable = true;
            refreshNativeAfterTable = true;
          }
        }
      }
      if (syncRowsAfterTable || refreshNativeAfterTable) {
        workbenchSortDeferredUntilClose_ = true;
      }
      // These mutations only edit SFS condition/action state; they never
      // change the actor's worn inventory. Resyncing from the actor here used
      // to normalize and reorder rows immediately after a drop/delete. The
      // next ordinary equipment refresh will still synchronize actual gear.
      if (refreshNativeAfterTable) {
        workbench_.RefreshNativeArmorOverridesForActorWithoutEquipmentSync(
            ResolveNewWorkbenchRowOwnerActorFormID(), conditionStore_.revision);
      }
    }
    ImGui::EndChild();
  }
}
} // namespace sfs
