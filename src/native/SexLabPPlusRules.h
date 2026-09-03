#pragma once

#include <cstdint>
#include <span>

namespace sfs::native::sexlab_pplus::rules {

// SexLab P+ Registry::Position::StripData.  These values are part of the
// v2.12.0 Papyrus/native contract and are identical in its SE 1.5.97 and AE
// 1.6.1170 packages.
inline constexpr std::uint8_t kStripHelmet = 1U << 0U;
inline constexpr std::uint8_t kStripGloves = 1U << 1U;
inline constexpr std::uint8_t kStripBoots = 1U << 2U;
inline constexpr std::uint8_t kStripDefault = 1U << 7U;
inline constexpr std::uint8_t kStripAll = 0xFFU;

[[nodiscard]] constexpr std::uint32_t EquipmentSlotMask(
    const std::uint32_t a_slot) noexcept {
  return a_slot >= 30U && a_slot <= 61U ? 1U << (a_slot - 30U) : 0U;
}

// Mirrors P+ StripByDataEx exactly: an explicit overwrite wins first, All is
// next, and the animation's Default/Boots/Gloves/Helmet bits are considered
// only when neither of those paths was selected.
[[nodiscard]] inline std::uint32_t ResolveStripSlotMask(
    const std::int32_t a_stripData,
    const std::span<const std::int32_t> a_defaults,
    const std::span<const std::int32_t> a_overwrites) noexcept {
  const auto stripData = static_cast<std::uint8_t>(a_stripData);
  if (stripData == 0U) {
    return 0U;
  }
  if (a_overwrites.size() >= 2U) {
    return static_cast<std::uint32_t>(a_overwrites[0]);
  }
  if (stripData == kStripAll) {
    return UINT32_MAX;
  }

  std::uint32_t result = 0U;
  if ((stripData & kStripDefault) != 0U && a_defaults.size() >= 2U) {
    result = static_cast<std::uint32_t>(a_defaults[0]);
  }
  if ((stripData & kStripBoots) != 0U) {
    result |= EquipmentSlotMask(37U);
  }
  if ((stripData & kStripGloves) != 0U) {
    result |= EquipmentSlotMask(33U);
  }
  if ((stripData & kStripHelmet) != 0U) {
    result |= EquipmentSlotMask(30U);
  }
  return result;
}

// P+ checks NoStrip before AlwaysStrip.  Keep the same precedence for a
// registered appearance's source ARMO, then apply the actor-local token mask.
[[nodiscard]] constexpr bool ShouldStripAppearance(
    const std::uint32_t a_requestedMask,
    const std::uint32_t a_appearanceTokenMask, const bool a_hasNoStrip,
    const bool a_hasAlwaysStrip) noexcept {
  if (a_hasNoStrip || a_appearanceTokenMask == 0U) {
    return false;
  }
  return a_hasAlwaysStrip ||
         (a_requestedMask & a_appearanceTokenMask) != 0U;
}

[[nodiscard]] constexpr std::uint32_t SelectRestoreTokenMask(
    const std::uint32_t a_requestedMask,
    const std::uint32_t a_appearanceTokenMask,
    const bool a_hasAlwaysStrip) noexcept {
  const auto candidates =
      a_hasAlwaysStrip ? a_appearanceTokenMask
                       : (a_requestedMask & a_appearanceTokenMask);
  return candidates & (0U - candidates);
}

} // namespace sfs::native::sexlab_pplus::rules
