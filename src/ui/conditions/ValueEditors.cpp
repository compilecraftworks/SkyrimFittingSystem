#include "ui/conditions/ValueEditors.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "StringUtils.h"
#include "conditions/ParamEnumOptions.h"
#include "conditions/Status.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/Localization.h"
#include "ui/Menu.h"
#include "ui/components/EditableCombo.h"
#include "ui/conditions/FunctionRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <charconv>
#include <sstream>
#include <unordered_set>

namespace {
using ValueEditorKind = sfs::ui::condition_editor::ValueEditorKind;
using ObjectRefDropdownItem =
    sfs::ui::components::EditableDropdownItem<std::string>;

struct ObjectRefEditorItem {
  ObjectRefDropdownItem item;
  RE::FormID formID{0};
};

std::string FormatNumberStringImpl(const double a_value) {
  std::ostringstream stream;
  stream.setf(std::ios::fixed, std::ios::floatfield);
  stream << std::setprecision(3) << a_value;
  auto text = stream.str();
  const auto dotIndex = text.find('.');
  if (dotIndex != std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  return text.empty() ? "0" : text;
}

ValueEditorKind
GetEditorKindForParamTypeImpl(const RE::SCRIPT_PARAM_TYPE a_type) {
  if (sfs::conditions::HasParamEnumOptions(a_type) ||
      sfs::conditions::HasParamTextOptions(a_type)) {
    return ValueEditorKind::CachedOption;
  }

  switch (a_type) {
  case RE::SCRIPT_PARAM_TYPE::kChar:
  case RE::SCRIPT_PARAM_TYPE::kInt:
  case RE::SCRIPT_PARAM_TYPE::kStage:
  case RE::SCRIPT_PARAM_TYPE::kRelationshipRank:
  case RE::SCRIPT_PARAM_TYPE::kCrimeType:
  case RE::SCRIPT_PARAM_TYPE::kAlignment:
  case RE::SCRIPT_PARAM_TYPE::kEquipType:
  case RE::SCRIPT_PARAM_TYPE::kSkillAction:
    return ValueEditorKind::Integer;
  case RE::SCRIPT_PARAM_TYPE::kFloat:
    return ValueEditorKind::Number;
  default:
    return sfs::ui::conditions::ConditionParamOptionCache::Supports(a_type)
               ? ValueEditorKind::CachedOption
               : ValueEditorKind::Unsupported;
  }
}

bool DrawNumericClauseValueEditorImpl(const char *a_id, std::string &a_value,
                                      const ValueEditorKind a_kind,
                                      const float a_width) {
  if (a_kind == ValueEditorKind::Unsupported ||
      a_kind == ValueEditorKind::CachedOption) {
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(a_width);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%s",
                  sfs::ui::Localization::GetSingleton()->GetCStr(
                      "conditions.unsupported"));
    ImGui::InputText(a_id, buffer, sizeof(buffer),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    return false;
  }

  ImGui::SetNextItemWidth(a_width);

  if (a_kind == ValueEditorKind::Integer) {
    int numericValue = 0;
    if (!a_value.empty()) {
      try {
        numericValue = std::stoi(a_value);
      } catch (const std::exception &) {
      }
    }

    if (ImGui::InputInt(a_id, &numericValue, 0, 0)) {
      a_value = std::to_string(numericValue);
      return true;
    }
    return false;
  }

  if (a_kind == ValueEditorKind::Number) {
    double numericValue = 0.0;
    if (!a_value.empty()) {
      try {
        numericValue = std::stod(a_value);
      } catch (const std::exception &) {
      }
    }

    if (ImGui::InputDouble(a_id, &numericValue, 0.0, 0.0, "%.3f")) {
      a_value = FormatNumberStringImpl(numericValue);
      return true;
    }
    return false;
  }

  return false;
}

std::string BuildObjectReferenceToken(const RE::TESObjectREFR *a_ref) {
  if (!a_ref) {
    return {};
  }

  if (const auto *player = RE::PlayerCharacter::GetSingleton();
      player && a_ref->GetFormID() == player->GetFormID()) {
    return "Player";
  }

  if (const auto editorID = sfs::armor::GetEditorID(a_ref); !editorID.empty()) {
    return editorID;
  }

  if (const auto identifier = sfs::armor::GetFormIdentifier(a_ref);
      !identifier.empty()) {
    return identifier;
  }

  return sfs::armor::FormatFormID(a_ref->GetFormID());
}

RE::TESObjectREFR *LookupObjectReferenceByToken(const std::string &a_token) {
  const auto trimmed = sfs::strings::TrimText(a_token);
  if (trimmed.empty()) {
    return nullptr;
  }

  if (sfs::strings::EqualsInsensitive(trimmed, "Player")) {
    return RE::PlayerCharacter::GetSingleton();
  }

  if (auto *ref = sfs::armor::LookupByIdentifier<RE::TESObjectREFR>(trimmed)) {
    return ref;
  }

  if (auto *form = RE::TESForm::LookupByEditorID(trimmed)) {
    if (auto *ref = form->As<RE::TESObjectREFR>()) {
      return ref;
    }
  }

  if (std::ranges::all_of(trimmed, [](const unsigned char a_char) {
        return std::isxdigit(a_char) != 0;
      })) {
    RE::FormID formID = 0;
    const auto *begin = trimmed.data();
    const auto *end = begin + trimmed.size();
    if (const auto [ptr, ec] = std::from_chars(begin, end, formID, 16);
        ec == std::errc{} && ptr == end) {
      return RE::TESForm::LookupByID<RE::TESObjectREFR>(formID);
    }
  }

  return nullptr;
}

std::string BuildObjectReferenceLabel(RE::TESObjectREFR *a_ref) {
  if (!a_ref) {
    return {};
  }

  if (const auto *player = RE::PlayerCharacter::GetSingleton();
      player && a_ref->GetFormID() == player->GetFormID()) {
    return std::string(sfs::ui::Localization::GetSingleton()->Get(
               "workbench.filters.player_prefix")) +
           sfs::armor::FormatFormID(a_ref->GetFormID()) + "]";
  }

  std::string label;
  if (auto *actor = a_ref->As<RE::Actor>()) {
    if (const auto *displayName = actor->GetDisplayFullName();
        displayName != nullptr && displayName[0] != '\0') {
      label = displayName;
    } else if (const auto *name = actor->GetName();
               name != nullptr && name[0] != '\0') {
      label = name;
    } else if (const auto *actorBase = actor->GetActorBase()) {
      label = sfs::armor::GetDisplayName(actorBase);
    }
  }

  if (label.empty()) {
    if (const auto *displayName = a_ref->GetDisplayFullName();
        displayName != nullptr && displayName[0] != '\0') {
      label = displayName;
    } else if (const auto *name = a_ref->GetName();
               name != nullptr && name[0] != '\0') {
      label = name;
    } else {
      label = sfs::armor::GetDisplayName(a_ref);
    }
  }

  return label + " [" + sfs::armor::FormatFormID(a_ref->GetFormID()) + "]";
}

std::string ExtractObjectReferenceTokenFromLabel(std::string_view a_label) {
  const auto trimmed = sfs::strings::TrimText(a_label);
  if (trimmed.empty()) {
    return {};
  }

  const auto closeBracket = trimmed.rfind(']');
  const auto openBracket = closeBracket == std::string::npos
                               ? std::string::npos
                               : trimmed.rfind('[', closeBracket);
  if (openBracket != std::string::npos && closeBracket == trimmed.size() - 1 &&
      openBracket + 1 < closeBracket) {
    return sfs::strings::TrimText(
        trimmed.substr(openBracket + 1, closeBracket - openBracket - 1));
  }

  return trimmed;
}

RE::Actor *ResolveCrosshairActor() {
  const auto *crosshairPickData = RE::CrosshairPickData::GetSingleton();
  if (!crosshairPickData) {
    return nullptr;
  }

  RE::ObjectRefHandle handle;
#if defined(EXCLUSIVE_SKYRIM_FLAT)
  handle = crosshairPickData->targetActor;
#else
  handle = crosshairPickData->targetActor[RE::VR_DEVICE::kHeadset];
#endif

  auto ref = handle.get();
  return ref ? ref->As<RE::Actor>() : nullptr;
}

void AppendObjectReferenceOption(std::vector<ObjectRefEditorItem> &a_items,
                                 std::unordered_set<RE::FormID> &a_seenFormIDs,
                                 RE::TESObjectREFR *a_ref,
                                 const std::string_view a_valueOverride = {}) {
  if (!a_ref) {
    return;
  }

  const auto formID = a_ref->GetFormID();
  if (formID == 0 || !a_seenFormIDs.insert(formID).second) {
    return;
  }

  auto value = a_valueOverride.empty() ? BuildObjectReferenceToken(a_ref)
                                       : std::string(a_valueOverride);
  if (value.empty()) {
    value = sfs::armor::FormatFormID(formID);
  }

  a_items.push_back({.item = {.label = BuildObjectReferenceLabel(a_ref),
                              .value = std::move(value)},
                     .formID = formID});
}

std::vector<ObjectRefEditorItem>
BuildObjectReferenceEditorItems(const std::string &a_currentValue) {
  std::vector<ObjectRefEditorItem> items;
  std::unordered_set<RE::FormID> seenFormIDs;

  if (auto *currentRef = LookupObjectReferenceByToken(a_currentValue)) {
    AppendObjectReferenceOption(items, seenFormIDs, currentRef, a_currentValue);
  }

  AppendObjectReferenceOption(items, seenFormIDs, ResolveCrosshairActor());
  AppendObjectReferenceOption(items, seenFormIDs,
                              RE::PlayerCharacter::GetSingleton());

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu) {
    return items;
  }

  auto &conditions = menu->GetConditions();
  for (const auto &condition : conditions) {
    if (!sfs::conditions::IsWorkbenchSelectable(condition)) {
      continue;
    }

    const auto materialized =
        sfs::conditions::MaterializeConditionById(condition.id, conditions);
    if (!materialized.has_value()) {
      continue;
    }

    for (const auto actorFormID : materialized->refreshTargets.actorFormIDs) {
      AppendObjectReferenceOption(
          items, seenFormIDs, RE::TESForm::LookupByID<RE::Actor>(actorFormID));
    }
  }

  return items;
}

bool DrawObjectReferenceParamEditor(const char *a_id, std::string &a_value,
                                    const float a_width) {
  auto editorItems = BuildObjectReferenceEditorItems(a_value);
  std::vector<ObjectRefDropdownItem> dropdownItems;
  dropdownItems.reserve(editorItems.size());

  auto *currentRef = LookupObjectReferenceByToken(a_value);
  const auto currentFormID = currentRef ? currentRef->GetFormID() : 0;
  int selectedIndex = -1;
  std::string displayValue = a_value;
  for (std::size_t index = 0; index < editorItems.size(); ++index) {
    const auto &editorItem = editorItems[index];
    dropdownItems.push_back(editorItem.item);
    if (selectedIndex < 0 &&
        ((currentFormID != 0 && editorItem.formID == currentFormID) ||
         (editorItem.item.value.has_value() &&
          sfs::strings::EqualsInsensitive(*editorItem.item.value, a_value)))) {
      selectedIndex = static_cast<int>(index);
      displayValue = editorItem.item.label;
    }
  }

  if (selectedIndex < 0 && currentRef) {
    displayValue = BuildObjectReferenceLabel(currentRef);
  }

  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "%s", displayValue.c_str());
  std::optional<std::string> selectedValue;
  const bool changed = sfs::ui::components::DrawEditableStringDropdown(
      a_id,
      sfs::ui::Localization::GetSingleton()->GetCStr("conditions.select_value"),
      buffer, sizeof(buffer),
      std::span<const ObjectRefDropdownItem>(dropdownItems.data(),
                                             dropdownItems.size()),
      a_width, &selectedIndex, &selectedValue);

