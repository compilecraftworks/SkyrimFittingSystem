#include "input/KitListNavigation.h"
#include "ui/catalog/FilterFocus.h"
#include "kit_generator/ResultSelection.h"
#include "ui/catalog/FavoriteFilters.h"
#include <nlohmann/json.hpp>

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using sfs::input::kit_list::FromScanCode;
using sfs::input::kit_list::KeyboardCommand;
using sfs::input::kit_list::MoveDelta;

void Require(const bool a_condition, const std::string_view a_message) {
  if (!a_condition) {
    throw std::runtime_error(std::string(a_message));
  }
}

void TestWasdMatchesExistingNavigation() {
  Require(FromScanCode(0x11) == FromScanCode(0xC8) &&
              MoveDelta(FromScanCode(0x11)) == -1,
          "W must match Arrow Up");
  Require(FromScanCode(0x1F) == FromScanCode(0xD0) &&
              MoveDelta(FromScanCode(0x1F)) == 1,
          "S must match Arrow Down");
  Require(FromScanCode(0x1E) == FromScanCode(0xCB) &&
              FromScanCode(0x1E) == KeyboardCommand::Back,
          "A must match Arrow Left");
  Require(FromScanCode(0x20) == FromScanCode(0xCD) &&
              FromScanCode(0x20) == KeyboardCommand::NextPane,
          "D must match Arrow Right");
}

void TestExistingActionsRemainUnchanged() {
  Require(FromScanCode(0x1C) == KeyboardCommand::Apply &&
              FromScanCode(0x9C) == KeyboardCommand::Apply,
          "Enter mappings must remain Apply");
  Require(FromScanCode(0x39) == KeyboardCommand::Preview,
          "Space must remain Preview");
  Require(FromScanCode(0x12) == KeyboardCommand::None,
          "Unrelated keys must remain unhandled");
}

void TestListOnlyFilterFocus() {
  struct Row { std::string id; };
  const Row a{"first-sorted-row"}, b{"second"};
  const std::vector<const Row *> rows{&a, &b};
  for (int cycle = 0; cycle < 128; ++cycle) {
   for (const bool gear : {false, true}) {
    sfs::ui::catalog::FilterFocus focus;
    std::string selected = b.id;
    std::vector<std::string> selectedGear{b.id, "old-hidden"};
    focus.Request();
    const auto first = focus.Consume(rows, selected, selectedGear, gear);
    Require(first.consumed && first.row == 0 && selected == a.id,
            "Filter changes must focus the first SORTED row even if the old row survives");
    Require(focus.IsFocusOnly(a.id) && !focus.IsFocusOnly(b.id),
            "Focus-only selection must not toggle off a not-yet-previewed first row");
    Require(gear ? selectedGear == std::vector<std::string>{a.id} : selectedGear.empty(),
            "Hidden multi-selection must not remain in the new list focus");
    selected = b.id;
    Require(!focus.Consume(rows, selected, selectedGear, gear).consumed && selected == b.id,
            "First-row focus must run once, not undo subsequent keyboard/gamepad movement");
    focus.AcceptSelection();
    Require(!focus.IsFocusOnly(a.id), "Explicit interaction ends focus-only status");
    focus.Request();
    const auto empty = focus.Consume(std::vector<const Row *>{}, selected, selectedGear, gear);
    Require(empty.consumed && empty.row == -1 && selected.empty() && selectedGear.empty(),
            "Empty results must have no stale keyboard/gamepad action target");
    focus.Request();
    focus.Reset();
    Require(!focus.Consume(rows, selected, selectedGear, gear).consumed,
            "Closing/changing the catalog context must cancel pending focus");
   }
  }
}

