#pragma once

// Minimal, verbatim ABI subset from Grid Inventory's public
// plugin/src/api/GridInventoryAPI.h.  SFS is only a receiver of the Costume
// state broadcast and does not implement Grid Inventory's provider interface.
// Keep the layout checks aligned with the upstream ABI v1 header.

#include <cstdint>

namespace GridInvAPI {
inline constexpr std::uint32_t kABIVersion = 1;
inline constexpr std::uint32_t kMsgCostumeState = 0x47494353; // 'GICS'

struct ItemKey {
  std::uint32_t owner;
  std::uint32_t base;
  std::uint16_t uid;
  std::uint16_t _pad0;
  std::uint32_t _pad1;
};
static_assert(sizeof(ItemKey) == 16,
              "Grid Inventory ItemKey ABI v1 size changed");

// The pieces pointer belongs to Grid Inventory and is valid only for the
// duration of the SKSE messaging callback.  Receivers must copy base FormIDs
// before the callback returns.
struct CostumeState {
  std::uint32_t structSize;
  std::uint32_t abiVersion;
  std::int32_t tab;
  std::uint32_t pieceCount;
  const ItemKey *pieces;
};
static_assert(sizeof(CostumeState) == 24,
              "Grid Inventory CostumeState ABI v1 size changed");
} // namespace GridInvAPI
