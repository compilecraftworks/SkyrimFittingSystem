#include "ui/workbench/Tooltips.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "conditions/Status.h"
#include "conditions/Validation.h"
#include "imgui_internal.h"
#include "ui/Localization.h"
#include "ui/components/PinnableTooltip.h"

namespace sfs::ui::workbench {
namespace {
constexpr float kConditionTooltipOrGroupIndicatorWidth = 6.0f;
constexpr float kConditionTooltipOrGroupIndicatorInsetX = 4.0f;
constexpr float kConditionTooltipOrGroupBoundaryGap = 4.0f;
constexpr float kConditionTooltipOrGroupIndicatorRounding = 4.0f;

struct ConditionTooltipOrGroupVisual {
  ImRect operatorColumnRect;
  bool initialized{false};
};

void DrawConditionTooltipHeader(
    const std::string_view a_title,
    const std::optional<ui::conditions::Color> &a_color = std::nullopt) {
  const auto *theme = ThemeConfig::GetSingleton();
  const auto headerMin = ImGui::GetCursorScreenPos();
  const auto headerWidth = ImGui::GetContentRegionAvail().x;
  const auto headerHeight = ImGui::GetFontSize() * 2.4f;
  const auto headerMax =
      ImVec2(headerMin.x + headerWidth, headerMin.y + headerHeight);
  auto *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(headerMin, headerMax, theme->GetColorU32("BG"),
                          8.0f);
  const auto accentColor =
      a_color.has_value()
          ? ImGui::GetColorU32(ui::conditions::ToImGuiColor(*a_color))
          : theme->GetColorU32("PRIMARY", 0.65f);
  drawList->AddRect(headerMin, headerMax, accentColor, 8.0f);
  if (a_color.has_value()) {
    drawList->AddRectFilledMultiColor(
        headerMin, headerMax,
        ImGui::GetColorU32(
            ImVec4(a_color->x, a_color->y, a_color->z, 0.18f)),
        ImGui::GetColorU32(
            ImVec4(a_color->x, a_color->y, a_color->z, 0.18f)),
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

void DrawConditionTooltipBulletLine(const std::string_view a_text) {
  ImGui::Bullet();
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x + 4.0f);
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(a_text.data(), a_text.data() + a_text.size());
  ImGui::PopTextWrapPos();
}
} // namespace

void DrawWrappedColoredTextRuns(
    const std::initializer_list<std::pair<std::string_view, ImU32>> a_runs) {
  const auto wrapWidth = ImGui::GetContentRegionAvail().x;
  const auto lineHeight = ImGui::GetTextLineHeight();
  const auto origin = ImGui::GetCursorScreenPos();
  auto *drawList = ImGui::GetWindowDrawList();

  float x = 0.0f;
  float y = 0.0f;

  const auto advanceLine = [&]() {
    x = 0.0f;
    y += lineHeight;
  };

  for (const auto &[text, color] : a_runs) {
    std::size_t index = 0;
    while (index < text.size()) {
      const char current = text[index];
      if (current == '\n') {
        advanceLine();
        ++index;
        continue;
      }

      const bool isSpace = current == ' ' || current == '\t';
      std::size_t end = index;
      while (end < text.size()) {
        const char ch = text[end];
        if (ch == '\n') {
          break;
        }
        const bool sameClass = ((ch == ' ' || ch == '\t') == isSpace);
        if (!sameClass) {
          break;
        }
        ++end;
      }

      const auto token = text.substr(index, end - index);
      const auto tokenSize =
          ImGui::CalcTextSize(token.data(), token.data() + token.size());

      if (isSpace) {
        if (x > 0.0f) {
          if (x + tokenSize.x > wrapWidth) {
            advanceLine();
          } else {
            x += tokenSize.x;
          }
        }
      } else {
        if (x > 0.0f && x + tokenSize.x > wrapWidth) {
          advanceLine();
        }

        drawList->AddText(ImVec2(origin.x + x, origin.y + y), color,
                          token.data(), token.data() + token.size());
        x += tokenSize.x;
      }

      index = end;
    }
  }

  ImGui::Dummy(ImVec2(wrapWidth, y + lineHeight));
}

void DrawConflictEntry(const ui::workbench_conflicts::ConflictEntry &a_desc,
                       const ThemeConfig *a_theme) {
  const auto *localization = ui::Localization::GetSingleton();
  ImGui::Bullet();
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::BeginGroup();
  DrawWrappedColoredTextRuns(
      {{a_desc.primaryName, a_theme->GetColorU32("TEXT")},
       {a_desc.isOverride ? localization->Get("workbench.conflict.override_on")
                          : localization->Get("workbench.conflict.separator"),
        a_theme->GetColorU32("TEXT_DISABLED")},
       {a_desc.secondaryName, a_theme->GetColorU32("PRIMARY")},
       {a_desc.targetLabel.empty() ? "" : "  [", a_theme->GetColorU32("TEXT")},
       {a_desc.targetLabel, a_theme->GetColorU32("TEXT_HEADER", 0.92f)},
       {a_desc.targetLabel.empty() ? "" : "]", a_theme->GetColorU32("TEXT")}});

  ImGui::EndGroup();
}

void DrawConflictTooltipSection(
    const char *a_title,
    const std::function<void(const ThemeConfig *)> &a_drawEntry) {
  const auto *theme = ThemeConfig::GetSingleton();
  ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("WARN"));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();
  ImGui::TextColored(theme->GetColor("WARN"), "%s", a_title);
  ImGui::Spacing();
  a_drawEntry(theme);
}

void DrawSimplePinnableTooltip(const std::string_view a_id,
                               const bool a_hoveredSource,
                               const std::function<void()> &a_drawBody) {
  ui::components::DrawPinnableTooltip(a_id, a_hoveredSource, a_drawBody);
}

void DrawSimplePinnableTooltip(const std::string_view a_id,
                               const bool a_hoveredSource,
                               const std::function<void()> &a_drawBody,
                               const float a_minimumWidth) {
  ui::components::DrawPinnableTooltip(
      a_id, a_hoveredSource, a_drawBody, {.minimumWidth = a_minimumWidth});
}

void DrawWorkbenchFilterOptionTooltip(
    const ui::workbench::FilterOption &a_option) {
  if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_option.actorFormID)) {
    ImGui::Text("%s %s",
                ui::Localization::GetSingleton()
                    ->Get("workbench.filters.ref_name_prefix")
                    .data(),
                armor::GetDisplayName(actor).c_str());
  } else {
    ImGui::TextDisabled("%s", ui::Localization::GetSingleton()
                                  ->Get("workbench.filters.ref_unresolved")
                                  .data());
  }
}

