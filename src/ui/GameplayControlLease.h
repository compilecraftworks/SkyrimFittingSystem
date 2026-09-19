#pragma once

#include <cstdint>
#include <utility>

namespace sfs::ui {
// Own only the bits SFS actually disabled. Never replace the complete control
// state: other menus/mods may change unrelated bits while SFS is open.
class GameplayControlLease {
public:
  std::uint32_t Acquire(std::uint32_t enabled, std::uint32_t blocked) {
    if (active_) { return 0; }
    active_ = true;
    return owned_ = enabled & blocked;
  }

  std::uint32_t Release(std::uint32_t stored, std::uint32_t invalidStored) {
    active_ = false;
    const auto owned = std::exchange(owned_, 0);
    // Respect the engine's current saved controls when present, including
    // stored-state disables issued after the menu opened. Do not modify them.
    return stored == invalidStored ? owned : owned & stored;
  }

private:
  std::uint32_t owned_{0};
  bool active_{false};
};
} // namespace sfs::ui
