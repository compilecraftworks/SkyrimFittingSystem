#pragma once

namespace sfs::native::genital_compatibility::rules {

struct Environment {
  bool sosInstalled{false};
  bool tngInstalled{false};
};

[[nodiscard]] constexpr bool IsSosCompatibilityAvailable(
    const Environment a_environment, const bool a_resolvedGenitalArmor,
    const bool a_equippedGenitalArmor,
    const bool a_playerKeywordFallback) noexcept {
  return a_environment.sosInstalled &&
         (a_resolvedGenitalArmor || a_equippedGenitalArmor ||
          a_playerKeywordFallback);
}

[[nodiscard]] constexpr bool IsTngCompatibilityAvailable(
    const Environment a_environment, const bool a_actorSkinUsesGenitalSlot,
    const bool a_tngCoverEquipped) noexcept {
  return a_environment.tngInstalled &&
         (a_actorSkinUsesGenitalSlot || a_tngCoverEquipped);
}

} // namespace sfs::native::genital_compatibility::rules
