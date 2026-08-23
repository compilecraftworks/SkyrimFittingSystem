#include "poc/DeviousDevicesHiderPoC.h"

#if defined(SFS_VIRTUAL_TOKEN_POC)

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "VariantWorkbench.h"
#include "conditions/Status.h"
#include "native/FittingSlotState.h"
#include "poc/VirtualWornTokenPoC.h"
#include "ui/Menu.h"
#include "workbench/AutomaticEquipmentVisibility.h"
#include "workbench/EquipmentRefreshEventSink.h"

#include <array>
#include <cctype>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr auto kDevicesPlugin = "Devious Devices - Integration.esm"sv;
constexpr RE::FormID kDevicesUnderneathQuestLocalID = 0x41472;
constexpr auto kDevicesUnderneathScript = "zadDevicesUnderneathScript"sv;
constexpr std::size_t kSlotMaskFilterCount = 128;

struct HiderSettings {
  std::array<std::uint32_t, kSlotMaskFilterCount> filters{};
  bool available{false};
  bool enabled{false};
};

struct LatchedAppearanceSuppression {
  std::uint64_t sourceSlotMask{0};
};

struct StripLinkPolicySignature {
  sfs::workbench::ExternalModStripLinkMode configuredMode{
      sfs::workbench::ExternalModStripLinkMode::Disabled};
  sfs::workbench::ExternalModStripLinkMode customBaseMode{
      sfs::workbench::ExternalModStripLinkMode::ModSettingsSlots};
  sfs::workbench::ExternalModStripLinkMode directAutomaticBaseMode{
      sfs::workbench::ExternalModStripLinkMode::ModSettingsSlots};
  std::uint64_t disabledAppearanceSlotMask{0};
  sfs::workbench::AutomaticEquipmentSlotMappings directMappings{};
  sfs::workbench::AutomaticEquipmentSlotOverrides directOverrides{};

  bool operator==(const StripLinkPolicySignature &) const = default;
};

std::mutex g_settingsMutex;
HiderSettings g_settings;
std::unordered_map<RE::FormID, std::unordered_set<RE::FormID>>
    g_pendingRenderedDeviceVisibility;
std::unordered_map<RE::FormID, std::unordered_set<RE::FormID>>
    g_ordinaryVisibleRenderedDevices;
std::unordered_map<RE::FormID,
                   std::unordered_map<std::uint32_t,
                                       LatchedAppearanceSuppression>>
    g_latchedSuppressedAppearanceSources;
// Direct rendered-device conflicts survive eye overrides only until the DD
// cycle settles. They are actor-local because the same fitting slot may be in
// a different DD lifecycle on another actor.
std::unordered_map<
    RE::FormID,
    std::unordered_map<std::uint32_t, std::unordered_set<RE::FormID>>>
    g_directDeviceCycleAppearances;
std::unordered_map<RE::FormID, std::uint64_t> g_redressedSourceSlotMasks;
std::unordered_map<RE::FormID, std::uint32_t>
    g_previousRenderedDeviceSlotMasks;
std::optional<StripLinkPolicySignature> g_latchedStripLinkPolicy;

[[nodiscard]] bool IsDeviousDevicesInstalled() {
  auto *data = RE::TESDataHandler::GetSingleton();
  return data != nullptr &&
         data->LookupForm<RE::TESQuest>(kDevicesUnderneathQuestLocalID,
                                        kDevicesPlugin) != nullptr;
}

[[nodiscard]] bool IsDeviousDevicesRuntimeAvailable() {
  if (!IsDeviousDevicesInstalled()) {
    return false;
  }
  std::lock_guard lock(g_settingsMutex);
  return g_settings.available;
}

[[nodiscard]] StripLinkPolicySignature CurrentStripLinkPolicySignature() {
  return {
      .configuredMode = sfs::workbench::GetExternalModStripLinkMode(),
      .customBaseMode =
          sfs::workbench::GetCustomExternalModStripLinkBaseMode(),
      .directAutomaticBaseMode =
          sfs::workbench::GetCustomDirectStripLinkAutomaticBaseMode(),
      .disabledAppearanceSlotMask =
          sfs::workbench::GetCustomStripLinkDisabledAppearanceSlotMask(),
      .directMappings =
          sfs::workbench::GetCustomDirectStripLinkMappings(),
      .directOverrides =
          sfs::workbench::GetCustomDirectStripLinkOverrides()};
}

[[nodiscard]] std::uint32_t SlotMaskForNumber(const std::uint32_t a_slot) {
  return static_cast<std::uint32_t>(sfs::armor::GetArmorSlotMask(a_slot));
}

[[nodiscard]] bool IsPlayerActor(const RE::Actor *a_actor) {
  const auto *player = RE::PlayerCharacter::GetSingleton();
  return a_actor && player && a_actor->GetFormID() == player->GetFormID();
}

