// Real ImGui and the production dropdown: no game window/GPU required.
#include "ui/components/EditableCombo.h"
#include "ui/conditions/TextValueEditor.h"
#include <imgui_internal.h>
#include <cstdlib>
#include <iostream>

void Expect(bool value, const char *message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = {800, 600};
  io.DeltaTime = 1.0f / 60.0f;
  io.ConfigInputTrickleEventQueue = false;
  unsigned char *pixels; int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  std::string value;
  std::vector<std::string> options;
  bool custom = true;
  bool referenceMode = false;
  bool textMode = false;
  const std::vector<sfs::ui::components::EditableDropdownItem<std::string>> referenceOptions{
      {.label = "Existing NPC [00000014]", .value = "Player"}};
  int referenceSelection = 0;
  bool changed = false;
  const auto frame = [&] {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({750, 550});
    ImGui::Begin("Fixture", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    ImGui::SetCursorScreenPos({20, 20});
    if (textMode) {
      changed = sfs::ui::condition_editor::DrawTextClauseValueEditor("##text", value, 400);
    } else if (referenceMode) {
      char buffer[1024];
      std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
      std::optional<std::string> selectedValue;
      changed = sfs::ui::components::DrawEditableStringDropdown(
          "##arg", "Reference", buffer, sizeof(buffer), referenceOptions,
          400, &referenceSelection, &selectedValue);
      if (changed) { value = selectedValue ? *selectedValue : buffer; }
    } else {
      changed = sfs::ui::components::DrawSearchableStringDropdown(
          "##arg", "Form", value, options, 400, custom);
    }
    ImGui::SetCursorScreenPos({20, 450});
    ImGui::Button("Other", {100, 30});
    ImGui::End();
    ImGui::Render();
  };
  const auto click = [&](float x, float y) {
    io.AddMousePosEvent(x, y); io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
  };
  const auto type = [&](const char *text) { io.AddInputCharactersUTF8(text); frame(); };
  frame(); frame();
  click(45, 28);
  Expect(ImGui::GetActiveID() != 0, "input activated");
  type("0x00012345");
  Expect(value == "0x00012345" && changed, "empty-list ID typed and reported changed");
  click(45, 460); frame();
  Expect(value == "0x00012345", "empty-list ID survives focus loss");
  options = {"WhiterunInterior", "Skyrim.esm|00012345", "00012345"};
  frame();
  Expect(value == "0x00012345", "newly loaded suggestions do not overwrite input");
  click(45, 28); type("UnresolvedCell");
  click(45, 460); frame();
  Expect(value == "UnresolvedCell", "invalid text retained for save-time error");
  value = "WhiterunInterior"; frame();
  click(45, 28); type("Skyrim.esm|00023456");
  Expect(value == "Skyrim.esm|00023456", "typing replaces existing selection");
  io.AddKeyEvent(ImGuiKey_Enter, true); frame();
  io.AddKeyEvent(ImGuiKey_Enter, false); frame();
  click(45, 460); frame();
  Expect(value == "Skyrim.esm|00023456", "Enter and blur retain custom token");
  value.clear(); frame(); click(45, 28); type("Whiterun"); frame();
  Expect(!GImGui->OpenPopupStack.empty() && GImGui->OpenPopupStack.back().Window,
         "filtered suggestion popup exists");
  const auto popupPos = GImGui->OpenPopupStack.back().Window->Pos;
  click(popupPos.x + 30, popupPos.y + ImGui::GetStyle().WindowPadding.y + 5);
  click(45, 460); frame();
  Expect(value == "WhiterunInterior", "first click commits filtered suggestion");
  // Known fixed enums remain selection-only and preserve their existing value.
  custom = false; options = {"Male", "Female"}; value = "Male"; frame();
  click(45, 28); type("not_an_enum"); click(45, 460); frame();
  Expect(value == "Male", "fixed enum behavior unchanged");
  referenceMode = true; value = "Existing NPC [00000014]"; frame();
  click(45, 28); type("0x00012345");
  Expect(referenceSelection == -1 && value == "0x00012345",
         "reference typing clears stale selection instead of restoring Player");
  click(45, 460); frame();
  Expect(value == "0x00012345", "reference custom input survives blur");
  textMode = true; value.clear(); frame(); frame();
  click(45, 28); type("iState");
  Expect(value == "iState" && changed, "kChar name accepts alphabetic text");
  click(45, 460); frame();
  Expect(value == "iState", "kChar survives focus loss");
  value = "0012"; frame(); click(45, 28);
  io.AddKeyEvent(ImGuiMod_Ctrl, true); io.AddKeyEvent(ImGuiKey_A, true); frame();
  io.AddKeyEvent(ImGuiKey_A, false); io.AddKeyEvent(ImGuiMod_Ctrl, false); frame();
  const std::string longName = "Variable_" + std::string(4096, 'x');
  type(longName.c_str());
  Expect(value == longName, "kChar paste grows beyond initial buffer without truncation");
  click(45, 460); frame();
  Expect(value == longName, "long kChar survives blur");
  value = "0012"; frame(); click(45, 28); click(45, 460); frame();
  Expect(value == "0012", "numeric-looking variable name is not normalized");
  click(45, 28);
  io.AddKeyEvent(ImGuiMod_Ctrl, true); io.AddKeyEvent(ImGuiKey_A, true); frame();
  io.AddKeyEvent(ImGuiKey_A, false); io.AddKeyEvent(ImGuiMod_Ctrl, false); frame();
  io.AddKeyEvent(ImGuiKey_Backspace, true); frame();
  io.AddKeyEvent(ImGuiKey_Backspace, false); frame();
  Expect(value.empty(), "deleting a text argument updates string length to zero");
  type("::MyProperty_var"); click(45, 460); frame();
  Expect(value == "::MyProperty_var", "punctuation in variable identifiers stays intact");
  ImGui::DestroyContext();
  std::cout << "Condition dropdown real-ImGui input/focus tests passed\n";
}