void DrawConditionDefinitionTooltip(
    const std::string_view a_tooltipId,
    const ::sfs::conditions::Definition &a_condition,
    const bool a_hoveredSource,
    std::vector<::sfs::conditions::Definition> &a_conditions) {
  if (!ui::components::ShouldDrawPinnableTooltip(a_tooltipId,
                                                  a_hoveredSource)) {
    return;
  }

  const auto materialized = ::sfs::conditions::MaterializeConditionById(
      a_condition.id, a_conditions);
  const auto conditionStatus =
      ::sfs::conditions::EvaluateDefinitionStatus(a_condition, a_conditions);
  constexpr float tooltipWidth = 460.0f;
  ImGui::SetNextWindowSize(
      ImVec2(tooltipWidth + ImGui::GetStyle().WindowPadding.x * 2.0f, 0.0f),
      ImGuiCond_Always);
  ui::components::DrawPinnableTooltip(a_tooltipId, a_hoveredSource, [&]() {
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
      for (const auto &missingChain :
           conditionStatus.missingDependencyChains) {
        DrawConditionTooltipBulletLine(
            ::sfs::conditions::FormatMissingDependencyChain(missingChain, 1));
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

    if (!ImGui::BeginTable("##condition-expanded-form", 2,
                           ImGuiTableFlags_BordersInnerV |
                               ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_SizingStretchProp)) {
      return;
    }
    ImGui::TableSetupColumn(
        localization->Get("conditions.catalog.expression").data(),
        ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##operator", ImGuiTableColumnFlags_WidthFixed,
                            44.0f);
    std::vector<ConditionTooltipOrGroupVisual> orGroupVisuals;
    orGroupVisuals.reserve(materialized->displayCnf.size());

    for (std::size_t groupIndex = 0;
         groupIndex < materialized->displayCnf.size(); ++groupIndex) {
      const auto &group = materialized->displayCnf[groupIndex];
      const bool isOrGroup = group.size() > 1;
      ConditionTooltipOrGroupVisual groupVisual;
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
      const auto indicatorMin = ImVec2(
          groupVisual.operatorColumnRect.Min.x +
              kConditionTooltipOrGroupIndicatorInsetX,
          groupVisual.operatorColumnRect.Min.y +
              kConditionTooltipOrGroupBoundaryGap);
      const auto indicatorMax = ImVec2(
          indicatorMin.x + kConditionTooltipOrGroupIndicatorWidth,
          groupVisual.operatorColumnRect.Max.y -
              kConditionTooltipOrGroupBoundaryGap);
      drawList->AddRectFilled(
          indicatorMin, indicatorMax,
          theme->GetColorU32("PRIMARY", 0.85f),
          kConditionTooltipOrGroupIndicatorRounding);
    }
    ImGui::EndTable();
  });
}

RowConditionVisualState ResolveRowConditionVisualState(
    const ::sfs::workbench::VariantWorkbenchRow &a_row,
    const std::vector<ui::conditions::Definition> &a_conditions) {
  RowConditionVisualState state;
  if (!a_row.conditionId.has_value()) {
    return state;
  }

  if (const auto *condition = ::sfs::conditions::FindDefinitionById(
          a_conditions, *a_row.conditionId);
      condition != nullptr) {
    if (const auto *catalog = condition->GetCatalog(); catalog != nullptr) {
      state.color = ui::conditions::ToImGuiColor(catalog->color);
    }
    const auto conditionStatus =
        ::sfs::conditions::EvaluateDefinitionStatus(*condition, a_conditions);
    if (conditionStatus.IsBroken()) {
      state.name = condition->name;
      state.description = condition->description;
      if (!state.description.empty()) {
        state.description.append("\n\n");
      }
      state.description.append(ui::Localization::GetSingleton()->Get(
          "workbench.condition.broken_description"));
      state.description.append(ui::Localization::GetSingleton()->Get(
          "workbench.condition.missing_list_header"));
      for (const auto &missingChain : conditionStatus.missingDependencyChains) {
        state.description.append("\n- ");
        state.description.append(
            ::sfs::conditions::FormatMissingDependencyChain(missingChain, 1));
      }
      state.disabled = true;
      state.brokenCondition = true;
      return state;
    }
    state.name = condition->name;
    state.description = condition->description;
    return state;
  }

  state.name = std::string(ui::Localization::GetSingleton()->Get(
      "workbench.condition.missing_name"));
  state.description = std::string(ui::Localization::GetSingleton()->Get(
      "workbench.condition.missing_description"));
  state.disabled = true;
  state.missing = true;
  return state;
}
} // namespace sfs::ui::workbench
