#pragma once

#include "../../extras/SkyrimFittingSystemRenderedOutfitAPI.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace sfs::native::ied::detail {
// Verified against stable ied-dev 3f014c3e8574ef0e88b2ec0b7cdf58b86c9737b0
// and both 1.7.4 distributions recorded in IedConditionBinary.h and
// docs/DEVELOPMENT-v1.6.5-IED-AND-UI.md.
// This is a read-only synthetic input to match_biped, NOT an engine BipedAnim.
// It never enters inventory/skinning code or owns smart pointers. The only
// fields the verified helpers read on this route are initialized below.
inline constexpr std::size_t kFlags = 0x8, kBipedSlot = 0x44;
inline constexpr std::uint32_t kMatchSkin = 1u << 12;
inline constexpr std::uint32_t kMatchVisible = 1u << 17, kNegateVisible = 1u << 16;

template<class T> T Read(const void* data, std::size_t offset) {
  T value;
  std::memcpy(&value, static_cast<const std::byte*>(data) + offset, sizeof(T));
  return value;
}
template<class T> void Write(void* data, std::size_t offset, const T& value) {
  std::memcpy(static_cast<std::byte*>(data) + offset, &value, sizeof(T));
}

// nullopt means the original IED route still owns this condition. ARMO slots
// 30..61 use the final displayed set (including managed-empty). Weapon, quiver,
// race-specific sentinel slots and explicit skin queries are not outfit slots.
// Form/keyword/bolt predicates and the node match side effect stay in IED's
// ORIGINAL helper; only its local cached biped input is substituted.
template<class ResolveArmor, class Original>
std::optional<bool> MatchBiped(std::span<const rendered_outfit_api::Item> items,
    const void* condition, ResolveArmor&& resolveArmor, Original&& original) {
  const auto slot = Read<std::uint32_t>(condition, kBipedSlot);
  auto flags = Read<std::uint32_t>(condition, kFlags);
  if (slot >= 32 || (flags & kMatchSkin)) { return std::nullopt; }
  // Every candidate belongs to the final *displayed* set. Invisible equipment
  // cannot satisfy a negative Visible test by leaking back from worn inventory.
  if ((flags & kMatchVisible) && (flags & kNegateVisible)) { return false; }
  flags &= ~kMatchVisible;
  alignas(8) std::array<std::byte, 0x48> match;
  std::memcpy(match.data(), condition, match.size());
  Write(match.data(), kFlags, flags);
  alignas(8) std::array<std::byte, 0x70> params{};
  // No zero-fill of the entire 5 KB buffer per condition: only this slot is read.
  alignas(8) std::array<std::byte, 0x13C0> biped;
  Write(params.data(), 0x60, biped.data());
  Write(params.data(), 0x68, std::uint8_t{1});
  const auto entry = 0x10 + slot * 0x78;
  Write(biped.data(), entry + 0x08, static_cast<void*>(nullptr));
  Write(biped.data(), entry + 0x20, static_cast<void*>(nullptr));
  for (const auto& item : items) {
    if (!(item.visibleSlots & (1u << slot))) { continue; }
    if (auto* armor = resolveArmor(item.formID)) {
      Write(biped.data(), entry, static_cast<const void*>(armor));
      if (original(params.data(), match.data())) { return true; }
    }
  }
  return false;
}
} // namespace sfs::native::ied::detail
