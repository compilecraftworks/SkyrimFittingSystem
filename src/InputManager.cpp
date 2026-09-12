#include "InputManager.h"

#include "Keycode.h"
#include "api/SkyrimFittingSystemAPI.h"
#include "imgui.h"
#include "input/KitListNavigation.h"
#include "input/MenuCancel.h"
#include "ui/InputSinkBridge.h"

#include <SKSE/InputMap.h>

#include <chrono>
#include <memory>

namespace sfs {
namespace {
enum ModifierSideBit : std::uint8_t {
  kLeftShiftBit = 1u << 0u,
  kRightShiftBit = 1u << 1u,
  kLeftCtrlBit = 1u << 2u,
  kRightCtrlBit = 1u << 3u,
  kLeftAltBit = 1u << 4u,
  kRightAltBit = 1u << 5u,
};

auto GetModifierBit(const std::uint32_t a_scanCode) -> std::uint8_t {
  switch (a_scanCode) {
  case 0x2A:
    return kLeftShiftBit;
  case 0x36:
    return kRightShiftBit;
  case 0x1D:
    return kLeftCtrlBit;
  case 0x9D:
    return kRightCtrlBit;
  case 0x38:
    return kLeftAltBit;
  case 0xB8:
    return kRightAltBit;
  default:
    return 0;
  }
}

[[nodiscard]] ImGuiKey ScanCodeToImGuiKey(const std::uint32_t a_scanCode) {
  switch (a_scanCode) {
  case 0x01: return ImGuiKey_Escape;
  case 0x02: return ImGuiKey_1;
  case 0x03: return ImGuiKey_2;
  case 0x04: return ImGuiKey_3;
  case 0x05: return ImGuiKey_4;
  case 0x06: return ImGuiKey_5;
  case 0x07: return ImGuiKey_6;
  case 0x08: return ImGuiKey_7;
  case 0x09: return ImGuiKey_8;
  case 0x0A: return ImGuiKey_9;
  case 0x0B: return ImGuiKey_0;
  case 0x0C: return ImGuiKey_Minus;
  case 0x0D: return ImGuiKey_Equal;
  case 0x0E: return ImGuiKey_Backspace;
  case 0x0F: return ImGuiKey_Tab;
  case 0x10: return ImGuiKey_Q;
  case 0x11: return ImGuiKey_W;
  case 0x12: return ImGuiKey_E;
  case 0x13: return ImGuiKey_R;
  case 0x14: return ImGuiKey_T;
  case 0x15: return ImGuiKey_Y;
  case 0x16: return ImGuiKey_U;
  case 0x17: return ImGuiKey_I;
  case 0x18: return ImGuiKey_O;
  case 0x19: return ImGuiKey_P;
  case 0x1A: return ImGuiKey_LeftBracket;
  case 0x1B: return ImGuiKey_RightBracket;
  case 0x1C: return ImGuiKey_Enter;
  case 0x1D: return ImGuiKey_LeftCtrl;
  case 0x1E: return ImGuiKey_A;
  case 0x1F: return ImGuiKey_S;
  case 0x20: return ImGuiKey_D;
  case 0x21: return ImGuiKey_F;
  case 0x22: return ImGuiKey_G;
  case 0x23: return ImGuiKey_H;
  case 0x24: return ImGuiKey_J;
  case 0x25: return ImGuiKey_K;
  case 0x26: return ImGuiKey_L;
  case 0x27: return ImGuiKey_Semicolon;
  case 0x28: return ImGuiKey_Apostrophe;
  case 0x29: return ImGuiKey_GraveAccent;
  case 0x2A: return ImGuiKey_LeftShift;
  case 0x2B: return ImGuiKey_Backslash;
  case 0x2C: return ImGuiKey_Z;
  case 0x2D: return ImGuiKey_X;
  case 0x2E: return ImGuiKey_C;
  case 0x2F: return ImGuiKey_V;
  case 0x30: return ImGuiKey_B;
  case 0x31: return ImGuiKey_N;
  case 0x32: return ImGuiKey_M;
  case 0x33: return ImGuiKey_Comma;
  case 0x34: return ImGuiKey_Period;
  case 0x35: return ImGuiKey_Slash;
  case 0x36: return ImGuiKey_RightShift;
  case 0x38: return ImGuiKey_LeftAlt;
  case 0x39: return ImGuiKey_Space;
  case 0x3A: return ImGuiKey_CapsLock;
  case 0x9C: return ImGuiKey_KeypadEnter;
  case 0x9D: return ImGuiKey_RightCtrl;
  case 0xB8: return ImGuiKey_RightAlt;
  case 0xC7: return ImGuiKey_Home;
  case 0xC8: return ImGuiKey_UpArrow;
  case 0xC9: return ImGuiKey_PageUp;
  case 0xCB: return ImGuiKey_LeftArrow;
  case 0xCD: return ImGuiKey_RightArrow;
  case 0xCF: return ImGuiKey_End;
  case 0xD0: return ImGuiKey_DownArrow;
  case 0xD1: return ImGuiKey_PageDown;
  case 0xD2: return ImGuiKey_Insert;
  case 0xD3: return ImGuiKey_Delete;
  default: break;
  }

  if (a_scanCode >= 0x3B && a_scanCode <= 0x44) {
    return static_cast<ImGuiKey>(ImGuiKey_F1 + (a_scanCode - 0x3B));
  }
  if (a_scanCode == 0x57) {
    return ImGuiKey_F11;
  }
  if (a_scanCode == 0x58) {
    return ImGuiKey_F12;
  }
  return ImGuiKey_None;
}

bool QueueGamepadKitListMove(const std::uint32_t a_scanCode) {
  switch (keycode::NormalizeGamepadKeyCode(a_scanCode)) {
  case SKSE::InputMap::kGamepadButtonOffset_DPAD_UP:
    return ui::QueueKitListMove(-1);
  case SKSE::InputMap::kGamepadButtonOffset_DPAD_DOWN:
    return ui::QueueKitListMove(1);
  case SKSE::InputMap::kGamepadButtonOffset_DPAD_LEFT:
    return ui::QueueKitListBack();
  case SKSE::InputMap::kGamepadButtonOffset_DPAD_RIGHT:
    return ui::QueueKitListNextPane();
  default:
    return false;
  }
}

bool QueueKeyboardKitListMove(const std::uint32_t a_scanCode) {
  if (keycode::IsKeyModifier(a_scanCode)) {
    return false;
  }

  switch (input::kit_list::FromScanCode(a_scanCode)) {
  case input::kit_list::KeyboardCommand::MoveUp:
    return ui::QueueKitListMove(-1);
  case input::kit_list::KeyboardCommand::MoveDown:
    return ui::QueueKitListMove(1);
  case input::kit_list::KeyboardCommand::Back:
    return ui::QueueKitListBack();
  case input::kit_list::KeyboardCommand::NextPane:
    return ui::QueueKitListNextPane();
  case input::kit_list::KeyboardCommand::Apply:
    return ui::QueueKitListApply();
  case input::kit_list::KeyboardCommand::Preview:
    return ui::QueueKitListPreview();
  case input::kit_list::KeyboardCommand::None:
    return false;
  }
  return false;
}

[[nodiscard]] int KeyboardKitListMoveDelta(const std::uint32_t a_scanCode) {
  return input::kit_list::MoveDelta(input::kit_list::FromScanCode(a_scanCode));
}

[[nodiscard]] int GamepadKitListMoveDelta(const std::uint32_t a_scanCode) {
  switch (keycode::NormalizeGamepadKeyCode(a_scanCode)) {
  case SKSE::InputMap::kGamepadButtonOffset_DPAD_UP:
    return -1;
  case SKSE::InputMap::kGamepadButtonOffset_DPAD_DOWN:
    return 1;
  default:
    return 0;
  }
}

constexpr auto kKitListRepeatInitialDelay = std::chrono::milliseconds(340);
constexpr auto kKitListRepeatInterval = std::chrono::milliseconds(65);
} // namespace

auto InputManager::GetSingleton() -> InputManager * {
  static InputManager singleton;
  return std::addressof(singleton);
}

void InputManager::OnFocusChange(bool a_focus) {
  ResetGamepadRotation();
  SetShortcutSuppressionActive(false);
  toggleKeyDown_ = false;
  modifierSidesDown_ = 0;
  gamepadButtonsDown_.clear();
  pendingGamepadCaptureKey_ = 0;
  keyboardKitListMoveHeld_ = 0;
  gamepadKitListMoveHeld_ = 0;
  nextKitListMoveRepeatAt_ = {};
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  io.ClearInputKeys();
  io.ClearEventsQueue();
  io.AddFocusEvent(a_focus);

}

void InputManager::Flush() {
  ResetGamepadRotation();
  SetShortcutSuppressionActive(false);
  {
    std::scoped_lock lock(inputLock_);
    inputQueue_.clear();
  }

  if (auto *inputMgr = RE::BSInputDeviceManager::GetSingleton();
      inputMgr != nullptr) {
    if (auto *device = inputMgr->GetKeyboard(); device != nullptr) {
      device->ClearInputState();
    }
  }

  if (auto *eventQueue = RE::BSInputEventQueue::GetSingleton();
      eventQueue != nullptr) {
    eventQueue->ClearInputQueue();
  }

  toggleKeyDown_ = false;
  modifierSidesDown_ = 0;
  gamepadButtonsDown_.clear();
  pendingGamepadCaptureKey_ = 0;
  keyboardKitListMoveHeld_ = 0;
  gamepadKitListMoveHeld_ = 0;
  nextKitListMoveRepeatAt_ = {};
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  io.ClearInputKeys();
  io.ClearEventsQueue();
}

void InputManager::AddEventToQueue(RE::InputEvent **a_events) {
  if (!a_events || !*a_events) {
    return;
  }

  std::scoped_lock lock(inputLock_);
  for (auto event = *a_events; event; event = event->next) {
    // Copy analog values before the event list is filtered/reused by Skyrim.
    if (event->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
      if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
        const auto* stick = event->AsThumbstickEvent();
        if (stick && stick->IsRight()) { gamepadRotation_.SetRightX(stick->xValue); }
      } else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
        const auto* button = event->AsButtonEvent();
        if (button && keycode::NormalizeGamepadKeyCode(button->GetIDCode()) ==
                SKSE::InputMap::kGamepadButtonOffset_LT) {
          gamepadRotation_.SetTrigger(button->value);
        }
      } else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kDeviceConnect) {
        ResetGamepadRotation();
      }
    }
    inputQueue_.push_back(event);
  }
}

