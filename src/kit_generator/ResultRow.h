#pragma once

#include "imgui.h"
#include "imgui_internal.h"

namespace sfs::kit_generator {
struct ResultRowAction {
  bool checkChanged{false};
  bool clicked{false};
  bool openDetail{false};
};

// Separate hit targets: no spanning Selectable behind the checkbox. ImGui
// owns press/release capture and scrolling/popup clipping for all four cells.
inline ResultRowAction DrawResultRow(bool& checked, bool highlighted,
                                     bool focused, const char* name,
                                     const char* plugin, const char* count) {
  ResultRowAction action;
  ImGui::TableNextRow();
  if (highlighted || focused) {
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
        focused ? IM_COL32(72, 115, 176, 86) : IM_COL32(72, 115, 176, 42));
  }
  ImGui::TableSetColumnIndex(0);
  action.checkChanged = ImGui::Checkbox("##checked", &checked);
  ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
  const char* labels[]{name, plugin, count};
  for (int column = 1; column <= 3; ++column) {
    ImGui::TableSetColumnIndex(column);
    ImGui::PushID(column);
    const auto position = ImGui::GetCursorScreenPos();
    const auto width = ImGui::GetContentRegionAvail().x;
    const bool pressed = ImGui::Selectable("##result-row", highlighted,
        ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, ImGui::GetFrameHeight()));
    const bool hovered = ImGui::IsItemHovered();
    action.openDetail |= pressed && hovered &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    action.clicked |= pressed;
    ImGui::RenderTextClipped(position,
        ImVec2(position.x + width, position.y + ImGui::GetFrameHeight()),
        labels[column - 1], nullptr, nullptr,
        ImVec2(column == 3 ? 1.0F : 0.0F, 0.5F));
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::PopID();
  }
  ImGui::PopStyleColor(3);
  return action;
}
} // namespace sfs::kit_generator
