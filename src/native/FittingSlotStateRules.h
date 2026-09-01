#pragma once

#include <cstdint>

namespace sfs::native::fitting_slot_rules {

struct ActorFittingSlotState {
  std::uint32_t virtualTokenSuppressedSlotMask{0};
  std::uint32_t headgearToggleSuppressedSlotMask{0};
  std::uint32_t headgearToggleManualVisibleSlotMask{0};
};

[[nodiscard]] inline constexpr bool
IsEmpty(const ActorFittingSlotState &a_state) noexcept {
  return a_state.virtualTokenSuppressedSlotMask == 0 &&
         a_state.headgearToggleSuppressedSlotMask == 0 &&
         a_state.headgearToggleManualVisibleSlotMask == 0;
}

[[nodiscard]] inline constexpr std::uint32_t
GetEffectiveHeadgearSuppressedMask(
    const ActorFittingSlotState &a_state) noexcept {
  return a_state.headgearToggleSuppressedSlotMask &
         ~a_state.headgearToggleManualVisibleSlotMask;
}

inline constexpr void SetVirtualTokenSuppressed(
    ActorFittingSlotState &a_state, const std::uint32_t a_slotMask,
    const bool a_suppressed) noexcept {
  if (a_suppressed) {
    a_state.virtualTokenSuppressedSlotMask |= a_slotMask;
  } else {
    a_state.virtualTokenSuppressedSlotMask &= ~a_slotMask;
  }
}

inline constexpr void SetHeadgearSuppressed(
    ActorFittingSlotState &a_state, const std::uint32_t a_slotMask,
    const bool a_suppressed) noexcept {
  if (a_suppressed) {
    const auto newlySuppressed =
        a_slotMask & ~a_state.headgearToggleSuppressedSlotMask;
    a_state.headgearToggleSuppressedSlotMask |= a_slotMask;
    a_state.headgearToggleManualVisibleSlotMask &= ~newlySuppressed;
  } else {
    a_state.headgearToggleSuppressedSlotMask &= ~a_slotMask;
    a_state.headgearToggleManualVisibleSlotMask &= ~a_slotMask;
  }
}

[[nodiscard]] inline constexpr bool ReplaceHeadgearSuppressed(
    ActorFittingSlotState &a_state,
    const std::uint32_t a_slotMask) noexcept {
  const auto previous = GetEffectiveHeadgearSuppressedMask(a_state);
  const auto newlySuppressed =
      a_slotMask & ~a_state.headgearToggleSuppressedSlotMask;
  a_state.headgearToggleSuppressedSlotMask = a_slotMask;
  a_state.headgearToggleManualVisibleSlotMask &= a_slotMask;
  a_state.headgearToggleManualVisibleSlotMask &= ~newlySuppressed;
  return previous != GetEffectiveHeadgearSuppressedMask(a_state);
}

[[nodiscard]] inline constexpr bool SetHeadgearManualVisible(
    ActorFittingSlotState &a_state, const std::uint32_t a_slotMask,
    const bool a_visible) noexcept {
  const auto previous = GetEffectiveHeadgearSuppressedMask(a_state);
  if (a_visible) {
    a_state.headgearToggleManualVisibleSlotMask |=
        a_slotMask & a_state.headgearToggleSuppressedSlotMask;
  } else {
    a_state.headgearToggleManualVisibleSlotMask &= ~a_slotMask;
  }
  return previous != GetEffectiveHeadgearSuppressedMask(a_state);
}

} // namespace sfs::native::fitting_slot_rules
