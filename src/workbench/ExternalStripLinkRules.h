#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace sfs::workbench {

enum class ExternalModStripLinkMode : std::uint8_t {
  Disabled = 0,
  ModSettingsSlots = 1,
  VanillaSlots = 2,
  Custom = 3,
  // Internal migration/custom-editor base. It is not a fifth public option.
  DirectSlots = 4
};

inline constexpr std::uint32_t kAutomaticEquipmentFirstSlot = 30;
inline constexpr std::uint32_t kAutomaticEquipmentLastSlot = 61;
inline constexpr std::size_t kAutomaticEquipmentSlotCount =
    kAutomaticEquipmentLastSlot - kAutomaticEquipmentFirstSlot + 1;
inline constexpr std::array<std::uint32_t, 11>
    kAutomaticEquipmentVanillaAnchorSlots{30, 31, 32, 33, 34, 35,
                                          36, 37, 38, 39, 42};
using AutomaticEquipmentSlotMappings =
    std::array<std::uint8_t, kAutomaticEquipmentSlotCount>;
using AutomaticEquipmentSlotOverrides =
    std::array<bool, kAutomaticEquipmentSlotCount>;

inline constexpr std::size_t kMaximumVanillaAnchorCandidates = 3;

struct VanillaAnchorPriority {
  std::array<std::uint64_t, kMaximumVanillaAnchorCandidates> slotMasks{};
  std::size_t count{0};

  [[nodiscard]] bool empty() const noexcept { return count == 0; }
};

struct ExternalStripLinkPolicyState {
  ExternalModStripLinkMode configuredMode{
      ExternalModStripLinkMode::ModSettingsSlots};
  ExternalModStripLinkMode customBaseMode{
      ExternalModStripLinkMode::ModSettingsSlots};
  ExternalModStripLinkMode directAutomaticBaseMode{
      ExternalModStripLinkMode::ModSettingsSlots};
  AutomaticEquipmentSlotMappings directMappings{};
  AutomaticEquipmentSlotOverrides directOverrides{};
  std::uint64_t disabledAppearanceSlotMask{0};
  std::uint64_t protectedAppearanceSlotMask{0};
};

[[nodiscard]] inline constexpr std::uint64_t
EquipmentSlotMask(const std::uint32_t a_slotNumber) noexcept {
  return a_slotNumber >= kAutomaticEquipmentFirstSlot &&
                 a_slotNumber <= kAutomaticEquipmentLastSlot
             ? std::uint64_t{1}
                   << (a_slotNumber - kAutomaticEquipmentFirstSlot)
             : 0;
}

[[nodiscard]] inline constexpr ExternalModStripLinkMode
SanitizeExternalStripLinkMode(const ExternalModStripLinkMode a_mode) noexcept {
  if (a_mode == ExternalModStripLinkMode::DirectSlots) {
    return ExternalModStripLinkMode::Custom;
  }
  return a_mode <= ExternalModStripLinkMode::Custom
             ? a_mode
             : ExternalModStripLinkMode::Disabled;
}

[[nodiscard]] inline constexpr ExternalModStripLinkMode
SanitizeCustomStripLinkBaseMode(
    const ExternalModStripLinkMode a_mode) noexcept {
  if (a_mode == ExternalModStripLinkMode::ModSettingsSlots ||
      a_mode == ExternalModStripLinkMode::DirectSlots) {
    return a_mode;
  }
  return ExternalModStripLinkMode::VanillaSlots;
}

[[nodiscard]] inline constexpr ExternalModStripLinkMode
SanitizeDirectAutomaticBaseMode(
    const ExternalModStripLinkMode a_mode) noexcept {
  return a_mode == ExternalModStripLinkMode::VanillaSlots
             ? ExternalModStripLinkMode::VanillaSlots
             : ExternalModStripLinkMode::ModSettingsSlots;
}

[[nodiscard]] inline constexpr ExternalModStripLinkMode
ResolveEffectiveStripLinkMode(
    const ExternalStripLinkPolicyState &a_policy) noexcept {
  return a_policy.configuredMode == ExternalModStripLinkMode::Custom
             ? a_policy.customBaseMode
             : a_policy.configuredMode;
}

[[nodiscard]] inline constexpr bool IsModSettingsPolicyActive(
    const ExternalStripLinkPolicyState &a_policy) noexcept {
  const auto mode = ResolveEffectiveStripLinkMode(a_policy);
  return mode == ExternalModStripLinkMode::ModSettingsSlots ||
         (mode == ExternalModStripLinkMode::DirectSlots &&
          a_policy.directAutomaticBaseMode ==
              ExternalModStripLinkMode::ModSettingsSlots);
}

