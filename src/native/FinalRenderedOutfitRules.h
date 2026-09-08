#pragma once

#include <cstdint>
#include <optional>

namespace sfs::native::final_outfit::rules {

[[nodiscard]] inline constexpr bool IsVisuallyNakedForSlots(
    const std::uint32_t a_visibleActualSlotMask,
    const std::uint32_t a_visibleAdditionalSlotMask,
    const std::uint32_t a_requestedSlotMask) noexcept {
  return a_requestedSlotMask != 0 &&
         ((a_visibleActualSlotMask | a_visibleAdditionalSlotMask) &
          a_requestedSlotMask) == 0;
}

// nullopt means SFS does not own this actor's footwear decision. A present
// zero is intentionally different: SFS owns the final result and it is
// barefoot, so a consumer must not fall back to technically worn armor.
[[nodiscard]] inline constexpr std::optional<std::uint32_t>
ResolveDisplayedFootwear(const bool a_managedBySfs,
                         const std::uint32_t a_visibleAdditionalFormID,
                         const std::uint32_t a_visibleActualFormID) noexcept {
  if (!a_managedBySfs) {
    return std::nullopt;
  }
  return a_visibleAdditionalFormID != 0 ? a_visibleAdditionalFormID
                                       : a_visibleActualFormID;
}

} // namespace sfs::native::final_outfit::rules
