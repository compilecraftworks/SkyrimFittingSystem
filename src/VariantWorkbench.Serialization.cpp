#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "ui/Menu.h"
#include "workbench/AutomaticEquipmentVisibility.h"
#include "workbench/ItemFactory.h"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr std::uint32_t kSerializationType = 'ROWS';
constexpr std::uint32_t kSerializationVersion = 11;
constexpr int kActorOwnershipSchemaVersion = 2;

std::string BuildRowKey(const std::string_view a_sourceKey,
                        const std::optional<std::string> &a_conditionId,
                        const RE::FormID a_ownerActorFormID = 0) {
  std::string key(a_sourceKey);
  key.append("|condition:");
  if (a_conditionId.has_value()) {
    key.append(*a_conditionId);
  } else {
    key.append("null");
  }
  if (a_ownerActorFormID != 0) {
    key.append("|actor:");
    key.append(sfs::armor::FormatFormID(a_ownerActorFormID));
  }
  return key;
}

void UpdateRowIdentity(sfs::workbench::VariantWorkbenchRow &a_row) {
  a_row.key =
      BuildRowKey(a_row.sourceKey, a_row.conditionId, a_row.ownerActorFormID);
  a_row.equipped.key = a_row.key;
}

bool SerializeRowSource(const sfs::workbench::VariantWorkbenchRow &a_row,
                        nlohmann::json &a_serializedRow) {
  a_serializedRow["type"] = a_row.IsSlotRow() ? "slot" : "armor";
  if (a_row.IsSlotRow()) {
    const auto slotNumber =
        sfs::armor::GetArmorSlotNumber(a_row.equipped.slotMask);
    if (slotNumber == 0) {
      return false;
    }
    a_serializedRow["slot"] = slotNumber;
    return true;
  }

  a_serializedRow["equipped"] = sfs::armor::GetFormIdentifier(
      RE::TESForm::LookupByID(a_row.equipped.formID));
  return !a_serializedRow["equipped"].get_ref<const std::string &>().empty();
}

bool DeserializeRowSource(const nlohmann::json &a_serializedRow,
                          sfs::workbench::VariantWorkbenchRow &a_row) {
  const auto rowType = a_serializedRow.value("type", std::string{"armor"});
  if (rowType == "slot") {
    const auto slotMask =
        sfs::armor::GetArmorSlotMask(a_serializedRow.value("slot", 0));
    sfs::workbench::EquipmentWidgetItem slotItem{};
    if (slotMask == 0 || !sfs::workbench::BuildSlotItem(slotMask, slotItem)) {
      return false;
    }

    a_row.sourceKey = slotItem.key;
    a_row.equipped = std::move(slotItem);
    return true;
  }

  const auto *equippedForm = sfs::armor::LookupByIdentifier<RE::TESObjectARMO>(
      a_serializedRow.value("equipped", std::string{}));
  sfs::workbench::EquipmentWidgetItem equipped{};
  if (!equippedForm ||
      !sfs::workbench::BuildCatalogItem(equippedForm->GetFormID(), equipped)) {
    return false;
  }

  a_row.sourceKey =
      "armor:" + sfs::armor::FormatFormID(equippedForm->GetFormID());
  a_row.equipped = std::move(equipped);
  return true;
}

struct ActorOwnershipResolutionStats {
  std::size_t remappedRows{0};
  std::size_t droppedRows{0};
  std::size_t remappedRules{0};
  std::size_t droppedRules{0};
  std::size_t remappedActorStates{0};
  std::size_t droppedActorStates{0};
};

[[nodiscard]] std::string RemapActorSuffix(
    std::string a_key, const RE::FormID a_savedActorFormID,
    const RE::FormID a_resolvedActorFormID) {
  if (a_key.empty() || a_savedActorFormID == 0 ||
      a_savedActorFormID == a_resolvedActorFormID) {
    return a_key;
  }

  const auto savedSuffix =
      "|actor:" + sfs::armor::FormatFormID(a_savedActorFormID);
  const auto suffixPosition = a_key.rfind(savedSuffix);
  if (suffixPosition == std::string::npos ||
      suffixPosition + savedSuffix.size() != a_key.size()) {
    return a_key;
  }

  a_key.replace(suffixPosition, savedSuffix.size(),
                "|actor:" +
                    sfs::armor::FormatFormID(a_resolvedActorFormID));
  return a_key;
}