[[nodiscard]] inline constexpr bool IsActualEquipmentPolicyActive(
    const ExternalStripLinkPolicyState &a_policy) noexcept {
  const auto mode = ResolveEffectiveStripLinkMode(a_policy);
  return mode == ExternalModStripLinkMode::VanillaSlots ||
         (mode == ExternalModStripLinkMode::DirectSlots &&
          a_policy.directAutomaticBaseMode ==
              ExternalModStripLinkMode::VanillaSlots);
}

[[nodiscard]] inline constexpr bool IsStripLinkedAppearanceEnabled(
    const ExternalStripLinkPolicyState &a_policy,
    const std::uint64_t a_appearanceSlotMask,
    const bool a_isProtected) noexcept {
  if (a_isProtected) {
    return false;
  }
  if (a_policy.configuredMode != ExternalModStripLinkMode::Custom ||
      a_policy.customBaseMode == ExternalModStripLinkMode::DirectSlots) {
    return true;
  }
  return a_appearanceSlotMask == 0 ||
         (a_appearanceSlotMask & a_policy.disabledAppearanceSlotMask) == 0;
}

[[nodiscard]] inline constexpr bool
IsVanillaAnchorSlot(const std::uint32_t a_slotNumber) noexcept {
  for (const auto slot : kAutomaticEquipmentVanillaAnchorSlots) {
    if (slot == a_slotNumber) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline constexpr std::optional<std::uint64_t>
ResolveDirectSlotMask(const ExternalStripLinkPolicyState &a_policy,
                      const std::uint64_t a_appearanceSlotMask,
                      const bool a_tokenPipeline) noexcept {
  if (a_policy.configuredMode != ExternalModStripLinkMode::Custom ||
      (a_tokenPipeline &&
       a_policy.directAutomaticBaseMode !=
           ExternalModStripLinkMode::ModSettingsSlots)) {
    return std::nullopt;
  }

  for (std::uint32_t appearanceSlot = kAutomaticEquipmentFirstSlot;
       appearanceSlot <= kAutomaticEquipmentLastSlot; ++appearanceSlot) {
    if ((a_appearanceSlotMask & EquipmentSlotMask(appearanceSlot)) == 0) {
      continue;
    }
    const auto index = appearanceSlot - kAutomaticEquipmentFirstSlot;
    if (!a_policy.directOverrides[index] &&
        a_policy.customBaseMode != ExternalModStripLinkMode::DirectSlots) {
      continue;
    }
    const auto mappedSlot =
        a_policy.directOverrides[index]
            ? a_policy.directMappings[index]
            : static_cast<std::uint8_t>(appearanceSlot);
    if (mappedSlot < kAutomaticEquipmentFirstSlot ||
        mappedSlot > kAutomaticEquipmentLastSlot) {
      return std::uint64_t{0};
    }
    if (!a_tokenPipeline &&
        a_policy.customBaseMode != ExternalModStripLinkMode::DirectSlots &&
        a_policy.directAutomaticBaseMode ==
            ExternalModStripLinkMode::VanillaSlots &&
        !IsVanillaAnchorSlot(mappedSlot)) {
      return std::uint64_t{0};
    }
    const auto result = EquipmentSlotMask(mappedSlot);
    return (result & a_policy.protectedAppearanceSlotMask) == 0
               ? std::optional<std::uint64_t>{result}
               : std::optional<std::uint64_t>{0};
  }
  return std::nullopt;
}

[[nodiscard]] inline constexpr std::uint64_t ResolveVanillaAnchorSlotMask(
    const VanillaAnchorPriority &a_priority,
    const std::uint64_t a_protectedSlotMask,
    const std::uint64_t a_suppressedSlotMask,
    const std::uint64_t a_wornSlotMask) noexcept {
  std::uint64_t firstEligible = 0;
  for (std::size_t index = 0; index < a_priority.count; ++index) {
    const auto candidate = a_priority.slotMasks[index];
    if ((candidate & a_protectedSlotMask) == 0) {
      firstEligible = candidate;
      break;
    }
  }
  if (firstEligible == 0) {
    return 0;
  }
  for (std::size_t index = 0; index < a_priority.count; ++index) {
    const auto candidate = a_priority.slotMasks[index];
    if ((candidate & a_protectedSlotMask) == 0 &&
        (candidate & a_suppressedSlotMask) != 0) {
      return candidate;
    }
  }
  for (std::size_t index = 0; index < a_priority.count; ++index) {
    const auto candidate = a_priority.slotMasks[index];
    if ((candidate & a_protectedSlotMask) == 0 &&
        (candidate & a_wornSlotMask) != 0) {
      return candidate;
    }
  }
  return firstEligible;
}

} // namespace sfs::workbench
