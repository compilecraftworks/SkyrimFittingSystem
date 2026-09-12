#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sfs::native::ied::detail {
struct BinaryLayout {
  std::uint32_t timestamp, imageSize, equipped, node;
  std::uint64_t equippedHash, nodeHash;
  // Four equipment dispatcher variants and the node-override dispatcher.
  std::array<std::uint32_t, 5> sites;
};
// Stable IED 1.7.4, immutable upstream commit in the adapter/audit document.
// Whole helper fingerprints (including jump tables), PE identity and each
// original call target are checked BEFORE any call site is changed.
inline constexpr std::array kLayouts{
    BinaryLayout{0x6575B68A, 0x582000, 0x2A130, 0x144540,
        0x9529915cffb8463cull, 0x5d241ce281d39310ull,
        {0x28D8B, 0x295B2, 0x2C5D3, 0x2D187, 0x142AF1}},
    BinaryLayout{0x6575B58C, 0x581000, 0x2A1A0, 0x1445C0,
        0xcfa9a1fe416addfeull, 0x66be920daf22b587ull,
        {0x28DFB, 0x29622, 0x2C643, 0x2D1F7, 0x142B71}}};
inline std::uint64_t Fingerprint(std::span<const std::byte> bytes) {
  std::uint64_t result = 14695981039346656037ull;
  for (const auto byte : bytes) {
    result = (result ^ std::to_integer<std::uint8_t>(byte)) * 1099511628211ull;
  }
  return result;
}
inline bool Validate(std::span<const std::byte> image, const BinaryLayout& layout) {
  if (image.size() < layout.imageSize) { return false; }
  if (Fingerprint(image.subspan(layout.equipped, 727)) != layout.equippedHash ||
      Fingerprint(image.subspan(layout.node, 903)) != layout.nodeHash) { return false; }
  for (std::size_t i = 0; i < layout.sites.size(); ++i) {
    const auto site = layout.sites[i];
    if (image[site] != std::byte{i == 0 ? std::uint8_t{0xE9} : std::uint8_t{0xE8}}) {
      return false;
    }
    std::int32_t displacement;
    std::memcpy(&displacement, image.data() + site + 1, 4);
    if (static_cast<std::int64_t>(site) + 5 + displacement !=
        (i == 4 ? layout.node : layout.equipped)) { return false; }
  }
  return true;
}
} // namespace sfs::native::ied::detail
