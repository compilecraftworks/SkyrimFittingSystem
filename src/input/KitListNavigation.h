#pragma once

#include <cstdint>

namespace sfs::input::kit_list {
enum class KeyboardCommand : std::uint8_t {
  None,
  MoveUp,
  MoveDown,
  Back,
  NextPane,
  Apply,
  Preview,
};

[[nodiscard]] constexpr KeyboardCommand
FromScanCode(const std::uint32_t a_scanCode) {
  switch (a_scanCode) {
  case 0x11: // W
  case 0xC8: // Arrow Up
    return KeyboardCommand::MoveUp;
  case 0x1F: // S
  case 0xD0: // Arrow Down
    return KeyboardCommand::MoveDown;
  case 0x1E: // A
  case 0xCB: // Arrow Left
    return KeyboardCommand::Back;
  case 0x20: // D
  case 0xCD: // Arrow Right
    return KeyboardCommand::NextPane;
  case 0x1C: // Enter
  case 0x9C: // Numpad Enter
    return KeyboardCommand::Apply;
  case 0x39: // Space
    return KeyboardCommand::Preview;
  default:
    return KeyboardCommand::None;
  }
}

[[nodiscard]] constexpr int MoveDelta(const KeyboardCommand a_command) {
  if (a_command == KeyboardCommand::MoveUp) {
    return -1;
  }
  if (a_command == KeyboardCommand::MoveDown) {
    return 1;
  }
  return 0;
}
} // namespace sfs::input::kit_list
