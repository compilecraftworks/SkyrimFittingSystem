#include "Hooks.h"

#include "InputManager.h"
#include "Keycode.h"
#include "api/SkyrimFittingSystemAPI.h"
#include "ui/Menu.h"
#include "ui/MenuHost.h"
#include "workbench/EquipmentRefreshEventSink.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace {
std::atomic_bool g_windowShutdownObserved{false};
std::mutex g_wndProcMapMutex;
std::unordered_map<ATOM, WNDPROC> g_originalWndProcsByAtom;

auto GetOriginalWndProc(HWND a_hwnd) -> WNDPROC {
  const auto atom =
      static_cast<ATOM>(::GetClassLongPtrA(a_hwnd, GCW_ATOM) & 0xFFFF);
  if (atom == 0) {
    return nullptr;
  }

  std::scoped_lock lock(g_wndProcMapMutex);
  if (const auto it = g_originalWndProcsByAtom.find(atom);
      it != g_originalWndProcsByAtom.end()) {
    return it->second;
  }

  return nullptr;
}

std::mutex g_shortcutFilterMutex;
std::unordered_set<std::uint32_t> g_downKeyboardButtons;
std::unordered_set<std::uint32_t> g_preSuppressionButtons;
std::unordered_set<std::uint32_t> g_swallowedUntilReleaseButtons;
std::unordered_set<std::uint32_t> g_releaseMustPassButtons;
std::unordered_set<std::uint32_t> g_downGamepadButtons;
std::unordered_set<std::uint32_t> g_swallowedGamepadUntilReleaseButtons;
bool g_shortcutSuppressionWasActive{false};

void ResetShortcutFilterState() {
  std::scoped_lock lock(g_shortcutFilterMutex);
  g_downKeyboardButtons.clear();
  g_preSuppressionButtons.clear();
  g_swallowedUntilReleaseButtons.clear();
  g_releaseMustPassButtons.clear();
  g_downGamepadButtons.clear();
  g_swallowedGamepadUntilReleaseButtons.clear();
  g_shortcutSuppressionWasActive = false;
}