void InputManager::ProcessInputEvents() {
  std::vector<RE::InputEvent *> queuedEvents;
  {
    std::scoped_lock lock(inputLock_);
    queuedEvents.swap(inputQueue_);
  }

  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  const auto inputSinkState = ui::GetInputSinkState();
  const bool nativeHotkeyEnabled = api::IsHotkeyEnabled();

  for (const auto *event : queuedEvents) {
    switch (event->GetEventType()) {
    case RE::INPUT_EVENT_TYPE::kChar: {
      break;
    }
    case RE::INPUT_EVENT_TYPE::kButton: {
      const auto *buttonEvent = static_cast<const RE::ButtonEvent *>(event);
      const auto device = buttonEvent->device.get();
      const auto scanCode = buttonEvent->GetIDCode();
      const bool keyIsDown = buttonEvent->IsPressed();
      const bool keyWentDown = buttonEvent->IsDown();

      if (inputSinkState.enabled && input::IsMenuCancel(*buttonEvent)) {
        if (keyWentDown) {
          ui::CancelInputSink();
          io.ClearInputKeys();
        }
        // The raw event is consumed by Hooks before Skyrim can emit a second
        // Cancel user event. One press closes exactly one transient/main UI.
        continue;
      }

      if (const auto modifierBit = GetModifierBit(scanCode); modifierBit != 0) {
        const bool modifierWasDown = (modifierSidesDown_ & modifierBit) != 0;
        if (modifierWasDown != keyIsDown) {
          if (keyIsDown) {
            modifierSidesDown_ |= modifierBit;
          } else {
            modifierSidesDown_ &= static_cast<std::uint8_t>(~modifierBit);
          }
        }
      }

      switch (device) {
      case RE::INPUT_DEVICE::kMouse:
        break;
      case RE::INPUT_DEVICE::kKeyboard: {
        const auto moveDelta = KeyboardKitListMoveDelta(scanCode);
        if (!keyIsDown && moveDelta != 0 &&
            keyboardKitListMoveHeld_ == moveDelta) {
          keyboardKitListMoveHeld_ = 0;
        }
        if (inputSinkState.wantsTextInput) {
          const auto imguiKey = ScanCodeToImGuiKey(scanCode);
          if (imguiKey != ImGuiKey_None &&
              (keyWentDown || buttonEvent->IsUp())) {
            io.AddKeyEvent(imguiKey, keyIsDown);
          }
          break;
        }

        if (inputSinkState.capturingToggleKey && keyWentDown) {
          if (keycode::IsKeyModifier(scanCode)) {
            break;
          }

          pendingGamepadCaptureKey_ = 0;
          ui::HandleToggleKeyCapture(scanCode, GetActiveModifierScanCode());
          io.ClearInputKeys();
          break;
        }

        const bool isToggleKey = scanCode == inputSinkState.toggleKey;
        const bool toggleKeyWentDown = isToggleKey && keyIsDown && !toggleKeyDown_;
        if (isToggleKey) {
          toggleKeyDown_ = keyIsDown;
        }

        if (nativeHotkeyEnabled && !inputSinkState.wantsTextInput &&
            scanCode == inputSinkState.toggleKey && IsBoundModifierDown() &&
            toggleKeyWentDown) {
          ui::ToggleInputSinkVisibility();
          io.ClearInputKeys();
          break;
        }

        if (!inputSinkState.enabled) {
          break;
        }

        if (scanCode == keycode::kTabScanCode &&
            (keyWentDown || buttonEvent->IsUp())) {
          io.AddKeyEvent(ImGuiKey_Tab, keyIsDown);
          break;
        }

        if (keyWentDown && QueueKeyboardKitListMove(scanCode)) {
          if (moveDelta != 0) {
            keyboardKitListMoveHeld_ = moveDelta;
            nextKitListMoveRepeatAt_ = std::chrono::steady_clock::now() +
                                       kKitListRepeatInitialDelay;
          }
          io.ClearInputKeys();
          break;
        }

        break;
      }
      case RE::INPUT_DEVICE::kGamepad: {
        const auto gamepadKey = keycode::NormalizeGamepadKeyCode(scanCode);
        if (!keycode::IsGamepadKey(gamepadKey)) {
          break;
        }

        if (keyIsDown) {
          gamepadButtonsDown_.insert(gamepadKey);
        } else {
          gamepadButtonsDown_.erase(gamepadKey);
        }

        if (inputSinkState.capturingToggleKey) {
          if (keyWentDown) {
            if (pendingGamepadCaptureKey_ == 0) {
              pendingGamepadCaptureKey_ = gamepadKey;
            } else if (pendingGamepadCaptureKey_ != gamepadKey) {
              ui::HandleToggleKeyCapture(gamepadKey,
                                         pendingGamepadCaptureKey_);
              pendingGamepadCaptureKey_ = 0;
              io.ClearInputKeys();
            }
          } else if (buttonEvent->IsUp() &&
                     pendingGamepadCaptureKey_ == gamepadKey) {
            ui::HandleToggleKeyCapture(gamepadKey, 0);
            pendingGamepadCaptureKey_ = 0;
            io.ClearInputKeys();
          }
          break;
        }

        const auto moveDelta = GamepadKitListMoveDelta(gamepadKey);
        if (!keyIsDown && moveDelta != 0 &&
            gamepadKitListMoveHeld_ == moveDelta) {
          gamepadKitListMoveHeld_ = 0;
        }
        pendingGamepadCaptureKey_ = 0;

        const bool gamepadBinding =
            keycode::IsGamepadKey(inputSinkState.toggleKey);
        const bool isToggleKey =
            gamepadBinding && gamepadKey == inputSinkState.toggleKey;
        const bool toggleKeyWentDown =
            isToggleKey && keyIsDown && !toggleKeyDown_;
        if (isToggleKey) {
          toggleKeyDown_ = keyIsDown;
        }

        const bool modifierDown =
            inputSinkState.toggleModifier == 0 ||
            (keycode::IsGamepadKey(inputSinkState.toggleModifier) &&
             gamepadButtonsDown_.contains(inputSinkState.toggleModifier));
        if (nativeHotkeyEnabled && !inputSinkState.wantsTextInput &&
            toggleKeyWentDown &&
            modifierDown) {
          ui::ToggleInputSinkVisibility();
          io.ClearInputKeys();
          break;
        }

        if (!inputSinkState.enabled || inputSinkState.wantsTextInput ||
            !keyWentDown ||
            gamepadKey == inputSinkState.toggleModifier) {
          break;
        }

        // Preview/apply are intentionally handled by MenuHost's Skyrim user
        // events (Jump and Activate).  That preserves the player's remapped
        // controls instead of assuming fixed physical Y/A buttons.
        if (QueueGamepadKitListMove(gamepadKey)) {
          if (moveDelta != 0) {
            gamepadKitListMoveHeld_ = moveDelta;
            nextKitListMoveRepeatAt_ = std::chrono::steady_clock::now() +
                                       kKitListRepeatInitialDelay;
          }
          io.ClearInputKeys();
        }
        break;
      }
      default:
        break;
      }
      break;
    }
    default:
      break;
    }
  }

  // Skyrim reports the initial button transition, but it does not provide a
  // portable repeat stream for both keyboard arrows and the gamepad D-pad.
  // Keep repeat generation here so every catalog and Kit Generator list has
  // the same one-step initial press followed by paced continuous traversal.
  if (!inputSinkState.enabled || inputSinkState.wantsTextInput) {
    keyboardKitListMoveHeld_ = 0;
    gamepadKitListMoveHeld_ = 0;
    return;
  }
  const auto heldMoveDelta = keyboardKitListMoveHeld_ != 0
                                 ? keyboardKitListMoveHeld_
                                 : gamepadKitListMoveHeld_;
  const auto now = std::chrono::steady_clock::now();
  if (heldMoveDelta != 0 && now >= nextKitListMoveRepeatAt_) {
    if (ui::QueueKitListMove(heldMoveDelta)) {
      // At most one row is emitted per frame even after a hitch. This keeps
      // a held input predictable and prevents skipped previews.
      nextKitListMoveRepeatAt_ = now + kKitListRepeatInterval;
    } else {
      keyboardKitListMoveHeld_ = 0;
      gamepadKitListMoveHeld_ = 0;
    }
  }
}

