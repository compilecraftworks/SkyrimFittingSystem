#pragma once

#include <cstdint>

namespace RE {
class Actor;
}

namespace sfs::ui {

enum class MenuCharacterSide : std::uint8_t {
  Disabled = 0,
  Left = 1,
  Right = 2,
};

// Owns only the temporary camera/actor presentation used while the SFS menu is
// open. It never changes an actor's world position and restores every field it
// touches when the actor selection changes or the menu closes.
class MenuCharacterPresentation {
public:
  static MenuCharacterPresentation *GetSingleton();

  void Apply(MenuCharacterSide a_side);
  void Apply(MenuCharacterSide a_side, RE::Actor *a_actor);
  void Restore();
  void UpdateRotationInteraction();

private:
  MenuCharacterPresentation() = default;

  struct State;
  State *state_{nullptr};
};

} // namespace sfs::ui
