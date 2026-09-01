#pragma once

#include "imgui.h"
#include "workbench/Items.h"

#include <functional>
#include <optional>

namespace sfs::ui::components {
enum class EquipmentWidgetConflictStyle { None, Warning, Error };

struct EquipmentWidgetOptions {
  bool showDeleteButton{false};
  bool deleteButtonEnabled{true};
  bool showHideButton{false};
  bool hideButtonEnabled{true};
  const char *hideButtonTooltipKey{nullptr};
  // Independent renderer-only action. It never changes visibility,
  // registration, equipment, conditions, or external strip state.
  bool showDyeButton{false};
  bool dyeButtonEnabled{true};
  const char *dyeButtonTooltip{nullptr};
  bool hidden{false};
  bool disabledAppearance{false};
  bool preserveContentTextColors{false};
  bool interactive{true};
  bool allowContextMenu{true};
  float minimumHeight{0.0f};
  const char *statusText{nullptr};
  std::optional<ImVec4> statusColor;
  // Compact marker drawn immediately after the item name (for example, the
  // registered-appearance lock state).
  const char *nameTailText{nullptr};
  float nameTailScale{0.72f};
  std::optional<ImVec4> nameTailColor;
  const char *slotIconText{nullptr};
  float slotIconScale{1.0f};
  const char *slotAccentText{nullptr};
  int slotAccentLineIndex{-1};
  const char *slotTailText{nullptr};
  std::optional<ImVec4> slotAccentColor;
  std::optional<ImVec4> accentColor;
  std::optional<ImVec4> fillColor;
  std::optional<ImVec4> borderColor;
  EquipmentWidgetConflictStyle conflictStyle{
      EquipmentWidgetConflictStyle::None};
  std::function<void()> drawTooltipExtras{};
  std::function<void()> drawContextMenuEntries{};
};

struct EquipmentWidgetResult {
  bool hovered{false};
  bool active{false};
  bool clicked{false};
  bool doubleClicked{false};
  bool hideHovered{false};
  bool hideClicked{false};
  bool dyeHovered{false};
  bool dyeClicked{false};
  bool deleteHovered{false};
  bool deleteClicked{false};
};

[[nodiscard]] bool
BuildEquipmentTooltipItem(RE::FormID a_formID, const char *a_key,
                          workbench::EquipmentWidgetItem &a_item);
void DrawEquipmentInfoTooltip(std::string_view a_tooltipId,
                              bool a_hoveredSource,
                              const workbench::EquipmentWidgetItem &a_item,
                              const std::function<void()> &a_drawExtras = {});
[[nodiscard]] EquipmentWidgetResult
DrawEquipmentWidget(const char *a_id,
                    const workbench::EquipmentWidgetItem &a_item,
                    const EquipmentWidgetOptions &a_options = {});
} // namespace sfs::ui::components
