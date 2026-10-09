#pragma once

#include <cstdint>

namespace sfs::native::long_hair::rules {

inline constexpr std::uint32_t kLongHairSlotMask = std::uint32_t{1} << 11;
inline constexpr std::uint32_t kHairSlotMask = std::uint32_t{1} << 1;
inline constexpr std::uint32_t kHeadgearSlotMask =
    (std::uint32_t{1} << 0) | (std::uint32_t{1} << 12);

[[nodiscard]] inline constexpr bool IsOccludableRegisteredLongHair(
    const std::uint32_t a_visualSlotMask, const bool a_locked,
    const bool a_unrestrictedPreview = false) noexcept {
  // Only a standalone LongHair (41) card. Never split a multi-slot helmet,
  // reinterpret partial Hair (31), or override an explicit appearance lock.
  return a_visualSlotMask == kLongHairSlotMask && !a_locked &&
         !a_unrestrictedPreview;
}

[[nodiscard]] inline constexpr bool HeadgearOccludesLongHair(
    const std::uint32_t a_visualSlotMask) noexcept {
  // Evidence must belong to ONE armor. A circlet alongside a separate wig
  // must not accidentally become a hair-covering helmet through mask union.
  return (a_visualSlotMask & kHeadgearSlotMask) != 0 &&
         (a_visualSlotMask & (kHairSlotMask | kLongHairSlotMask)) != 0;
}

[[nodiscard]] inline constexpr bool LongHairDisplayChanged(
    const std::uint32_t a_before, const std::uint32_t a_after) noexcept {
  return ((a_before ^ a_after) & kLongHairSlotMask) != 0;
}

} // namespace sfs::native::long_hair::rules
