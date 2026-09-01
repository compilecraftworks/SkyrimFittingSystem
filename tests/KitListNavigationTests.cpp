#include "input/KitListNavigation.h"

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
} // namespace

int main() {
  try {
    TestWasdMatchesExistingNavigation();
    TestExistingActionsRemainUnchanged();
    std::cout << "KitListNavigationTests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "KitListNavigationTests failed: " << error.what() << '\n';
    return 1;
  }
}
