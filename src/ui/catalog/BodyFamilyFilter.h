#pragma once

#include "catalog/BodyFamily.h"

#include <array>

namespace sfs::ui::catalog {
// Persisted indices: keep the ordering stable when adding future choices.
enum class BodyFamilyFilter : int { All, Cbbe, Unp, Ube, Himbo, Sam, Vanilla };

inline constexpr std::array kBodyFamilyFilters{
    BodyFamilyFilter::All, BodyFamilyFilter::Cbbe, BodyFamilyFilter::Unp,
    BodyFamilyFilter::Ube, BodyFamilyFilter::Himbo, BodyFamilyFilter::Sam,
    BodyFamilyFilter::Vanilla};

[[nodiscard]] constexpr BodyFamilyFilter BodyFamilyFilterFromIndex(int a_index) {
  return a_index >= 0 && a_index < static_cast<int>(kBodyFamilyFilters.size())
             ? kBodyFamilyFilters[static_cast<std::size_t>(a_index)]
             : BodyFamilyFilter::All;
}

[[nodiscard]] constexpr bool MatchesBodyFamilyFilter(
    body_family::Mask a_families, BodyFamilyFilter a_filter) {
  using body_family::Bit;
  using body_family::Family;
  switch (a_filter) {
  case BodyFamilyFilter::All:
    return true;
  case BodyFamilyFilter::Cbbe:
    return (a_families & Bit(Family::Cbbe)) != 0;
  case BodyFamilyFilter::Unp:
    return (a_families & Bit(Family::Unp)) != 0;
  case BodyFamilyFilter::Ube:
    return (a_families & Bit(Family::Ube)) != 0;
  case BodyFamilyFilter::Himbo:
    return (a_families & Bit(Family::Himbo)) != 0;
  case BodyFamilyFilter::Sam:
    return (a_families & Bit(Family::Sam)) != 0;
  case BodyFamilyFilter::Vanilla: {
    constexpr auto vanilla = Bit(Family::FemaleVanilla) | Bit(Family::MaleVanilla);
    // Generic pieces/models must not label a CBBE/UBE/etc. set as Vanilla.
    return (a_families & vanilla) != 0 &&
           (a_families & (body_family::kAllFamilies & ~vanilla)) == 0;
  }
  }
  return true;
}
} // namespace sfs::ui::catalog
