#include "ui/components/TitleBarHint.h"
#include <imgui_internal.h>
#include <algorithm>

namespace sfs::ui::components {
void DrawTitleBarHint(std::string_view title, std::string_view hint,
                      std::string_view compactHint) {
  auto* window = ImGui::GetCurrentWindow();
  const auto& style = ImGui::GetStyle();
  const auto bar = window->TitleBarRect();
  const auto titleSize = ImGui::CalcTextSize(title.data(), title.data() + title.size(), true);
  const auto left = bar.Min.x + window->WindowBorderSize + style.FramePadding.x +
                    titleSize.x + style.ItemSpacing.x;
  const auto right = bar.Max.x - window->WindowBorderSize - style.FramePadding.x -
                     ImGui::GetFontSize() - style.ItemInnerSpacing.x;
  if (right <= left) { return; }
  auto size = ImGui::CalcTextSize(hint.data(), hint.data() + hint.size());
  if (size.x > right - left) {
    hint = compactHint;
    size = ImGui::CalcTextSize(hint.data(), hint.data() + hint.size());
  }
  ImRect clip({left, bar.Min.y}, {right, bar.Max.y});
  clip.ClipWith(window->OuterRectClipped);
  if (clip.IsInverted()) { return; }
  window->DrawList->PushClipRect(clip.Min, clip.Max, false);
  window->DrawList->AddText({(std::max)(left, right - size.x),
                            bar.Min.y + (bar.GetHeight() - size.y) * 0.5f},
      ImGui::GetColorU32(ImGuiCol_TextDisabled), hint.data(), hint.data() + hint.size());
  window->DrawList->PopClipRect();
}
} // namespace sfs::ui::components
