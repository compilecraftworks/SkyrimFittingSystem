#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace sfs::ui::catalog {
struct FilterFocusResult {
  bool consumed{false};
  int row{-1};
};

// List navigation only. No actor, workbench or preview state is accessible.
struct FilterFocus {
  bool pending{false};
  std::string focusOnlyKey;

  void Request() { pending = true; focusOnlyKey.clear(); }
  void Reset() { pending = false; focusOnlyKey.clear(); }
  void AcceptSelection() { focusOnlyKey.clear(); }
  [[nodiscard]] bool IsFocusOnly(std::string_view key) const {
    return !focusOnlyKey.empty() && focusOnlyKey == key;
  }

  template <class Rows>
  FilterFocusResult Consume(const Rows &rows, std::string &selection,
                            std::vector<std::string> &gearSelection,
                            bool gearTab) {
    if (!pending) return {};
    pending = false;
    selection.clear();
    gearSelection.clear();
    focusOnlyKey.clear();
    if (rows.empty()) return {true, -1};
    selection = rows.front()->id;
    if (gearTab) gearSelection.push_back(selection);
    focusOnlyKey = selection;
    return {true, 0};
  }
};
} // namespace sfs::ui::catalog
