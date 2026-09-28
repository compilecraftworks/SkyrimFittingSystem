#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace sfs::kit_generator {
// Source indices remain stable while browsing/sorting; reset after any list
// mutation or new scan, including a new scan with the same number of results.
struct ResultSelection {
  std::vector<bool> checked;
  std::vector<bool> highlighted;
  std::optional<std::size_t> anchor;

  void Reset(std::size_t count) {
    checked.assign(count, false);
    highlighted.assign(count, false);
    anchor.reset();
  }
  void ClearHighlights() {
    std::fill(highlighted.begin(), highlighted.end(), false);
    anchor.reset();
  }
  void SelectRow(std::span<const std::size_t> visible, std::size_t source,
                 bool ctrl, bool shift) {
    const auto clicked = std::ranges::find(visible, source);
    if (source >= highlighted.size() || clicked == visible.end()) return;
    const auto start = anchor ? std::ranges::find(visible, *anchor) : visible.end();
    if (shift && start != visible.end()) {
      if (!ctrl) std::fill(highlighted.begin(), highlighted.end(), false);
      for (auto row = (std::min)(start, clicked);
           row != (std::max)(start, clicked) + 1; ++row) {
        if (*row < highlighted.size()) highlighted[*row] = true;
      }
    } else {
      const bool selected = !ctrl || !highlighted[source];
      if (!ctrl) std::fill(highlighted.begin(), highlighted.end(), false);
      highlighted[source] = selected;
      anchor = source;
    }
  }
  void CheckRow(std::span<const std::size_t> visible, std::size_t source,
                bool value) {
    if (source >= checked.size() ||
        std::ranges::find(visible, source) == visible.end()) return;
    if (highlighted[source]) {
      for (const auto row : visible) {
        if (row < checked.size() && highlighted[row]) checked[row] = value;
      }
    } else {
      checked[source] = value;
    }
  }
  void CheckVisible(std::span<const std::size_t> visible) {
    for (const auto row : visible) if (row < checked.size()) checked[row] = true;
  }
  void ClearChecks() { std::fill(checked.begin(), checked.end(), false); }
  [[nodiscard]] std::vector<std::size_t> CheckedIndices() const {
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < checked.size(); ++i) if (checked[i]) result.push_back(i);
    return result;
  }
};
} // namespace sfs::kit_generator