void FilterBlockedInputEvents(RE::InputEvent **a_events) {
  if (a_events == nullptr) {
    return;
  }

  std::scoped_lock lock(g_shortcutFilterMutex);
  if (auto *ui = RE::UI::GetSingleton();
      ui != nullptr && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
    // Console input is owned by Skyrim and other console/IME extensions.  SFS
    // must never consume Enter or a stale suppression release while it is
    // open, even if the SFS menu was closed during an interrupted input frame.
    g_downKeyboardButtons.clear();
    g_preSuppressionButtons.clear();
    g_swallowedUntilReleaseButtons.clear();
    g_releaseMustPassButtons.clear();
    g_downGamepadButtons.clear();
    g_swallowedGamepadUntilReleaseButtons.clear();
    g_shortcutSuppressionWasActive = false;
    sfs::InputManager::GetSingleton()->SetShortcutSuppressionActive(false);
    return;
  }

  auto *menu = sfs::Menu::GetSingleton();
  const bool menuEnabled = menu->IsEnabled();
  const bool toggleCaptureActive = menuEnabled && menu->IsCapturingToggleKey();
  const bool nativeHotkeyEnabled = sfs::api::IsHotkeyEnabled();
  const bool shortcutSuppressionActive =
      menuEnabled &&
      (sfs::InputManager::GetSingleton()->IsShortcutSuppressionActive() ||
       toggleCaptureActive);
  const auto toggleKey = menu->GetToggleKey();
  const auto toggleModifier = menu->GetToggleModifier();
  const bool gamepadBinding = sfs::keycode::IsGamepadKey(toggleKey);
  if (shortcutSuppressionActive && !g_shortcutSuppressionWasActive) {
    g_preSuppressionButtons = g_downKeyboardButtons;
    for (const auto scanCode : g_swallowedUntilReleaseButtons) {
      g_preSuppressionButtons.erase(scanCode);
    }
  } else if (!shortcutSuppressionActive && g_shortcutSuppressionWasActive) {
    for (const auto scanCode : g_preSuppressionButtons) {
      if (g_downKeyboardButtons.contains(scanCode)) {
        g_releaseMustPassButtons.insert(scanCode);
      }
    }
    g_preSuppressionButtons.clear();
  }
  g_shortcutSuppressionWasActive = shortcutSuppressionActive;

  auto **link = a_events;
  while (*link != nullptr) {
    auto *event = *link;
    bool blockEvent = false;

    if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton &&
        event->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
      const auto *buttonEvent = event->AsButtonEvent();
      if (buttonEvent != nullptr) {
        const auto scanCode = buttonEvent->GetIDCode();
        const bool isRelease = buttonEvent->IsUp();
        const bool releaseMustPass =
            g_releaseMustPassButtons.contains(scanCode);
        const bool preSuppressionRelease =
            isRelease && g_preSuppressionButtons.contains(scanCode);

        if (shortcutSuppressionActive) {
          if (g_swallowedUntilReleaseButtons.contains(scanCode)) {
            blockEvent = true;
            if (isRelease) {
              g_swallowedUntilReleaseButtons.erase(scanCode);
            }
          } else if (g_preSuppressionButtons.contains(scanCode)) {
            blockEvent = !isRelease;
            if (isRelease) {
              g_preSuppressionButtons.erase(scanCode);
            }
          } else {
            blockEvent = true;
            if (!isRelease && buttonEvent->IsPressed()) {
              g_swallowedUntilReleaseButtons.insert(scanCode);
            }
          }
        } else if (g_swallowedUntilReleaseButtons.contains(scanCode)) {
          blockEvent = true;
          if (isRelease) {
            g_swallowedUntilReleaseButtons.erase(scanCode);
          }
        }

        if (menuEnabled && scanCode == sfs::keycode::kTabScanCode &&
            !releaseMustPass && !preSuppressionRelease) {
          blockEvent = true;
        }

        if (isRelease) {
          g_downKeyboardButtons.erase(scanCode);
          g_releaseMustPassButtons.erase(scanCode);
        } else if (buttonEvent->IsPressed()) {
          g_downKeyboardButtons.insert(scanCode);
        }
      }
    } else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton &&
               event->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
      const auto *buttonEvent = event->AsButtonEvent();
      if (buttonEvent != nullptr) {
        const auto gamepadKey =
            sfs::keycode::NormalizeGamepadKeyCode(buttonEvent->GetIDCode());
        if (sfs::keycode::IsGamepadKey(gamepadKey)) {
          const bool isRelease = buttonEvent->IsUp();
          const bool isPressed = buttonEvent->IsPressed();
          const bool modifierDown =
              toggleModifier == 0 ||
              (sfs::keycode::IsGamepadKey(toggleModifier) &&
               (g_downGamepadButtons.contains(toggleModifier) ||
                (gamepadKey == toggleModifier && isPressed)));
          const bool boundModifierEvent = nativeHotkeyEnabled && gamepadBinding &&
                                          toggleModifier != 0 &&
                                          gamepadKey == toggleModifier;
          const bool boundToggleEvent =
              nativeHotkeyEnabled && gamepadBinding && gamepadKey == toggleKey &&
              modifierDown;
          const bool alreadySwallowed =
              g_swallowedGamepadUntilReleaseButtons.contains(gamepadKey);

          if (toggleCaptureActive || boundModifierEvent || boundToggleEvent ||
              alreadySwallowed) {
            blockEvent = true;
            if (isRelease) {
              g_swallowedGamepadUntilReleaseButtons.erase(gamepadKey);
            } else if (isPressed) {
              g_swallowedGamepadUntilReleaseButtons.insert(gamepadKey);
            }
          }

          if (isRelease) {
            g_downGamepadButtons.erase(gamepadKey);
          } else if (isPressed) {
            g_downGamepadButtons.insert(gamepadKey);
          }
        }
      }
    }

    if (blockEvent) {
      *link = event->next;
      continue;
    }
    link = &event->next;
  }
}

static void
hk_PollInputDevices(RE::BSTEventSource<RE::InputEvent *> *a_dispatcher,
                    RE::InputEvent **a_events);
static inline REL::Relocation<decltype(hk_PollInputDevices)> g_inputHandler;
static inline REL::Relocation<uintptr_t> g_registerClass{
    REL::VariantID(75591, 77226, 0xDC4B90)};

void hk_PollInputDevices(RE::BSTEventSource<RE::InputEvent *> *a_dispatcher,
                         RE::InputEvent **a_events) {
  if (a_events) {
    sfs::InputManager::GetSingleton()->AddEventToQueue(a_events);
    FilterBlockedInputEvents(a_events);
  }

  g_inputHandler(a_dispatcher, a_events);
}

