#include "Keycode.h"
#include "ui/Localization.h"
#include <SKSE/InputMap.h>

#include <Windows.h>

#include <array>
#include <format>

namespace sfs::keycode {
bool IsKeyModifier(const std::uint32_t a_key) {
  return a_key == 0x2A || a_key == 0x36 || a_key == 0x1D || a_key == 0x9D ||
         a_key == 0x38 || a_key == 0xB8;
}

bool IsGamepadKey(const std::uint32_t a_key) {
  return a_key >= SKSE::InputMap::kMacro_GamepadOffset &&
         a_key < SKSE::InputMap::kMaxMacros;
}

std::uint32_t NormalizeGamepadKeyCode(const std::uint32_t a_key) {
  if (IsGamepadKey(a_key)) {
    return a_key;
  }
  return SKSE::InputMap::GamepadMaskToKeycode(a_key);
}

bool IsValidHotkey(const std::uint32_t a_key) {
  if (IsGamepadKey(a_key)) {
    return true;
  }

  return a_key != 0x01 && a_key != kTabScanCode && a_key != 0x00 &&
         a_key != 0x1C &&
         a_key != 0x39 && a_key != 0x14;
}

std::string GetKeyName(const std::uint32_t a_scanCode) {
  if (a_scanCode == 0) {
    return std::string(ui::Localization::GetSingleton()->Get("common.none"));
  }

  if (IsGamepadKey(a_scanCode)) {
    const auto name = SKSE::InputMap::GetGamepadButtonName(a_scanCode);
    if (!name.empty()) {
      return name;
    }
  }

  LONG lParam = (static_cast<LONG>(a_scanCode & 0xFFU) << 16U);
  if ((a_scanCode & 0x100U) != 0U) {
    lParam |= (1L << 24);
  }

  std::array<char, 128> keyName{};
  const auto length =
      GetKeyNameTextA(lParam, keyName.data(), static_cast<int>(keyName.size()));
  if (length > 0) {
    return {keyName.data(), static_cast<std::size_t>(length)};
  }

  return sfs::strings::SafeVFormat(
      std::string(ui::Localization::GetSingleton()->Get("common.scan_code")),
      std::make_format_args(a_scanCode));
}
} // namespace sfs::keycode
