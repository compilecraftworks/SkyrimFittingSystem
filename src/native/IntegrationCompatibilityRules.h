#pragma once

#include <cstdint>

namespace sfs::native::integration_rules {
// Consume the last known message prefix, including on newer ABI numbers.
// Size/bounds failures are malformed input, not an unknown-version policy.
[[nodiscard]] constexpr bool CanReadCostumePrefix(
    std::uint32_t a_dataLength, std::uint32_t a_structSize,
    std::uint32_t a_pieceCount, bool a_hasPieces) noexcept {
  return a_dataLength >= 24 && a_structSize >= 24 &&
         a_structSize <= a_dataLength && a_pieceCount <= 1024 &&
         (a_pieceCount == 0 || a_hasPieces);
}
} // namespace sfs::native::integration_rules
