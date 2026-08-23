#pragma once

#include "conditions/Definition.h"

#include <cstdint>
#include <vector>

namespace sfs::conditions {
struct Store {
  int nextConditionId{1};
  bool samplesSeeded{false};
  std::vector<Definition> definitions;
  std::uint64_t revision{0};
};
} // namespace sfs::conditions