[[nodiscard]] bool ResolveSavedActorFormID(
    SKSE::SerializationInterface *a_skse,
    const RE::FormID a_savedActorFormID,
    RE::FormID &a_resolvedActorFormID) {
  a_resolvedActorFormID = 0;
  return a_savedActorFormID != 0 && a_skse != nullptr &&
         a_skse->ResolveFormID(a_savedActorFormID, a_resolvedActorFormID) &&
         a_resolvedActorFormID != 0;
}

ActorOwnershipResolutionStats ResolveSerializedActorOwnership(
    nlohmann::json &a_root, SKSE::SerializationInterface *a_skse) {
  ActorOwnershipResolutionStats stats;

  if (auto &rows = a_root["rows"]; rows.is_array()) {
    nlohmann::json resolvedRows = nlohmann::json::array();
    for (auto serializedRow : rows) {
      if (!serializedRow.is_object()) {
        continue;
      }
      const auto savedActorFormID =
          serializedRow.value("ownerActorFormID", RE::FormID{0});
      if (savedActorFormID == 0) {
        resolvedRows.push_back(std::move(serializedRow));
        continue;
      }

      RE::FormID resolvedActorFormID = 0;
      if (!ResolveSavedActorFormID(a_skse, savedActorFormID,
                                   resolvedActorFormID)) {
        ++stats.droppedRows;
        continue;
      }
      serializedRow["ownerActorFormID"] = resolvedActorFormID;
      stats.remappedRows += resolvedActorFormID != savedActorFormID ? 1u : 0u;
      resolvedRows.push_back(std::move(serializedRow));
    }
    rows = std::move(resolvedRows);
  }

  if (auto &rules = a_root["conditionalVisibilityRules"]; rules.is_array()) {
    nlohmann::json resolvedRules = nlohmann::json::array();
    for (auto serializedRule : rules) {
      if (!serializedRule.is_object()) {
        continue;
      }
      const auto savedActorFormID =
          serializedRule.value("ownerActorFormID", RE::FormID{0});
      if (savedActorFormID == 0) {
        resolvedRules.push_back(std::move(serializedRule));
        continue;
      }

      RE::FormID resolvedActorFormID = 0;
      if (!ResolveSavedActorFormID(a_skse, savedActorFormID,
                                   resolvedActorFormID)) {
        ++stats.droppedRules;
        continue;
      }
      serializedRule["ownerActorFormID"] = resolvedActorFormID;
      stats.remappedRules += resolvedActorFormID != savedActorFormID ? 1u : 0u;
      resolvedRules.push_back(std::move(serializedRule));
    }
    rules = std::move(resolvedRules);
  }

  if (auto &actorStates = a_root["equippedHiddenByActor"];
      actorStates.is_array()) {
    nlohmann::json resolvedActorStates = nlohmann::json::array();
    for (auto actorState : actorStates) {
      if (!actorState.is_object()) {
        continue;
      }
      const auto savedActorFormID =
          actorState.value("formID", RE::FormID{0});
      RE::FormID resolvedActorFormID = 0;
      if (!ResolveSavedActorFormID(a_skse, savedActorFormID,
                                   resolvedActorFormID)) {
        ++stats.droppedActorStates;
        continue;
      }

      actorState["formID"] = resolvedActorFormID;
      if (auto &rowStates = actorState["rows"]; rowStates.is_array()) {
        for (auto &rowState : rowStates) {
          if (!rowState.is_object()) {
            continue;
          }
          rowState["key"] = RemapActorSuffix(
              rowState.value("key", std::string{}), savedActorFormID,
              resolvedActorFormID);
        }
      }
      stats.remappedActorStates +=
          resolvedActorFormID != savedActorFormID ? 1u : 0u;
      resolvedActorStates.push_back(std::move(actorState));
    }
    actorStates = std::move(resolvedActorStates);
  }

  return stats;
}
} // namespace