[[nodiscard]] std::string ToLower(const std::string_view a_value) {
  std::string lower;
  lower.reserve(a_value.size());
  for (const auto ch : a_value) {
    lower.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return lower;
}

[[nodiscard]] bool ContainsInsensitive(const std::string_view a_value,
                                       const std::string_view a_needle) {
  return ToLower(a_value).find(ToLower(a_needle)) != std::string::npos;
}

[[nodiscard]] bool IsRenderedDevice(const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return false;
  }

  const auto pluginName = sfs::armor::GetPluginName(a_armor);
  const bool fromDeviousPlugin =
      ContainsInsensitive(pluginName, "Devious Devices");
  bool hasRenderedDeviceKeyword = false;
  bool hasInventoryOrHiderKeyword = false;

  for (const auto *keyword : a_armor->GetKeywords()) {
    if (!keyword) {
      continue;
    }
    const auto editorID = ToLower(sfs::armor::GetEditorID(keyword));
    if (editorID == "zad_inventorydevice" ||
        editorID.find("devicehider") != std::string::npos ||
        editorID.find("device_hider") != std::string::npos) {
      hasInventoryOrHiderKeyword = true;
    } else if (editorID.starts_with("zad_devious") ||
               editorID.starts_with("zadx_devious") ||
               editorID.starts_with("zadc_devious") ||
               editorID == "zad_lockable" ||
               editorID.find("_devious") != std::string::npos) {
      hasRenderedDeviceKeyword = true;
    }
  }

  const auto armorEditorID = ToLower(sfs::armor::GetEditorID(a_armor));
  if (armorEditorID.find("devicehider") != std::string::npos ||
      armorEditorID.find("device_hider") != std::string::npos ||
      (fromDeviousPlugin &&
       armorEditorID.find("inventory") != std::string::npos)) {
    hasInventoryOrHiderKeyword = true;
  }
  if (hasInventoryOrHiderKeyword) {
    return false;
  }
  return hasRenderedDeviceKeyword ||
         (fromDeviousPlugin &&
          armorEditorID.find("rendered") != std::string::npos);
}

[[nodiscard]] bool IsDeviousInventoryOrHider(
    const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return false;
  }
  const auto pluginName = sfs::armor::GetPluginName(a_armor);
  const auto armorEditorID = ToLower(sfs::armor::GetEditorID(a_armor));
  if (armorEditorID.find("devicehider") != std::string::npos ||
      armorEditorID.find("device_hider") != std::string::npos ||
      (ContainsInsensitive(pluginName, "Devious Devices") &&
       armorEditorID.find("inventory") != std::string::npos)) {
    return true;
  }
  return std::ranges::any_of(a_armor->GetKeywords(), [](const auto *a_keyword) {
    return a_keyword != nullptr &&
           ToLower(sfs::armor::GetEditorID(a_keyword)) ==
               "zad_inventorydevice";
  });
}

[[nodiscard]] bool
IsConditionActiveForActor(const std::optional<std::string> &a_conditionID,
                          RE::Actor *a_actor) {
  if (!a_conditionID || !a_actor) {
    return false;
  }
  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return false;
  }
  auto conditionStateLock = menu->AcquireConditionStateLock();
  auto &conditions = menu->GetConditions();
  const auto *definition =
      sfs::conditions::FindDefinitionById(conditions, *a_conditionID);
  if (!definition || !sfs::conditions::IsWorkbenchSelectable(*definition) ||
      !sfs::conditions::EvaluateDefinitionStatus(*definition, conditions)
           .IsActive()) {
    return false;
  }
  auto materialized =
      sfs::conditions::MaterializeConditionById(*a_conditionID, conditions);
  return materialized && materialized->condition &&
         materialized->condition->IsTrue(a_actor, a_actor);
}

[[nodiscard]] bool
IsRowActiveForActor(const sfs::workbench::VariantWorkbenchRow &a_row,
                    RE::Actor *a_actor) {
  if (!a_row.IsOwnedByActor(a_actor)) {
    return false;
  }
  return a_row.conditionId
             ? IsConditionActiveForActor(a_row.conditionId, a_actor)
             : a_row.HasOwnerActor() || IsPlayerActor(a_actor);
}

[[nodiscard]] std::uint32_t
GetConfiguredTargetMask(const std::uint32_t a_sourceMask) {
  HiderSettings settings;
  {
    std::lock_guard lock(g_settingsMutex);
    settings = g_settings;
  }
  if (!settings.available || !settings.enabled) {
    return 0;
  }

  std::uint32_t targetMask = 0;
  for (std::uint32_t sourceSlot = 30; sourceSlot <= 61; ++sourceSlot) {
    if ((a_sourceMask & SlotMaskForNumber(sourceSlot)) == 0) {
      continue;
    }
    const auto filterIndex = static_cast<std::size_t>(sourceSlot - 30) * 4;
    for (std::size_t lane = 0; lane < 4; ++lane) {
      targetMask |= settings.filters[filterIndex + lane];
    }
  }
  return targetMask;
}