void TestIndependentFavoritesSettings() {
  using sfs::ui::catalog::FavoriteFilters;
  for (int cycle = 0; cycle < 128; ++cycle) {
    for (unsigned bits = 0; bits < 8; ++bits) {
      FavoriteFilters filters{bool(bits & 1), bool(bits & 2), bool(bits & 4)};
      nlohmann::json settings{{"unrelated", 42}, {"catalogFavoritesOnly", true}};
      filters.Save(settings);
      FavoriteFilters restored;
      restored.Load(settings);
      Require(restored.gear == filters.gear && restored.outfits == filters.outfits &&
                  restored.kits == filters.kits && settings["unrelated"] == 42 &&
                  !settings.contains("catalogFavoritesOnly"),
              "All independent favorite combinations must survive saving/reloading without stale shared state");
      restored.gear = !restored.gear;
      Require(restored.kits == filters.kits && restored.outfits == filters.outfits,
              "Toggling Gear cannot alter Kits or Outfits");
    }
  }
  FavoriteFilters filters;
  filters.Load(nlohmann::json::object());
  Require(!filters.gear && !filters.outfits && !filters.kits, "First-run favorites are off for each tab");
  filters.Load(nlohmann::json{{"catalogFavoritesOnly", true}});
  Require(filters.gear && filters.outfits && filters.kits, "Legacy shared preference migrates without changing existing lists");
  filters.Load(nlohmann::json{{"catalogFavoritesOnly", true}, {"catalogGearFavoritesOnly", false},
                             {"catalogKitFavoritesOnly", false}});
  Require(!filters.gear && filters.outfits && !filters.kits, "Explicit false must override a true legacy preference");
  filters.Load(nlohmann::json{{"catalogFavoritesOnly", "bad"}, {"catalogKitFavoritesOnly", true},
                             {"catalogGearFavoritesOnly", 1}});
  Require(!filters.gear && !filters.outfits && filters.kits, "Malformed fields cannot destroy valid independent choices");
}
} // namespace

int main() {
  try {
    for (int cycle = 0; cycle < 128; ++cycle) {
      sfs::kit_generator::ResultSelection state;
      state.Reset(6);
      const std::vector<std::size_t> rows{4, 1, 5, 2, 0, 3};
      state.SelectRow(rows, 1, false, false);
      state.SelectRow(rows, 0, false, true);
      state.CheckRow(rows, 5, true);
      Require(state.CheckedIndices() == std::vector<std::size_t>({0, 1, 2, 5}),
              "Shift uses sorted visible order; one checkbox checks highlighted rows");
      state.CheckRow(rows, 2, false);
      Require(state.CheckedIndices().empty(), "Multi-uncheck uses the clicked desired state");
      state.SelectRow(rows, 4, true, false);
      state.SelectRow(rows, 2, true, false);
      Require(state.highlighted[4] && !state.highlighted[2], "Ctrl toggles one highlight");
      state.CheckRow(rows, 2, true);
      Require(state.CheckedIndices() == std::vector<std::size_t>{2},
              "Checking an unhighlighted row must not touch the highlighted group");
      state.CheckRow(std::vector<std::size_t>{4, 1}, 1, true);
      Require(state.CheckedIndices() == std::vector<std::size_t>({1, 2, 4}),
              "Batch checkbox cannot include hidden highlights");
      state.ClearHighlights();
      state.SelectRow(std::vector<std::size_t>{3, 0}, 0, false, true);
      Require(state.highlighted[0] && !state.highlighted[3], "Filtered-out/missing range anchor selects only clicked row");
      state.SelectRow(rows, 5, false, false);
      Require(state.CheckedIndices() == std::vector<std::size_t>({1, 2, 4}),
              "Keyboard traversal/highlight and preview cannot alter explicit checks");
      state.CheckVisible(std::vector<std::size_t>{3, 0});
      Require(state.CheckedIndices() == std::vector<std::size_t>({0, 1, 2, 3, 4}),
              "Select all checks visible only and preserves prior hidden checks");
      state.ClearChecks();
      Require(state.CheckedIndices().empty(), "Clear all also clears hidden checks");
      state.CheckVisible(rows);
      state.Reset(6);
      Require(state.CheckedIndices().empty() && !state.anchor &&
              std::ranges::none_of(state.highlighted, [](bool v) { return v; }),
              "Same-size new scan resets checks, highlights and anchor");
      state.CheckRow(rows, 100, true);
      state.SelectRow(rows, 100, true, true);
      Require(state.CheckedIndices().empty(), "Stale result indices cannot check another kit");
    }
    TestWasdMatchesExistingNavigation();
    TestExistingActionsRemainUnchanged();
    TestListOnlyFilterFocus();
    TestIndependentFavoritesSettings();
    std::cout << "KitListNavigationTests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "KitListNavigationTests failed: " << error.what() << '\n';
    return 1;
  }
}
