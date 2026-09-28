// Execute the production filter/dispatch, with only the engine boundary faked.
#include "input/KitListNavigation.h"
#include "kit_generator/PluginSelection.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace RE {
enum class INPUT_DEVICE { kKeyboard, kMouse, kGamepad };
enum class INPUT_EVENT_TYPE { kButton, kThumbstick, kChar };
struct ButtonEvent;
struct ThumbstickEvent;
struct InputEvent {
  InputEvent* next{};
  INPUT_DEVICE device{INPUT_DEVICE::kKeyboard};
  INPUT_EVENT_TYPE type{INPUT_EVENT_TYPE::kButton};
  auto GetDevice() const { return device; }
  auto GetEventType() const { return type; }
  const ButtonEvent* AsButtonEvent() const;
  const ThumbstickEvent* AsThumbstickEvent() const;
};
struct ButtonEvent : InputEvent {
  std::uint32_t code{};
  int phase{}; // 0: down, 1: held, 2: released
  auto GetIDCode() const { return code; }
  bool IsUp() const { return phase == 2; }
  bool IsDown() const { return phase == 0; }
  bool IsPressed() const { return phase != 2; }
};
struct ThumbstickEvent : InputEvent { bool right{true}; bool IsRight() const { return right; } };
const ButtonEvent* InputEvent::AsButtonEvent() const {
  return type == INPUT_EVENT_TYPE::kButton ? static_cast<const ButtonEvent*>(this) : nullptr;
}
const ThumbstickEvent* InputEvent::AsThumbstickEvent() const {
  return type == INPUT_EVENT_TYPE::kThumbstick ? static_cast<const ThumbstickEvent*>(this) : nullptr;
}
struct Console { static constexpr auto MENU_NAME = "Console"; };
struct UI {
  bool console{};
  static UI* GetSingleton() { static UI ui; return &ui; }
  bool IsMenuOpen(const char*) const { return console; }
};
template<class T> struct BSTEventSource {};
}
namespace sfs {
namespace ui {
namespace catalog {
enum class BrowserTab { Gear, Outfits, Conditions, Kits, KitGenerator, Options };
struct BrowserState { BrowserTab activeTab{BrowserTab::Kits}; };
}
struct MenuCharacterPresentation {
  bool active{};
  static auto* GetSingleton() { static MenuCharacterPresentation p; return &p; }
  bool IsActive() const { return active; }
};
}
struct Menu {
  bool enabled_{}, wantTextInput_{}, capture{};
  std::uint32_t toggle{0x40}, modifier{};
  ui::catalog::BrowserState browser;
  static Menu* GetSingleton() { static Menu m; return &m; }
  bool IsEnabled() const { return enabled_; }
  bool WantsTextInput() const { return wantTextInput_; }
  bool IsCapturingToggleKey() const { return capture; }
  auto GetToggleKey() const { return toggle; }
  auto GetToggleModifier() const { return modifier; }
  const auto& CatalogBrowserState() const { return browser; }
  bool IsKeyboardListNavigationActive() const;
};
#include "NavigationContext.production.inc"
struct InputManager {
  bool suppression{}, rotation{};
  std::vector<RE::InputEvent*> queued;
  static InputManager* GetSingleton() { static InputManager i; return &i; }
  bool IsShortcutSuppressionActive() const { return suppression; }
  void SetShortcutSuppressionActive(bool active) { suppression = active; }
  bool IsGamepadRotationChordDown() const { return rotation; }
  void AddEventToQueue(RE::InputEvent** events) {
    for (auto event = *events; event; event = event->next) queued.push_back(event);
  }
};
namespace api { bool IsHotkeyEnabled() { return true; } }
namespace keycode {
bool IsGamepadKey(std::uint32_t key) { return key >= 256; }
auto NormalizeGamepadKeyCode(std::uint32_t key) { return key; }
}
namespace input {
bool IsMenuCancel(const RE::ButtonEvent& event) {
  return event.code == (event.device == RE::INPUT_DEVICE::kKeyboard ? 15u : 277u);
}
}
}
namespace {
#include "InputFilter.production.inc"
std::vector<RE::InputEvent*> delivered;
void g_inputHandler(RE::BSTEventSource<RE::InputEvent*>*, RE::InputEvent** events) {
  if (events) for (auto e = *events; e; e = e->next) delivered.push_back(e);
}
#include "InputDispatch.production.inc"

void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void Reset() {
  ResetShortcutFilterState();
  *sfs::Menu::GetSingleton() = {};
  *sfs::InputManager::GetSingleton() = {};
  *RE::UI::GetSingleton() = {};
  *sfs::ui::MenuCharacterPresentation::GetSingleton() = {};
  delivered.clear();
}
bool Send(std::uint32_t key, int phase, RE::INPUT_DEVICE device = RE::INPUT_DEVICE::kKeyboard) {
  RE::ButtonEvent event;
  event.code = key; event.phase = phase; event.device = device;
  RE::InputEvent* head = &event;
  delivered.clear();
  sfs::InputManager::GetSingleton()->queued.clear();
  hk_PollInputDevices(nullptr, &head);
  Check(sfs::InputManager::GetSingleton()->queued == std::vector<RE::InputEvent*>{&event},
        "SFS must receive every original event before downstream filtering");
  return !delivered.empty();
}
void TestOwnership() {
  using Tab = sfs::ui::catalog::BrowserTab;
  constexpr std::array keys{0x11u, 0x1Fu, 0x1Eu, 0x20u, 0xC8u, 0xD0u, 0xCBu, 0xCDu, 0x1Cu, 0x9Cu, 0x39u};
  for (const auto tab : {Tab::Gear, Tab::Outfits, Tab::Kits, Tab::KitGenerator}) {
    for (const auto key : keys) {
      Reset();
      auto* menu = sfs::Menu::GetSingleton();
      menu->enabled_ = true; menu->browser.activeTab = tab;
      for (int cycle = 0; cycle < 128; ++cycle) {
        Check(!Send(key, 0) && !Send(key, 1), "SFS list down/hold leaked to mod sink");
        // Exercise the same close transition as OnMenuHide.
        ResetShortcutFilterState(true); menu->enabled_ = false;
        Check(!Send(key, 1) && !Send(key, 2), "UI-owned trailing hold/release leaked on close");
        Check(Send(key, 0) && Send(key, 2), "New key presses must work after UI closes");
        Check(g_swallowedUntilReleaseButtons.empty() && g_downKeyboardButtons.empty(),
              "Closed-UI press ownership must not accumulate");
        menu->enabled_ = true;
      }
      Check(Send(0x12, 0) && Send(0x12, 2), "Unrelated mod keys must remain available");
      Check(Send(0x2A, 0) && Send(0x2A, 2), "Shift selection modifier must remain available");
      Check(Send(0x1D, 0) && Send(0x1D, 2), "Ctrl selection modifier must remain available");
    }
  }
  for (const auto tab : {Tab::Options, Tab::Conditions}) {
    Reset(); sfs::Menu::GetSingleton()->enabled_ = true;
    sfs::Menu::GetSingleton()->browser.activeTab = tab;
    for (auto key : keys) Check(Send(key, 0) && Send(key, 2), "Do not reserve keys in non-list tabs");
  }
}
void TestTransitions() {
  Reset();
  auto* menu = sfs::Menu::GetSingleton();
  Check(Send(0xC8, 0), "Outside-UI initial press must pass");
  menu->enabled_ = true;
  Check(!Send(0xC8, 1) && Send(0xC8, 2), "Previously delivered down must receive its release");
  menu->enabled_ = false;
  Check(Send(0xC8, 0), "Outside-UI initial press must pass");
  menu->enabled_ = true;
  Check(!Send(0xC8, 1), "List holds are reserved");
  ResetShortcutFilterState(true); menu->enabled_ = false;
  menu->enabled_ = true;
  Check(!Send(0xC8, 1) && Send(0xC8, 2), "Reopening must retain release owed to an outside-UI press");
  Check(!Send(0xC8, 0), "UI press consumed");
  ResetShortcutFilterState(true); menu->enabled_ = false;
  Check(Send(0xC8, 0) && Send(0xC8, 2), "Fresh down must recover a missing release after close");
  menu->enabled_ = true; Check(!Send(0xC8, 0), "UI press consumed");
  menu->browser.activeTab = sfs::ui::catalog::BrowserTab::Options;
  Check(!Send(0xC8, 2) && Send(0xC8, 0) && Send(0xC8, 2), "Tab change must only finish old owned press");
  menu->wantTextInput_ = true;
  sfs::InputManager::GetSingleton()->suppression = true;
  Check(!Send(0x12, 0) && !Send(0x12, 2), "Text input suppression preserved");
  menu->wantTextInput_ = false; sfs::InputManager::GetSingleton()->suppression = false;
  menu->capture = true;
  Check(!Send(0x12, 0) && !Send(0x12, 2), "Toggle capture suppression preserved");
  menu->capture = false;
  Check(!Send(15, 0), "Cancel must remain consumed");
  RE::UI::GetSingleton()->console = true;
  Check(Send(15, 2) && Send(0x1C, 0) && Send(0x1C, 2), "Console retains input ownership");
  Check(g_swallowedUntilReleaseButtons.empty(), "Console bypass clears stale ownership");
}
void TestMixedAndGamepad() {
  Reset(); sfs::Menu::GetSingleton()->enabled_ = true;
  RE::ButtonEvent up, other, space, mouse;
  up.code = 0xC8; other.code = 0x12; space.code = 0x39;
  mouse.device = RE::INPUT_DEVICE::kMouse;
  up.next = &other; other.next = &space; space.next = &mouse;
  RE::InputEvent* head = &up;
  hk_PollInputDevices(nullptr, &head);
  Check(delivered == std::vector<RE::InputEvent*>{&other, &mouse}, "Mixed chain must preserve unrelated order");
  Check(sfs::InputManager::GetSingleton()->queued.size() == 4, "SFS needs the unfiltered mixed chain");
  for (auto key : {256u, 257u, 266u, 267u})
    Check(Send(key, 0, RE::INPUT_DEVICE::kGamepad) && Send(key, 2, RE::INPUT_DEVICE::kGamepad),
          "Existing remapped pad navigation/activate/jump route must remain available");
  Check(!Send(277, 0, RE::INPUT_DEVICE::kGamepad) && !Send(277, 2, RE::INPUT_DEVICE::kGamepad), "Pad cancel remains consumed");
  ResetShortcutFilterState();
  Check(g_swallowedUntilReleaseButtons.empty() && g_downKeyboardButtons.empty(), "Focus loss must reset ownership");
}
void TestPluginSelection() {
  using sfs::kit_generator::SelectPluginRows;
  const std::array<std::size_t, 4> visible{4, 1, 3, 0}; // sorted view, index 2 hidden
  std::array<bool, 5> selected{};
  std::optional<std::size_t> anchor;
  auto set = [&](std::size_t row, bool value) { selected.at(row) = value; };
  SelectPluginRows(visible, 1, true, false, anchor, set);
  SelectPluginRows(visible, 0, true, true, anchor, set);
  Check(selected == std::array{true, true, false, true, false}, "Range must follow sorted visible order only");
  SelectPluginRows(visible, 4, true, false, anchor, set);
  SelectPluginRows(visible, 3, false, false, anchor, set);
  Check(selected[4] && !selected[3] && selected[1], "Individual toggle preserves others");
  SelectPluginRows(visible, 0, false, true, anchor, set);
  Check(!selected[0] && !selected[3] && selected[1], "Range deselection leaves outside rows intact");
  anchor = 2;
  SelectPluginRows(visible, 0, true, true, anchor, set);
  Check(anchor == 0 && !selected[2], "Hidden anchor falls back to clicked row");
  const auto saved = selected;
  SelectPluginRows(std::span<const std::size_t>{}, 0, false, true, anchor, set);
  Check(selected == saved, "Empty results never mutate hidden selections");
  struct Record { std::vector<std::string> armorModelPaths; };
  Check(!sfs::kit_generator::HasWornArmorModel(std::vector<Record>{{{}}, {{""}}}), "Empty model paths are not visual armor");
  Check(sfs::kit_generator::HasWornArmorModel(std::vector<Record>{{{}}, {{"armor/wig.nif"}}}), "Accessory-only plugins must remain visible");
}
}
int main() {
  try {
    TestOwnership(); TestTransitions(); TestMixedAndGamepad(); TestPluginSelection();
    std::cout << "KeyboardInputRoutingTests passed (production dispatch/filter, close/reopen, visible plugin ranges)\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