struct HiderScan {
  struct Appearance {
    const RE::TESObjectARMO *armor{nullptr};
    std::uint32_t visualSlotMask{0};
    std::uint64_t automaticAnchorSlotMask{0};
  };
  std::uint32_t sourceSlotMask{0};
  // Only genuinely equipped DD rendered devices belong here. A registered
  // SFS appearance may be a Hider source, but it must never start/settle an
  // actual restraint cycle or claim direct physical slot occupancy.
  std::uint32_t equippedRenderedDeviceSlotMask{0};
  std::vector<Appearance> appearances;
};

[[nodiscard]] HiderScan CollectRows(
    RE::Actor *a_actor,
    const std::vector<sfs::workbench::VariantWorkbenchRow> &a_rows,
    const bool a_ignoreConditions,
    const std::uint32_t a_existingSuppressionMask,
    std::uint32_t &a_occupiedDisplaySlots) {
  HiderScan scan;

  std::vector<const sfs::workbench::VariantWorkbenchRow *> orderedRows;
  orderedRows.reserve(a_rows.size());
  for (const auto &row : a_rows) {
    orderedRows.push_back(&row);
  }
  std::stable_partition(orderedRows.begin(), orderedRows.end(),
                        [](const auto *a_row) {
                          return a_row->HasCondition();
                        });

  for (const auto *rowPtr : orderedRows) {
    const auto &row = *rowPtr;
    if (!row.IsOwnedByActor(a_actor) ||
        (!a_ignoreConditions && !IsRowActiveForActor(row, a_actor))) {
      continue;
    }

    if (row.isEquipped && !row.equipped.IsSlot()) {
      const auto equippedMask =
          static_cast<std::uint32_t>(row.equipped.slotMask);
      const auto *equippedArmor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
      const bool renderedDevice = IsRenderedDevice(equippedArmor);
      bool hidden = row.hideEquipped;
      if (auto *menu = sfs::Menu::GetSingleton()) {
        hidden = menu->GetWorkbench().ResolveEquippedHiddenForActor(a_actor,
                                                                    row);
      }
      if (!hidden || renderedDevice) {
        scan.sourceSlotMask |= equippedMask;
      }
      if (renderedDevice) {
        scan.equippedRenderedDeviceSlotMask |= equippedMask;
      }
    }

    for (const auto &item : row.overrides) {
      const auto visualMask = static_cast<std::uint32_t>(
          row.GetOverrideVisualSlotMask(item));
      if (visualMask == 0 || item.hidden ||
          (visualMask & a_existingSuppressionMask) != 0 ||
          (visualMask & a_occupiedDisplaySlots) != 0) {
        continue;
      }
      a_occupiedDisplaySlots |= visualMask;
      scan.sourceSlotMask |= visualMask;

      const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(item.formID);
      scan.appearances.push_back({.armor = armor,
                                  .visualSlotMask = visualMask,
                                  .automaticAnchorSlotMask =
                                      item.automaticEquipmentAnchorSlotMask});
    }
  }
  return scan;
}
} // namespace

