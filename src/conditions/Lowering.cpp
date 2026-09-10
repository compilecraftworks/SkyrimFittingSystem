#include "conditions/Lowering.h"
#include "conditions/FormTokens.h"
#include "conditions/NativeConditionStorage.h"

#include "conditions/ValueParsing.h"
#include "StringUtils.h"
#include "conditions/CnfBuilder.h"
#include "conditions/ParamEnumOptions.h"

#include <RE/M/MagicItem.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESFurniture.h>
#include <RE/T/TESWorldSpace.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <exception>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using Clause = sfs::conditions::Clause;
using Comparator = sfs::conditions::Comparator;
using Definition = sfs::conditions::Definition;
using ParamType = RE::SCRIPT_PARAM_TYPE;
using sfs::conditions::TryParseInt;
using sfs::conditions::TryParseFloat;

struct NativeLiteral {
  const RE::SCRIPT_FUNCTION *command{nullptr};
  std::string functionName;
  std::array<std::string, 2> arguments{};
  std::array<ParamType, 2> parameterTypes{ParamType::kForm, ParamType::kForm};
  std::uint16_t parameterCount{0};
  Comparator comparator{Comparator::Equal};
  std::string comparand{"1"};
};

using ConditionCnf = sfs::conditions::cnf::Expression<NativeLiteral>;

union ConditionParam {
  std::int32_t i;
  float f;
  RE::TESForm *form;
};

ParamType ResolveEditorParamType(const std::string_view a_functionName,
                                 const std::uint16_t a_paramIndex,
                                 const ParamType a_type) {
  // SFS conditions are evaluated against actors, so constrain GetIsID to actor
  // bases even if the runtime command table exposes the parameter more broadly.
  if (a_paramIndex == 0 &&
      sfs::strings::EqualsInsensitive(a_functionName, "GetIsID")) {
    return ParamType::kActorBase;
  }

  return a_type;
}

Comparator InvertComparator(const Comparator a_comparator) {
  switch (a_comparator) {
  case Comparator::Equal:
    return Comparator::NotEqual;
  case Comparator::NotEqual:
    return Comparator::Equal;
  case Comparator::Greater:
    return Comparator::LessOrEqual;
  case Comparator::GreaterOrEqual:
    return Comparator::Less;
  case Comparator::Less:
    return Comparator::GreaterOrEqual;
  case Comparator::LessOrEqual:
    return Comparator::Greater;
  }

  return Comparator::NotEqual;
}

std::string ComparatorToken(const Comparator a_comparator) {
  switch (a_comparator) {
  case Comparator::Equal:
    return "==";
  case Comparator::NotEqual:
    return "!=";
  case Comparator::Greater:
    return ">";
  case Comparator::GreaterOrEqual:
    return ">=";
  case Comparator::Less:
    return "<";
  case Comparator::LessOrEqual:
    return "<=";
  }

  return "==";
}

RE::CONDITION_ITEM_DATA::OpCode ToOpCode(const Comparator a_comparator) {
  switch (a_comparator) {
  case Comparator::Equal:
    return RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
  case Comparator::NotEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kNotEqualTo;
  case Comparator::Greater:
    return RE::CONDITION_ITEM_DATA::OpCode::kGreaterThan;
  case Comparator::GreaterOrEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo;
  case Comparator::Less:
    return RE::CONDITION_ITEM_DATA::OpCode::kLessThan;
  case Comparator::LessOrEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kLessThanOrEqualTo;
  }

  return RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
}

bool IsIntegerParamType(const ParamType a_type) {
  switch (a_type) {
  case ParamType::kInt:
  case ParamType::kStage:
  case ParamType::kRelationshipRank:
  case ParamType::kCrimeType:
  case ParamType::kFormType:
  case ParamType::kAlignment:
  case ParamType::kEquipType:
  case ParamType::kCritStage:
  case ParamType::kWardState:
  case ParamType::kFurnitureAnimType:
  case ParamType::kFurnitureEntryType:
  case ParamType::kSkillAction:
    return true;
  default:
    return false;
  }
}

