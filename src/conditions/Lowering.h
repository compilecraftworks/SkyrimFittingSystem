#pragma once

#include "conditions/Definition.h"

#include <RE/Skyrim.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sfs::conditions {
// Uses the exact lowering parser, including the expected form type. Called
// once on save rather than doing expensive EditorID fallbacks every UI frame.
[[nodiscard]] RE::TESForm *ResolveConditionFormArgument(
    const std::string &a_text, RE::SCRIPT_PARAM_TYPE a_type);
using DisplayOrClause = std::vector<std::string>;
using DisplayCnf = std::vector<DisplayOrClause>;

struct LoweredMaterialization {
  std::shared_ptr<RE::TESCondition> condition;
  std::string signature;
  DisplayCnf displayCnf;
};

[[nodiscard]] std::optional<LoweredMaterialization>
LowerAndEmitCondition(const Definition &a_definition,
                      const std::vector<Definition> &a_conditions);
} // namespace sfs::conditions
