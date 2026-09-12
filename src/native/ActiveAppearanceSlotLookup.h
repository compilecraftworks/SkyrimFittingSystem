#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sfs::native::rules {
// Index the first winner for each Skyrim biped slot (30..61). Preserve
// collection order, including an invalid first entry: callers must not fall
// through to a lower-priority appearance when its identity cannot be resolved.
// The input is one actor-local snapshot, never a persistent condition cache.
template <class Entries>
[[nodiscard]] std::array<std::size_t, 32>
BuildFirstAppearanceSlotLookup(const Entries &a_entries) {
  std::array<std::size_t, 32> winners;
  winners.fill(a_entries.size());
  for (std::size_t index = 0; index < a_entries.size(); ++index) {
    const auto mask = a_entries[index].slotMask;
    for (std::size_t bit = 0; bit < winners.size(); ++bit) {
      if (winners[bit] == a_entries.size() &&
          (mask & (std::uint32_t{1} << bit)) != 0) {
        winners[bit] = index;
      }
    }
  }
  return winners;
}
} // namespace sfs::native::rules
