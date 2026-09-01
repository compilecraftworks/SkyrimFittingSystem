#include "ui/components/EquipmentWidget.h"

#include "ArmorUtils.h"
#include "imgui_internal.h"
#include "ui/InputWidgets.h"
#include "ui/Localization.h"
#include "ui/ThemeConfig.h"
#include "ui/components/PinnableTooltip.h"
#include "workbench/ItemFactory.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace {
constexpr std::string_view kIconEditorId = "\xee\x84\x8b";   // ICON_LC_LIST
constexpr std::string_view kIconPlugin = "\xee\x84\xac";     // ICON_LC_PACKAGE
constexpr std::string_view kIconFormId = "\xee\x83\xb2";     // ICON_LC_HASH
constexpr std::string_view kIconIdentifier = "\xee\x84\x87"; // ICON_LC_LINK
constexpr std::string_view kIconSlot = "\xee\x87\x89";       // ICON_LC_SHIRT
constexpr std::string_view kIconTrash = "\xee\x86\x8c";      // ICON_LC_TRASH
constexpr std::string_view kIconEye = "\xee\x82\xbe";        // ICON_LC_EYE
constexpr std::string_view kIconEyeOff = "\xee\x82\xbf";     // ICON_LC_EYE_OFF
constexpr std::string_view kIconDye =
    "\xee\x8b\xa5"; // ICON_LC_PAINT_BUCKET (U+E2E5)

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DrawTooltipInfoRow(const char *a_icon, const char *a_label,
                        const std::string &a_value) {
  if (a_value.empty()) {
    return;
  }

  const auto *theme = sfs::ThemeConfig::GetSingleton();
  ImGui::TableNextRow();

  ImGui::TableSetColumnIndex(0);
  if (a_icon && a_icon[0] != '\0') {
    ImGui::TextColored(theme->GetColor("PRIMARY"), "%s", a_icon);
    ImGui::SameLine(0.0f, 6.0f);
  }
  if (a_label && a_label[0] != '\0') {
    ImGui::TextDisabled("%s", a_label);
  }

  ImGui::TableSetColumnIndex(1);
  const auto availableWidth = ImGui::GetContentRegionAvail().x;
  const auto valueWidth = ImGui::CalcTextSize(a_value.c_str()).x;
  if (valueWidth < availableWidth) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - valueWidth);
    ImGui::TextUnformatted(a_value.c_str());
  } else {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + availableWidth);
    ImGui::TextWrapped("%s", a_value.c_str());
    ImGui::PopTextWrapPos();
  }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
float ComputeEquipmentTooltipWidth(
    const std::string &a_displayName, const std::string &a_editorID,
    const std::string &a_plugin, const std::string &a_formID,
    const std::string &a_identifier,
    const std::vector<std::string> &a_slotLabels,
    const std::vector<std::string> &a_addonSlotLabels) {
  float widestValueWidth = ImGui::CalcTextSize(a_displayName.c_str()).x;
  for (const auto *value : {a_editorID.c_str(), a_plugin.c_str(),
                            a_formID.c_str(), a_identifier.c_str()}) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(value).x);
  }
  for (const auto &slotLabel : a_slotLabels) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(slotLabel.c_str()).x);
  }
  for (const auto &slotLabel : a_addonSlotLabels) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(slotLabel.c_str()).x);
  }

  return (std::max)(330.0f, widestValueWidth + 190.0f);
}

