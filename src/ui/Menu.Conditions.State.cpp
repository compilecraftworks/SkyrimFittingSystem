#include "Menu.h"

#include "ConditionMaterializer.h"
#include "conditions/Creation.h"
#include "conditions/Defaults.h"
#include "conditions/Status.h"
#include "ui/Localization.h"
#include "ui/conditions/EditorSupport.h"

#include <algorithm>
#include <format>

namespace sfs {
using ConditionDefinition = ui::conditions::Definition;
using ConditionFunctionInfo = ui::condition_editor::FunctionInfo;
using ConditionValueEditorKind = ui::condition_editor::ValueEditorKind;
using ui::condition_editor::BuildSuggestedConditionName;
using ui::condition_editor::BuildUniqueConditionName;
using ui::condition_editor::CompareTextInsensitive;
using ui::condition_editor::FindConditionFunctionInfo;
using ui::condition_editor::GetEditorKindForParamType;
using ui::condition_editor::IsBooleanComparator;
using ui::condition_editor::ResolveConditionFunctionInfo;
using ui::condition_editor::ResolveEditorParamType;
using ui::condition_editor::TrimText;
using ui::condition_editor::ValidateConditionDraft;

namespace {
struct SampleLocalizationMetadata {
  std::string_view id;
  std::string_view keySuffix;
  std::array<std::string_view, 3> generatedNames;
  std::array<std::string_view, 4> generatedDescriptions;
};

constexpr std::array<SampleLocalizationMetadata, 3> kSampleLocalization = {{
    {"sample-city-life",
     "city_life",
     {"City Life", "\xEB\x8F\x84\xEC\x8B\x9C\x20\xEC\x83\x9D\xED\x99\x9C",
      "\xE5\x9F\x8E\xE5\xB8\x82\xE7\x94\x9F\xE6\xB4\xBB"},
     {"City AND non-combat", "Applies in cities while out of combat.",
      "\xEB\x8F\x84\xEC\x8B\x9C\xEC\x97\x90\xEC\x84\x9C\x20\xEB\xB9\x84\xEC\xA0"
      "\x84\xED\x88\xAC\x20\xEC\x83\x81\xED\x83\x9C\xEC\x9D\xBC\x20\xEB\x95\x8C"
      "\x20\xEC\xA0\x81\xEC\x9A\xA9\xEB\x90\xA9\xEB\x8B\x88\xEB\x8B\xA4\x2E",
      "\xE5\x9C\xA8\xE5\x9F\x8E\xE5\xB8\x82\xE4\xB8\x94\xE5\xA4\x84\xE4\xBA\x8E"
      "\xE9\x9D\x9E\xE6\x88\x98\xE6\x96\x97\xE7\x8A\xB6\xE6\x80\x81\xE6\x97\xB6"
      "\xE5\xBA\x94\xE7\x94\xA8\xE3\x80\x82"}},
    {"sample-dungeon-exploration",
     "dungeon_exploration",
     {"Dungeon Exploration",
      "\xEB\x8D\x98\xEC\xA0\x84\x20\xED\x83\x90\xED\x97\x98",
      "\xE5\x9C\xB0\xE7\x89\xA2\xE6\x8E\xA2\xE7\xB4\xA2"},
     {"Dungeon AND combat", "Applies in dungeons while in combat.",
      "\xEB\x8D\x98\xEC\xA0\x84\xEC\x97\x90\xEC\x84\x9C\x20\xEC\xA0\x84\xED\x88"
      "\xAC\x20\xEC\x83\x81\xED\x83\x9C\xEC\x9D\xBC\x20\xEB\x95\x8C\x20\xEC\xA0"
      "\x81\xEC\x9A\xA9\xEB\x90\xA9\xEB\x8B\x88\xEB\x8B\xA4\x2E",
      "\xE5\x9C\xA8\xE5\x9C\xB0\xE7\x89\xA2\xE4\xB8\x94\xE5\xA4\x84\xE4\xBA\x8E"
      "\xE6\x88\x98\xE6\x96\x97\xE7\x8A\xB6\xE6\x80\x81\xE6\x97\xB6\xE5\xBA\x94"
      "\xE7\x94\xA8\xE3\x80\x82"}},
    {"sample-night-infiltration",
     "night_infiltration",
     {"Night Infiltration",
      "\xEC\x95\xBC\xEA\xB0\x84\x20\xEC\x9E\xA0\xEC\x9E\x85",
      "\xE5\xA4\x9C\xE9\x97\xB4\xE6\xBD\x9C\xE5\x85\xA5"},
     {"Night AND sneaking", "Applies at night while sneaking.",
      "\xEB\xB0\xA4\xEC\x97\x90\x20\xEC\x9D\x80\xEC\x8B\xA0\x20\xEC\x83\x81\xED"
      "\x83\x9C\xEC\x9D\xBC\x20\xEB\x95\x8C\x20\xEC\xA0\x81\xEC\x9A\xA9\xEB\x90"
      "\xA9\xEB\x8B\x88\xEB\x8B\xA4\x2E",
      "\xE5\x9C\xA8\xE5\xA4\x9C\xE9\x97\xB4\xE6\xBD\x9C\xE8\xA1\x8C\xE6\x97\xB6"
      "\xE5\xBA\x94\xE7\x94\xA8\xE3\x80\x82"}},
}};

const SampleLocalizationMetadata *
FindSampleLocalization(const std::string_view a_id) {
  const auto it = std::ranges::find(kSampleLocalization, a_id,
                                    &SampleLocalizationMetadata::id);
  return it != kSampleLocalization.end() ? std::addressof(*it) : nullptr;
}

template <std::size_t Size>
bool IsGeneratedSampleText(
    const std::string_view a_text,
    const std::array<std::string_view, Size> &a_generatedValues) {
  return std::ranges::find(a_generatedValues, a_text) !=
         a_generatedValues.end();
}

bool LocalizeGeneratedSample(ConditionDefinition &a_sample,
                             const SampleLocalizationMetadata &a_metadata,
                             const ui::Localization &a_localization) {
  bool changed = false;
  const auto nameKey = "conditions.sample." + std::string(a_metadata.keySuffix);
  const auto descriptionKey = nameKey + ".description";
  const auto localizedName = std::string(a_localization.Get(nameKey));
  const auto localizedDescription =
      std::string(a_localization.Get(descriptionKey));

  if (IsGeneratedSampleText(a_sample.name, a_metadata.generatedNames) &&
      a_sample.name != localizedName) {
    a_sample.name = localizedName;
    changed = true;
  }
  if (IsGeneratedSampleText(a_sample.description,
                            a_metadata.generatedDescriptions) &&
      a_sample.description != localizedDescription) {
    a_sample.description = localizedDescription;
    changed = true;
  }
  return changed;
}
} // namespace
void Menu::EnsureDefaultConditions() {
  auto &definitions = ConditionDefinitions();
  bool changed = false;
  const auto sameClause = [](const conditions::Clause &left,
                             const conditions::Clause &right) {
    return left.functionName == right.functionName &&
           left.customConditionId == right.customConditionId &&
           left.arguments == right.arguments &&
           left.comparator == right.comparator &&
           left.comparand == right.comparand &&
           left.connectiveToNext == right.connectiveToNext;
  };

  const auto oldSize = definitions.size();
  definitions.erase(
      std::remove_if(definitions.begin(), definitions.end(),
                     [](const ConditionDefinition &value) {
                       return value.id == conditions::kDefaultConditionId ||
                              conditions::IsActorOwnershipCondition(value);
                     }),
      definitions.end());
  changed |= definitions.size() != oldSize;

  const bool firstRun =
      std::ranges::none_of(definitions, [](const ConditionDefinition &value) {
        return value.IsCatalog() && !conditions::IsBuiltInCondition(value.id);
      });
  auto *localization = ui::Localization::GetSingleton();
  for (auto &builtin : conditions::BuildBuiltInConditions()) {
    const auto suffix = builtin.id.substr(std::string("builtin-").size());
    builtin.name =
        std::string(localization->Get("conditions.builtin." + suffix));
    builtin.description = std::string(
        localization->Get("conditions.builtin." + suffix + ".description"));
    if (auto *existing =
            conditions::FindDefinitionById(definitions, builtin.id);
        existing != nullptr) {
      const bool clausesChanged =
          existing->clauses.size() != builtin.clauses.size() ||
          !std::ranges::equal(existing->clauses, builtin.clauses, sameClause);
      changed |= existing->name != builtin.name ||
                 existing->description != builtin.description || clausesChanged;
      existing->name = builtin.name;
      existing->description = builtin.description;
      existing->clauses = builtin.clauses;
      existing->EnsureCatalog().color = builtin.EnsureCatalog().color;
    } else {
      definitions.push_back(std::move(builtin));
      changed = true;
    }
  }
  const bool seedSamples = !conditionStore_.samplesSeeded && firstRun;
  for (auto &sample : conditions::BuildSampleConditions()) {
    const auto *metadata = FindSampleLocalization(sample.id);
    if (!metadata) {
      continue;
    }

    if (auto *existing =
            conditions::FindDefinitionById(definitions, sample.id)) {
      changed |= LocalizeGeneratedSample(*existing, *metadata, *localization);
      continue;
    }
    if (!seedSamples) {
      continue;
    }

    LocalizeGeneratedSample(sample, *metadata, *localization);
    definitions.push_back(std::move(sample));
    changed = true;
  }
  if (!conditionStore_.samplesSeeded) {
    if (seedSamples) {
      NextConditionId() = (std::max)(NextConditionId(), 4);
    }
    conditionStore_.samplesSeeded = true;
    changed = true;
  }
  if (changed) {
    BumpConditionStoreRevision();
    sfs::conditions::RebuildConditionDependencyMetadata(definitions);
    sfs::conditions::InvalidateConditionMaterializationCaches(definitions);
  }
}
std::size_t Menu::CountCatalogConditions() const {
  return static_cast<std::size_t>(std::ranges::count_if(
      ConditionDefinitions(), [](const ConditionDefinition &a_definition) {
        return a_definition.IsCatalog();
      }));
}

bool Menu::IsWorkbenchSelectableCondition(
    const ConditionDefinition &a_condition) const {
  return !conditions::IsActorOwnershipCondition(a_condition) &&
         conditions::IsWorkbenchSelectable(a_condition);
}

int Menu::AllocateConditionEditorWindowSlot() const {
  int slot = 1;
  while (true) {
    const auto it = std::ranges::find(ConditionEditors(), slot,
                                      &ConditionEditorState::windowSlot);
    if (it == ConditionEditors().end()) {
      return slot;
    }
    ++slot;
  }
}

void Menu::OpenNewConditionDialog() {
  std::vector<ui::conditions::Color> existingColors;
  existingColors.reserve(ConditionDefinitions().size() +
                         ConditionEditors().size());
  for (const auto &condition : ConditionDefinitions()) {
    if (const auto *catalog = condition.GetCatalog(); catalog != nullptr) {
      existingColors.push_back(catalog->color);
    }
  }
  for (const auto &existingEditor : ConditionEditors()) {
    if (existingEditor.isNew) {
      if (const auto *catalog = existingEditor.draft.GetCatalog();
          catalog != nullptr) {
        existingColors.push_back(catalog->color);
      }
    }
  }

  const auto suggestedName = BuildSuggestedConditionName(
      ConditionDefinitions(), NextConditionId(),
      [&](std::string_view a_candidate) {
        return std::ranges::any_of(
            ConditionEditors(), [&](const ConditionEditorState &a_editor) {
              return a_editor.isNew &&
                     CompareTextInsensitive(TrimText(a_editor.draft.name),
                                            a_candidate) == 0;
            });
      });

  ConditionEditorState editor;
  editor.windowSlot = AllocateConditionEditorWindowSlot();
  editor.draft = conditions::BuildNewConditionTemplate(
      suggestedName, conditions::PickDistinctConditionColor(existingColors));
  editor.isNew = true;
  editor.focusOnNextDraw = true;
  ConditionEditors().push_back(std::move(editor));
}

void Menu::OpenConditionEditorDialog(const std::size_t a_index) {
  if (a_index >= ConditionDefinitions().size()) {
    return;
  }

  const auto &condition = ConditionDefinitions()[a_index];
  if (conditions::IsBuiltInCondition(condition.id)) {
    return;
  }
  const auto existingIt =
      std::ranges::find(ConditionEditors(), condition.id,
                        &ConditionEditorState::sourceConditionId);
  if (existingIt != ConditionEditors().end()) {
    existingIt->focusOnNextDraw = true;
    existingIt->open = true;
    return;
  }

  ConditionEditorState editor;
  editor.windowSlot = AllocateConditionEditorWindowSlot();
  editor.sourceConditionId = condition.id;
  editor.draft = condition;
  editor.focusOnNextDraw = true;
  ConditionEditors().push_back(std::move(editor));
}

void Menu::OpenConditionEditorDialogById(const std::string_view a_conditionId) {
  const auto it = std::ranges::find(ConditionDefinitions(), a_conditionId,
                                    &ConditionDefinition::id);
  if (it == ConditionDefinitions().end()) {
    return;
  }
  OpenConditionEditorDialog(static_cast<std::size_t>(
      std::distance(ConditionDefinitions().begin(), it)));
}

bool Menu::SaveConditionEditor(ConditionEditorState &a_editor) {
  auto *localization = ui::Localization::GetSingleton();
  if (const auto validationError =
          ValidateConditionDraft(a_editor.draft, ConditionDefinitions());
      !validationError.empty()) {
    a_editor.error = validationError;
    return false;
  }

  for (std::size_t index = 0; index < a_editor.draft.clauses.size(); ++index) {
    auto &clause = a_editor.draft.clauses[index];
    const auto clauseNumber = index + 1;
    clause.functionName = TrimText(clause.functionName);
    clause.arguments[0] = TrimText(clause.arguments[0]);
    clause.arguments[1] = TrimText(clause.arguments[1]);
    clause.comparand = TrimText(clause.comparand);

    std::optional<ConditionFunctionInfo> customFunctionInfo;
    const auto *functionInfo = ResolveConditionFunctionInfo(
        clause, ConditionDefinitions(), customFunctionInfo);
    if (!functionInfo) {
      a_editor.error = sfs::strings::SafeVFormat(
          std::string(
              localization->Get("conditions.validation.unknown_function")),
          std::make_format_args(clauseNumber));
      return false;
    }
    if (!clause.customConditionId.empty()) {
      if (clause.customConditionId == a_editor.draft.id &&
          !a_editor.draft.id.empty()) {
        a_editor.error = sfs::strings::SafeVFormat(
            std::string(
                localization->Get("conditions.validation.self_reference")),
            std::make_format_args(clauseNumber));
        return false;
      }
      clause.arguments[0].clear();
      clause.arguments[1].clear();
      clause.functionName.clear();
    }

    for (std::uint16_t paramIndex = 0;
         paramIndex < functionInfo->parameterCount && paramIndex < 2;
         ++paramIndex) {
      const auto &parameterLabel = functionInfo->parameterLabels[paramIndex];
      if (!functionInfo->parameterOptional[paramIndex] &&
          clause.arguments[paramIndex].empty()) {
        a_editor.error = sfs::strings::SafeVFormat(
            std::string(
                localization->Get("conditions.validation.parameter_required")),
            std::make_format_args(parameterLabel, clauseNumber));
        return false;
      }

      const auto editorKind = GetEditorKindForParamType(
          ResolveEditorParamType(clause.functionName, paramIndex,
                                 functionInfo->parameterTypes[paramIndex]));
      if (editorKind == ConditionValueEditorKind::Unsupported) {
        a_editor.error = sfs::strings::SafeVFormat(
            std::string(localization->Get(
                "conditions.validation.parameter_unsupported")),
            std::make_format_args(parameterLabel, clauseNumber));
        return false;
      }
      if (editorKind == ConditionValueEditorKind::Integer &&
          !clause.arguments[paramIndex].empty()) {
        try {
          clause.arguments[paramIndex] =
              std::to_string(std::stoi(clause.arguments[paramIndex]));
        } catch (const std::exception &) {
          a_editor.error = sfs::strings::SafeVFormat(
              std::string(
                  localization->Get("conditions.validation.parameter_integer")),
              std::make_format_args(parameterLabel, clauseNumber));
          return false;
        }
      } else if (editorKind == ConditionValueEditorKind::Number &&
                 !clause.arguments[paramIndex].empty()) {
        try {
          clause.arguments[paramIndex] =
              ui::condition_editor::FormatNumberString(
                  std::stod(clause.arguments[paramIndex]));
        } catch (const std::exception &) {
          a_editor.error = sfs::strings::SafeVFormat(
              std::string(
                  localization->Get("conditions.validation.parameter_numeric")),
              std::make_format_args(parameterLabel, clauseNumber));
          return false;
        }
      }
    }

    if (functionInfo->returnsBooleanResult) {
      if (!IsBooleanComparator(clause.comparator)) {
        a_editor.error = sfs::strings::SafeVFormat(
            std::string(
                localization->Get("conditions.validation.boolean_comparator")),
            std::make_format_args(clauseNumber));
        return false;
      }
      clause.comparand =
          ui::condition_editor::ParseBooleanComparand(clause.comparand, false)
              ? "1"
              : "0";
    } else {
      if (clause.comparand.empty()) {
        a_editor.error = sfs::strings::SafeVFormat(
            std::string(
                localization->Get("conditions.validation.comparison_required")),
            std::make_format_args(clauseNumber));
        return false;
      }

      try {
        clause.comparand = ui::condition_editor::FormatNumberString(
            std::stod(clause.comparand));
      } catch (const std::exception &) {
        a_editor.error = sfs::strings::SafeVFormat(
            std::string(
                localization->Get("conditions.validation.comparison_numeric")),
            std::make_format_args(clauseNumber));
        return false;
      }
    }
  }

  a_editor.draft.name = TrimText(a_editor.draft.name);
  if (auto *catalog = a_editor.draft.GetCatalog(); catalog != nullptr) {
    catalog->color.w = 1.0f;
  }

  if (a_editor.isNew) {
    a_editor.draft.id = conditions::BuildConditionId(NextConditionId()++);
    ConditionDefinitions().push_back(a_editor.draft);
    a_editor.isNew = false;
    a_editor.sourceConditionId = a_editor.draft.id;
  } else {
    const auto it =
        std::ranges::find(ConditionDefinitions(), a_editor.sourceConditionId,
                          &ConditionDefinition::id);
    if (it == ConditionDefinitions().end()) {
      a_editor.error = std::string(
          localization->Get("conditions.validation.no_longer_exists"));
      return false;
    }
    *it = a_editor.draft;
  }

  BumpConditionStoreRevision();
  sfs::conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
  sfs::conditions::InvalidateConditionMaterializationCachesFrom(
      ConditionDefinitions(), a_editor.draft.id);

  a_editor.error.clear();
  return true;
}

} // namespace sfs
