#include "workbench/AppearanceSlotProtection.h"

#include <atomic>

namespace sfs::workbench {
namespace {
std::atomic_uint64_t g_specialEffectProtectedSlotMask{
    kDefaultSpecialEffectProtectedSlotMask};
std::atomic_bool g_shieldAppearanceSlotEnabled{false};
}

std::uint64_t GetSpecialEffectProtectedSlotMask() {
  return g_specialEffectProtectedSlotMask.load(std::memory_order_relaxed);
}

void SetSpecialEffectProtectedSlotMask(const std::uint64_t a_slotMask) {
  // Slot 39 belongs exclusively to the dedicated shield-appearance option.
  // Never let an imported or legacy special-effect list mix both controls.
  g_specialEffectProtectedSlotMask.store(a_slotMask & ~kShieldSlotMask,
                                         std::memory_order_relaxed);
}

bool IsShieldAppearanceSlotEnabled() {
  return g_shieldAppearanceSlotEnabled.load(std::memory_order_relaxed);
}

void SetShieldAppearanceSlotEnabled(const bool a_enabled) {
  g_shieldAppearanceSlotEnabled.store(a_enabled, std::memory_order_relaxed);
}

std::uint64_t GetEffectiveAppearanceProtectedSlotMask() {
  auto result = GetSpecialEffectProtectedSlotMask();
  if (!IsShieldAppearanceSlotEnabled()) {
    result |= kShieldSlotMask;
  }
  return result;
}

bool IsAppearanceRegistrationProtectedSlotMask(
    const std::uint64_t a_slotMask) {
  return (a_slotMask & GetEffectiveAppearanceProtectedSlotMask()) != 0;
}
} // namespace sfs::workbench