bool IsValueParamType(const ParamType a_type) {
  return IsIntegerParamType(a_type) || a_type == ParamType::kFloat ||
         a_type == ParamType::kActorValue || a_type == ParamType::kAxis ||
         a_type == ParamType::kSex || a_type == ParamType::kCastingSource ||
         a_type == ParamType::kMiscStat;
}

const RE::SCRIPT_FUNCTION *
FindConditionFunction(const std::string_view a_name) {
  const auto trimmed = sfs::strings::TrimText(a_name);
  if (trimmed.empty()) {
    return nullptr;
  }

  const auto *command =
      RE::SCRIPT_FUNCTION::LocateScriptCommand(trimmed.c_str());
  if (!command || !command->conditionFunction) {
    return nullptr;
  }

  return command;
}

template <class T> T *LookupTypedFormByToken(const std::string &a_token) {
  return sfs::conditions::LookupFormToken<T>(a_token);
}

template <class T>
RE::TESForm *LookupAssignableFormByToken(const std::string &a_token) {
  return sfs::conditions::LookupFormTokenIf(a_token, [](RE::TESForm *a_form) {
    return a_form->As<T>() != nullptr;
  });
}

template <class... T>
RE::TESForm *LookupAnyAssignableFormByToken(const std::string &a_token) {
  RE::TESForm *result = nullptr;
  ((result = result ? result : LookupAssignableFormByToken<T>(a_token)), ...);
  return result;
}

RE::TESObjectREFR *LookupReferenceByToken(const std::string &a_token) {
  if (sfs::strings::EqualsInsensitive(a_token, "Player")) {
    return RE::PlayerCharacter::GetSingleton();
  }

  return sfs::conditions::LookupFormToken<RE::TESObjectREFR>(a_token);
}

RE::TESForm *LookupGenericFormByToken(const std::string &a_token) {
  return sfs::conditions::LookupFormToken(a_token);
}