namespace sfs::poc {
void ResetDeviousDevicesHider() {
  std::lock_guard lock(g_settingsMutex);
  g_settings = {};
  g_pendingRenderedDeviceVisibility.clear();
  g_ordinaryVisibleRenderedDevices.clear();
  g_latchedSuppressedAppearanceSources.clear();
  g_directDeviceCycleAppearances.clear();
  g_redressedSourceSlotMasks.clear();
  g_previousRenderedDeviceSlotMasks.clear();
  g_latchedStripLinkPolicy.reset();
}

bool IsDeviousDevicesRenderedDevice(const RE::TESObjectARMO *a_armor) {
  return IsDeviousDevicesRuntimeAvailable() && IsRenderedDevice(a_armor);
}

bool IsDeviousDevicesEquipmentTransactionArmor(
    const RE::TESObjectARMO *a_armor) {
  // Do not depend on the Hider settings having finished their Papyrus sync.
  // The generic equipment observer can see DD calls before that happens, and
  // classification is still safe when DD is absent because the integration
  // quest lookup then fails.
  return IsDeviousDevicesInstalled() &&
         (IsRenderedDevice(a_armor) || IsDeviousInventoryOrHider(a_armor));
}

void ObserveDeviousDevicesRenderedDeviceEquipEvent(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor, const bool a_equipped) {
  if (!a_actor || !a_armor) {
    return;
  }
  if (!IsDeviousDevicesRuntimeAvailable()) {
    return;
  }
  if (sfs::workbench::GetExternalModStripLinkMode() ==
      sfs::workbench::ExternalModStripLinkMode::Disabled) {
    return;
  }
  if (!IsRenderedDevice(a_armor)) {
    if (a_equipped && !IsDeviousInventoryOrHider(a_armor)) {
      const auto sourceMask =
          sfs::workbench::GetAutomaticEquipmentControlSlotMask(a_armor);
      ReleaseDeviousDevicesHiderSuppressionForSourceMask(
          a_actor->GetFormID(), sourceMask);
    }
    return;
  }
  std::lock_guard lock(g_settingsMutex);
  auto &pending = g_pendingRenderedDeviceVisibility[a_actor->GetFormID()];
  if (a_equipped) {
    // A newly equipped rendered device begins a new actor-local DD cycle.
    // Any source observed as redressed during the preceding cycle may be
    // suppressed again by this device.
    g_redressedSourceSlotMasks.erase(a_actor->GetFormID());
    pending.insert(a_armor->GetFormID());
  } else {
    pending.erase(a_armor->GetFormID());
    if (auto visible =
            g_ordinaryVisibleRenderedDevices.find(a_actor->GetFormID());
        visible != g_ordinaryVisibleRenderedDevices.end()) {
      visible->second.erase(a_armor->GetFormID());
      if (visible->second.empty()) {
        g_ordinaryVisibleRenderedDevices.erase(visible);
      }
    }
    if (pending.empty()) {
      g_pendingRenderedDeviceVisibility.erase(a_actor->GetFormID());
    }
  }
}

void ReconcileDeviousDevicesRenderedDeviceVisibility(RE::Actor *a_actor) {
  if (!a_actor || !IsDeviousDevicesRuntimeAvailable() ||
      sfs::workbench::GetExternalModStripLinkMode() ==
                       sfs::workbench::ExternalModStripLinkMode::Disabled) {
    return;
  }
  const auto actorID = a_actor->GetFormID();
  std::unordered_set<RE::FormID> pending;
  {
    std::lock_guard lock(g_settingsMutex);
    const auto it = g_pendingRenderedDeviceVisibility.find(actorID);
    if (it == g_pendingRenderedDeviceVisibility.end()) {
      return;
    }
    pending = it->second;
  }

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu) {
    return;
  }
  auto &workbench = menu->GetWorkbench();
  auto workbenchStateLock = workbench.AcquireStateLock();
  const auto &rows = workbench.GetRows();
  std::unordered_set<RE::FormID> initialized;
  for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
    const auto &row = rows[rowIndex];
    if (!row.IsOwnedByActor(a_actor) || !row.isEquipped || row.IsSlotRow() ||
        !pending.contains(row.equipped.formID)) {
      continue;
    }
    static_cast<void>(workbench.SetEquippedHiddenForActor(
        actorID, static_cast<int>(rowIndex), false));
    initialized.insert(row.equipped.formID);
  }
  if (initialized.empty()) {
    return;
  }
  {
    std::lock_guard lock(g_settingsMutex);
    auto it = g_pendingRenderedDeviceVisibility.find(actorID);
    if (it != g_pendingRenderedDeviceVisibility.end()) {
      for (const auto armorID : initialized) {
        it->second.erase(armorID);
      }
      if (it->second.empty()) {
        g_pendingRenderedDeviceVisibility.erase(it);
      }
    }
    auto &visible = g_ordinaryVisibleRenderedDevices[actorID];
    visible.insert(initialized.begin(), initialized.end());
  }
  logger::info("DD rendered device initialized to ordinary visible state "
               "actor={:08X} devices={}",
               actorID, initialized.size());
}

bool IsDeviousDevicesRenderedDeviceOrdinaryVisible(
    const RE::FormID a_actorFormID, const RE::FormID a_armorFormID) {
  if (a_actorFormID == 0 || a_armorFormID == 0 ||
      !IsDeviousDevicesRuntimeAvailable() ||
      sfs::workbench::GetExternalModStripLinkMode() ==
          sfs::workbench::ExternalModStripLinkMode::Disabled) {
    return false;
  }
  std::lock_guard lock(g_settingsMutex);
  const auto actor = g_ordinaryVisibleRenderedDevices.find(a_actorFormID);
  return actor != g_ordinaryVisibleRenderedDevices.end() &&
         actor->second.contains(a_armorFormID);
}

void SetDeviousDevicesRenderedDeviceUserVisible(
    const RE::FormID a_actorFormID, const RE::FormID a_armorFormID,
    const bool a_visible) {
  if (a_actorFormID == 0 || a_armorFormID == 0) {
    return;
  }
  if (!IsDeviousDevicesRuntimeAvailable()) {
    return;
  }
  std::lock_guard lock(g_settingsMutex);
  auto &visible = g_ordinaryVisibleRenderedDevices[a_actorFormID];
  if (a_visible) {
    visible.insert(a_armorFormID);
  } else {
    visible.erase(a_armorFormID);
    if (visible.empty()) {
      g_ordinaryVisibleRenderedDevices.erase(a_actorFormID);
    }
  }
}

void ClearDeviousDevicesRenderedDeviceOrdinaryVisibility(
    const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_settingsMutex);
  g_ordinaryVisibleRenderedDevices.erase(a_actorFormID);
}