void InputManager::SetShortcutSuppressionActive(const bool a_active) {
  shortcutSuppressionActive_.store(a_active, std::memory_order_release);
}

bool InputManager::IsShortcutSuppressionActive() const {
  return shortcutSuppressionActive_.load(std::memory_order_acquire);
}

bool InputManager::IsBoundModifierDown() const {
  switch (ui::GetInputSinkState().toggleModifier) {
  case 0x00:
    return true;
  case 0x2A:
  case 0x36:
    return (modifierSidesDown_ & (kLeftShiftBit | kRightShiftBit)) != 0;
  case 0x1D:
  case 0x9D:
    return (modifierSidesDown_ & (kLeftCtrlBit | kRightCtrlBit)) != 0;
  case 0x38:
  case 0xB8:
    return (modifierSidesDown_ & (kLeftAltBit | kRightAltBit)) != 0;
  default:
    return false;
  }
}

std::uint32_t InputManager::GetActiveModifierScanCode() const {
  if ((modifierSidesDown_ & kLeftShiftBit) != 0) {
    return 0x2A;
  }
  if ((modifierSidesDown_ & kRightShiftBit) != 0) {
    return 0x36;
  }
  if ((modifierSidesDown_ & kLeftCtrlBit) != 0) {
    return 0x1D;
  }
  if ((modifierSidesDown_ & kRightCtrlBit) != 0) {
    return 0x9D;
  }
  if ((modifierSidesDown_ & kLeftAltBit) != 0) {
    return 0x38;
  }
  if ((modifierSidesDown_ & kRightAltBit) != 0) {
    return 0xB8;
  }
  return 0;
}

void InputManager::UpdateMousePosition() const {
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto *ui = RE::UI::GetSingleton();
  if (ui == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  if (ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
    if (const auto *menuCursor = RE::MenuCursor::GetSingleton();
        menuCursor != nullptr) {
      io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
      io.AddMousePosEvent(menuCursor->cursorPosX, menuCursor->cursorPosY);
    }
    return;
  }

  POINT cursorPos{};
  if (GetCursorPos(&cursorPos) != FALSE) {
    io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
    io.AddMousePosEvent(static_cast<float>(cursorPos.x),
                        static_cast<float>(cursorPos.y));
  }
}
} // namespace sfs
