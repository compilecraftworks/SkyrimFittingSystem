#pragma once

#include "conditions/Definition.h"

#include <string>
#include <vector>

namespace sfs::conditions {
[[nodiscard]] Clause BuildDefaultPlayerClause();
[[nodiscard]] Definition BuildDefaultPlayerCondition();
[[nodiscard]] std::vector<Definition> BuildBuiltInConditions();
[[nodiscard]] std::vector<Definition> BuildSampleConditions();
[[nodiscard]] bool IsBuiltInCondition(std::string_view a_id);
[[nodiscard]] bool IsActorOwnershipCondition(const Definition &a_definition);
[[nodiscard]] std::string BuildConditionId(int a_nextConditionId);
} // namespace sfs::conditions
