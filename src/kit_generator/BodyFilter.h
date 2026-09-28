#pragma once

#include "Generator.h"
#include "ui/catalog/BodyFamilyFilter.h"

#include <algorithm>

namespace sfs::kit_generator {
// ESP membership is any matching armor, unlike a kit which is one combined
// appearance. Keep the Vanilla choice for mixed plugins containing both.
inline unsigned PluginBodyFilters(const std::vector<ArmorRecord>& armors) {
  unsigned result = 1U; // All also includes unclassified/model-less records.
  for (const auto& armor : armors) {
    for (const auto filter : ui::catalog::kBodyFamilyFilters) {
      if (ui::catalog::MatchesBodyFamilyFilter(armor.bodyFamilyMask, filter))
        result |= 1U << static_cast<unsigned>(filter);
    }
  }
  return result;
}

inline bool MatchesKitBodyFilter(const GeneratedKit& kit,
                                 ui::catalog::BodyFamilyFilter filter) {
  if (filter == ui::catalog::BodyFamilyFilter::All) return true;
  body_family::Mask families = 0;
  if (!kit.candidates.empty()) {
    const auto selected = (std::min)(kit.selectedCandidate, kit.candidates.size() - 1);
    for (const auto& armor : kit.candidates[selected].items)
      families = body_family::MergeCatalogMasks(families, armor.bodyFamilyMask);
  }
  return ui::catalog::MatchesBodyFamilyFilter(families, filter);
}
} // namespace sfs::kit_generator
