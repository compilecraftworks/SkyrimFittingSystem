#include "Menu.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "StringUtils.h"
#include "ThemeConfig.h"
#include "conditions/Creation.h"
#include "conditions/Defaults.h"
#include "conditions/Status.h"
#include "imgui_internal.h"
#include "ui/InputWidgets.h"
#include "ui/Localization.h"
#include "ui/TableReorder.h"
#include "ui/catalog/Widgets.h"
#include "ui/components/PinnableTooltip.h"
#include "ui/conditions/DraftValidation.h"
#include "ui/conditions/Widgets.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>

namespace sfs {
namespace {
using ConditionClause = ui::conditions::Clause;
using ConditionDefinition = ui::conditions::Definition;

ImU32 ConditionSurfaceColor(const int a_red, const int a_green,
                            const int a_blue, const int a_alpha = 255) {
  constexpr float kChannelScale = 1.0f / 255.0f;
  return ImGui::GetColorU32(
      ImVec4(static_cast<float>(a_red) * kChannelScale,
             static_cast<float>(a_green) * kChannelScale,
             static_cast<float>(a_blue) * kChannelScale,
             static_cast<float>(a_alpha) * kChannelScale));
}

void MoveConditionDefinitionToSlot(
    std::vector<ConditionDefinition> &a_conditions,
    const std::size_t a_sourceIndex, std::size_t a_slotIndex) {
  if (a_sourceIndex >= a_conditions.size() ||
      a_slotIndex > a_conditions.size()) {
    return;
  }

  auto condition = std::move(a_conditions[a_sourceIndex]);
  a_conditions.erase(a_conditions.begin() +
                     static_cast<std::ptrdiff_t>(a_sourceIndex));
  if (a_sourceIndex < a_slotIndex) {
    --a_slotIndex;
  }
  a_conditions.insert(a_conditions.begin() +
                          static_cast<std::ptrdiff_t>(a_slotIndex),
                      std::move(condition));
}

std::vector<std::size_t> BuildCustomConditionIndices(
    const std::vector<ConditionDefinition> &a_conditions) {
  std::vector<std::size_t> indices;
  indices.reserve(a_conditions.size());
  for (std::size_t index = 0; index < a_conditions.size(); ++index) {
    const auto &condition = a_conditions[index];
    if (!conditions::IsBuiltInCondition(condition.id) &&
        !conditions::IsActorOwnershipCondition(condition)) {
      indices.push_back(index);
    }
  }
  return indices;
}
void SortConditionIndicesByName(
    const std::vector<ConditionDefinition> &a_conditions,
    std::vector<std::size_t> &a_indices) {
  std::ranges::sort(
      a_indices, [&](const std::size_t a_left, const std::size_t a_right) {
        return strings::CompareTextInsensitive(a_conditions[a_left].name,
                                               a_conditions[a_right].name) < 0;
      });
}

void MoveFilteredConditionDefinitionToSlot(
    std::vector<ConditionDefinition> &a_conditions,
    const std::vector<std::size_t> &a_filteredIndices,
    const std::size_t a_sourceFilteredIndex, const std::size_t a_slotIndex) {
  if (a_sourceFilteredIndex >= a_filteredIndices.size() ||
      a_slotIndex > a_filteredIndices.size()) {
    return;
  }

  const auto sourceIndex = a_filteredIndices[a_sourceFilteredIndex];
  std::size_t rawSlotIndex = a_conditions.size();
  if (a_slotIndex < a_filteredIndices.size()) {
    rawSlotIndex = a_filteredIndices[a_slotIndex];
  }
  MoveConditionDefinitionToSlot(a_conditions, sourceIndex, rawSlotIndex);
}

struct ConditionDeleteUsage {
  std::size_t referencingConditionCount{0};
  std::size_t appliedRowCount{0};

  [[nodiscard]] bool CanDelete() const {
    // Applied workbench rows are not a blocker: deleting the definition
    // intentionally clears only their condition half and leaves their action
    // cards in an inactive row. A condition referenced by another condition
    // must still be retained to avoid silently changing that expression.
    return referencingConditionCount == 0;
  }

