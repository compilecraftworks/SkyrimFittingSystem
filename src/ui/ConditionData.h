#pragma once

#include "conditions/Definition.h"
#include "imgui.h"

namespace sfs::ui::conditions {
using Comparator = sfs::conditions::Comparator;
using Connective = sfs::conditions::Connective;
using Clause = sfs::conditions::Clause;
using Color = sfs::conditions::Color;
using Definition = sfs::conditions::Definition;

inline constexpr std::string_view kDefaultConditionId =
    sfs::conditions::kDefaultConditionId;

[[nodiscard]] inline ImVec4 ToImGuiColor(const Color &a_color) {
  return ImVec4(a_color.x, a_color.y, a_color.z, a_color.w);
}

[[nodiscard]] inline Color FromImGuiColor(const ImVec4 &a_color) {
  return Color{a_color.x, a_color.y, a_color.z, a_color.w};
}
} // namespace sfs::ui::conditions