void ReleaseDeviousDevicesHiderSuppressionForSourceMask(
    const RE::FormID a_actorFormID, const std::uint64_t a_sourceSlotMask) {
  if (a_actorFormID == 0 || a_sourceSlotMask == 0 ||
      !IsDeviousDevicesRuntimeAvailable()) {
    return;
  }
  std::lock_guard lock(g_settingsMutex);
  g_redressedSourceSlotMasks[a_actorFormID] |= a_sourceSlotMask;
  const auto actor =
      g_latchedSuppressedAppearanceSources.find(a_actorFormID);
  if (actor == g_latchedSuppressedAppearanceSources.end()) {
    return;
  }
  for (auto suppression = actor->second.begin();
       suppression != actor->second.end();) {
    suppression->second.sourceSlotMask &= ~a_sourceSlotMask;
    if (suppression->second.sourceSlotMask == 0) {
      suppression = actor->second.erase(suppression);
      continue;
    }
    ++suppression;
  }
  if (actor->second.empty()) {
    g_latchedSuppressedAppearanceSources.erase(actor);
  }
}

void ReleaseDeviousDevicesHiderSuppressionForAppearanceMask(
    const RE::FormID a_actorFormID,
    const std::uint64_t a_appearanceSlotMask) {
  if (a_actorFormID == 0 || a_appearanceSlotMask == 0 ||
      !IsDeviousDevicesRuntimeAvailable()) {
    return;
  }
  std::lock_guard lock(g_settingsMutex);
  const auto actor =
      g_latchedSuppressedAppearanceSources.find(a_actorFormID);
  if (actor == g_latchedSuppressedAppearanceSources.end()) {
    return;
  }
  std::uint64_t releasedSourceMask = 0;
  std::erase_if(actor->second, [&](const auto &a_entry) {
    if ((a_entry.first & a_appearanceSlotMask) == 0) {
      return false;
    }
    releasedSourceMask |= a_entry.second.sourceSlotMask;
    return true;
  });
  if (releasedSourceMask != 0) {
    g_redressedSourceSlotMasks[a_actorFormID] |= releasedSourceMask;
  }
  if (actor->second.empty()) {
    g_latchedSuppressedAppearanceSources.erase(actor);
  }
}

void InitializeDeviousDevicesHider() {
  ResetDeviousDevicesHider();
  static_cast<void>(RefreshDeviousDevicesHiderSettings());
}

bool UpdateDeviousDevicesHiderSettings(
    const std::vector<std::int32_t> &a_slotMaskFilters,
    const std::int32_t a_setting) {
  if (!IsDeviousDevicesInstalled()) {
    return false;
  }
  if (a_slotMaskFilters.size() != kSlotMaskFilterCount) {
    logger::warn("Ignored DD NG Hider settings: expected {} filters, got {}",
                 kSlotMaskFilterCount, a_slotMaskFilters.size());
    return false;
  }

  HiderSettings next{.available = true, .enabled = a_setting != 0};
  for (std::size_t index = 0; index < next.filters.size(); ++index) {
    next.filters[index] =
        static_cast<std::uint32_t>(a_slotMaskFilters[index]);
  }
  bool changed = false;
  {
    std::lock_guard lock(g_settingsMutex);
    changed = next.filters != g_settings.filters ||
              next.available != g_settings.available ||
              next.enabled != g_settings.enabled;
    g_settings = next;
    if (changed) {
      // A filter edit changes the meaning of every latched source/device
      // relationship. Never carry a suppression created by the old DD Hider
      // policy into the new one.
      g_latchedSuppressedAppearanceSources.clear();
      g_directDeviceCycleAppearances.clear();
      g_redressedSourceSlotMasks.clear();
      g_previousRenderedDeviceSlotMasks.clear();
    }
  }
  if (changed) {
    logger::info("DD NG Hider synchronized in DLL: enabled={} filters={}",
                 next.enabled, next.filters.size());
    sfs::workbench::EquipmentRefreshEventSink::GetSingleton()->QueueRefresh();
  }
  return changed;
}

bool RefreshDeviousDevicesHiderSettings() {
  auto *data = RE::TESDataHandler::GetSingleton();
  auto *quest = data ? data->LookupForm<RE::TESQuest>(
                           kDevicesUnderneathQuestLocalID, kDevicesPlugin)
                     : nullptr;
  auto *vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
  auto *handlePolicy = vm ? vm->GetObjectHandlePolicy() : nullptr;
  if (!quest || !vm || !handlePolicy) {
    return false;
  }
  const auto handle =
      handlePolicy->GetHandleForObject(quest->GetFormType(), quest);
  if (handle == handlePolicy->EmptyHandle()) {
    return false;
  }

  RE::BSTSmartPointer<RE::BSScript::Object> scriptObject;
  if (!vm->FindBoundObject(handle, kDevicesUnderneathScript.data(),
                           scriptObject) ||
      !scriptObject) {
    return false;
  }
  const auto *filtersProperty =
      scriptObject->GetProperty(RE::BSFixedString("SlotMaskFilters"));
  const auto *settingProperty =
      scriptObject->GetProperty(RE::BSFixedString("Setting"));
  if (!filtersProperty || !filtersProperty->IsArray() || !settingProperty ||
      !settingProperty->IsInt()) {
    return false;
  }
  return UpdateDeviousDevicesHiderSettings(
      filtersProperty->Unpack<std::vector<std::int32_t>>(),
      settingProperty->GetSInt());
}