void DrawEquipmentTooltipHeader(const std::string &a_displayName) {
  const auto *theme = sfs::ThemeConfig::GetSingleton();
  const auto headerMin = ImGui::GetCursorScreenPos();
  const auto headerWidth = ImGui::GetContentRegionAvail().x;
  const auto headerHeight = ImGui::GetFontSize() * 2.4f;
  const auto headerMax =
      ImVec2(headerMin.x + headerWidth, headerMin.y + headerHeight);
  auto *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(headerMin, headerMax, theme->GetColorU32("BG"), 8.0f);
  drawList->AddRect(headerMin, headerMax, theme->GetColorU32("BORDER"), 8.0f);
  drawList->AddRectFilledMultiColor(
      headerMin, headerMax, theme->GetColorU32("PRIMARY", 0.18f),
      theme->GetColorU32("PRIMARY", 0.18f), theme->GetColorU32("NONE"),
      theme->GetColorU32("NONE"));

  const auto titleFontSize = ImGui::GetFontSize() * 1.15f;
  const auto titleSize =
      ImGui::CalcTextSize(a_displayName.c_str(), nullptr, false, headerWidth);
  drawList->AddText(
      ImGui::GetFont(), titleFontSize,
      ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
             headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
      theme->GetColorU32("TEXT"), a_displayName.c_str());
  ImGui::Dummy(ImVec2(headerWidth, headerHeight));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("PRIMARY"));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DrawEquipmentInfoTooltipBody(
    const sfs::workbench::EquipmentWidgetItem &a_item,
    const std::string &a_displayName, const std::string &a_editorID,
    const std::string &a_plugin, const std::string &a_formID,
    const std::string &a_identifier,
    const std::vector<std::string> &a_slotLabels,
    const std::vector<std::string> &a_addonSlotLabels) {
  const auto *form = RE::TESForm::LookupByID(a_item.formID);
  if (!form) {
    return;
  }

  DrawEquipmentTooltipHeader(a_displayName);

  if (ImGui::BeginTable("##equipment-info-tooltip", 2,
                        ImGuiTableFlags_NoSavedSettings |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed,
                            138.0f);
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

    auto *localization = sfs::ui::Localization::GetSingleton();
    DrawTooltipInfoRow(kIconEditorId.data(),
                       localization->GetCStr("equipment.editor_id"),
                       a_editorID);
    DrawTooltipInfoRow(kIconPlugin.data(),
                       localization->GetCStr("common.plugin"), a_plugin);
    DrawTooltipInfoRow(kIconFormId.data(),
                       localization->GetCStr("equipment.form_id"), a_formID);
    DrawTooltipInfoRow(kIconIdentifier.data(),
                       localization->GetCStr("equipment.identifier"),
                       a_identifier);
    for (std::size_t index = 0; index < a_slotLabels.size(); ++index) {
      DrawTooltipInfoRow(index == 0 ? kIconSlot.data() : "",
                         index == 0 ? localization->GetCStr("equipment.slots")
                                    : "",
                         a_slotLabels[index]);
    }
    for (std::size_t index = 0; index < a_addonSlotLabels.size(); ++index) {
      DrawTooltipInfoRow(
          index == 0 ? kIconSlot.data() : "",
          index == 0 ? localization->GetCStr("equipment.addon_slots") : "",
          a_addonSlotLabels[index]);
    }

    ImGui::EndTable();
  }

  if (!a_item.IsSlot() && !a_item.hasArmorAddons) {
    ImGui::Spacing();
    ImGui::PushTextWrapPos();
    ImGui::TextColored(sfs::ThemeConfig::GetSingleton()->GetColor("WARN"), "%s",
                       sfs::ui::Localization::GetSingleton()->GetCStr(
                           "equipment.no_addons_warning"));
    ImGui::PopTextWrapPos();
  }
}
} // namespace

