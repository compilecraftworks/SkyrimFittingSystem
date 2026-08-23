#pragma once

#include <cstdint>

namespace sfs::workbench {
inline constexpr std::uint64_t kDefaultSpecialEffectProtectedSlotMask =
    std::uint64_t{0xC0300000}; // Slots 50, 51, 60, and 61.
inline constexpr std::uint64_t kShieldSlotMask =
    std::uint64_t{0x00000200}; // Slot 39.

[[nodiscard]] std::uint64_t GetSpecialEffectProtectedSlotMask();
void SetSpecialEffectProtectedSlotMask(std::uint64_t a_slotMask);
[[nodiscard]] bool IsShieldAppearanceSlotEnabled();
void SetShieldAppearanceSlotEnabled(bool a_enabled);
[[nodiscard]] std::uint64_t GetEffectiveAppearanceProtectedSlotMask();
[[nodiscard]] bool
IsAppearanceRegistrationProtectedSlotMask(std::uint64_t a_slotMask);
} // namespace sfs::workbench