std::optional<ConditionParam> ParseParam(const std::string &a_text,
                                         const ParamType a_type) {
  ConditionParam param{};
  const auto trimmed = sfs::strings::TrimText(a_text);

  switch (a_type) {
  case ParamType::kChar:
  case ParamType::kVMScriptVar:
    // String storage belongs to the emitted condition, never this temporary
    // parsing union or a form lookup. Handled by BuildConditionItemData.
    return std::nullopt;
  case ParamType::kInt:
  case ParamType::kStage:
  case ParamType::kRelationshipRank:
  case ParamType::kCrimeType:
  case ParamType::kAlignment:
  case ParamType::kEquipType:
  case ParamType::kSkillAction:
    if (const auto value = TryParseInt(trimmed)) {
      param.i = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kFormType:
  case ParamType::kCritStage:
  case ParamType::kWardState:
  case ParamType::kFurnitureAnimType:
  case ParamType::kFurnitureEntryType:
    if (const auto value =
            sfs::conditions::ParseParamEnumOption(a_type, trimmed)) {
      param.i = *value;
    } else if (const auto integer = TryParseInt(trimmed)) {
      param.i = *integer;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kFloat:
    if (const auto value = TryParseFloat(trimmed)) {
      param.f = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kActorValue: {
    const auto value = sfs::conditions::ParseActorValueArgument(trimmed);
    if (!value) { return std::nullopt; }
    param.i = *value;
    break;
  }
  case ParamType::kAxis: {
    const auto value = sfs::conditions::ParseAxisArgument(trimmed);
    if (!value) { return std::nullopt; }
    param.i = *value;
    break;
  }
  case ParamType::kSex:
    if (const auto value =
            sfs::conditions::ParseParamEnumOption(a_type, trimmed)) {
      param.i = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kCastingSource:
    if (const auto value =
            sfs::conditions::ParseParamEnumOption(a_type, trimmed)) {
      param.i = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kMiscStat:
    if (const auto value =
            sfs::conditions::ParseParamTextOption(a_type, trimmed)) {
      param.i = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kObjectRef:
    param.form = LookupReferenceByToken(trimmed);
    break;
  case ParamType::kActor: {
    // Actor parameters refer to placed actors, not NPC base records.
    auto *ref = LookupReferenceByToken(trimmed);
    param.form = ref ? ref->As<RE::Actor>() : nullptr;
    break;
  }
  case ParamType::kActorBase:
  case ParamType::kNPC:
    param.form = LookupTypedFormByToken<RE::TESNPC>(trimmed);
    break;
  case ParamType::kRace:
    param.form = LookupTypedFormByToken<RE::TESRace>(trimmed);
    break;
  case ParamType::kClass:
    param.form = LookupTypedFormByToken<RE::TESClass>(trimmed);
    break;
  case ParamType::kFaction:
    param.form = LookupTypedFormByToken<RE::TESFaction>(trimmed);
    break;
  case ParamType::kGlobal:
    param.form = LookupTypedFormByToken<RE::TESGlobal>(trimmed);
    break;
  case ParamType::kQuest:
    param.form = LookupTypedFormByToken<RE::TESQuest>(trimmed);
    break;
  case ParamType::kKeyword:
    param.form = LookupTypedFormByToken<RE::BGSKeyword>(trimmed);
    break;
  case ParamType::kPerk:
    param.form = LookupTypedFormByToken<RE::BGSPerk>(trimmed);
    break;
  case ParamType::kVoiceType:
    param.form = LookupTypedFormByToken<RE::BGSVoiceType>(trimmed);
    break;
  case ParamType::kCell:
    param.form = LookupTypedFormByToken<RE::TESObjectCELL>(trimmed);
    break;
  case ParamType::kLocation:
    param.form = LookupTypedFormByToken<RE::BGSLocation>(trimmed);
    break;
  case ParamType::kWeather:
    param.form = LookupTypedFormByToken<RE::TESWeather>(trimmed);
    break;
  case ParamType::kShout:
    param.form = LookupTypedFormByToken<RE::TESShout>(trimmed);
    break;
  case ParamType::kWordOfPower:
    param.form = LookupTypedFormByToken<RE::TESWordOfPower>(trimmed);
    break;
  case ParamType::kFormList:
    param.form = LookupTypedFormByToken<RE::BGSListForm>(trimmed);
    break;
  case ParamType::kSpellItem:
    param.form = LookupTypedFormByToken<RE::SpellItem>(trimmed);
    break;
  case ParamType::kRegion:
    param.form = LookupTypedFormByToken<RE::TESRegion>(trimmed);
    break;
  case ParamType::kPackage:
    param.form = LookupTypedFormByToken<RE::TESPackage>(trimmed);
    break;
  case ParamType::kMagicEffect:
    param.form = LookupTypedFormByToken<RE::EffectSetting>(trimmed);
    break;
  case ParamType::kBGSScene:
    param.form = LookupTypedFormByToken<RE::BGSScene>(trimmed);
    break;
  case ParamType::kAssociationType:
    param.form = LookupTypedFormByToken<RE::BGSAssociationType>(trimmed);
    break;
  case ParamType::kNote:
    param.form = LookupTypedFormByToken<RE::BGSNote>(trimmed);
    break;
  case ParamType::kEncounterZone:
    param.form = LookupTypedFormByToken<RE::BGSEncounterZone>(trimmed);
    break;
  case ParamType::kIdleForm:
    param.form = LookupTypedFormByToken<RE::TESIdleForm>(trimmed);
    break;
  case ParamType::kRefType:
    param.form = LookupTypedFormByToken<RE::BGSLocationRefType>(trimmed);
    break;
  case ParamType::kMagicItem:
    param.form = LookupAssignableFormByToken<RE::MagicItem>(trimmed);
    break;
  case ParamType::kObject:
  case ParamType::kInventoryObject:
    param.form = LookupAssignableFormByToken<RE::TESBoundObject>(trimmed);
    break;
  case ParamType::kFurnitureOrFormList:
    param.form = LookupAnyAssignableFormByToken<RE::TESFurniture,
                                                RE::BGSListForm>(trimmed);
    break;
  case ParamType::kOwner:
    param.form =
        LookupAnyAssignableFormByToken<RE::TESNPC, RE::TESFaction>(trimmed);
    break;
  case ParamType::kInvObjectOrFormList:
  case ParamType::kObjectOrFormList:
    param.form = LookupAnyAssignableFormByToken<RE::TESBoundObject,
                                                RE::BGSListForm>(trimmed);
    break;
  case ParamType::kWorldOrList:
    param.form = LookupAnyAssignableFormByToken<RE::TESWorldSpace,
                                                RE::BGSListForm>(trimmed);
    break;
  case ParamType::kKnowableForm:
  case ParamType::kForm:
  default:
    param.form = LookupGenericFormByToken(trimmed);
    break;
  }

  return param;
}

std::string BuildLiteralSignature(const NativeLiteral &a_literal) {
  std::string signature = a_literal.functionName;
  for (std::uint16_t paramIndex = 0; paramIndex < a_literal.parameterCount &&
                                     paramIndex < a_literal.arguments.size();
       ++paramIndex) {
    signature.push_back('(');
    signature.append(a_literal.arguments[paramIndex]);
    signature.push_back(')');
  }
  signature.push_back(' ');
  signature.append(ComparatorToken(a_literal.comparator));
  signature.push_back(' ');
  signature.append(a_literal.comparand);
  return signature;
}

std::string BuildLiteralDisplay(const NativeLiteral &a_literal) {
  std::string display = a_literal.functionName;
  display.push_back('(');
  bool firstArgument = true;
  for (std::uint16_t paramIndex = 0; paramIndex < a_literal.parameterCount &&
                                     paramIndex < a_literal.arguments.size();
       ++paramIndex) {
    if (!firstArgument) {
      display.append(", ");
    }
    firstArgument = false;
    display.append(a_literal.arguments[paramIndex]);
  }
  display.push_back(')');
  display.push_back(' ');
  display.append(ComparatorToken(a_literal.comparator));
  display.push_back(' ');
  display.append(a_literal.comparand);
  return display;
}

std::string BuildCnfSignature(const ConditionCnf &a_cnf) {
  std::string signature;
  bool firstGroup = true;
  for (const auto &group : a_cnf) {
    if (!firstGroup) {
      signature.append(" AND ");
    }
    firstGroup = false;

    signature.push_back('(');
    bool firstLiteral = true;
    for (const auto &literal : group) {
      if (!firstLiteral) {
        signature.append(" OR ");
      }
      firstLiteral = false;
      signature.append(BuildLiteralSignature(literal));
    }
    signature.push_back(')');
  }
  return signature;
}

sfs::conditions::DisplayCnf BuildDisplayCnf(const ConditionCnf &a_cnf) {
  sfs::conditions::DisplayCnf displayCnf;
  displayCnf.reserve(a_cnf.size());
  for (const auto &group : a_cnf) {
    auto &displayGroup = displayCnf.emplace_back();
    displayGroup.reserve(group.size());
    for (const auto &literal : group) {
      displayGroup.push_back(BuildLiteralDisplay(literal));
    }
  }
  return displayCnf;
}

std::optional<NativeLiteral> BuildNativeLiteral(const Clause &a_clause) {
  const auto *command = FindConditionFunction(a_clause.functionName);
  if (!command) {
    return std::nullopt;
  }

  NativeLiteral literal;
  literal.command = command;
  literal.functionName = sfs::strings::TrimText(a_clause.functionName);
  literal.parameterCount = command->numParams;
  literal.comparator = a_clause.comparator;
  literal.comparand = sfs::strings::TrimText(a_clause.comparand);

  for (std::uint16_t paramIndex = 0;
       paramIndex < command->numParams && paramIndex < 2; ++paramIndex) {
    literal.arguments[paramIndex] =
        sfs::strings::TrimText(a_clause.arguments[paramIndex]);
    literal.parameterTypes[paramIndex] = ResolveEditorParamType(
        literal.functionName, paramIndex,
        command->params ? command->params[paramIndex].paramType.get()
                        : ParamType::kForm);
  }

  return literal;
}

std::optional<RE::CONDITION_ITEM_DATA>
BuildConditionItemData(const NativeLiteral &a_literal,
                       const bool a_isORToNext,
                       sfs::conditions::NativeConditionStorage &a_storage) {
  RE::CONDITION_ITEM_DATA data{};

  const auto functionIndex = std::to_underlying(a_literal.command->output) -
                             RE::SCRIPT_FUNCTION::Commands::kScriptOpBase;
  data.functionData.function =
      static_cast<RE::FUNCTION_DATA::FunctionID>(functionIndex);

  for (std::uint16_t paramIndex = 0;
       paramIndex < a_literal.parameterCount && paramIndex < 2; ++paramIndex) {
    const auto &argument = a_literal.arguments[paramIndex];
    if (argument.empty()) {
      continue;
    }

    if (a_literal.parameterTypes[paramIndex] == ParamType::kChar ||
        a_literal.parameterTypes[paramIndex] == ParamType::kVMScriptVar) {
      auto *text = a_storage.StoreText(argument);
      if (!text) {
        return std::nullopt;
      }
      data.functionData.params[paramIndex] = text;
      continue;
    }

    const auto param =
        ParseParam(argument, a_literal.parameterTypes[paramIndex]);
    if (!param) {
      return std::nullopt;
    }
    if (!IsValueParamType(a_literal.parameterTypes[paramIndex]) &&
        !param->form) {
      return std::nullopt;
    }

    data.functionData.params[paramIndex] = std::bit_cast<void *>(*param);
  }

  data.flags.opCode = ToOpCode(a_literal.comparator);
  data.flags.isOR = a_isORToNext;
  data.object = RE::CONDITIONITEMOBJECT::kSelf;
  const auto comparand = TryParseFloat(a_literal.comparand);
  if (!comparand) {
    return std::nullopt;
  }
  data.comparisonValue.f = *comparand;
  data.flags.global = false;
  return data;
}

std::optional<std::shared_ptr<RE::TESCondition>>
EmitCondition(const ConditionCnf &a_cnf) {
  auto storage = std::make_shared<sfs::conditions::NativeConditionStorage>();
  auto condition = std::shared_ptr<RE::TESCondition>(storage, &storage->condition);
  RE::TESConditionItem *previous = nullptr;

  for (const auto &group : a_cnf) {
    for (std::size_t literalIndex = 0; literalIndex < group.size();
         ++literalIndex) {
      const auto data = BuildConditionItemData(group[literalIndex],
                                               literalIndex + 1 < group.size(),
                                               *storage);
      if (!data) {
        return std::nullopt;
      }

      auto *item = new RE::TESConditionItem();
      item->data = *data;
      item->next = nullptr;
      if (previous) {
        previous->next = item;
      } else {
        condition->head = item;
      }
      previous = item;
    }
  }

  return condition;
}
} // namespace

namespace sfs::conditions {
RE::TESForm *ResolveConditionFormArgument(const std::string &a_text,
                                         const RE::SCRIPT_PARAM_TYPE a_type) {
  if (IsValueParamType(a_type) || a_type == ParamType::kChar ||
      a_type == ParamType::kVMScriptVar) { return nullptr; }
  const auto param = ParseParam(a_text, a_type);
  return param ? param->form : nullptr;
}

std::optional<LoweredMaterialization>
LowerAndEmitCondition(const Definition &a_definition,
                      const std::vector<Definition> &a_conditions) {
  auto expression = cnf::Build<NativeLiteral>(
      a_definition, a_conditions,
      [](const Clause &a_clause,
         const bool a_negated) -> std::optional<NativeLiteral> {
        auto literal = BuildNativeLiteral(a_clause);
        if (literal && a_negated) {
          literal->comparator = InvertComparator(literal->comparator);
        }
        return literal;
      });
  if (!expression || expression->empty()) {
    return std::nullopt;
  }

  auto condition = EmitCondition(*expression);
  if (!condition) {
    return std::nullopt;
  }

  return LoweredMaterialization{.condition = *condition,
                                .signature = BuildCnfSignature(*expression),
                                .displayCnf = BuildDisplayCnf(*expression)};
}
} // namespace sfs::conditions