namespace sfs::workbench {
nlohmann::json VariantWorkbench::SerializeState() const {
  auto stateLock = AcquireStateLock();
  nlohmann::json root;
  root["actorOwnershipSchema"] = kActorOwnershipSchemaVersion;
  root["rows"] = nlohmann::json::array();
  root["equippedHiddenByActor"] = nlohmann::json::array();
  root["conditionalVisibilityRules"] = nlohmann::json::array();

  for (const auto &row : rows_) {
    // Preserve either half of a condition row independently, but never save a
    // row whose condition and action sides are both empty.
    const bool fullyEmptyConditionalRow =
        row.IsSlotRow() && row.conditionId.has_value() &&
        row.conditionId->empty() && !row.HasOverridesOrHideState();
    if (fullyEmptyConditionalRow ||
        (!row.HasOverridesOrHideState() && !row.conditionId.has_value())) {
      continue;
    }

    nlohmann::json serializedRow;
    if (!SerializeRowSource(row, serializedRow)) {
      continue;
    }
    if (row.conditionId.has_value()) {
      serializedRow["conditionId"] = *row.conditionId;
    } else {
      serializedRow["conditionId"] = nullptr;
    }
    if (row.ownerActorFormID != 0) {
      serializedRow["ownerActorFormID"] = row.ownerActorFormID;
    }
    serializedRow["registrationOrder"] = row.registrationOrder;
    serializedRow["hideEquipped"] = row.hideEquipped;
    serializedRow["overrides"] = nlohmann::json::array();
    for (const auto &overrideItem : row.overrides) {
      if (const auto *overrideForm =
              RE::TESForm::LookupByID(overrideItem.formID)) {
        const auto identifier = armor::GetFormIdentifier(overrideForm);
        if (identifier.empty()) {
          continue;
        }

        const bool serializeHidden = overrideItem.hidden;
        const bool serializeAutomaticBinding =
            overrideItem.automaticEquipmentBindingMode ==
                static_cast<std::uint8_t>(
                    AutomaticEquipmentVisibilityMode::VanillaSlots) ||
            overrideItem.automaticEquipmentBindingMode ==
                static_cast<std::uint8_t>(
                    AutomaticEquipmentVisibilityMode::ModdingSlots);
        if (!serializeHidden && !serializeAutomaticBinding) {
          serializedRow["overrides"].push_back(identifier);
          continue;
        }

        nlohmann::json serializedOverride{{"id", identifier}};
        if (serializeHidden) {
          serializedOverride["hidden"] = true;
        }
        if (overrideItem.automaticEquipmentBindingMode ==
                static_cast<std::uint8_t>(
                    AutomaticEquipmentVisibilityMode::VanillaSlots) &&
            overrideItem.automaticEquipmentAnchorSlotMask != 0) {
          serializedOverride["automaticEquipment"] = {
              {"mode", "vanilla"},
              {"slot", armor::GetArmorSlotNumber(
                           overrideItem.automaticEquipmentAnchorSlotMask)}};
        } else if (overrideItem.automaticEquipmentBindingMode ==
                       static_cast<std::uint8_t>(
                           AutomaticEquipmentVisibilityMode::ModdingSlots) &&
                   overrideItem.automaticEquipmentAnchorSlotMask != 0) {
          serializedOverride["automaticEquipment"] = {
              {"mode", "modding"},
              {"slot", armor::GetArmorSlotNumber(
                           overrideItem.automaticEquipmentAnchorSlotMask)}};
        }
        serializedRow["overrides"].push_back(std::move(serializedOverride));
      }
    }

    root["rows"].push_back(std::move(serializedRow));
  }

  for (const auto &[actorFormID, rowStates] : equippedHiddenByActor_) {
    if (actorFormID == 0 || rowStates.empty()) {
      continue;
    }

    nlohmann::json actorState;
    actorState["formID"] = actorFormID;
    actorState["rows"] = nlohmann::json::array();
    for (const auto &[rowKey, hidden] : rowStates) {
      if (rowKey.empty()) {
        continue;
      }
      actorState["rows"].push_back({{"key", rowKey}, {"hidden", hidden}});
    }
    if (!actorState["rows"].empty()) {
      root["equippedHiddenByActor"].push_back(std::move(actorState));
    }
  }

  for (const auto &rule : conditionalVisibilityRules_) {
    const auto identifier =
        armor::GetFormIdentifier(RE::TESForm::LookupByID(rule.target.formID));
    // Save incomplete rows as well. They are intentional workbench drop
    // targets, not malformed rules; the native visibility layer ignores them
    // until both a condition and target have been set.
    if (rule.conditionId.empty() && identifier.empty()) {
      continue;
    }
    nlohmann::json serializedRule{
        {"conditionId", rule.conditionId},
        {"targetKind",
         rule.targetKind == ConditionalVisibilityTargetKind::Actual
             ? "actual"
             : "fitting"},
        {"target", identifier},
        {"visible", rule.visibleWhenTrue}};
    if (rule.ownerActorFormID != 0) {
      serializedRule["ownerActorFormID"] = rule.ownerActorFormID;
    }
    serializedRule["registrationOrder"] = rule.registrationOrder;
    root["conditionalVisibilityRules"].push_back(std::move(serializedRule));
  }

  return root;
}

bool VariantWorkbench::DeserializeState(
    const nlohmann::json &a_root,
    const std::optional<std::string> &a_missingConditionId,
    std::string *a_error, const bool a_queueArmorRefresh) {
  auto stateLock = AcquireStateLock();
  if (!a_root.is_object() || !a_root["rows"].is_array()) {
    if (a_error) {
      *a_error = "Workbench JSON is missing a rows array.";
    }
    return false;
  }

  Revert(a_queueArmorRefresh);
  needsConditionOwnershipMigration_ =
      a_root.value("actorOwnershipSchema", 0) < kActorOwnershipSchemaVersion;

  for (const auto &serializedRow : a_root["rows"]) {
    if (!serializedRow.is_object()) {
      continue;
    }

    VariantWorkbenchRow row{};
    if (!DeserializeRowSource(serializedRow, row)) {
      continue;
    }
    if (const auto conditionIt = serializedRow.find("conditionId");
        conditionIt != serializedRow.end()) {
      if (conditionIt->is_null()) {
        row.conditionId = std::nullopt;
      } else if (conditionIt->is_string()) {
        const auto conditionId = conditionIt->get<std::string>();
        // An empty string is a deliberately retained, inactive condition
        // layer. It must not collapse into the base layer on load.
        row.conditionId = conditionId;
      } else {
        row.conditionId = a_missingConditionId;
      }
    } else {
      row.conditionId = a_missingConditionId;
    }
    row.ownerActorFormID =
        serializedRow.value("ownerActorFormID", RE::FormID{0});
    if (const auto savedOrder =
            serializedRow.value("registrationOrder", std::uint64_t{0});
        savedOrder != 0) {
      row.registrationOrder = savedOrder;
      ObserveVariantWorkbenchRegistrationOrder(savedOrder);
    }
    UpdateRowIdentity(row);
    std::unordered_set<RE::FormID> seenOverrideForms;
    if (const auto &serializedOverrides = serializedRow["overrides"];
        serializedOverrides.is_array()) {
      for (const auto &overrideValue : serializedOverrides) {
        std::string identifier;
        bool hidden = false;
        if (overrideValue.is_string()) {
          identifier = overrideValue.get<std::string>();
        } else if (overrideValue.is_object()) {
          identifier = overrideValue.value("id", std::string{});
          hidden = overrideValue.value("hidden", false);
        } else {
          continue;
        }

        const auto *overrideForm =
            armor::LookupByIdentifier<RE::TESObjectARMO>(identifier);
        EquipmentWidgetItem overrideItem{};
        if (!overrideForm ||
            !sfs::workbench::BuildCatalogItem(overrideForm->GetFormID(),
                                              overrideItem) ||
            !seenOverrideForms.insert(overrideForm->GetFormID()).second) {
          continue;
        }

        overrideItem.hidden = hidden;
        if (overrideValue.is_object()) {
          const auto automaticIt = overrideValue.find("automaticEquipment");
          if (automaticIt != overrideValue.end() && automaticIt->is_object()) {
            const auto mode = automaticIt->value("mode", std::string{});
            if (mode == "vanilla") {
              const auto slotMask = armor::GetArmorSlotMask(
                  automaticIt->value("slot", std::uint32_t{0}));
              if (slotMask != 0) {
                overrideItem.automaticEquipmentBindingMode =
                    static_cast<std::uint8_t>(
                        AutomaticEquipmentVisibilityMode::VanillaSlots);
                overrideItem.automaticEquipmentAnchorSlotMask = slotMask;
              }
            } else if (mode == "modding") {
              auto slotMask = armor::GetArmorSlotMask(
                  automaticIt->value("slot", std::uint32_t{0}));
              // Migrate the short-lived development format that stored a
              // frozen armor FormID to the armor's primary occupied slot.
              if (slotMask == 0) {
                const auto *anchorArmor =
                    armor::LookupByIdentifier<RE::TESObjectARMO>(
                        automaticIt->value("armor", std::string{}));
                if (anchorArmor) {
                  const auto anchorMask =
                      armor::GetArmorDisplaySlotMask(anchorArmor);
                  for (std::uint32_t slotNumber =
                           kAutomaticEquipmentFirstSlot;
                       slotNumber <= kAutomaticEquipmentLastSlot;
                       ++slotNumber) {
                    const auto candidate = armor::GetArmorSlotMask(slotNumber);
                    if ((anchorMask & candidate) != 0) {
                      slotMask = candidate;
                      break;
                    }
                  }
                }
              }
              if (slotMask != 0) {
                overrideItem.automaticEquipmentBindingMode =
                    static_cast<std::uint8_t>(
                        AutomaticEquipmentVisibilityMode::ModdingSlots);
                overrideItem.automaticEquipmentAnchorFormID = 0;
                overrideItem.automaticEquipmentAnchorSlotMask = slotMask;
              }
            }
          }
        }
        row.overrides.push_back(std::move(overrideItem));
      }
    }

    row.hideEquipped = serializedRow.value("hideEquipped", false);
    // Keep a half-complete row, but migrate away old save data containing a
    // condition row with neither side filled.
    const bool fullyEmptyConditionalRow =
        row.IsSlotRow() && row.conditionId.has_value() &&
        row.conditionId->empty() && !row.HasOverridesOrHideState();
    if (fullyEmptyConditionalRow ||
        (!row.HasOverridesOrHideState() && !row.conditionId.has_value())) {
      continue;
    }

    rows_.push_back(std::move(row));
    rowOrder_.push_back(rows_.back().key);
  }

  equippedHiddenByActor_.clear();
  if (const auto &actorStates = a_root["equippedHiddenByActor"];
      actorStates.is_array()) {
    for (const auto &actorState : actorStates) {
      if (!actorState.is_object()) {
        continue;
      }
      const auto actorFormID = actorState.value("formID", RE::FormID{0});
      if (actorFormID == 0) {
        continue;
      }
      const auto &rowStates = actorState["rows"];
      if (!rowStates.is_array()) {
        continue;
      }
      auto &hiddenRows = equippedHiddenByActor_[actorFormID];
      for (const auto &rowState : rowStates) {
        if (!rowState.is_object()) {
          continue;
        }
        const auto rowKey = rowState.value("key", std::string{});
        if (rowKey.empty()) {
          continue;
        }
        hiddenRows[rowKey] = rowState.value("hidden", false);
      }
      if (hiddenRows.empty()) {
        equippedHiddenByActor_.erase(actorFormID);
      }
    }
  }

  conditionalVisibilityRules_.clear();
  if (const auto &serializedRules = a_root["conditionalVisibilityRules"];
      serializedRules.is_array()) {
    for (const auto &serializedRule : serializedRules) {
      if (!serializedRule.is_object()) {
        continue;
      }
      const auto conditionId =
          serializedRule.value("conditionId", std::string{});
      const auto targetKindText =
          serializedRule.value("targetKind", std::string{});
      const auto *targetForm = armor::LookupByIdentifier<RE::TESObjectARMO>(
          serializedRule.value("target", std::string{}));
      if ((conditionId.empty() &&
           serializedRule.value("target", std::string{}).empty()) ||
          (targetKindText != "actual" && targetKindText != "fitting")) {
        continue;
      }
      EquipmentWidgetItem target{};
      if (targetForm != nullptr &&
          !BuildCatalogItem(targetForm->GetFormID(), target)) {
        continue;
      }
      ConditionalVisibilityRule rule{
          .conditionId = conditionId,
          .ownerActorFormID =
              serializedRule.value("ownerActorFormID", RE::FormID{0}),
          .targetKind = targetKindText == "actual"
                            ? ConditionalVisibilityTargetKind::Actual
                            : ConditionalVisibilityTargetKind::Fitting,
          .target = std::move(target),
          .visibleWhenTrue = serializedRule.value("visible", true)};
      if (const auto savedOrder =
              serializedRule.value("registrationOrder", std::uint64_t{0});
          savedOrder != 0) {
        rule.registrationOrder = savedOrder;
        ObserveVariantWorkbenchRegistrationOrder(savedOrder);
      }
      conditionalVisibilityRules_.push_back(std::move(rule));
    }
  }

  // Protection is destructive only for registered appearances. Loading an
  // older save or importing JSON must obey the same rule as changing the
  // option in the live menu, while actual equipment remains untouched.
  static_cast<void>(RemoveProtectedAppearanceRegistrations());
  MarkChanged();
  return true;
}

bool VariantWorkbench::MigrateLegacyConditionAssignments(
    std::vector<conditions::Definition> &a_conditions,
    const RE::FormID a_defaultOwnerActorFormID) {
  auto stateLock = AcquireStateLock();
  auto *menu = sfs::Menu::GetSingleton();
  auto conditionStateLock =
      menu != nullptr ? menu->AcquireConditionStateLock()
                      : sfs::Menu::ConditionStateLock{};
  if (!needsConditionOwnershipMigration_) {
    return false;
  }

  std::vector<VariantWorkbenchRow> migratedRows;
  migratedRows.reserve(rows_.size());
  std::unordered_map<std::string, std::size_t> rowIndexByKey;
  std::unordered_map<std::string, std::string> rowKeyRedirects;
  std::size_t clearedConditionCount = 0;
  std::size_t mergedRowCount = 0;

  for (auto &row : rows_) {
    const auto oldKey = row.key;
    auto ownerActorFormID = row.ownerActorFormID;
    if (ownerActorFormID == 0 && row.conditionId.has_value()) {
      if (const auto materialized = conditions::MaterializeConditionById(
              *row.conditionId, a_conditions);
          materialized.has_value() &&
          materialized->refreshTargets.actorFormIDs.size() == 1) {
        ownerActorFormID = materialized->refreshTargets.actorFormIDs.front();
      }
    }
    if (ownerActorFormID == 0) {
      ownerActorFormID = a_defaultOwnerActorFormID;
    }

    clearedConditionCount += row.conditionId.has_value() ? 1u : 0u;
    row.conditionId.reset();
    row.ownerActorFormID = ownerActorFormID;
    UpdateRowIdentity(row);
    if (!oldKey.empty() && oldKey != row.key) {
      rowKeyRedirects.insert_or_assign(oldKey, row.key);
    }

    if (const auto existing = rowIndexByKey.find(row.key);
        existing != rowIndexByKey.end()) {
      auto &target = migratedRows[existing->second];
      target.hideEquipped = target.hideEquipped || row.hideEquipped;
      for (auto &overrideItem : row.overrides) {
        const auto duplicate =
            std::ranges::find(target.overrides, overrideItem.formID,
                              &EquipmentWidgetItem::formID);
        if (duplicate == target.overrides.end()) {
          target.overrides.push_back(std::move(overrideItem));
        } else {
          duplicate->hidden = duplicate->hidden && overrideItem.hidden;
        }
      }
      ++mergedRowCount;
      continue;
    }

    rowIndexByKey.emplace(row.key, migratedRows.size());
    migratedRows.push_back(std::move(row));
  }

  rows_ = std::move(migratedRows);
  for (auto &actorState : equippedHiddenByActor_) {
    std::unordered_map<std::string, bool> migratedHiddenRows;
    for (const auto &[rowKey, hidden] : actorState.second) {
      const auto redirect = rowKeyRedirects.find(rowKey);
      const auto &migratedKey =
          redirect != rowKeyRedirects.end() ? redirect->second : rowKey;
      const auto [entry, inserted] =
          migratedHiddenRows.emplace(migratedKey, hidden);
      if (!inserted) {
        entry->second = entry->second || hidden;
      }
    }
    actorState.second = std::move(migratedHiddenRows);
  }
  needsConditionOwnershipMigration_ = false;
  RebuildRowOrder();
  MarkChanged();
  logger::info(
      "Migrated legacy SFS condition assignments to actor-owned rows: rows={}, "
      "conditionsCleared={}, rowsMerged={}",
      rows_.size(), clearedConditionCount, mergedRowCount);
  return true;
}

void VariantWorkbench::ReplaceState(VariantWorkbench &&a_source) {
  auto stateLock = AcquireStateLock();
  ClearPreview();
  rows_ = std::move(a_source.rows_);
  conditionalVisibilityRules_ = std::move(a_source.conditionalVisibilityRules_);
  rowOrder_ = std::move(a_source.rowOrder_);
  equippedHiddenByActor_ = std::move(a_source.equippedHiddenByActor_);
  needsConditionOwnershipMigration_ =
      a_source.needsConditionOwnershipMigration_;
  MarkChanged();
}

void VariantWorkbench::Serialize(SKSE::SerializationInterface *a_skse) const {
  auto stateLock = AcquireStateLock();
  const auto root = SerializeState();
  const auto payload = root.dump();
  a_skse->WriteRecord(kSerializationType, kSerializationVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

std::uint32_t VariantWorkbench::Deserialize(
    SKSE::SerializationInterface *a_skse,
    std::optional<std::string> a_missingConditionId) {
  auto stateLock = AcquireStateLock();
  Revert(false);

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    return 0;
  }

  if (type != kSerializationType) {
    return 0;
  }

  if (version != 2 && version != 3 && version != 4 && version != 5 &&
      version != 6 && version != 7 && version != 8 && version != 9 &&
      version != 10 && version != kSerializationVersion) {
    logger::warn("Skipping SFS serialized rows from unsupported version {}",
                 version);
    return 0;
  }

  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::error("Failed to read SFS serialized workbench payload");
    return 0;
  }

  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object() || !root["rows"].is_array()) {
    logger::error("Failed to parse SFS serialized workbench payload");
    return 0;
  }