struct WndProcHook {
  static LRESULT thunk(HWND a_hwnd, UINT a_msg, WPARAM a_wParam,
                       LPARAM a_lParam) {
    switch (a_msg) {
    case WM_CLOSE:
      ResetShortcutFilterState();
      if (!g_windowShutdownObserved.exchange(true, std::memory_order_relaxed)) {
        sfs::Menu::GetSingleton()->NotifyWindowShutdown();
      }
      break;
    case WM_DESTROY:
    case WM_NCDESTROY:
      ResetShortcutFilterState();
      g_windowShutdownObserved.store(true, std::memory_order_relaxed);
      break;
    case WM_ACTIVATE: {
      const auto activationType = LOWORD(a_wParam);
      if (activationType == WA_INACTIVE) {
        ResetShortcutFilterState();
      } else {
        ResetShortcutFilterState();
        sfs::InputManager::GetSingleton()->Flush();
        sfs::InputManager::GetSingleton()->OnFocusChange(true);
      }
      break;
    }
    case WM_SETFOCUS:
      ResetShortcutFilterState();
      sfs::InputManager::GetSingleton()->Flush();
      sfs::InputManager::GetSingleton()->OnFocusChange(true);
      break;
    case WM_KILLFOCUS:
      ResetShortcutFilterState();
      sfs::InputManager::GetSingleton()->OnFocusChange(false);
      break;
    default:
      break;
    }

    const auto originalWndProc = GetOriginalWndProc(a_hwnd);
    if (originalWndProc == nullptr) {
      logger::warn("SFS hook: missing original WndProc hwnd={} msg=0x{:X}",
                   static_cast<void *>(a_hwnd), a_msg);
      return DefWindowProcA(a_hwnd, a_msg, a_wParam, a_lParam);
    }

    return CallWindowProcA(originalWndProc, a_hwnd, a_msg, a_wParam, a_lParam);
  }
};

struct RegisterClassAHook {
  static ATOM thunk(WNDCLASSA *a_wndClass) {
    const auto originalWndProc = a_wndClass->lpfnWndProc;
    a_wndClass->lpfnWndProc = &WndProcHook::thunk;
    const auto atom = func(a_wndClass);
    if (atom != 0 && originalWndProc != nullptr) {
      std::scoped_lock lock(g_wndProcMapMutex);
      g_originalWndProcsByAtom[atom] = originalWndProc;
    }
    return atom;
  }

  static inline REL::Relocation<decltype(thunk)> func;
};
} // namespace

namespace sfs::hooks {
void ResetInputFilterState() {
  ResetShortcutFilterState();
  sfs::InputManager::GetSingleton()->SetShortcutSuppressionActive(false);
}

bool IsWindowShutdownObserved() {
  return g_windowShutdownObserved.load(std::memory_order_relaxed);
}

struct D3DInitHook {
  static void thunk() {
    func();

    auto *renderer = RE::BSGraphics::Renderer::GetSingleton();
    auto *context = reinterpret_cast<ID3D11DeviceContext *>(
        renderer->GetRuntimeData().context);
    auto *swapChain = reinterpret_cast<IDXGISwapChain *>(
        renderer->GetRuntimeData().renderWindows->swapChain);
    auto *device =
        reinterpret_cast<ID3D11Device *>(renderer->GetRuntimeData().forwarder);

    Menu::GetSingleton()->Init(swapChain, device, context);
    MenuHost::RegisterMenu();
  }

  static inline REL::Relocation<decltype(thunk)> func;
};

struct PresentHook {
  static void thunk(std::uint32_t a_argument) {
    func(a_argument);
    if (g_windowShutdownObserved.load(std::memory_order_relaxed)) {
      return;
    }
    sfs::api::ProcessMenuRequests();
    InputManager::GetSingleton()->ProcessInputEvents();
    sfs::workbench::EquipmentRefreshEventSink::GetSingleton()
        ->TickConditionState();
  }

  static inline REL::Relocation<decltype(thunk)> func;
};

void Install() {
  auto &trampoline = SKSE::GetTrampoline();

  logger::info("Hooking BSInputDeviceManager::PollInputDevices");
  g_inputHandler =
      trampoline.write_call<5>(REL::RelocationID(67315, 68617).address() +
                                   REL::Relocate(0x7B, 0x7B, 0x81),
                               hk_PollInputDevices);

  logger::info("Hooking RegisterClassA");
  const auto registerClassTarget = trampoline.write_call<6>(
      g_registerClass.address() +
          REL::VariantOffset(0x8E, 0x15C, 0x99).offset(),
      RegisterClassAHook::thunk);
  if (registerClassTarget == 0) {
    logger::critical("Failed to hook RegisterClassA");
    return;
  }
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  RegisterClassAHook::func =
      *reinterpret_cast<const uintptr_t *>(registerClassTarget);

  logger::info("Hooking BSGraphics::Renderer::InitD3D");
  D3DInitHook::func = trampoline.write_call<5>(
      REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC),
      D3DInitHook::thunk);

  logger::info("Hooking DXGI present");
  PresentHook::func =
      trampoline.write_call<5>(REL::RelocationID(75461, 77246).address() +
                                   REL::VariantOffset(0x9, 0x9, 0x15).offset(),
                               PresentHook::thunk);
}
} // namespace sfs::hooks