  [[nodiscard]] std::string BuildTooltip() const {
    if (CanDelete()) {
      return {};
    }

    if (referencingConditionCount != 0) {
      return std::string(ui::Localization::GetSingleton()->Get(
          "conditions.catalog.delete_reason_referenced"));
    }
    return {};
  }
};

constexpr char kIconTrash[] = "\xee\x86\x8c";      // ICON_LC_TRASH
constexpr char kIconCircleHelp[] = "\xee\x82\x82"; // ICON_LC_CIRCLE_HELP
constexpr float kTooltipOrGroupIndicatorWidth = 6.0f;
constexpr float kTooltipOrGroupIndicatorInsetX = 4.0f;
constexpr float kTooltipOrGroupBoundaryGap = 4.0f;
constexpr float kTooltipOrGroupIndicatorRounding = 4.0f;

struct TooltipOrGroupVisual {
  ImRect operatorColumnRect;
  bool initialized{false};
};

[[nodiscard]] float ComputeCatalogConditionRowHeight() {
  const auto &style = ImGui::GetStyle();
  const auto lineHeight = ImGui::GetTextLineHeight();
  return style.FramePadding.y * 2.0f + lineHeight * 2.0f + style.ItemSpacing.y;
}

std::vector<ui::conditions::Color> CollectCatalogColorsForNewCondition(
    const std::vector<ConditionDefinition> &a_conditions,
    const std::vector<ui::conditions::editor::State> &a_editors) {
  std::vector<ui::conditions::Color> colors;
  colors.reserve(a_conditions.size() + a_editors.size());
  for (const auto &condition : a_conditions) {
    if (const auto *catalog = condition.GetCatalog(); catalog != nullptr) {
      colors.push_back(catalog->color);
    }
  }
  for (const auto &editor : a_editors) {
    if (!editor.isNew) {
      continue;
    }
    if (const auto *catalog = editor.draft.GetCatalog(); catalog != nullptr) {
      colors.push_back(catalog->color);
    }
  }
  return colors;
}

void DrawConditionTooltipHeader(
    std::string_view a_title,
    const std::optional<ui::conditions::Color> &a_color = std::nullopt) {
  const auto *theme = ThemeConfig::GetSingleton();
  const auto headerMin = ImGui::GetCursorScreenPos();
  const auto headerWidth = ImGui::GetContentRegionAvail().x;
  const auto headerHeight = ImGui::GetFontSize() * 2.4f;
  const auto headerMax =
      ImVec2(headerMin.x + headerWidth, headerMin.y + headerHeight);
  auto *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(headerMin, headerMax, theme->GetColorU32("BG"), 8.0f);
  const auto accentColor =
      a_color.has_value()
          ? ImGui::GetColorU32(ui::conditions::ToImGuiColor(*a_color))
          : theme->GetColorU32("PRIMARY", 0.65f);
  drawList->AddRect(headerMin, headerMax, accentColor, 8.0f);
  if (a_color.has_value()) {
    drawList->AddRectFilledMultiColor(
        headerMin, headerMax,
        ImGui::GetColorU32(ImVec4(a_color->x, a_color->y, a_color->z, 0.18f)),
        ImGui::GetColorU32(ImVec4(a_color->x, a_color->y, a_color->z, 0.18f)),
        theme->GetColorU32("NONE"), theme->GetColorU32("NONE"));
  }

  const auto titleFontSize = ImGui::GetFontSize() * 1.15f;
  const auto titleSize =
      ImGui::CalcTextSize(a_title.data(), nullptr, false, headerWidth);
  drawList->AddText(
      ImGui::GetFont(), titleFontSize,
      ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
             headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
      theme->GetColorU32("TEXT"), a_title.data(),
      a_title.data() + a_title.size());
  ImGui::Dummy(ImVec2(headerWidth, headerHeight));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Separator, accentColor);
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

void DrawConditionTooltipSectionHeader(const char *a_title) {
  ImGui::TextDisabled("%s", a_title);
  ImGui::Spacing();
}

void DrawConditionTooltipBulletLine(std::string_view a_text) {
  ImGui::Bullet();
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x + 4.0f);
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(a_text.data(), a_text.data() + a_text.size());
  ImGui::PopTextWrapPos();
}

void DrawConditionTooltip(const ConditionDefinition &a_condition,
                          const bool a_hoveredSource,
                          std::vector<ConditionDefinition> &a_conditions) {
  const auto tooltipId = "condition:" + a_condition.id;
  if (!ui::components::ShouldDrawPinnableTooltip(tooltipId, a_hoveredSource)) {
    return;
  }

  const auto materialized =
      conditions::MaterializeConditionById(a_condition.id, a_conditions);
  const auto conditionStatus =
      conditions::EvaluateDefinitionStatus(a_condition, a_conditions);
  const auto tooltipWidth = 460.0f;
  ImGui::SetNextWindowSize(
      ImVec2(tooltipWidth + ImGui::GetStyle().WindowPadding.x * 2.0f, 0.0f),
      ImGuiCond_Always);
  ui::components::DrawPinnableTooltip(tooltipId, a_hoveredSource, [&]() {
    auto *localization = ui::Localization::GetSingleton();
    DrawConditionTooltipHeader(
        a_condition.name, [&]() -> std::optional<ui::conditions::Color> {
          if (const auto *catalog = a_condition.GetCatalog();
              catalog != nullptr) {
            return catalog->color;
          }
          return std::nullopt;
        }());

    if (!a_condition.description.empty()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", a_condition.description.c_str());
      ImGui::PopTextWrapPos();
      ImGui::Spacing();
    }

    if (!conditionStatus.missingDependencyChains.empty()) {
      DrawConditionTooltipSectionHeader(
          localization->GetCStr("conditions.catalog.missing_references"));
      for (const auto &missingChain : conditionStatus.missingDependencyChains) {
        const auto label =
            conditions::FormatMissingDependencyChain(missingChain, 1);
        DrawConditionTooltipBulletLine(label);
      }
      ImGui::Spacing();
    }

    DrawConditionTooltipSectionHeader(
        localization->GetCStr("conditions.catalog.expanded_form"));
    if (!materialized.has_value() || materialized->displayCnf.empty()) {
      ImGui::TextDisabled(
          "%s", localization->GetCStr("conditions.catalog.unavailable"));
      return;
    }

    if (ImGui::BeginTable("##condition-expanded-form", 2,
                          ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn(
          localization->Get("conditions.catalog.expression").data(),
          ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("##operator", ImGuiTableColumnFlags_WidthFixed,
                              44.0f);
      std::vector<TooltipOrGroupVisual> orGroupVisuals;
      orGroupVisuals.reserve(materialized->displayCnf.size());

      for (std::size_t groupIndex = 0;
           groupIndex < materialized->displayCnf.size(); ++groupIndex) {
        const auto &group = materialized->displayCnf[groupIndex];
        const bool isOrGroup = group.size() > 1;
        TooltipOrGroupVisual groupVisual;
        for (std::size_t literalIndex = 0; literalIndex < group.size();
             ++literalIndex) {
          ImGui::TableNextRow();

          ImGui::TableSetColumnIndex(0);
          ImGui::PushTextWrapPos(0.0f);
          ImGui::TextUnformatted(group[literalIndex].c_str());
          ImGui::PopTextWrapPos();

          ImGui::TableSetColumnIndex(1);
          const char *op = "";
          if (literalIndex + 1 < group.size()) {
            op = "OR";
          } else if (groupIndex + 1 < materialized->displayCnf.size()) {
            op = "AND";
          }
          if (op[0] != '\0') {
            ImGui::TextDisabled("%s", op);
          }

          if (isOrGroup) {
            const auto rowRect =
                ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 1);
            if (!groupVisual.initialized) {
              groupVisual.operatorColumnRect = rowRect;
              groupVisual.initialized = true;
            } else {
              groupVisual.operatorColumnRect.Add(rowRect.Min);
              groupVisual.operatorColumnRect.Add(rowRect.Max);
            }
          }
        }

        if (groupVisual.initialized) {
          orGroupVisuals.push_back(groupVisual);
        }
      }

      const auto *theme = ThemeConfig::GetSingleton();
      auto *drawList = ImGui::GetWindowDrawList();
      for (const auto &groupVisual : orGroupVisuals) {
        const auto indicatorMin = ImVec2(groupVisual.operatorColumnRect.Min.x +
                                             kTooltipOrGroupIndicatorInsetX,
                                         groupVisual.operatorColumnRect.Min.y +
                                             kTooltipOrGroupBoundaryGap);
        const auto indicatorMax = ImVec2(
            indicatorMin.x + kTooltipOrGroupIndicatorWidth,
            groupVisual.operatorColumnRect.Max.y - kTooltipOrGroupBoundaryGap);
        drawList->AddRectFilled(indicatorMin, indicatorMax,
                                theme->GetColorU32("PRIMARY", 0.85f),
                                kTooltipOrGroupIndicatorRounding);
      }

      ImGui::EndTable();
    }
  });
}
} // namespace

bool Menu::AssignConditionToTopEmptyWorkbenchCard(
    const std::string_view a_conditionId) {
  if (a_conditionId.empty()) {
    return false;
  }
  auto *actor = ResolveWorkbenchPreviewActor();
  if (!actor) {
    return false;
  }

  const auto &rows = workbench_.GetRows();
  int bestRowIndex = -1;
  std::size_t bestSessionOrder = (std::numeric_limits<std::size_t>::max)();
  std::uint64_t bestRegistrationOrder =
      (std::numeric_limits<std::uint64_t>::max)();
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size());
       ++rowIndex) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    if (!row.IsSlotRow() || !row.IsOwnedByActor(actor) ||
        !row.conditionId.has_value() || row.HasOverridesOrHideState()) {
      continue;
    }
    const bool emptyConditionCard =
        row.conditionId->empty() ||
        conditions::FindDefinitionById(
            ConditionDefinitions(), *row.conditionId) ==
            nullptr;
    if (!emptyConditionCard) {
      continue;
    }
    const auto sessionIt =
        workbenchConditionalSessionOrder_.find(row.uiIdentity);
    const auto sessionOrder =
        sessionIt != workbenchConditionalSessionOrder_.end()
            ? sessionIt->second
            : (std::numeric_limits<std::size_t>::max)();
    if (bestRowIndex < 0 || sessionOrder < bestSessionOrder ||
        (sessionOrder == bestSessionOrder &&
         row.registrationOrder < bestRegistrationOrder)) {
      bestRowIndex = rowIndex;
      bestSessionOrder = sessionOrder;
      bestRegistrationOrder = row.registrationOrder;
    }
  }
  if (bestRowIndex < 0 || !workbench_.SetConditionAssignmentKeepRow(
                              bestRowIndex, a_conditionId)) {
    return false;
  }
  workbenchSortDeferredUntilClose_ = true;
  workbench_.RefreshNativeArmorOverridesForActor(
      actor->GetFormID(), conditionStore_.revision);
  return true;
}