namespace sfs::ui::components {
bool BuildEquipmentTooltipItem(const RE::FormID a_formID, const char *a_key,
                               workbench::EquipmentWidgetItem &a_item) {
  if (!workbench::BuildCatalogItem(a_formID, a_item)) {
    return false;
  }

  a_item.key = a_key ? a_key : "";
  return true;
}

void DrawEquipmentInfoTooltip(const std::string_view a_tooltipId,
                              const bool a_hoveredSource,
                              const workbench::EquipmentWidgetItem &a_item,
                              const std::function<void()> &a_drawExtras) {
  if (!ShouldDrawPinnableTooltip(a_tooltipId, a_hoveredSource)) {
    return;
  }

  const bool hasInfoBody = a_item.SupportsInfoTooltip();
  const auto *form =
      hasInfoBody ? RE::TESForm::LookupByID(a_item.formID) : nullptr;
  const auto displayName =
      form ? sfs::armor::GetDisplayName(form) : a_item.name;
  const auto editorID =
      hasInfoBody && form ? sfs::armor::GetEditorID(form) : std::string{};
  const auto plugin =
      hasInfoBody && form ? sfs::armor::GetPluginName(form) : std::string{};
  const auto formID = hasInfoBody
                          ? (form ? sfs::armor::FormatFormID(form->GetFormID())
                                  : sfs::armor::FormatFormID(a_item.formID))
                          : std::string{};
  const auto identifier =
      hasInfoBody && form ? sfs::armor::GetFormIdentifier(form) : std::string{};
  const auto slotLabels = hasInfoBody
                              ? sfs::armor::GetArmorSlotLabels(a_item.slotMask)
                              : std::vector<std::string>{};
  const auto addonSlotLabels =
      hasInfoBody && form && form->As<RE::TESObjectARMO>()
          ? sfs::armor::GetArmorAddonSlotLabels(form->As<RE::TESObjectARMO>())
          : std::vector<std::string>{};
  const auto tooltipContentWidth =
      hasInfoBody ? ComputeEquipmentTooltipWidth(displayName, editorID, plugin,
                                                 formID, identifier, slotLabels,
                                                 addonSlotLabels)
                  : 360.0f;
  ImGui::SetNextWindowSize(
      ImVec2(tooltipContentWidth + ImGui::GetStyle().WindowPadding.x * 2.0f,
             0.0f),
      ImGuiCond_Always);
  DrawPinnableTooltip(a_tooltipId, a_hoveredSource, [&]() {
    if (hasInfoBody) {
      DrawEquipmentInfoTooltipBody(a_item, displayName, editorID, plugin,
                                   formID, identifier, slotLabels,
                                   addonSlotLabels);
    } else {
      DrawEquipmentTooltipHeader(displayName);
    }
    if (a_drawExtras) {
      if (hasInfoBody || !displayName.empty()) {
        ImGui::Spacing();
      }
      a_drawExtras();
    }
  });
}

EquipmentWidgetResult
DrawEquipmentWidget(const char *a_id,
                    const workbench::EquipmentWidgetItem &a_item,
                    const EquipmentWidgetOptions &a_options) {
  ImGui::PushID(a_id);

  constexpr float paddingX = 10.0f;
  constexpr float paddingY = 7.0f;
  const auto lineHeight = ImGui::GetTextLineHeight();
  const auto slotLineCount =
      1 + static_cast<int>(std::ranges::count(a_item.slotText, '\n'));
  const auto contentLineCount = 1 + slotLineCount;
  const auto baseFrameHeight =
      paddingY * 2.0f + lineHeight * contentLineCount + 4.0f;
  const auto frameHeight =
      (std::max)(baseFrameHeight, a_options.minimumHeight);
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto size = ImVec2(width > 0.0f ? width : 1.0f, frameHeight);

  ImGui::InvisibleButton("##equipment-widget", size);

  EquipmentWidgetResult result{};
  result.hovered = ImGui::IsItemHovered();
  if (a_options.interactive) {
    result.active = ImGui::IsItemActive();
    result.clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    result.doubleClicked =
        result.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  }

  const auto rectMin = ImGui::GetItemRectMin();
  const auto rectMax = ImGui::GetItemRectMax();
  auto *drawList = ImGui::GetWindowDrawList();
  const auto *theme = ThemeConfig::GetSingleton();
  const auto hasWarningConflict =
      a_options.conflictStyle == EquipmentWidgetConflictStyle::Warning;
  const auto hasErrorConflict =
      a_options.conflictStyle == EquipmentWidgetConflictStyle::Error;
  constexpr float accentWidth = 5.0f;

  ImU32 fillColor =
      a_options.fillColor.has_value()
          ? ImGui::GetColorU32(*a_options.fillColor)
          : theme->GetColorU32("BG_LIGHT");
  ImU32 borderColor =
      a_options.borderColor.has_value()
          ? ImGui::GetColorU32(*a_options.borderColor)
          : theme->GetColorU32("BORDER");
  if (a_options.disabledAppearance) {
    fillColor = theme->GetColorU32("BG", 0.92f);
    borderColor = theme->GetColorU32("TEXT_DISABLED", 0.55f);
  }
  if (hasWarningConflict) {
    fillColor = theme->GetColorU32("WARN", 0.28f);
    borderColor = theme->GetColorU32("WARN", 0.88f);
  } else if (hasErrorConflict) {
    fillColor = theme->GetColorU32("ERROR", 0.45f);
    borderColor = theme->GetColorU32("ERROR");
  }
  const bool useHoverAppearance = a_options.interactive && result.hovered;
  if (result.active) {
    if (hasWarningConflict) {
      fillColor = theme->GetColorU32("WARN", 0.48f);
      borderColor = theme->GetColorU32("WARN");
    } else if (hasErrorConflict) {
      fillColor = theme->GetColorU32("ERROR", 0.75f);
      borderColor = theme->GetColorU32("ERROR");
    } else {
      fillColor = theme->GetColorU32("PRIMARY", 0.80f);
      borderColor = theme->GetColorU32("PRIMARY");
    }
  } else if (useHoverAppearance) {
    if (hasWarningConflict) {
      fillColor = theme->GetColorU32("WARN", 0.38f);
      borderColor = theme->GetColorU32("WARN");
    } else if (hasErrorConflict) {
      fillColor = theme->GetColorU32("ERROR", 0.62f);
      borderColor = theme->GetColorU32("ERROR");
    } else {
      fillColor = theme->GetColorU32("BG_LIGHT", 1.0f);
      borderColor = theme->GetColorU32("PRIMARY", 0.70f);
    }
  }

  drawList->AddRectFilled(rectMin, rectMax, fillColor, 8.0f);
  drawList->AddRect(rectMin, rectMax, borderColor, 8.0f);
  if (a_options.accentColor.has_value()) {
    drawList->AddRectFilled(rectMin, ImVec2(rectMin.x + accentWidth, rectMax.y),
                            ImGui::GetColorU32(*a_options.accentColor), 8.0f,
                            ImDrawFlags_RoundCornersTopLeft |
                                ImDrawFlags_RoundCornersBottomLeft);
  }

  const auto contentStartX =
      rectMin.x + paddingX +
      (a_options.accentColor.has_value() ? accentWidth : 0.0f);
  const auto contentBlockHeight = lineHeight * contentLineCount + 4.0f;
  const auto contentOffsetY =
      (std::max)(paddingY, (frameHeight - contentBlockHeight) * 0.5f);
  const auto namePos = ImVec2(contentStartX, rectMin.y + contentOffsetY);
  const auto slotPos =
      ImVec2(contentStartX, namePos.y + lineHeight + 4.0f);
  const bool hasSlotIcon =
      a_options.slotIconText != nullptr && a_options.slotIconText[0] != '\0';
  const auto slotIconFontSize =
      ImGui::GetFontSize() * std::clamp(a_options.slotIconScale, 0.5f, 1.0f);
  const auto slotIconSize =
      hasSlotIcon
          ? ImGui::GetFont()->CalcTextSizeA(
                slotIconFontSize, FLT_MAX, 0.0f, a_options.slotIconText)
          : ImVec2{};
  const auto slotTextPos =
      ImVec2(slotPos.x + (hasSlotIcon ? slotIconSize.x + 9.0f : 0.0f),
             slotPos.y);
  constexpr float actionButtonWidth = 34.0f;
  const auto dyePaneWidth =
      a_options.showDyeButton ? actionButtonWidth : 0.0f;
  const auto hidePaneWidth =
      a_options.showHideButton ? actionButtonWidth : 0.0f;
  const auto deletePaneWidth =
      a_options.showDeleteButton ? actionButtonWidth : 0.0f;
  const auto actionPaneWidth = dyePaneWidth + hidePaneWidth + deletePaneWidth;
  const bool hasStatusText =
      a_options.statusText != nullptr && a_options.statusText[0] != '\0';
  const auto statusTextSize =
      hasStatusText ? ImGui::CalcTextSize(a_options.statusText) : ImVec2{};
  const auto statusPaneWidth = hasStatusText ? statusTextSize.x + 12.0f : 0.0f;
  const auto contentMaxX = rectMax.x - paddingX - actionPaneWidth;
  const auto slotContentMaxX = contentMaxX - statusPaneWidth;
  auto actionCursorX = rectMax.x;
  const auto deleteButtonMin =
      ImVec2(actionCursorX - deletePaneWidth, rectMin.y);
  const auto deleteButtonMax = ImVec2(actionCursorX, rectMax.y);
  actionCursorX -= deletePaneWidth;
  const auto hideButtonMin = ImVec2(actionCursorX - hidePaneWidth, rectMin.y);
  const auto hideButtonMax = ImVec2(actionCursorX, rectMax.y);
  actionCursorX -= hidePaneWidth;
  const auto dyeButtonMin = ImVec2(actionCursorX - dyePaneWidth, rectMin.y);
  const auto dyeButtonMax = ImVec2(actionCursorX, rectMax.y);
  bool deleteHeld = false;
  bool hideHeld = false;
  bool dyeHeld = false;
  if (a_options.showDyeButton && a_options.interactive) {
    const auto dyeState = ui::input_widgets::EvaluateRectClickTarget(
        ImGui::GetID("##equipment-widget-dye"), dyeButtonMin, dyeButtonMax);
    result.dyeHovered = dyeState.hovered;
    dyeHeld = a_options.dyeButtonEnabled && dyeState.held;
    result.dyeClicked = a_options.dyeButtonEnabled && dyeState.pressed;
  } else {
    result.dyeHovered = false;
  }
  if (a_options.showHideButton && a_options.interactive) {
    const auto hideState = ui::input_widgets::EvaluateRectClickTarget(
        ImGui::GetID("##equipment-widget-hide"), hideButtonMin, hideButtonMax);
    result.hideHovered = hideState.hovered;
    hideHeld = a_options.hideButtonEnabled && hideState.held;
    result.hideClicked = a_options.hideButtonEnabled && hideState.pressed;
  } else {
    result.hideHovered = false;
  }
  if (a_options.showDeleteButton && a_options.interactive) {
    const auto deleteState = ui::input_widgets::EvaluateRectClickTarget(
        ImGui::GetID("##equipment-widget-delete"), deleteButtonMin,
        deleteButtonMax);
    result.deleteHovered = deleteState.hovered;
    deleteHeld = a_options.deleteButtonEnabled && deleteState.held;
    result.deleteClicked = a_options.deleteButtonEnabled && deleteState.pressed;
  } else {
    result.deleteHovered = false;
  }
  if (result.dyeHovered || result.hideHovered || result.deleteHovered) {
    result.clicked = false;
    result.doubleClicked = false;
    result.active = false;
  }
  const bool dimContentText =
      a_options.disabledAppearance &&
      !a_options.preserveContentTextColors;
  const auto nameColor = dimContentText
                             ? theme->GetColorU32("TEXT_DISABLED")
                             : theme->GetColorU32("TEXT");
  const auto slotColor = dimContentText
                             ? theme->GetColorU32("TEXT_DISABLED", 0.82f)
                             : theme->GetColorU32("TEXT_HEADER", 0.92f);

  drawList->PushClipRect(
      ImVec2(contentStartX, namePos.y),
      ImVec2(contentMaxX, namePos.y + lineHeight), true);
  drawList->AddText(namePos, nameColor, a_item.name.c_str());
  if (a_options.nameTailText != nullptr &&
      a_options.nameTailText[0] != '\0') {
    const auto tailFontSize = ImGui::GetFontSize() *
                              std::clamp(a_options.nameTailScale, 0.5f, 1.0f);
    const auto nameWidth = ImGui::CalcTextSize(a_item.name.c_str()).x;
    const auto tailPosition =
        ImVec2(namePos.x + nameWidth + 6.0f,
               namePos.y + (ImGui::GetFontSize() - tailFontSize));
    drawList->AddText(
        ImGui::GetFont(), tailFontSize, tailPosition,
        a_options.nameTailColor.has_value()
            ? ImGui::GetColorU32(*a_options.nameTailColor)
            : theme->GetColorU32("SECONDARY"),
        a_options.nameTailText);
  }
  drawList->PopClipRect();

  drawList->PushClipRect(
      ImVec2(contentStartX, slotPos.y),
      ImVec2(slotContentMaxX, rectMax.y - paddingY), true);
  if (hasSlotIcon) {
    drawList->AddText(
        ImGui::GetFont(), slotIconFontSize,
        ImVec2(slotPos.x,
               slotPos.y + (ImGui::GetFontSize() - slotIconFontSize)),
        slotColor, a_options.slotIconText);
  }
  drawList->AddText(slotTextPos, slotColor, a_item.slotText.c_str());
  if (a_options.slotAccentText != nullptr &&
      a_options.slotAccentText[0] != '\0') {
    const auto suffixFontSize = ImGui::GetFontSize() * 0.88f;
    const auto accentLineIndex = std::clamp(
        a_options.slotAccentLineIndex >= 0 ? a_options.slotAccentLineIndex : 0,
        0, slotLineCount - 1);
    const std::string_view slotText(a_item.slotText);
    std::size_t accentLineStart = 0;
    for (int lineIndex = 0; lineIndex < accentLineIndex; ++lineIndex) {
      const auto newline = slotText.find('\n', accentLineStart);
      if (newline == std::string_view::npos) {
        break;
      }
      accentLineStart = newline + 1;
    }
    const auto accentLineEnd = slotText.find('\n', accentLineStart);
    const auto accentLine = slotText.substr(
        accentLineStart,
        accentLineEnd == std::string_view::npos
            ? std::string_view::npos
            : accentLineEnd - accentLineStart);
    const auto accentLineWidth =
        ImGui::CalcTextSize(accentLine.data(),
                            accentLine.data() + accentLine.size())
            .x;
    auto suffixX = slotTextPos.x + accentLineWidth + 6.0f;
    const auto suffixY =
        slotPos.y + (lineHeight * accentLineIndex) +
        (ImGui::GetFontSize() - suffixFontSize);
    constexpr const char *separator = "\xC2\xB7";
    const auto separatorWidth = ImGui::CalcTextSize(separator).x;
    drawList->AddText(ImGui::GetFont(), suffixFontSize,
                      ImVec2(suffixX, suffixY),
                      theme->GetColorU32("TEXT_DISABLED"), separator);
    suffixX += separatorWidth + 6.0f;
    const auto accentTextColor =
        a_options.slotAccentColor.has_value()
            ? ImGui::GetColorU32(*a_options.slotAccentColor)
            : theme->GetColorU32("SECONDARY");
    drawList->AddText(ImGui::GetFont(), suffixFontSize,
                      ImVec2(suffixX, suffixY), accentTextColor,
                      a_options.slotAccentText);
    drawList->AddText(ImGui::GetFont(), suffixFontSize,
                      ImVec2(suffixX + 0.45f, suffixY), accentTextColor,
                      a_options.slotAccentText);
    suffixX += ImGui::CalcTextSize(a_options.slotAccentText).x + 6.0f;
    if (a_options.slotTailText != nullptr &&
        a_options.slotTailText[0] != '\0') {
      drawList->AddText(ImGui::GetFont(), suffixFontSize,
                        ImVec2(suffixX, suffixY),
                        theme->GetColorU32("TEXT_DISABLED"),
                        a_options.slotTailText);
    }
  }
  drawList->PopClipRect();

  if (hasStatusText) {
    const auto statusX = contentMaxX - statusTextSize.x;
    drawList->AddText(
        ImVec2(statusX, slotPos.y),
        a_options.statusColor.has_value()
            ? ImGui::GetColorU32(*a_options.statusColor)
            : theme->GetColorU32("SECONDARY"),
        a_options.statusText);
  }

  const auto drawActionButton = [&](const ImVec2 &a_min, const ImVec2 &a_max,
                                    const ImU32 a_fillColor, const char *a_icon,
                                    const bool a_enabled,
                                    const ImDrawFlags a_roundingFlags) {
    drawList->AddRectFilled(a_min, a_max, a_fillColor, 8.0f, a_roundingFlags);
    drawList->AddLine(ImVec2(a_min.x, rectMin.y + 1.0f),
                      ImVec2(a_min.x, rectMax.y - 1.0f),
                      theme->GetColorU32("BORDER"), 1.0f);
    const auto labelSize = ImGui::CalcTextSize(a_icon);
    drawList->AddText(
        ImVec2(a_min.x + (((a_max.x - a_min.x) - labelSize.x) * 0.5f),
               rectMin.y + ((frameHeight - labelSize.y) * 0.5f) - 1.0f),
        a_enabled ? theme->GetColorU32("TEXT")
                  : theme->GetColorU32("TEXT_DISABLED"),
        a_icon);
  };

  if (a_options.showHideButton) {
    const auto hideFill =
        !a_options.hideButtonEnabled
            ? theme->GetColorU32("TEXT_DISABLED", 0.26f)
        : hideHeld ? theme->GetColorU32("SECONDARY")
        : result.hideHovered
            ? theme->GetColorU32("SECONDARY", 0.95f)
            : theme->GetColorU32(a_options.hidden ? "SECONDARY" : "PRIMARY",
                                 0.78f);
    drawActionButton(hideButtonMin, hideButtonMax, hideFill,
                     a_options.hidden ? kIconEyeOff.data() : kIconEye.data(),
                     a_options.hideButtonEnabled,
                     a_options.showDeleteButton
                         ? ImDrawFlags_RoundCornersNone
                         : ImDrawFlags_RoundCornersRight);
  }

  if (a_options.showDyeButton) {
    const auto dyeFill = IM_COL32(0, 0, 0, 255);
    const auto dyeIconColor = !a_options.dyeButtonEnabled
                                  ? theme->GetColorU32("TEXT_DISABLED")
                              : dyeHeld
                                  ? theme->GetColorU32("PRIMARY")
                              : result.dyeHovered
                                  ? theme->GetColorU32("PRIMARY", 0.95f)
                                  : theme->GetColorU32("PRIMARY");
    const auto rounding =
        a_options.showHideButton || a_options.showDeleteButton
            ? ImDrawFlags_RoundCornersLeft
            : ImDrawFlags_RoundCornersAll;
    drawList->AddRectFilled(dyeButtonMin, dyeButtonMax, dyeFill, 8.0f,
                            rounding);
    drawList->AddLine(ImVec2(dyeButtonMax.x, rectMin.y + 1.0f),
                      ImVec2(dyeButtonMax.x, rectMax.y - 1.0f),
                      theme->GetColorU32("BORDER"), 1.0f);
    const auto iconSize = ImGui::CalcTextSize(kIconDye.data());
    drawList->AddText(
        ImVec2(dyeButtonMin.x + ((actionButtonWidth - iconSize.x) * 0.5f),
               rectMin.y + ((frameHeight - iconSize.y) * 0.5f) - 1.0f),
        dyeIconColor, kIconDye.data());
  }

  if (a_options.showDeleteButton) {
    const auto deleteFill = !a_options.deleteButtonEnabled
                                ? theme->GetColorU32("TEXT_DISABLED", 0.26f)
                            : deleteHeld ? theme->GetColorU32("DECLINE")
                            : result.deleteHovered
                                ? theme->GetColorU32("DECLINE", 0.95f)
                                : theme->GetColorU32("DECLINE", 0.78f);
    drawActionButton(deleteButtonMin, deleteButtonMax, deleteFill,
                     kIconTrash.data(), a_options.deleteButtonEnabled,
                     ImDrawFlags_RoundCornersRight);
  }

  if (result.hideHovered && !ImGui::IsDragDropActive()) {
    const auto *localization = Localization::GetSingleton();
    const auto *tooltipKey =
        a_options.hideButtonTooltipKey
            ? a_options.hideButtonTooltipKey
            : (a_options.hidden ? "workbench.show" : "workbench.hide");
    ImGui::SetTooltip("%s", localization->GetCStr(tooltipKey));
  }
  if (result.dyeHovered && !ImGui::IsDragDropActive()) {
    ImGui::SetTooltip(
        "%s", a_options.dyeButtonTooltip != nullptr
                  ? a_options.dyeButtonTooltip
                  : Localization::GetSingleton()->GetCStr(
                        "dye.workbench.tooltip"));
  }
  if (result.deleteHovered && !ImGui::IsDragDropActive()) {
    ImGui::SetTooltip("%s",
                      Localization::GetSingleton()->GetCStr("common.delete"));
  }

  const bool hasContextMenuEntries =
      a_options.interactive && a_options.allowContextMenu &&
      (static_cast<bool>(a_options.drawContextMenuEntries) ||
       a_options.showDyeButton || a_options.showHideButton ||
       a_options.showDeleteButton);
  if (hasContextMenuEntries && result.hovered &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    ImGui::OpenPopup("##equipment-widget-context");
  }

  const auto tooltipId = "equipment:" + a_item.key;
  if ((a_item.SupportsInfoTooltip() || a_item.IsSlot() ||
       a_options.drawTooltipExtras) &&
      !result.dyeHovered && !result.hideHovered && !result.deleteHovered &&
      !ImGui::IsDragDropActive()) {
    DrawEquipmentInfoTooltip(tooltipId, result.hovered, a_item,
                             a_options.drawTooltipExtras);
  }

  if (hasContextMenuEntries &&
      ImGui::BeginPopup("##equipment-widget-context")) {
    bool drewCustomEntries = false;
    if (a_options.drawContextMenuEntries) {
      a_options.drawContextMenuEntries();
      drewCustomEntries = true;
    }

    if (a_options.showDyeButton) {
      if (drewCustomEntries) {
        ImGui::Separator();
      }
      ImGui::BeginDisabled(!a_options.dyeButtonEnabled);
      const auto *dyeText =
          a_options.dyeButtonTooltip != nullptr
              ? a_options.dyeButtonTooltip
              : Localization::GetSingleton()->GetCStr(
                    "dye.workbench.tooltip");
      const std::string dyeLabel = std::string(kIconDye) + " " + dyeText;
      if (ImGui::MenuItem(dyeLabel.c_str())) {
        result.dyeClicked = true;
      }
      ImGui::EndDisabled();
      drewCustomEntries = true;
    }

    if (a_options.showHideButton) {
      if (drewCustomEntries) {
        ImGui::Separator();
      }
      ImGui::BeginDisabled(!a_options.hideButtonEnabled);
      const std::string hideLabel =
          std::string(a_options.hidden ? kIconEyeOff : kIconEye) + " " +
          std::string(Localization::GetSingleton()->Get(
              a_options.hidden ? "workbench.show" : "workbench.hide"));
      if (ImGui::MenuItem(hideLabel.c_str())) {
        result.hideClicked = true;
      }
      ImGui::EndDisabled();
      drewCustomEntries = true;
    }

    if (a_options.showDeleteButton) {
      if (drewCustomEntries) {
        ImGui::Separator();
      }
      ImGui::PushStyleColor(ImGuiCol_Text, theme->GetColor("DECLINE"));
      ImGui::BeginDisabled(!a_options.deleteButtonEnabled);
      const std::string deleteLabel = std::string(kIconTrash) + " Delete";
      if (ImGui::MenuItem(deleteLabel.c_str())) {
        result.deleteClicked = true;
      }
      ImGui::EndDisabled();
      ImGui::PopStyleColor();
    }

    ImGui::EndPopup();
  }

  ImGui::PopID();
  return result;
}
} // namespace sfs::ui::components
