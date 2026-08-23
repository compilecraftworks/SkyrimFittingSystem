#pragma once

#include <cstdint>
#include <string>

namespace sfs::keycode {
inline constexpr std::uint32_t kTabScanCode = 0x0F;

[[nodiscard]] bool IsKeyModifier(std::uint32_t a_key);
[[nodiscard]] bool IsGamepadKey(std::uint32_t a_key);
[[nodiscard]] std::uint32_t NormalizeGamepadKeyCode(std::uint32_t a_key);
[[nodiscard]] bool IsValidHotkey(std::uint32_t a_key);
[[nodiscard]] std::string GetKeyName(std::uint32_t a_scanCode);
} // namespace sfs::keycode