bool Menu::DrawConditionTab() {
  EnsureDefaultConditions();
  auto *localization = ui::Localization::GetSingleton();
  bool rowClicked = false;

  if (ImGui::BeginChild("##conditions-pane", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_None)) {
    struct Category {
      std::initializer_list<const char *> ids;
    };
    const Category categories[] = {
        {{"builtin-interior", "builtin-exterior", "builtin-city",
          "builtin-town", "builtin-dungeon", "builtin-home"}},
        {{"builtin-combat", "builtin-noncombat"}},
        {{"builtin-day", "builtin-night"}},
        {{"builtin-rain", "builtin-snow", "builtin-underwater"}},
        {{"builtin-sneaking", "builtin-weapon"}}};

    for (const auto &category : categories) {

      std::vector<ConditionDefinition *> cards;
      cards.reserve(category.ids.size());
      for (const auto *id : category.ids) {
        if (auto *condition =
                conditions::FindDefinitionById(ConditionDefinitions(), id);
            condition != nullptr) {
          cards.push_back(condition);
        }
      }

      constexpr float minimumCardWidth = 82.0f;
      constexpr float accentWidth = 5.0f;
      const float spacing = ImGui::GetStyle().ItemSpacing.x;
      const float cardHeight = ComputeCatalogConditionRowHeight();
      std::size_t cardIndex = 0;
      while (cardIndex < cards.size()) {
        const float available = ImGui::GetContentRegionAvail().x;
        const auto maximumCardsInRow = static_cast<std::size_t>((std::max)(
            1.0f, (available + spacing) / (minimumCardWidth + spacing)));
        const auto cardsInRow =
            (std::min)(maximumCardsInRow, cards.size() - cardIndex);
        const float cardWidth =
            (available - spacing * static_cast<float>(cardsInRow - 1)) /
            static_cast<float>(cardsInRow);

        for (std::size_t rowIndex = 0; rowIndex < cardsInRow; ++rowIndex) {
          auto &condition = *cards[cardIndex + rowIndex];
          if (rowIndex != 0) {
            ImGui::SameLine(0.0f, spacing);
          }

          ImGui::PushID(condition.id.c_str());
          ImGui::InvisibleButton("##builtin-condition-card",
                                 ImVec2(cardWidth, cardHeight));
          const bool hovered = ImGui::IsItemHovered();
          const bool held = ImGui::IsItemActive();
          const ImRect cardRect(ImGui::GetItemRectMin(),
                                ImGui::GetItemRectMax());
          auto *drawList = ImGui::GetWindowDrawList();
          const auto bodyColor = held || hovered
                                     ? ConditionSurfaceColor(42, 42, 44)
                                     : ConditionSurfaceColor(34, 34, 36);
          drawList->AddRectFilled(cardRect.Min, cardRect.Max, bodyColor, 4.0f);
          drawList->AddRect(cardRect.Min, cardRect.Max,
                            ImGui::GetColorU32(ImGuiCol_Border), 4.0f);

          const auto color = condition.EnsureCatalog().color;
          drawList->AddRectFilled(
              ImVec2(cardRect.Min.x, cardRect.Min.y + 2.0f),
              ImVec2(cardRect.Min.x + accentWidth, cardRect.Max.y - 2.0f),
              ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, color.w)),
              3.0f);

          const auto textSize = ImGui::CalcTextSize(condition.name.c_str());
          const float textMinX =
              cardRect.Min.x + accentWidth + ImGui::GetStyle().FramePadding.x;
          const float textMaxX =
              cardRect.Max.x - ImGui::GetStyle().FramePadding.x;
          const float textX =
              textMinX +
              (std::max)(0.0f, (textMaxX - textMinX - textSize.x) * 0.5f);
          const float textY =
              cardRect.Min.y +
              (std::max)(0.0f, (cardRect.GetHeight() - textSize.y) * 0.5f);
          drawList->PushClipRect(ImVec2(textMinX, cardRect.Min.y),
                                 ImVec2(textMaxX, cardRect.Max.y), true);
          drawList->AddText(ImVec2(textX, textY),
                            ImGui::GetColorU32(ImGuiCol_Text),
                            condition.name.c_str());
          drawList->PopClipRect();

          DrawConditionTooltip(condition, hovered, ConditionDefinitions());
          if (hovered &&
              ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            static_cast<void>(
                AssignConditionToTopEmptyWorkbenchCard(condition.id));
          }
          if (ImGui::BeginDragDropSource()) {
            DraggedConditionPayload payload{};
            std::snprintf(payload.conditionId.data(),
                          payload.conditionId.size(), "%s",
                          condition.id.c_str());
            ImGui::SetDragDropPayload("SFS_CONDITION", &payload,
                                      sizeof(payload));
            ImGui::TextUnformatted(condition.name.c_str());
            ImGui::EndDragDropSource();
          }
          ImGui::PopID();
        }
        cardIndex += cardsInRow;
      }
      ImGui::Spacing();
    }
    ImGui::SeparatorText(localization->GetCStr("conditions.custom.title"));
    rowClicked = DrawConditionCatalogTable();
  }
  ImGui::EndChild();
  return rowClicked;
}