std::uint32_t GetDeviousDevicesHiderSuppressedFittingSlotMask(
    RE::Actor *a_actor) {
  return CalculateDeviousDevicesHiderSuppressedFittingSlotMask(
      a_actor, nullptr, false, 0);
}

std::uint32_t CalculateDeviousDevicesHiderSuppressedFittingSlotMask(
    RE::Actor *a_actor,
    const std::vector<sfs::workbench::VariantWorkbenchRow> *a_previewRows,
    const bool a_previewReplacesRows,
    const std::uint32_t a_suppressedFittingSlotMask) {
  if (!a_actor || !IsDeviousDevicesRuntimeAvailable()) {
    return 0;
  }
  const auto policy = CurrentStripLinkPolicySignature();
  const auto configuredMode = policy.configuredMode;
  const auto actorID = a_actor->GetFormID();
  std::uint64_t redressedSourceMask = 0;
  {
    std::lock_guard lock(g_settingsMutex);
    if (!g_latchedStripLinkPolicy.has_value() ||
        *g_latchedStripLinkPolicy != policy) {
      g_latchedSuppressedAppearanceSources.clear();
      g_directDeviceCycleAppearances.clear();
      g_redressedSourceSlotMasks.clear();
      g_previousRenderedDeviceSlotMasks.clear();
      g_latchedStripLinkPolicy = policy;
    }
    if (configuredMode ==
        sfs::workbench::ExternalModStripLinkMode::Disabled) {
      g_latchedSuppressedAppearanceSources.erase(actorID);
      g_directDeviceCycleAppearances.erase(actorID);
      g_redressedSourceSlotMasks.erase(actorID);
      g_previousRenderedDeviceSlotMasks.erase(actorID);
      return 0;
    }
    if (const auto redressed = g_redressedSourceSlotMasks.find(actorID);
        redressed != g_redressedSourceSlotMasks.end()) {
      redressedSourceMask = redressed->second;
    }
  }
  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return 0;
  }
  auto workbenchStateLock = menu->GetWorkbench().AcquireStateLock();

  HiderScan scan;
  std::uint32_t occupiedDisplaySlots = 0;
  if (a_previewRows) {
    const auto preview = CollectRows(a_actor, *a_previewRows, true,
                                     a_suppressedFittingSlotMask,
                                     occupiedDisplaySlots);
    scan.sourceSlotMask |= preview.sourceSlotMask;
    scan.equippedRenderedDeviceSlotMask |=
        preview.equippedRenderedDeviceSlotMask;
    scan.appearances.insert(scan.appearances.end(),
                            preview.appearances.begin(),
                            preview.appearances.end());
  }
  if (!a_previewReplacesRows) {
    const auto live = CollectRows(a_actor, menu->GetWorkbench().GetRows(),
                                  false, a_suppressedFittingSlotMask,
                                  occupiedDisplaySlots);
    scan.sourceSlotMask |= live.sourceSlotMask;
    scan.equippedRenderedDeviceSlotMask |=
        live.equippedRenderedDeviceSlotMask;
    scan.appearances.insert(scan.appearances.end(), live.appearances.begin(),
                            live.appearances.end());
  }
  const auto effectiveMode =
      configuredMode == sfs::workbench::ExternalModStripLinkMode::Custom
          ? policy.customBaseMode
          : configuredMode;
  const bool directMode =
      effectiveMode == sfs::workbench::ExternalModStripLinkMode::DirectSlots;
  const bool customPolicy =
      configuredMode == sfs::workbench::ExternalModStripLinkMode::Custom;
  const auto baseMode =
      directMode ? policy.directAutomaticBaseMode : effectiveMode;
  std::uint32_t suppressedSlotMask = 0;
  std::unordered_map<std::uint32_t, LatchedAppearanceSuppression>
      currentSuppressions;
  std::unordered_map<std::uint32_t, std::unordered_set<RE::FormID>>
      currentDirectDeviceAppearances;

  const auto vanillaAnchorFor = [](const HiderScan::Appearance &a_appearance) {
    if (a_appearance.automaticAnchorSlotMask != 0) {
      return a_appearance.automaticAnchorSlotMask;
    }
    const auto priority =
        sfs::workbench::GetVanillaAnchorPriority(a_appearance.armor);
    if (priority.count != 0) {
      return priority.slotMasks[0];
    }
    return std::uint64_t{0};
  };
  // DD's real contract is source -> hidden target: when source slot 1 is
  // equipped, SlotMaskFilters[slot 1] hides slot 2. Keep that relationship
  // separate from a rendered device directly occupying the linked slot.
  // A visible fitting or ordinary garment may itself occupy a configured DD
  // Hider source slot (the default 32 -> 56 rule is the common case). Do not
  // let a kit apply or strip-link policy rebuild activate that relationship
  // when the actor has no genuinely equipped rendered restraint. Direct slot
  // conflicts remain independent below and still work when Hider is disabled.
  const auto hiderTargetSlotMask =
      scan.equippedRenderedDeviceSlotMask != 0
          ? static_cast<std::uint64_t>(
                GetConfiguredTargetMask(scan.sourceSlotMask))
          : std::uint64_t{0};
  for (const auto &appearance : scan.appearances) {
    if (!appearance.armor || appearance.visualSlotMask == 0 ||
        !sfs::workbench::IsExternalModStripLinkAppearanceEnabled(
            appearance.visualSlotMask)) {
      continue;
    }

    std::optional<std::uint64_t> directAnchor;
    if (customPolicy) {
      directAnchor =
          sfs::workbench::ResolveCustomDirectStripLinkAnchorSlotMask(
              appearance.visualSlotMask);
    }
    std::uint64_t sourceMask = 0;
    if (directAnchor.has_value()) {
      // Zero is an explicit Do Not Link target. Any concrete target becomes
      // the DD Hider source context for this appearance, regardless of
      // whether it is a vanilla or extension slot.
      if (*directAnchor == 0) {
        continue;
      }
      sourceMask = *directAnchor;
    } else if (baseMode ==
               sfs::workbench::ExternalModStripLinkMode::VanillaSlots) {
      sourceMask = vanillaAnchorFor(appearance);
    } else {
      sourceMask = appearance.visualSlotMask;
    }
    const auto directDeviceSourceMask =
        sourceMask & scan.equippedRenderedDeviceSlotMask;
    if (directDeviceSourceMask != 0 && appearance.armor->GetFormID() != 0) {
      currentDirectDeviceAppearances[appearance.visualSlotMask].insert(
          appearance.armor->GetFormID());
    }

    const auto activeSourceMask = sourceMask & ~redressedSourceMask;
    const auto activeDirectDeviceSourceMask =
        activeSourceMask & scan.equippedRenderedDeviceSlotMask;
    const auto activeHiderTargetSourceMask =
        activeSourceMask & hiderTargetSlotMask;
    if (activeDirectDeviceSourceMask != 0 ||
        activeHiderTargetSourceMask != 0) {
      suppressedSlotMask |= appearance.visualSlotMask;
      auto &suppression = currentSuppressions[appearance.visualSlotMask];
      suppression.sourceSlotMask |= activeDirectDeviceSourceMask |
                                    activeHiderTargetSourceMask;
    }
  }
  std::unordered_map<std::uint32_t, std::unordered_set<RE::FormID>>
      directDeviceAppearancesToPersistHidden;
  {
    std::lock_guard lock(g_settingsMutex);
    auto &latched = g_latchedSuppressedAppearanceSources[actorID];
    for (const auto &[appearanceMask, current] : currentSuppressions) {
      auto &stored = latched[appearanceMask];
      stored.sourceSlotMask |= current.sourceSlotMask;
    }
    if (!a_previewReplacesRows && !currentDirectDeviceAppearances.empty()) {
      auto &directCycle = g_directDeviceCycleAppearances[actorID];
      for (const auto &[appearanceMask, armorFormIDs] :
           currentDirectDeviceAppearances) {
        directCycle[appearanceMask].insert(armorFormIDs.begin(),
                                           armorFormIDs.end());
      }
    }

    // Only a scan which includes the live rows is allowed to close a cycle;
    // a temporary replacement preview must never release actor runtime state.
    if (!a_previewReplacesRows) {
      const auto previous = g_previousRenderedDeviceSlotMasks.find(actorID);
      const auto previousDeviceMask =
          previous != g_previousRenderedDeviceSlotMasks.end()
              ? previous->second
              : 0;
      if (previousDeviceMask != 0 &&
          scan.equippedRenderedDeviceSlotMask == 0) {
        std::uint32_t restoredHiderAppearanceMask = 0;
        for (const auto &[appearanceMask, suppression] : latched) {
          static_cast<void>(suppression);
          restoredHiderAppearanceMask |= appearanceMask;
        }
        std::uint32_t directDeviceAppearanceMask = 0;
        if (const auto directCycle =
                g_directDeviceCycleAppearances.find(actorID);
            directCycle != g_directDeviceCycleAppearances.end()) {
          directDeviceAppearancesToPersistHidden = directCycle->second;
          for (const auto &[appearanceMask, armorFormIDs] :
               directDeviceAppearancesToPersistHidden) {
            static_cast<void>(armorFormIDs);
            directDeviceAppearanceMask |= appearanceMask;
          }
          g_directDeviceCycleAppearances.erase(directCycle);
        }
        restoredHiderAppearanceMask &= ~directDeviceAppearanceMask;
        // Hider-only suppression is a temporary render layer, so clearing it
        // reveals the fitting's pre-cycle/latest explicit eye state. A fitting
        // whose linked slot was directly occupied by a rendered device is
        // handed off as ordinary manual hidden state after the device leaves.
        latched.clear();
        g_redressedSourceSlotMasks.erase(actorID);
        logger::info(
            "DD Hider cycle settled actor={:08X} "
            "releasedTemporaryHiderAppearances={:08X} "
            "manualHiddenDirectDeviceAppearances={:08X}",
            actorID, restoredHiderAppearanceMask,
            directDeviceAppearanceMask);
      }
      if (scan.equippedRenderedDeviceSlotMask != 0) {
        g_previousRenderedDeviceSlotMasks[actorID] =
            scan.equippedRenderedDeviceSlotMask;
      } else {
        g_previousRenderedDeviceSlotMasks.erase(actorID);
      }
    }

    // A protected/disabled appearance cannot remain hidden through an old
    // latch. This is also what makes global policy edits safe for every actor.
    for (auto suppression = latched.begin(); suppression != latched.end();) {
      if (!sfs::workbench::IsExternalModStripLinkAppearanceEnabled(
              suppression->first)) {
        suppression = latched.erase(suppression);
        continue;
      }
      ++suppression;
    }
    for (const auto &[appearanceMask, suppression] : latched) {
      static_cast<void>(suppression);
      suppressedSlotMask |= appearanceMask;
    }
    if (latched.empty()) {
      g_latchedSuppressedAppearanceSources.erase(actorID);
    }
  }
  if (!directDeviceAppearancesToPersistHidden.empty()) {
    auto &workbench = menu->GetWorkbench();
    const auto &rows = workbench.GetRows();
    std::size_t persistedCount = 0;
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
      const auto &row = rows[rowIndex];
      if (!row.IsOwnedByActor(a_actor)) {
        continue;
      }
      for (std::size_t itemIndex = 0; itemIndex < row.overrides.size();
           ++itemIndex) {
        const auto &item = row.overrides[itemIndex];
        const auto visualMask = static_cast<std::uint32_t>(
            row.GetOverrideVisualSlotMask(item));
        const bool shouldPersist = std::ranges::any_of(
            directDeviceAppearancesToPersistHidden,
            [&](const auto &a_suppression) {
              const auto &[appearanceMask, armorFormIDs] = a_suppression;
              return (appearanceMask & visualMask) != 0 &&
                     armorFormIDs.contains(item.formID);
            });
        if (!shouldPersist || item.hidden) {
          continue;
        }
        if (workbench.PersistOverrideHiddenFromExternalSuppression(
                static_cast<int>(rowIndex), static_cast<int>(itemIndex))) {
          ++persistedCount;
        }
      }
    }
    logger::info("DD direct-slot fitting state handed to user control "
                 "actor={:08X} hiddenAppearances={}",
                 actorID, persistedCount);
  }
  return suppressedSlotMask;
}
} // namespace sfs::poc

