#pragma once

#include <cstdint>

namespace sfs::native::helmet_toggle::rules {

inline constexpr std::uint32_t kPlayerManagedActualSlotMask = 0x02005001;
inline constexpr std::uint32_t kNpcManagedActualSlotMask = 0x00005001;
inline constexpr std::uint32_t kManagedRegisteredAppearanceSlotMask =
    kPlayerManagedActualSlotMask;
inline constexpr std::uint32_t kHairSlotMask = 0x00000002;

[[nodiscard]] inline constexpr std::uint32_t ProjectActualArmorSlotMask(
    const std::uint32_t a_armorSlotMask, const bool a_player) noexcept {
  return a_armorSlotMask &
         (a_player ? kPlayerManagedActualSlotMask : kNpcManagedActualSlotMask);
}

[[nodiscard]] inline constexpr std::uint32_t ResolveObservedControllerMask(
    const std::uint32_t a_armorSlotMask,
    const std::uint32_t a_observedQuerySlotMask,
    const bool a_player) noexcept {
  // GetWornArmor's matching query is independent evidence of the occupied
  // logical slot. Keep it alongside the ARMO mask so a DAVE-resolved head
  // variant cannot collapse a 31+42 controller into its first surviving bit.
  const auto projected = ProjectActualArmorSlotMask(
      a_armorSlotMask | a_observedQuerySlotMask, a_player);
  if (projected != 0) {
    return projected;
  }

  // Keep v1.5.0's boundary: Hair (31) is not a controller by itself. HT2 may
  // find a real helmet through a slot-31 query, but its other actual ARMO/query
  // slots must identify which registered headgear slots follow that helmet.
  return 0;
}

// HT2 integration is independent of the user's general external-strip policy.
// Return only the intersecting controller bits, not the complete appearance
// mask: one matching bit hides the whole multi-slot card, while pure Hair and
// unrelated slots must remain outside the actor-wide suppression layer.
[[nodiscard]] inline constexpr std::uint32_t
ResolveRegisteredAppearanceSuppressionSlots(
    const std::uint32_t a_visualSlotMask,
    const std::uint32_t a_controllerSlotMask, const bool a_locked) noexcept {
  if (a_locked) {
    return 0;
  }
  return a_visualSlotMask & a_controllerSlotMask &
         kManagedRegisteredAppearanceSlotMask;
}

[[nodiscard]] inline constexpr std::uint32_t
ComputeActualHairSlotReleaseMask(
    const bool a_hidden, const std::uint32_t a_managedActualArmorSlotMask,
    const std::uint32_t a_displayedFittingSlotMask) noexcept {
  return a_hidden &&
                 (a_managedActualArmorSlotMask & kHairSlotMask) != 0 &&
                 (a_displayedFittingSlotMask & kHairSlotMask) == 0
             ? kHairSlotMask
             : 0;
}

[[nodiscard]] inline constexpr std::uint32_t ResolveSignalControllerMask(
    const bool a_hidden, const std::uint32_t a_observedControllerMask,
    const bool a_observedManagedHeadgear,
    const std::uint32_t a_lastVisibleControllerMask) noexcept {
  // HT2's animated hide path can publish its hidden signal after DAVE has
  // already removed the actual helmet from rendered worn-slot queries. Reuse
  // only this actor's last visible controller mask in that exact no-observation
  // case. A currently observed pure Hair item is authoritative and must not
  // inherit an older Circlet controller.
  if (!a_hidden || a_observedManagedHeadgear) {
    return a_observedControllerMask;
  }
  return a_lastVisibleControllerMask;
}

} // namespace sfs::native::helmet_toggle::rules