bool Menu::DrawConditionCatalogTable() {
  EnsureDefaultConditions();
  auto catalogIndices = BuildCustomConditionIndices(ConditionDefinitions());

  if (!ImGui::BeginTable("##conditions-table", 1,
                         ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_PadOuterX | ImGuiTableFlags_RowBg,
                         ImVec2(0.0f, 0.0f))) {
    return false;
  }

  auto *localization = ui::Localization::GetSingleton();
  ImGui::TableSetupColumn(localization->GetCStr("conditions.catalog.condition"),
                          ImGuiTableColumnFlags_WidthStretch);
  bool rowClicked = false;
  std::optional<std::size_t> pendingDeleteIndex;
  std::optional<std::size_t> pendingCopyIndex;
  std::vector<ImRect> reorderRowRects;
  reorderRowRects.reserve(catalogIndices.size());

  for (std::size_t filteredIndex = 0; filteredIndex < catalogIndices.size();
       ++filteredIndex) {
    const auto index = catalogIndices[filteredIndex];
    auto &condition = ConditionDefinitions()[index];
    const auto conditionStatus =
        conditions::EvaluateDefinitionStatus(condition, ConditionDefinitions());
    const bool broken = conditionStatus.IsBroken();
    ConditionDeleteUsage deleteUsage;
    for (const auto &otherCondition : ConditionDefinitions()) {
      if (otherCondition.id == condition.id) {
        continue;
      }
      if (std::ranges::any_of(
              otherCondition.clauses, [&](const ConditionClause &a_clause) {
                return a_clause.customConditionId == condition.id;
              })) {
        ++deleteUsage.referencingConditionCount;
      }
    }
    for (const auto &row : workbench_.GetRows()) {
      if (row.conditionId && *row.conditionId == condition.id) {
        ++deleteUsage.appliedRowCount;
      }
    }
    for (const auto &rule : workbench_.GetConditionalVisibilityRules()) {
      if (rule.conditionId == condition.id) {
        ++deleteUsage.appliedRowCount;
      }
    }
    for (const auto &pending : pendingSlotCreations_) {
      if (pending.conditionId && *pending.conditionId == condition.id) {
        ++deleteUsage.appliedRowCount;
      }
    }
    const auto deleteTooltip = deleteUsage.BuildTooltip();
    const bool deleteEnabled = deleteUsage.CanDelete();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(index));

    if (const auto *rowTable = ImGui::GetCurrentTable(); rowTable != nullptr) {
      reorderRowRects.emplace_back();
      const auto rowCellRect = ImGui::TableGetCellBgRect(rowTable, 0);
      const auto cellPadding = ImGui::GetStyle().CellPadding;
      const auto rowHeight = ComputeCatalogConditionRowHeight();
      const auto cellContentHeight =
          (rowCellRect.Max.y - rowCellRect.Min.y) - (cellPadding.y * 2.0f);
      const auto cellContentOffsetY =
          (std::max)(0.0f, (cellContentHeight - rowHeight) * 0.5f);
      ImGui::SetCursorScreenPos(
          ImVec2(rowCellRect.Min.x + cellPadding.x,
                 rowCellRect.Min.y + cellPadding.y + cellContentOffsetY));
      const auto width =
          (std::max)(0.0f, (rowCellRect.Max.x - rowCellRect.Min.x) -
                               (cellPadding.x * 2.0f));
      ImGui::InvisibleButton("##condition-row", ImVec2(width, rowHeight));
    } else {
      const auto rowHeight = ComputeCatalogConditionRowHeight();
      const auto width = ImGui::GetContentRegionAvail().x;
      ImGui::InvisibleButton("##condition-row", ImVec2(width, rowHeight));
      reorderRowRects.emplace_back(ImGui::GetItemRectMin(),
                                   ImGui::GetItemRectMax());
    }
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const auto stripeWidth = 6.0f;
    const auto deletePaneWidth = 34.0f;
    const auto rounding = ImGui::GetStyle().FrameRounding;
    const ImVec2 deleteMin(max.x - deletePaneWidth, min.y);
    const ImVec2 deleteMax = max;
    const auto deleteState = ui::input_widgets::EvaluateRectClickTarget(
        ImGui::GetID("##condition-row-delete"), deleteMin, deleteMax);
    const bool deleteHovered = deleteState.hovered;
    const bool deleteHeld = deleteState.held;
    const bool deletePressed = deleteState.pressed;
    const auto hovered = ImGui::IsItemHovered() || deleteHovered;
    const bool rowBodyHovered =
        hovered && !deleteHovered &&
        ImGui::IsMouseHoveringRect(min, ImVec2(deleteMin.x, max.y), false);
    DrawConditionTooltip(condition, rowBodyHovered, ConditionDefinitions());
    rowClicked |= rowBodyHovered && ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (rowBodyHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      if (!AssignConditionToTopEmptyWorkbenchCard(condition.id)) {
        OpenConditionEditorDialog(index);
      }
    }

    auto *drawList = ImGui::GetWindowDrawList();
    auto *theme = ThemeConfig::GetSingleton();
    const auto *catalog = condition.GetCatalog();
    if (catalog == nullptr) {
      ImGui::PopID();
      continue;
    }
    const auto &conditionColor = catalog->color;
    const auto bodyColor = hovered ? ConditionSurfaceColor(42, 42, 44)
                                   : ConditionSurfaceColor(34, 34, 36);
    drawList->AddRectFilled(min, max, bodyColor, rounding);
    drawList->AddRect(min, max, theme->GetColorU32("BORDER"), rounding);
    drawList->AddRectFilled(
        min, ImVec2(min.x + stripeWidth, max.y),
        broken ? theme->GetColorU32("WARN", 0.95f)
               : ImGui::GetColorU32(ImVec4(conditionColor.x, conditionColor.y,
                                           conditionColor.z, 1.0f)),
        rounding,
        ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft);
    const ImU32 deleteFillColor =
        deleteEnabled
            ? (deleteHeld      ? theme->GetColorU32("DECLINE")
               : deleteHovered ? theme->GetColorU32("DECLINE", 0.95f)
                               : theme->GetColorU32("DECLINE", 0.78f))
            : theme->GetColorU32("DECLINE", deleteHovered ? 0.35f : 0.24f);
    drawList->AddRectFilled(deleteMin, deleteMax, deleteFillColor, rounding,
                            ImDrawFlags_RoundCornersTopRight |
                                ImDrawFlags_RoundCornersBottomRight);
    drawList->AddLine(ImVec2(deleteMin.x, deleteMin.y),
                      ImVec2(deleteMin.x, deleteMax.y),
                      ConditionSurfaceColor(255, 255, 255, 18), 1.0f);
    const auto deleteIconSize = ImGui::CalcTextSize(kIconTrash);
    drawList->AddText(
        ImVec2(deleteMin.x + ((deletePaneWidth - deleteIconSize.x) * 0.5f),
               deleteMin.y +
                   (((deleteMax.y - deleteMin.y) - deleteIconSize.y) * 0.5f)),
        ImGui::GetColorU32(deleteEnabled ? ImGuiCol_Text
                                         : ImGuiCol_TextDisabled),
        kIconTrash);

    const auto contentMin = ImVec2(min.x + stripeWidth + 10.0f,
                                   min.y + ImGui::GetStyle().CellPadding.y);
    const auto clipRect =
        ImVec4(contentMin.x, min.y, deleteMin.x - 2.0f, max.y);
    const auto titleColor =
        broken ? theme->GetColorU32("WARN")
               : theme->GetColorU32("TEXT");
    drawList->PushClipRect(ImVec2(clipRect.x, clipRect.y),
                           ImVec2(clipRect.z, clipRect.w), true);
    drawList->AddText(contentMin, titleColor, condition.name.c_str());
    if (!condition.description.empty()) {
      const auto descriptionMin =
          ImVec2(contentMin.x, contentMin.y + ImGui::GetTextLineHeight() +
                                   ImGui::GetStyle().ItemSpacing.y);
      const auto descriptionMax = ImVec2(
          deleteMin.x - 3.0f, descriptionMin.y + ImGui::GetTextLineHeight());
      const auto descriptionSize =
          ImGui::CalcTextSize(condition.description.c_str());
      ImGui::RenderTextEllipsis(drawList, descriptionMin, descriptionMax,
                                descriptionMax.x, condition.description.c_str(),
                                nullptr, &descriptionSize);
    }
    drawList->PopClipRect();

    if (deleteHovered) {
      if (deleteEnabled) {
        if (deletePressed) {
          pendingDeleteIndex = index;
        }
      } else if (!deleteTooltip.empty()) {
        ui::condition_widgets::DrawHoverDescription(
            "conditions:delete-disabled:" + condition.id, true, deleteTooltip,
            0.2f);
      }
    }

    if (!deleteHovered &&
        ImGui::BeginPopupContextItem("##condition-row-context")) {
      if (ImGui::MenuItem(localization->GetCStr("common.edit"))) {
        OpenConditionEditorDialog(index);
      }
      if (ImGui::MenuItem(localization->GetCStr("common.copy"))) {
        pendingCopyIndex = index;
      }
      ImGui::Separator();
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          ImGui::ColorConvertU32ToFloat4(
              ThemeConfig::GetSingleton()->GetColorU32("DECLINE")));
      if (ImGui::MenuItem(localization->GetCStr("common.delete"), nullptr,
                          false, deleteEnabled)) {
        pendingDeleteIndex = index;
      }
      ImGui::PopStyleColor();
      if (!deleteEnabled) {
        ui::condition_widgets::DrawHoverDescription(
            "conditions:delete-disabled-menu:" + condition.id, deleteTooltip,
            0.2f, ImGuiHoveredFlags_AllowWhenDisabled);
      }
      ImGui::EndPopup();
    }

    if (!deleteHovered && ImGui::BeginDragDropSource()) {
      DraggedConditionPayload payload{};
      std::snprintf(payload.conditionId.data(), payload.conditionId.size(),
                    "%s", condition.id.c_str());
      ImGui::SetDragDropPayload("SFS_CONDITION", &payload, sizeof(payload));
      ImGui::TextUnformatted(condition.name.c_str());
      if (!condition.description.empty()) {
        ImGui::TextUnformatted(condition.description.c_str());
      }
      ImGui::EndDragDropSource();
    }
    if (const auto *rowTable = ImGui::GetCurrentTable(); rowTable != nullptr) {
      reorderRowRects.back() = ImGui::TableGetCellBgRect(rowTable, 0);
    }
    ImGui::PopID();
  }

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  if (ImGui::Button(localization->GetCStr("conditions.custom.add"),
                    ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
    OpenNewConditionDialog();
  }

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TextDisabled("%s",
                      localization->GetCStr("conditions.custom.drag_hint"));

  const auto *conditionTable = ImGui::GetCurrentTable();
  const bool hasConditionTable = conditionTable != nullptr;
  const auto conditionTableRect =
      hasConditionTable ? conditionTable->OuterRect : ImRect{};
  const auto reorderPreview =
      ImGui::IsDragDropActive() && hasConditionTable && !reorderRowRects.empty()
          ? ui::table_reorder::ComputeLinearReorderPreview(
                reorderRowRects, conditionTableRect.Min.x + 2.0f,
                conditionTableRect.Max.x - 2.0f)
          : ui::table_reorder::LinearReorderPreview{};
  ImGui::EndTable();
  if (hasConditionTable) {
    ui::table_reorder::DrawLinearReorderInsertionLine(
        reorderPreview,
        ImGui::GetColorU32(ThemeConfig::GetSingleton()->GetActive("PRIMARY")),
        3.0f);
  }

  if (reorderPreview.HasHoveredSlot() &&
      ImGui::BeginDragDropTargetCustom(
          reorderPreview.hoveredSlotRect,
          ImGui::GetID(("##condition-reorder-slot-" +
                        std::to_string(*reorderPreview.hoveredSlotIndex))
                           .c_str()))) {
    if (const auto *payload = ImGui::AcceptDragDropPayload(
            "SFS_CONDITION", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        payload && payload->DataSize == sizeof(DraggedConditionPayload)) {
      DraggedConditionPayload dragPayload{};
      std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
      if (const auto it = std::ranges::find(
              ConditionDefinitions(),
              std::string_view(dragPayload.conditionId.data()),
              &ConditionDefinition::id);
          it != ConditionDefinitions().end()) {
        const auto sourceIndex = static_cast<std::size_t>(
            std::distance(ConditionDefinitions().begin(), it));
        if (const auto filteredSourceIt =
                std::ranges::find(catalogIndices, sourceIndex);
            filteredSourceIt != catalogIndices.end()) {
          MoveFilteredConditionDefinitionToSlot(
              ConditionDefinitions(), catalogIndices,
              static_cast<std::size_t>(
                  std::distance(catalogIndices.begin(), filteredSourceIt)),
              *reorderPreview.hoveredSlotIndex);
          BumpConditionStoreRevision();
        }
      }
    }
    ImGui::EndDragDropTarget();
  }

  const auto buildExtraNameConflict = [&]() {
    return [&](std::string_view a_candidate) {
      return std::ranges::any_of(
          ConditionEditors(),
          [&](const ui::conditions::editor::State &a_editor) {
            return a_editor.isNew && strings::CompareTextInsensitive(
                                         strings::TrimText(a_editor.draft.name),
                                         a_candidate) == 0;
          });
    };
  };

  if (pendingCopyIndex && *pendingCopyIndex < ConditionDefinitions().size()) {
    const auto &source = ConditionDefinitions()[*pendingCopyIndex];
    auto copy = source;
    copy.id = conditions::BuildConditionId(NextConditionId()++);
    copy.name = ui::condition_editor::BuildUniqueConditionName(
        source.name, ConditionDefinitions(), buildExtraNameConflict());
    if (auto *catalog = copy.GetCatalog(); catalog != nullptr) {
      const auto existingColors = CollectCatalogColorsForNewCondition(
          ConditionDefinitions(), ConditionEditors());
      catalog->color = conditions::PickDistinctConditionColor(existingColors);
    }
    ConditionDefinitions().push_back(std::move(copy));
    BumpConditionStoreRevision();
    conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
    conditions::InvalidateConditionMaterializationCaches(
        ConditionDefinitions());
  }

  if (pendingDeleteIndex &&
      *pendingDeleteIndex < ConditionDefinitions().size()) {
    const auto deletedConditionId =
        ConditionDefinitions()[*pendingDeleteIndex].id;
    // Keep every conditional row and its registered cards; only clear the
    // deleted condition assignment so the row remains inactive in place.
    workbench_.DeleteRowsByConditionId(deletedConditionId, false);
    workbench_.ClearConditionalVisibilityRuleConditionsByConditionId(
        deletedConditionId);
    ConditionDefinitions().erase(
        ConditionDefinitions().begin() +
        static_cast<std::ptrdiff_t>(*pendingDeleteIndex));
    BumpConditionStoreRevision();
    sfs::conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
    sfs::conditions::InvalidateConditionMaterializationCaches(
        ConditionDefinitions());
    for (auto &editor : ConditionEditors()) {
      if (editor.sourceConditionId == deletedConditionId) {
        editor.error.clear();
        editor.open = false;
      }
    }
  }

  return rowClicked;
}

} // namespace sfs
