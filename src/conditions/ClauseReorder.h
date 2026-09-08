#pragma once

#include "conditions/Definition.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace sfs::conditions::clause_reorder {
enum class Status : std::uint8_t {
  Applied,
  NoChange,
  InvalidTarget,
  StaleSource,
};

struct Payload {
  std::uint32_t sourceIndex{0};
  std::uint64_t sourceFingerprint{0};
};

namespace detail {
inline constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
inline constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

inline void HashByte(std::uint64_t &a_hash, const std::uint8_t a_value) {
  a_hash ^= a_value;
  a_hash *= kFnvPrime;
}

inline void HashString(std::uint64_t &a_hash, const std::string_view a_value) {
  for (const auto value : a_value) {
    HashByte(a_hash, static_cast<std::uint8_t>(value));
  }
  // Preserve field boundaries so concatenated values cannot alias trivially.
  HashByte(a_hash, 0xFF);
}
} // namespace detail

[[nodiscard]] inline std::uint64_t Fingerprint(const Clause &a_clause) {
  auto hash = detail::kFnvOffset;
  detail::HashString(hash, a_clause.functionName);
  detail::HashString(hash, a_clause.customConditionId);
  detail::HashString(hash, a_clause.arguments[0]);
  detail::HashString(hash, a_clause.arguments[1]);
  detail::HashByte(hash, static_cast<std::uint8_t>(a_clause.comparator));
  detail::HashString(hash, a_clause.comparand);
  detail::HashByte(hash, static_cast<std::uint8_t>(a_clause.connectiveToNext));
  return hash;
}

[[nodiscard]] inline Status ApplyMoveTransaction(
    std::vector<Clause> &a_clauses, const Payload &a_payload,
    std::size_t a_slotIndex) {
  const auto sourceIndex = static_cast<std::size_t>(a_payload.sourceIndex);
  if (sourceIndex >= a_clauses.size() || a_slotIndex > a_clauses.size()) {
    return Status::InvalidTarget;
  }
  if (a_payload.sourceFingerprint == 0 ||
      Fingerprint(a_clauses[sourceIndex]) != a_payload.sourceFingerprint) {
    return Status::StaleSource;
  }
  if (a_slotIndex == sourceIndex || a_slotIndex == sourceIndex + 1) {
    return Status::NoChange;
  }

  auto planned = a_clauses;
  auto clause = std::move(planned[sourceIndex]);
  planned.erase(planned.begin() + static_cast<std::ptrdiff_t>(sourceIndex));
  if (sourceIndex < a_slotIndex) {
    --a_slotIndex;
  }
  planned.insert(planned.begin() + static_cast<std::ptrdiff_t>(a_slotIndex),
                 std::move(clause));
  a_clauses = std::move(planned);
  return Status::Applied;
}
} // namespace sfs::conditions::clause_reorder
