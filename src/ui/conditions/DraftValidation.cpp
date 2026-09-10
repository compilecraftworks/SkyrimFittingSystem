#include "ui/conditions/DraftValidation.h"

#include "conditions/Validation.h"
#include "conditions/ValueParsing.h"
#include "ui/Localization.h"
#include "ui/conditions/FunctionRegistry.h"
#include "ui/conditions/ValueEditors.h"

#include <algorithm>
#include <format>

namespace {
using Clause = sfs::conditions::Clause;
using Comparator = sfs::conditions::Comparator;
using Definition = sfs::conditions::Definition;
using FunctionInfo = sfs::ui::condition_editor::FunctionInfo;
using ValueEditorKind = sfs::ui::condition_editor::ValueEditorKind;
} // namespace

namespace sfs::ui::condition_editor {
bool IsBooleanComparator(const Comparator a_comparator) {
  return a_comparator == Comparator::Equal ||
         a_comparator == Comparator::NotEqual;
}

std::string BuildSuggestedConditionName(
    const std::vector<Definition> &a_conditions, const int a_seed,
    const std::function<bool(std::string_view)> &a_extraConflict) {
  const auto baseName =
      std::string(sfs::ui::Localization::GetSingleton()->Get(
          "conditions.default_name"));
  const auto conflicts = [&](std::string_view a_candidate) {
    if (FindConditionFunctionInfo(a_candidate) != nullptr ||
        sfs::conditions::FindDefinitionByName(a_conditions, a_candidate) !=
            nullptr) {
      return true;
    }

    return a_extraConflict && a_extraConflict(a_candidate);
  };

  for (int index = (std::max)(a_seed, 1);; ++index) {
    const auto candidate = baseName + " " + std::to_string(index);
    if (!conflicts(candidate)) {
      return candidate;
    }
  }
}

std::string BuildUniqueConditionName(
    const std::string_view a_baseName,
    const std::vector<Definition> &a_conditions,
    const std::function<bool(std::string_view)> &a_extraConflict) {
  auto baseName = TrimText(a_baseName);
  if (baseName.empty()) {
    baseName = std::string(
        sfs::ui::Localization::GetSingleton()->Get("conditions.default_name"));
  }

  const auto conflicts = [&](std::string_view a_candidate) {
    if (FindConditionFunctionInfo(a_candidate) != nullptr ||
        sfs::conditions::FindDefinitionByName(a_conditions, a_candidate) !=
            nullptr) {
      return true;
    }
    return a_extraConflict && a_extraConflict(a_candidate);
  };

  if (!conflicts(baseName)) {
    return baseName;
  }

  for (int index = 2;; ++index) {
    const auto candidate = baseName + " " + std::to_string(index);
    if (!conflicts(candidate)) {
      return candidate;
    }
  }
}

std::string
ValidateConditionDraft(const Definition &a_definition,
                       const std::vector<Definition> &a_conditions) {
  auto *localization = sfs::ui::Localization::GetSingleton();
  if (const auto baseValidation =
          sfs::conditions::ValidateDefinitionNameAndGraph(
              a_definition, a_conditions,
              [](std::string_view a_name) {
                return FindConditionFunctionInfo(a_name) != nullptr;
              });
      !baseValidation.empty()) {
    return baseValidation;
  }

  for (std::size_t index = 0; index < a_definition.clauses.size(); ++index) {
    const auto &clause = a_definition.clauses[index];
    const auto clauseNumber = index + 1;
    std::optional<FunctionInfo> customFunctionInfo;
    const auto *functionInfo =
        ResolveConditionFunctionInfo(clause, a_conditions, customFunctionInfo);
    if (!functionInfo) {
      return sfs::strings::SafeVFormat(
          std::string(localization->Get("conditions.validation.unknown_function")),
          std::make_format_args(clauseNumber));
    }
    if (!clause.customConditionId.empty() &&
        clause.customConditionId == a_definition.id &&
        !a_definition.id.empty()) {
      return sfs::strings::SafeVFormat(
          std::string(localization->Get("conditions.validation.self_reference")),
          std::make_format_args(clauseNumber));
    }

    for (std::uint16_t paramIndex = 0;
         paramIndex < functionInfo->parameterCount && paramIndex < 2;
         ++paramIndex) {
      const auto argument = TrimText(clause.arguments[paramIndex]);
      const auto &parameterLabel = functionInfo->parameterLabels[paramIndex];
      if (!functionInfo->parameterOptional[paramIndex] && argument.empty()) {
        return sfs::strings::SafeVFormat(
            std::string(localization->Get("conditions.validation.parameter_required")),
            std::make_format_args(parameterLabel, clauseNumber));
      }

      const auto paramType = ResolveEditorParamType(
          functionInfo->name, paramIndex, functionInfo->parameterTypes[paramIndex]);
      const auto editorKind = GetEditorKindForParamType(paramType);
      if (!argument.empty() &&
          ((paramType == RE::SCRIPT_PARAM_TYPE::kAxis &&
            !sfs::conditions::ParseAxisArgument(argument)) ||
           (paramType == RE::SCRIPT_PARAM_TYPE::kActorValue &&
            !sfs::conditions::ParseActorValueArgument(argument)))) {
        return sfs::strings::SafeVFormat(
            std::string(localization->Get("conditions.validation.parameter_choice")),
            std::make_format_args(parameterLabel, clauseNumber, argument));
      }
      if (editorKind == ValueEditorKind::Unsupported) {
        return sfs::strings::SafeVFormat(
            std::string(localization->Get("conditions.validation.parameter_unsupported")),
            std::make_format_args(parameterLabel, clauseNumber));
      }

      if (editorKind == ValueEditorKind::Text &&
          argument.find('\0') != std::string::npos) {
        return sfs::strings::SafeVFormat(
            std::string(localization->Get("conditions.validation.parameter_text")),
            std::make_format_args(parameterLabel, clauseNumber));
      }

      if ((editorKind == ValueEditorKind::Integer ||
           editorKind == ValueEditorKind::Number) &&
          !argument.empty()) {
        const bool valid = editorKind == ValueEditorKind::Integer
                               ? sfs::conditions::TryParseInt(argument).has_value()
                               : sfs::conditions::TryParseFloat(argument).has_value();
        if (!valid) {
          return sfs::strings::SafeVFormat(
              std::string(localization->Get(
                  editorKind == ValueEditorKind::Integer
                      ? "conditions.validation.parameter_integer"
                      : "conditions.validation.parameter_numeric")),
              std::make_format_args(parameterLabel, clauseNumber));
        }
      }
    }

    const auto comparand = TrimText(clause.comparand);
    if (functionInfo->returnsBooleanResult) {
      if (!IsBooleanComparator(clause.comparator)) {
        return sfs::strings::SafeVFormat(
            std::string(localization->Get("conditions.validation.boolean_comparator")),
            std::make_format_args(clauseNumber));
      }
      if (!comparand.empty() && comparand != "0" && comparand != "1") {
        return sfs::strings::SafeVFormat(
            std::string(localization->Get("conditions.validation.boolean_value")),
            std::make_format_args(clauseNumber));
      }
    } else {
      if (comparand.empty()) {
        return sfs::strings::SafeVFormat(
            std::string(localization->Get("conditions.validation.comparison_required")),
            std::make_format_args(clauseNumber));
      }

      if (!sfs::conditions::TryParseFloat(comparand)) {
        return sfs::strings::SafeVFormat(
            std::string(localization->Get("conditions.validation.comparison_numeric")),
            std::make_format_args(clauseNumber));
      }
    }
  }

  return {};
}

bool ParseBooleanComparand(std::string_view a_text, bool a_defaultValue) {
  const auto trimmed = TrimText(a_text);
  if (trimmed.empty()) {
    return a_defaultValue;
  }

  const auto value = sfs::conditions::TryParseFloat(trimmed);
  return value ? *value != 0.0f : a_defaultValue;
}
} // namespace sfs::ui::condition_editor
