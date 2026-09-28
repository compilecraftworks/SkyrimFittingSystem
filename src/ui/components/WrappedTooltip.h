#pragma once

#include "imgui.h"
#include <algorithm>

namespace sfs::ui::components {
inline void DrawWrappedTooltip(const char* a_text) {
  // Scale with the UI font, but keep even a narrow viewport within bounds.
  const auto& style = ImGui::GetStyle();
  const auto availableWidth = (std::max)(
      1.0f, ImGui::GetMainViewport()->WorkSize.x -
                2.0f * (style.WindowPadding.x + style.DisplaySafeAreaPadding.x));
  const auto wrapWidth = (std::min)(ImGui::GetFontSize() * 24.0f, availableWidth);
  if (ImGui::BeginTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
    ImGui::TextUnformatted(a_text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}
} // namespace sfs::ui::components