  if (!changed) {
    return false;
  }

  if (selectedValue.has_value()) {
    a_value = *selectedValue;
    return true;
  }

  const auto customToken = ExtractObjectReferenceTokenFromLabel(buffer);
  if (auto *customRef = LookupObjectReferenceByToken(customToken)) {
    a_value = BuildObjectReferenceToken(customRef);
    if (a_value.empty()) {
      a_value = customToken;
    }
    return true;
  }

  return false;
}
} // namespace

namespace sfs::ui::condition_editor {
ValueEditorKind GetEditorKindForParamType(const RE::SCRIPT_PARAM_TYPE a_type) {
  return GetEditorKindForParamTypeImpl(a_type);
}

RE::SCRIPT_PARAM_TYPE
ResolveEditorParamType(std::string_view a_functionName,
                       const std::uint16_t a_paramIndex,
                       const RE::SCRIPT_PARAM_TYPE a_type) {
  // SFS conditions are evaluated against actors, so constrain GetIsID to actor
  // bases even if the runtime command table exposes the parameter more broadly.
  if (a_paramIndex == 0 &&
      CompareTextInsensitive(a_functionName, "GetIsID") == 0) {
    return RE::SCRIPT_PARAM_TYPE::kActorBase;
  }

  return a_type;
}

std::string FormatNumberString(const double a_value) {
  return FormatNumberStringImpl(a_value);
}

bool DrawNumericClauseValueEditor(const char *a_id, std::string &a_value,
                                  const ValueEditorKind a_kind,
                                  const float a_width) {
  return DrawNumericClauseValueEditorImpl(a_id, a_value, a_kind, a_width);
}

bool DrawConditionParamEditor(const char *a_id, std::string &a_value,
                              const RE::SCRIPT_PARAM_TYPE a_type,
                              const float a_width) {
  if (a_type == RE::SCRIPT_PARAM_TYPE::kObjectRef) {
    return DrawObjectReferenceParamEditor(a_id, a_value, a_width);
  }

  const auto editorKind = GetEditorKindForParamTypeImpl(a_type);
  if (editorKind == ValueEditorKind::Integer ||
      editorKind == ValueEditorKind::Number) {
    return DrawNumericClauseValueEditorImpl(a_id, a_value, editorKind, a_width);
  }

  if (editorKind == ValueEditorKind::CachedOption) {
    auto &optionCache = sfs::ui::conditions::ConditionParamOptionCache::Get();
    const auto state = optionCache.Ensure(a_type);
    if (state == sfs::ui::conditions::ConditionParamOptionCache::State::Ready) {
      const auto *options = optionCache.GetOptions(a_type);
      if (options) {
        return sfs::ui::components::DrawSearchableStringDropdown(
            a_id,
            sfs::ui::Localization::GetSingleton()->GetCStr(
                "conditions.select_value"),
            a_value, std::span<const std::string>(*options), a_width);
      }
    } else if (state ==
               sfs::ui::conditions::ConditionParamOptionCache::State::Loading) {
      const auto progress =
          std::clamp(optionCache.GetProgress(a_type), 0.0f, 1.0f);
      const auto status = optionCache.GetStatus(a_type);
      ImGui::ProgressBar(progress, ImVec2(a_width, 0.0f),
                         status.empty() ? nullptr : status.data());
      return false;
    }
  }

  ImGui::BeginDisabled();
  ImGui::SetNextItemWidth(a_width);
  char buffer[32];
  std::snprintf(
      buffer, sizeof(buffer), "%s",
      sfs::ui::Localization::GetSingleton()->GetCStr("conditions.unsupported"));
  ImGui::InputText(a_id, buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
  ImGui::EndDisabled();
  return false;
}
} // namespace sfs::ui::condition_editor
