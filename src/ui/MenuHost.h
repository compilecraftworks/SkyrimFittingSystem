#pragma once

namespace sfs {
class MenuHost : public RE::IMenu {
public:
  static constexpr std::string_view MENU_NAME = "SkyrimFittingSystemMenu";

  static void RegisterMenu();

  // Fast SMP intentionally stops its simulation while a kPausesGame menu is
  // open.  A right-drag character rotation needs one live simulation frame at
  // a time, so temporarily release only this menu's pause contribution.  Both
  // calls are idempotent and must be paired before the menu is hidden.
  static bool BeginCharacterRotationUnpause();
  static void EndCharacterRotationUnpause();

  void PostDisplay() override;
  RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage &a_message) override;

private:
  static RE::IMenu *Creator();
  static void ForceCursor();
};
} // namespace sfs
