#include "input/CharacterRotation.h"
#include "ui/MenuCharacterRotationRules.h"
#include "ui/components/TitleBarHint.h"
#include "ui/components/WrappedTooltip.h"
#include <imgui_internal.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include "kit_generator/ResultRow.h"
#include "ui/catalog/BrowserState.h"
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
  for (const float viewportWidth : {1600.0f, 240.0f}) {
    io.DisplaySize = {viewportWidth, 1000};
    for (int n = 0; n < 3; ++n) {
      ImGui::NewFrame();
      ImGui::Begin("Tooltip layout test");
      const auto cursor = ImGui::GetCursorPos();
      const auto wrapBefore = ImGui::GetCurrentWindow()->DC.TextWrapPos;
      sfs::ui::components::DrawWrappedTooltip(
          "Review automatic grouping\n\n"
          "A group containing at least half the usable armor items has over 30 candidates. "
          "Many valid variants can also trigger this warning. Selection and generation remain available.");
      Check(ImGui::GetCursorPos().x == cursor.x && ImGui::GetCursorPos().y == cursor.y &&
            ImGui::GetCurrentWindow()->DC.TextWrapPos == wrapBefore,
            "tooltip must not change the source list layout/wrapping");
      if (n == 2) {
        bool found = false;
        for (auto* tooltip : GImGui->Windows) {
          if (!tooltip->Active || !(tooltip->Flags & ImGuiWindowFlags_Tooltip)) continue;
          found = true;
          const auto fontSize = ImGui::GetFontSize();
          Check(tooltip->Size.x <= fontSize * 24 + 2 * ImGui::GetStyle().WindowPadding.x + 2 &&
                tooltip->Size.x <= viewportWidth,
                "tooltip width must be compact and remain within a narrow viewport");
          Check(tooltip->ContentSize.y > fontSize * 5,
                "long tooltip text must wrap, not remain on one long line");
          Check(tooltip->Size.y >= tooltip->ContentSize.y,
                "tooltip height must grow to show the complete text");
        }
        Check(found, "production wrapped tooltip must render");
      }
      ImGui::End(); ImGui::Render();
    }
  }
  // Exercise the production generated-kit row, not an imitation of its hit
  // tests. Checkbox press/release, double-click, columns, scrolling and popups.
  io.DisplaySize = {1600, 1000};
  bool rowChecks[64]{};
  ImVec2 checkboxPositions[64]{}, namePositions[64]{}, espPositions[64]{};
  int changed = 0, clicked = 0, entered = 0;
  bool showModal = false;
  const auto resultFrame = [&](bool scrollBottom = false) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({50, 50});
    ImGui::SetNextWindowSize({540, 350});
    ImGui::Begin("Generated-kit results");
    if (showModal && !ImGui::IsPopupOpen("##result-modal")) ImGui::OpenPopup("##result-modal");
    if (ImGui::BeginTable("##test-results", 4, ImGuiTableFlags_ScrollY |
          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, {0, 230})) {
      ImGui::TableSetupColumn("check", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
      ImGui::TableSetupColumn("name");
      ImGui::TableSetupColumn("esp");
      ImGui::TableSetupColumn("count");
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();
      for (int row = 0; row < 64; ++row) {
        ImGui::PushID(row);
        const auto action = sfs::kit_generator::DrawResultRow(rowChecks[row], false,
            row == 0, "Kit", "Outfit.esp", "Candidates - 2 >");
        changed += action.checkChanged;
        clicked += action.clicked;
        entered += action.openDetail;
        const auto* table = ImGui::GetCurrentTable();
        const float y = table->RowPosY1 + ImGui::GetStyle().CellPadding.y + ImGui::GetFrameHeight() / 2;
        checkboxPositions[row] = {table->Columns[0].WorkMinX + ImGui::GetFrameHeight() / 2, y};
        namePositions[row] = {table->Columns[1].WorkMinX + 30, y};
        espPositions[row] = {table->Columns[2].WorkMinX + 30, y};
        ImGui::PopID();
      }
      if (scrollBottom) ImGui::SetScrollY(ImGui::GetCurrentTable()->InnerWindow, 100000);
      ImGui::EndTable();
    }
    if (ImGui::BeginPopupModal("##result-modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Rename modal owns input");
      if (!showModal) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    ImGui::End();
    ImGui::Render();
  };
  const auto resultClick = [&](ImVec2 pos) {
    io.AddMousePosEvent(pos.x, pos.y);
    io.AddMouseButtonEvent(0, true); resultFrame();
    io.AddMouseButtonEvent(0, false); resultFrame();
  };
  const auto idle = [&]() { for (int i = 0; i < 25; ++i) resultFrame(); };
  resultFrame(); resultFrame(); resultFrame();
  resultClick(checkboxPositions[0]);
  Check(rowChecks[0] && changed == 1 && clicked == 0 && entered == 0,
        "first checkbox release must check without selecting/previewing/opening the row");
  resultClick(checkboxPositions[0]);
  Check(!rowChecks[0] && changed == 2 && clicked == 0 && entered == 0,
        "checkbox double-click must never enter candidate detail");
  idle();
  resultClick(namePositions[0]);
  Check(clicked == 1 && entered == 0 && changed == 2,
        "single row click retains preview/highlight without changing checks");
  resultClick(namePositions[0]);
  Check(entered == 1 && changed == 2, "row double-click opens detail without checking");
  idle();
  const int clicksBefore = clicked;
  resultClick(espPositions[0]);
  Check(clicked == clicksBefore + 1 && entered == 1, "ESP cell is also a row selection target");
  idle();
  io.AddMousePosEvent(20, 20); io.AddMouseButtonEvent(0, true); resultFrame();
  io.AddMousePosEvent(namePositions[0].x, namePositions[0].y);
  io.AddMouseButtonEvent(0, false); resultFrame();
  Check(clicked == clicksBefore + 1, "release from an outside press cannot select a result row");
  resultFrame(true); resultFrame(); resultFrame();
  idle();
  resultClick(checkboxPositions[63]);
  Check(rowChecks[63] && !rowChecks[0] && changed == 3 && entered == 1,
        "scrolled checkbox must target its source row, never an earlier row");
  showModal = true; resultFrame(); resultFrame();
  resultClick(checkboxPositions[63]);
  Check(changed == 3 && rowChecks[63], "modal must block result checkbox input");
  showModal = false; resultFrame();
  sfs::ui::catalog::BrowserState browser;
  for (int cycle = 0; cycle < 128; ++cycle) {
    using Tab = sfs::ui::catalog::BrowserTab;
    browser.favoriteFilters = {};
    browser.activeTab = Tab::Gear;
    browser.ActiveFavoritesOnly() = true;
    browser.activeTab = Tab::Kits;
    Check(!browser.ActiveFavoritesOnly(), "Gear favorite checkbox must not check Kits");
    browser.ActiveFavoritesOnly() = true;
    browser.activeTab = Tab::Outfits;
    Check(browser.FavoritesOnlyFor(Tab::Gear) && browser.FavoritesOnlyFor(Tab::Kits) &&
              !browser.FavoritesOnlyFor(Tab::Conditions) && !browser.FavoritesOnlyFor(Tab::KitGenerator),
          "Only catalog tabs have a favorite-only filter; other tabs must not borrow Kits state");
    Check(!browser.ActiveFavoritesOnly(), "Each catalog tab must bind its own favorite setting");
    browser.activeTab = Tab::Gear;
    browser.ActiveFavoritesOnly() = false;
    Check(browser.favoriteFilters.kits && !browser.favoriteFilters.outfits,
          "Returning to Gear and toggling it must leave the other tabs unchanged");
  }
  ImGui::DestroyContext();
  std::puts("Menu gamepad/cancel rules and real-ImGui title/close/tooltip/result-row tests passed.");
}