  auto resolvedRoot = root;
  const auto ownershipResolution =
      ResolveSerializedActorOwnership(resolvedRoot, a_skse);
  if (ownershipResolution.remappedRows != 0 ||
      ownershipResolution.droppedRows != 0 ||
      ownershipResolution.remappedRules != 0 ||
      ownershipResolution.droppedRules != 0 ||
      ownershipResolution.remappedActorStates != 0 ||
      ownershipResolution.droppedActorStates != 0) {
    logger::info(
        "Resolved SFS workbench actor ownership: rowsRemapped={} "
        "rowsDropped={} rulesRemapped={} rulesDropped={} "
        "actorStatesRemapped={} actorStatesDropped={}",
        ownershipResolution.remappedRows, ownershipResolution.droppedRows,
        ownershipResolution.remappedRules, ownershipResolution.droppedRules,
        ownershipResolution.remappedActorStates,
        ownershipResolution.droppedActorStates);
  }

  if (version < 4) {
    auto migratedRoot = std::move(resolvedRoot);
    try {
      for (auto &serializedRow : migratedRoot["rows"]) {
        if (serializedRow.is_object()) {
          if (a_missingConditionId.has_value()) {
            serializedRow["conditionId"] = *a_missingConditionId;
          } else {
            serializedRow["conditionId"] = nullptr;
          }
        }
      }
      static_cast<void>(
          DeserializeState(migratedRoot, a_missingConditionId, nullptr, false));
    } catch (const std::exception &exception) {
      logger::error("Failed to load SFS serialized workbench payload: {}",
                    exception.what());
      Revert(false);
    }
    return version;
  }

  try {
    static_cast<void>(
        DeserializeState(resolvedRoot, a_missingConditionId, nullptr, false));
  } catch (const std::exception &exception) {
    logger::error("Failed to load SFS serialized workbench payload: {}",
                  exception.what());
    Revert(false);
  }
  return version;
}
} // namespace sfs::workbench
