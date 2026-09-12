#include "input/CharacterRotation.h"
#include "ui/MenuCharacterRotationRules.h"
#include "ui/components/TitleBarHint.h"
#include <imgui_internal.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <string_view>

// Same production mapping helper, with a replaceable game control-map boundary.
namespace RE {
enum class INPUT_DEVICE { kKeyboard, kMouse, kGamepad };
struct UserEvents { enum class INPUT_CONTEXT_ID { kMenuMode }; };
struct ButtonEvent {
  INPUT_DEVICE device;
  std::uint32_t id;
  std::string_view event;
  auto GetDevice() const { return device; }
  auto GetIDCode() const { return id; }
  auto QUserEvent() const { return event; }
};
struct ControlMap {
  static constexpr std::uint32_t kInvalid = 0xFFFFFFFF;
  std::uint32_t keyboard{15}, gamepad{0x2000};
  static auto* GetSingleton() { static ControlMap c; return &c; }
  std::uint32_t GetMappedKey(std::string_view name, INPUT_DEVICE device, UserEvents::INPUT_CONTEXT_ID) const {
    if (name != "Cancel") { std::abort(); }
    return device == INPUT_DEVICE::kKeyboard ? keyboard : gamepad;
  }
};
}
namespace SKSE::InputMap {
constexpr unsigned kMacro_GamepadOffset = 266, kMaxMacros = 282;
unsigned GamepadMaskToKeycode(unsigned mask) { return mask == 0x2000 ? 277 : mask == 0x1000 ? 276 : 0xFF; }
}
#define SFS_MENU_CANCEL_TEST
#include "input/MenuCancel.h"
void Check(bool value, const char* message) {
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
  using namespace sfs::input::character_rotation;
  GamepadState pad;
  pad.SetRightX(1);
  Check(pad.Radians(1.0f / 60) == 0, "RS alone does not rotate");
  pad.SetTrigger(1);
  const auto right = pad.Radians(1.0f / 60);
  Check(right < 0 && std::abs(right - 2 * pad.Radians(1.0f / 120)) < 0.00001f,
        "LT+RS right, frame-rate independent");
  pad.SetRightX(-1);
  Check(std::abs(pad.Radians(1.0f / 60) + right) < 0.00001f, "symmetric left/right");
  pad.SetRightX(0.19f); Check(pad.Radians(0.016f) == 0, "stick drift deadzone");
  pad.SetRightX(0.6f); Check(std::abs(pad.Radians(1.0f / 60) - right / 2) < 0.00001f, "analog speed");
  pad.SetRightX(1); Check(std::abs(pad.Radians(10)) <= kMaximumStep, "hitch rotation step capped");
  pad.SetTrigger(0); Check(pad.Radians(0.016f) == 0, "LT release stops immediately");
  pad.SetTrigger(1); pad.Reset(); Check(!pad.Held() && pad.Radians(0.016f) == 0, "focus/close/disconnect reset");
  pad.SetTrigger(1); pad.SetRightX(std::numeric_limits<float>::quiet_NaN());
  Check(pad.Radians(0.016f) == 0, "invalid axis cannot poison camera rotation");
  pad.SetRightX(1); Check(pad.Radians(std::numeric_limits<float>::infinity()) == 0, "invalid frame delta");
  Check(std::abs(MouseRadians(10) + 0.03f) < 0.000001f && MouseRadians(10000) == -kMaximumStep, "existing mouse direction/speed preserved");
  Check(!sfs::ui::character_rotation::BuildPlan(true).rotateActor &&
        sfs::ui::character_rotation::BuildPlan(true).orbitCamera, "paused rotation keeps SMP actor roots unchanged");

  using RE::INPUT_DEVICE;
  Check(sfs::input::IsMenuCancel({INPUT_DEVICE::kKeyboard, 15, {}}), "mapped keyboard cancel");
  Check(!sfs::input::IsMenuCancel({INPUT_DEVICE::kKeyboard, 1, {}}), "do not invent hardcoded Escape mapping");
  RE::ControlMap::GetSingleton()->keyboard = 46;
  Check(sfs::input::IsMenuCancel({INPUT_DEVICE::kKeyboard, 46, {}}) &&
        !sfs::input::IsMenuCancel({INPUT_DEVICE::kKeyboard, 15, {}}), "keyboard remap followed");
  Check(sfs::input::IsMenuCancel({INPUT_DEVICE::kGamepad, 0x2000, {}}) &&
        sfs::input::IsMenuCancel({INPUT_DEVICE::kGamepad, 277, {}}), "native and macro gamepad codes");
  RE::ControlMap::GetSingleton()->gamepad = 276;
  Check(sfs::input::IsMenuCancel({INPUT_DEVICE::kGamepad, 0x1000, {}}) &&
        !sfs::input::IsMenuCancel({INPUT_DEVICE::kGamepad, 0x2000, {}}), "gamepad remap followed");
  Check(sfs::input::IsMenuCancel({INPUT_DEVICE::kKeyboard, 77, "Cancel"}), "explicit Skyrim Cancel event");
  RE::ControlMap::GetSingleton()->gamepad = 0xFF;
  Check(!sfs::input::IsMenuCancel({INPUT_DEVICE::kGamepad, 0x80, {}}), "unbound mapping does not match invalid key");
  Check(!sfs::input::IsMenuCancel({INPUT_DEVICE::kMouse, 0, "Cancel"}), "mouse input not captured by keyboard/pad cancel");

  ImGui::CreateContext();
  auto& io = ImGui::GetIO(); io.IniFilename = nullptr;
  io.DisplaySize = {1600, 1000}; io.DeltaTime = 1.0f / 60;
  io.ConfigInputTrickleEventQueue = false;
  unsigned char* pixels; int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  bool open = true;
  ImVec2 closePos{};
  const auto frame = [&](float windowWidth) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({50, 50}); ImGui::SetNextWindowSize({windowWidth, 450});
    ImGui::Begin("Skyrim Fitting System", &open, ImGuiWindowFlags_NoCollapse);
    auto* window = ImGui::GetCurrentWindow();
    const auto cursor = ImGui::GetCursorPos();
    const auto lastID = GImGui->LastItemData.ID;
    const auto before = window->DrawList->VtxBuffer.Size;
    sfs::ui::components::DrawTitleBarHint("Skyrim Fitting System",
        "RMB drag: Rotate character | LT + RS left/right: Rotate", "Rotate: RMB drag / LT + RS");
    Check(ImGui::GetCursorPos().x == cursor.x && ImGui::GetCursorPos().y == cursor.y &&
          GImGui->LastItemData.ID == lastID, "title hint leaves layout/hit targets/navigation intact");
    const auto& style = ImGui::GetStyle();
    const auto bar = window->TitleBarRect();
    closePos = {bar.Max.x - window->WindowBorderSize - style.FramePadding.x - ImGui::GetFontSize() / 2,
                bar.Min.y + style.FramePadding.y + ImGui::GetFontSize() / 2};
    for (int n = before; n < window->DrawList->VtxBuffer.Size; ++n) {
      Check(window->DrawList->VtxBuffer[n].pos.y < bar.Max.y + 1, "hint renders inside title bar, not body");
    }
    ImGui::TextUnformatted("Body content");
    ImGui::End(); ImGui::Render();
  };
  frame(950); frame(950); frame(390); frame(240); frame(950);
  io.AddMousePosEvent(closePos.x, closePos.y); io.AddMouseButtonEvent(0, true); frame(950);
  io.AddMouseButtonEvent(0, false); frame(950);
  Check(!open, "X close remains clickable beside the hint");
  ImGui::DestroyContext();
  std::puts("Menu gamepad/cancel rules and real-ImGui title/close tests passed.");
}
