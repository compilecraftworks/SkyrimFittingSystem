#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace sfs {
class InputManager {
public:
  static InputManager *GetSingleton();

  void OnFocusChange(bool a_focus);
  void AddEventToQueue(RE::InputEvent **a_events);
  void Flush();
  void ProcessInputEvents();
  void UpdateMousePosition() const;
  void SetShortcutSuppressionActive(bool a_active);
  [[nodiscard]] bool IsShortcutSuppressionActive() const;
  [[nodiscard]] bool IsBoundModifierDown() const;
  [[nodiscard]] std::uint32_t GetActiveModifierScanCode() const;

private:
  std::mutex inputLock_;
  std::vector<RE::InputEvent *> inputQueue_;
  std::atomic_bool shortcutSuppressionActive_{false};
  bool toggleKeyDown_{false};
  std::uint8_t modifierSidesDown_{0};
  std::unordered_set<std::uint32_t> gamepadButtonsDown_;
  std::uint32_t pendingGamepadCaptureKey_{0};
  int keyboardKitListMoveHeld_{0};
  int gamepadKitListMoveHeld_{0};
  std::chrono::steady_clock::time_point nextKitListMoveRepeatAt_{};
};
} // namespace sfs
