#pragma once

#include <cstdint>

namespace sfs::ui {

enum class MenuCharacterSide : std::uint8_t {
  Disabled = 0,
  Left = 1,
  Right = 2,
};

// Owns only the temporary camera/player presentation used while the SFS menu
// is open. It never changes the player's world position and restores every
// field it touches when the menu closes.
class MenuCharacterPresentation {
public:
  static MenuCharacterPresentation *GetSingleton();

  void Apply(MenuCharacterSide a_side);
  void Restore();
  void UpdateRotationInteraction();

private:
  MenuCharacterPresentation() = default;

  struct State;
  State *state_{nullptr};
};

} // namespace sfs::ui
