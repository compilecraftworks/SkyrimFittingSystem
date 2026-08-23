#include "ui/InputSinkBridge.h"

#include "ui/Menu.h"

namespace sfs::ui {
InputSinkState GetInputSinkState() {
  const auto *menu = Menu::GetSingleton();
  return InputSinkState{.enabled = menu->IsEnabled(),
                        .wantsTextInput = menu->WantsTextInput(),
                        .capturingToggleKey = menu->IsCapturingToggleKey(),
                        .toggleKey = menu->GetToggleKey(),
                        .toggleModifier = menu->GetToggleModifier()};
}

void HandleToggleKeyCapture(const std::uint32_t a_scanCode,
                            const std::uint32_t a_modifierScanCode) {
  Menu::GetSingleton()->HandleToggleKeyCapture(a_scanCode, a_modifierScanCode);
}

void ToggleInputSinkVisibility() { Menu::GetSingleton()->Toggle(); }

bool QueueKitListMove(const int a_delta) {
  return Menu::GetSingleton()->QueueKitListMove(a_delta);
}

bool QueueKitListApply() { return Menu::GetSingleton()->QueueKitListApply(); }

bool QueueKitListPreview() {
  return Menu::GetSingleton()->QueueKitListPreview();
}

bool QueueKitListBack() { return Menu::GetSingleton()->QueueKitListBack(); }

bool QueueKitListNextPane() {
  return Menu::GetSingleton()->QueueKitListNextPane();
}
} // namespace sfs::ui
