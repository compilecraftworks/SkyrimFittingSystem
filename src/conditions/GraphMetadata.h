#pragma once

#include <string>
#include <vector>

namespace sfs::conditions {
struct GraphMetadata {
  std::vector<std::string> referencedConditionIds;
  std::vector<std::string> reverseDependencyIds;

  void Clear() {
    referencedConditionIds.clear();
    reverseDependencyIds.clear();
  }
};
} // namespace sfs::conditions
