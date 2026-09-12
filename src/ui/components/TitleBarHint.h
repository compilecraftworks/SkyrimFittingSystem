#pragma once
#include <string_view>
namespace sfs::ui::components {
// Decoration only: no interactive item, cursor, navigation or close hitbox.
// Call after Begin for a left-aligned, non-collapsible window.
void DrawTitleBarHint(std::string_view title, std::string_view hint,
                      std::string_view compactHint);
}
