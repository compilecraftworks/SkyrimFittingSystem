#include "ConditionMaterializer.h"

#include "conditions/GraphMetadata.h"
#include "conditions/Lowering.h"
#include "conditions/MaterializationState.h"
#include "conditions/Status.h"

#include <mutex>
#include <unordered_set>
#include <utility>

namespace {
using ConditionGraphMap =
    std::unordered_map<std::string, sfs::conditions::GraphMetadata>;
using ConditionRuntimeMap =
    std::unordered_map<std::string, sfs::conditions::MaterializationState>;

std::recursive_mutex g_conditionMaterializationMutex;

ConditionGraphMap &GetConditionGraphMap() {
  static ConditionGraphMap graph;
  return graph;
}

ConditionRuntimeMap &GetConditionRuntimeMap() {
  static ConditionRuntimeMap runtime;
  return runtime;
}
} // namespace

namespace sfs::conditions {
void RebuildConditionDependencyMetadata(std::vector<Definition> &a_conditions) {
  std::lock_guard lock(g_conditionMaterializationMutex);
  ConditionGraphMap rebuiltGraph;
  rebuiltGraph.reserve(a_conditions.size());

  for (auto &condition : a_conditions) {
    auto &metadata = rebuiltGraph[condition.id];
    std::unordered_set<std::string> seenReferencedConditionIds;
    for (const auto &clause : condition.clauses) {
      if (clause.customConditionId.empty()) {
        continue;
      }
      if (seenReferencedConditionIds.insert(clause.customConditionId).second) {
        metadata.referencedConditionIds.push_back(clause.customConditionId);
      }
    }
  }

  for (const auto &condition : a_conditions) {
    const auto graphIt = rebuiltGraph.find(condition.id);
    if (graphIt == rebuiltGraph.end()) {
      continue;
    }
    for (const auto &referencedConditionId :
         graphIt->second.referencedConditionIds) {
      if (auto referencedIt = rebuiltGraph.find(referencedConditionId);
          referencedIt != rebuiltGraph.end()) {
        referencedIt->second.reverseDependencyIds.push_back(condition.id);
      }
    }
  }

  auto &runtime = GetConditionRuntimeMap();
  for (auto it = runtime.begin(); it != runtime.end();) {
    if (!rebuiltGraph.contains(it->first)) {
      it = runtime.erase(it);
    } else {
      ++it;
    }
  }
  PruneConditionStatusCache(a_conditions);

  GetConditionGraphMap() = std::move(rebuiltGraph);
}

void InvalidateConditionMaterializationCaches(
    std::vector<Definition> &a_conditions) {
  std::lock_guard lock(g_conditionMaterializationMutex);
  (void)a_conditions;
  GetConditionRuntimeMap().clear();
  ClearConditionStatusCache();
}

void InvalidateConditionMaterializationCachesFrom(
    std::vector<Definition> &a_conditions,
    const std::string_view a_conditionId) {
  std::lock_guard lock(g_conditionMaterializationMutex);
  (void)a_conditions;
  std::vector<std::string> stack;
  std::unordered_set<std::string> visited;
  stack.emplace_back(a_conditionId);
  auto &graph = GetConditionGraphMap();
  auto &runtime = GetConditionRuntimeMap();

  while (!stack.empty()) {
    auto conditionId = std::move(stack.back());
    stack.pop_back();

    if (!visited.insert(conditionId).second) {
      continue;
    }

    runtime.erase(conditionId);
    EraseConditionStatusCache(conditionId);

    const auto graphIt = graph.find(conditionId);
    if (graphIt == graph.end()) {
      continue;
    }

    for (const auto &reverseDependencyId :
         graphIt->second.reverseDependencyIds) {
      stack.push_back(reverseDependencyId);
    }
  }
}

namespace {
// Caller holds g_conditionMaterializationMutex until the owning result has
// been copied. Never expose a runtime-map entry beyond that lock.
MaterializationState *EnsureMaterializedCondition(
    std::string_view a_conditionId, std::vector<Definition> &a_conditions) {
  auto *definition = FindDefinitionById(a_conditions, a_conditionId);
  if (!definition) {
    return nullptr;
  }

  auto &runtime = GetConditionRuntimeMap();
  if (auto runtimeIt = runtime.find(definition->id);
      runtimeIt != runtime.end() && runtimeIt->second.attempted) {
    if (!runtimeIt->second.valid || !runtimeIt->second.condition) {
      return nullptr;
    }
    return &runtimeIt->second;
  }

  const auto lowered = LowerAndEmitCondition(*definition, a_conditions);
  if (!lowered) {
    logger::warn("Failed to materialize SFS condition {}", definition->id);
    runtime[definition->id].MarkFailure();
    return nullptr;
  }

  const auto refreshTargets = BuildRefreshTargets(lowered->condition);
  auto &runtimeState = runtime[definition->id];
  runtimeState.attempted = true;
  runtimeState.valid = true;
  runtimeState.condition = lowered->condition;
  runtimeState.signature = lowered->signature;
  runtimeState.displayCnf = lowered->displayCnf;
  runtimeState.refreshActorFormIDs.assign(refreshTargets.actorFormIDs.begin(),
                                          refreshTargets.actorFormIDs.end());
  runtimeState.refreshUseNearbyFallback = refreshTargets.useNearbyFallback;

  return &runtimeState;
}
} // namespace

std::optional<MaterializedCondition>
MaterializeConditionById(std::string_view a_conditionId,
                         std::vector<Definition> &a_conditions) {
  std::lock_guard lock(g_conditionMaterializationMutex);
  const auto *state = EnsureMaterializedCondition(a_conditionId, a_conditions);
  if (!state) {
    return std::nullopt;
  }
  return MaterializedCondition{
      .condition = state->condition,
      .signature = state->signature,
      .displayCnf = state->displayCnf,
      .refreshTargets = RefreshTargets{state->refreshActorFormIDs,
                                       state->refreshUseNearbyFallback}};
}

std::shared_ptr<RE::TESCondition>
AcquireExecutableConditionById(std::string_view a_conditionId,
                               std::vector<Definition> &a_conditions) {
  std::lock_guard lock(g_conditionMaterializationMutex);
  const auto *state = EnsureMaterializedCondition(a_conditionId, a_conditions);
  return state ? state->condition : nullptr;
}
} // namespace sfs::conditions
