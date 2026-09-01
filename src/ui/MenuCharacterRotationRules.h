#pragma once

namespace sfs::ui::character_rotation {

struct Plan {
  bool rotateActor{false};
  bool orbitCamera{false};
};

[[nodiscard]] inline constexpr Plan BuildPlan(const bool a_gamePaused) noexcept {
  // FSMP suspends physics while Skyrim is paused. Moving only the camera keeps
  // the actor root and its last simulated physics transforms in one space.
  return a_gamePaused ? Plan{.rotateActor = false, .orbitCamera = true}
                      : Plan{.rotateActor = true, .orbitCamera = true};
}

} // namespace sfs::ui::character_rotation
