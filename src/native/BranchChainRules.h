#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>

namespace sfs::native::branch_chain::rules {
enum class TransferKind : std::uint8_t {
  DirectTarget,
  IndirectTargetSlot,
};

struct Transfer {
  TransferKind kind{TransferKind::DirectTarget};
  std::uintptr_t address{0};

  [[nodiscard]] bool operator==(const Transfer &) const = default;
};

namespace detail {
[[nodiscard]] inline std::uint32_t
ReadU32(const std::span<const std::uint8_t> a_bytes,
        const std::size_t a_offset) noexcept {
  std::uint32_t value = 0;
  std::memcpy(&value, a_bytes.data() + a_offset, sizeof(value));
  return value;
}

[[nodiscard]] inline std::uint64_t
ReadU64(const std::span<const std::uint8_t> a_bytes,
        const std::size_t a_offset) noexcept {
  std::uint64_t value = 0;
  std::memcpy(&value, a_bytes.data() + a_offset, sizeof(value));
  return value;
}

[[nodiscard]] inline std::optional<std::uintptr_t>
AddRelative(const std::uintptr_t a_instructionAddress,
            const std::size_t a_instructionLength,
            const std::int64_t a_displacement) noexcept {
  constexpr auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
  if (a_instructionAddress > maximum - a_instructionLength) {
    return std::nullopt;
  }
  const auto next = a_instructionAddress + a_instructionLength;
  if (a_displacement >= 0) {
    const auto positive = static_cast<std::uint64_t>(a_displacement);
    if (positive > maximum - next) {
      return std::nullopt;
    }
    return next + static_cast<std::uintptr_t>(positive);
  }

  const auto magnitude = static_cast<std::uint64_t>(-(a_displacement + 1)) + 1;
  if (magnitude > next) {
    return std::nullopt;
  }
  return next - static_cast<std::uintptr_t>(magnitude);
}
} // namespace detail

// Decodes only unconditional branch veneers used by SKSE/CommonLib and common
// x64 hook libraries. It deliberately does not follow calls or conditional
// branches: those are executable hook bodies, not transparent ownership
// trampolines.
[[nodiscard]] inline std::optional<Transfer>
DecodeTransfer(const std::uintptr_t a_instructionAddress,
               const std::span<const std::uint8_t> a_bytes) noexcept {
  if (a_bytes.size() >= 4 && a_bytes[0] == 0xF3 && a_bytes[1] == 0x0F &&
      a_bytes[2] == 0x1E && a_bytes[3] == 0xFA) {
    const auto nested = DecodeTransfer(a_instructionAddress + 4,
                                       a_bytes.subspan(4));
    return nested;
  }

  if (a_bytes.size() >= 5 && a_bytes[0] == 0xE9) {
    const auto displacement =
        std::bit_cast<std::int32_t>(detail::ReadU32(a_bytes, 1));
    const auto target = detail::AddRelative(a_instructionAddress, 5,
                                             displacement);
    return target ? std::optional<Transfer>{Transfer{
                        TransferKind::DirectTarget, *target}}
                  : std::nullopt;
  }

  if (a_bytes.size() >= 2 && a_bytes[0] == 0xEB) {
    const auto displacement = static_cast<std::int8_t>(a_bytes[1]);
    const auto target = detail::AddRelative(a_instructionAddress, 2,
                                             displacement);
    return target ? std::optional<Transfer>{Transfer{
                        TransferKind::DirectTarget, *target}}
                  : std::nullopt;
  }

  if (a_bytes.size() >= 6 && a_bytes[0] == 0xFF && a_bytes[1] == 0x25) {
    const auto displacement =
        std::bit_cast<std::int32_t>(detail::ReadU32(a_bytes, 2));
    const auto slot = detail::AddRelative(a_instructionAddress, 6,
                                           displacement);
    return slot ? std::optional<Transfer>{Transfer{
                      TransferKind::IndirectTargetSlot, *slot}}
                : std::nullopt;
  }

  if (a_bytes.size() >= 12 && a_bytes[0] == 0x48 && a_bytes[1] == 0xB8 &&
      a_bytes[10] == 0xFF && a_bytes[11] == 0xE0) {
    return Transfer{TransferKind::DirectTarget,
                    static_cast<std::uintptr_t>(detail::ReadU64(a_bytes, 2))};
  }

  if (a_bytes.size() >= 13 && a_bytes[0] == 0x49 && a_bytes[1] == 0xBB &&
      a_bytes[10] == 0x41 && a_bytes[11] == 0xFF && a_bytes[12] == 0xE3) {
    return Transfer{TransferKind::DirectTarget,
                    static_cast<std::uintptr_t>(detail::ReadU64(a_bytes, 2))};
  }

  return std::nullopt;
}
} // namespace sfs::native::branch_chain::rules
