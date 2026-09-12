#pragma once
#ifndef SFS_MENU_CANCEL_TEST
#include <RE/Skyrim.h>
#include <SKSE/InputMap.h>
#endif

namespace sfs::input {
// Respect the loaded game's MenuMode control map, including remapped keyboard
// and gamepad controls. Never hardcode Escape/B or inspect unrelated contexts.
inline bool IsMenuCancel(const RE::ButtonEvent& button) {
  const auto device = button.GetDevice();
  if (device != RE::INPUT_DEVICE::kKeyboard && device != RE::INPUT_DEVICE::kGamepad) { return false; }
  if (button.QUserEvent() == "Cancel") { return true; }
  const auto* controls = RE::ControlMap::GetSingleton();
  if (!controls) { return false; }
  auto mapped = controls->GetMappedKey("Cancel", device, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
  if (mapped == RE::ControlMap::kInvalid || mapped == 0xFF) { return false; }
  auto key = button.GetIDCode();
  if (device == RE::INPUT_DEVICE::kGamepad) {
    // Some providers use SKSE macro keycodes, others use the native bit mask.
    const auto normalize = [](std::uint32_t value) {
      return value >= SKSE::InputMap::kMacro_GamepadOffset && value < SKSE::InputMap::kMaxMacros
          ? value : SKSE::InputMap::GamepadMaskToKeycode(value);
    };
    key = normalize(key); mapped = normalize(mapped);
    if (mapped < SKSE::InputMap::kMacro_GamepadOffset || mapped >= SKSE::InputMap::kMaxMacros ||
        key < SKSE::InputMap::kMacro_GamepadOffset || key >= SKSE::InputMap::kMaxMacros) { return false; }
  }
  return key == mapped;
}
} // namespace sfs::input
