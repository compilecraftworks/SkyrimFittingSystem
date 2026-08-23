#include "conditions/Status.h"

#include "conditions/Validation.h"

#include <mutex>
#include <unordered_map>

namespace {
using ConditionStatusMap =
    std::unordered_map<std::string, sfs::conditions::DefinitionStatus>;

ConditionStatusMap &GetConditionStatusMap() {
  static ConditionStatusMap cache;
  return cache;
}
std::recursive_mutex g_conditionStatusMutex;
} // namespace

namespace sfs::conditions {
bool IsWorkbenchSelectable(const Definition &a_definition) {
  return a_definition.IsCatalog();
}

DefinitionStatus
EvaluateDefinitionStatus(const Definition &a_definition,
                         const std::vector<Definition> &a_conditions) {
  std::lock_guard lock(g_conditionStatusMutex);
  auto &cache = GetConditionStatusMap();
  if (!a_definition.id.empty()) {
    if (const auto it = cache.find(a_definition.id); it != cache.end()) {
      return it->second;
    }
  }

  DefinitionStatus status;
  status.missingDependencyChains =
      CollectMissingDependencyChains(a_definition, a_conditions);
  if (!status.missingDependencyChains.empty()) {
    status.availability = DefinitionAvailability::Broken;
  }

  if (!a_definition.id.empty()) {
    cache.insert_or_assign(a_definition.id, status);
  }
  return status;
}

void PruneConditionStatusCache(const std::vector<Definition> &a_conditions) {
  std::lock_guard lock(g_conditionStatusMutex);
  auto &cache = GetConditionStatusMap();
  for (auto it = cache.begin(); it != cache.end();) {
    if (FindDefinitionById(a_conditions, it->first) == nullptr) {
      it = cache.erase(it);
    } else {
      ++it;
    }
  }
}

void ClearConditionStatusCache() {
  std::lock_guard lock(g_conditionStatusMutex);
  GetConditionStatusMap().clear();
}

void EraseConditionStatusCache(const std::string_view a_conditionId) {
  std::lock_guard lock(g_conditionStatusMutex);
  GetConditionStatusMap().erase(std::string(a_conditionId));
}
} // namespace sfs::conditions