#else

namespace sfs::poc {
void InitializeDeviousDevicesHider() {}
void ResetDeviousDevicesHider() {}
bool IsDeviousDevicesRenderedDevice(const RE::TESObjectARMO *) { return false; }
bool IsDeviousDevicesEquipmentTransactionArmor(const RE::TESObjectARMO *) {
  return false;
}
void ObserveDeviousDevicesRenderedDeviceEquipEvent(RE::Actor *,
                                                   RE::TESObjectARMO *, bool) {}
void ReconcileDeviousDevicesRenderedDeviceVisibility(RE::Actor *) {}
bool IsDeviousDevicesRenderedDeviceOrdinaryVisible(RE::FormID, RE::FormID) {
  return false;
}
void SetDeviousDevicesRenderedDeviceUserVisible(RE::FormID, RE::FormID, bool) {}
void ClearDeviousDevicesRenderedDeviceOrdinaryVisibility(RE::FormID) {}
void ReleaseDeviousDevicesHiderSuppressionForSourceMask(RE::FormID,
                                                        std::uint64_t) {}
void ReleaseDeviousDevicesHiderSuppressionForAppearanceMask(RE::FormID,
                                                            std::uint64_t) {}
bool RefreshDeviousDevicesHiderSettings() { return false; }
bool UpdateDeviousDevicesHiderSettings(const std::vector<std::int32_t> &,
                                       std::int32_t) {
  return false;
}
std::uint32_t
GetDeviousDevicesHiderSuppressedFittingSlotMask(RE::Actor *) {
  return 0;
}
std::uint32_t CalculateDeviousDevicesHiderSuppressedFittingSlotMask(
    RE::Actor *, const std::vector<sfs::workbench::VariantWorkbenchRow> *,
    bool, std::uint32_t) {
  return 0;
}
} // namespace sfs::poc

#endif
