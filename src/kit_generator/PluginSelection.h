#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

namespace sfs::kit_generator {
// Visible, sorted indices only: a filtered-out plugin must never enter a range.
// The anchor is a source identity, not a row number that sorting can repurpose.
template <class SetSelected>
void SelectPluginRows(std::span<const std::size_t> a_visible,
                      std::size_t a_source, bool a_selected, bool a_range,
                      std::optional<std::size_t>& a_anchor,
                      SetSelected&& a_setSelected) {
  const auto clicked = std::ranges::find(a_visible, a_source);
  if (clicked == a_visible.end()) return;
  const auto anchor = a_anchor ? std::ranges::find(a_visible, *a_anchor)
                               : a_visible.end();
  if (a_range && anchor != a_visible.end()) {
    const auto first = (std::min)(anchor, clicked);
    const auto last = (std::max)(anchor, clicked);
    for (auto row = first; row != last + 1; ++row) a_setSelected(*row, a_selected);
  } else {
    a_setSelected(a_source, a_selected);
    a_anchor = a_source;
  }
}

template <class Records>
bool HasWornArmorModel(const Records& a_records) {
  return std::ranges::any_of(a_records, [](const auto& record) {
    return std::ranges::any_of(record.armorModelPaths,
                               [](const auto& path) { return !path.empty(); });
  });
}
} // namespace sfs::kit_generator
