#pragma once

#include <cstdint>

namespace sfs::ui {
struct InputSinkState {
  bool enabled{false};
  bool wantsTextInput{false};
  bool capturingToggleKey{false};
  std::uint32_t toggleKey{0};
  std::uint32_t toggleModifier{0};
};

[[nodiscard]] InputSinkState GetInputSinkState();
void HandleToggleKeyCapture(std::uint32_t a_scanCode,
                            std::uint32_t a_modifierScanCode);
void ToggleInputSinkVisibility();
void CancelInputSink();
[[nodiscard]] bool QueueKitListMove(int a_delta);
[[nodiscard]] bool QueueKitListApply();
[[nodiscard]] bool QueueKitListPreview();
[[nodiscard]] bool QueueKitListBack();
[[nodiscard]] bool QueueKitListNextPane();
} // namespace sfs::ui
