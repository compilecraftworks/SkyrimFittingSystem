#include "native/HelmetToggle2Integration.h"

#include "ArmorUtils.h"
#include "native/ArmorSkinning.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/FittingSlotState.h"
#include "native/HelmetToggle2Rules.h"
#include "ui/Menu.h"

#include <SKSE/SKSE.h>

#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr auto kHelmetTogglePluginName = "Helmet Toggle 2.esp";

struct HelmetToggleForms {
  RE::TESGlobal *helmetState{nullptr};
  RE::SpellItem *headgearEquipped{nullptr};

  RE::BGSKeyword *armorHelmet{nullptr};
  RE::BGSKeyword *clothingHead{nullptr};
  RE::BGSKeyword *clothingBody{nullptr};
  RE::BGSKeyword *clothingCirclet{nullptr};
  RE::BGSKeyword *armorHood{nullptr};
  RE::BGSKeyword *armorMask{nullptr};
  RE::BGSKeyword *armorFaceMask{nullptr};
  RE::BGSKeyword *armorVisor{nullptr};
  RE::BGSKeyword *armorHelmetAlt{nullptr};
  RE::BGSKeyword *ignoreHeadgear{nullptr};

  RE::TESGlobal *enableHelmet{nullptr};
  RE::TESGlobal *enableHood{nullptr};
  RE::TESGlobal *enableHat{nullptr};
  RE::TESGlobal *enableMask{nullptr};

  std::array<RE::SpellItem *, 3> monitorSpells{};
};

HelmetToggleForms g_forms;
std::atomic_bool g_available{false};
std::mutex g_actorStateMutex;
std::unordered_set<RE::FormID> g_hiddenActors;
std::mutex g_managedActorMutex;
std::unordered_set<RE::FormID> g_managedNpcActors;
std::mutex g_controllerMaskMutex;
std::unordered_map<RE::FormID, std::uint32_t> g_lastVisibleControllerMasks;
std::mutex g_signalTaskMutex;
std::unordered_map<RE::FormID, std::uint64_t> g_latestSignalTask;
std::atomic_uint64_t g_nextSignalTask{0};

[[nodiscard]] bool IsHelmetToggleForm(const RE::TESForm *a_form) {
  const auto pluginName = sfs::armor::GetPluginName(a_form);
  return !pluginName.empty() &&
         _stricmp(pluginName.c_str(), kHelmetTogglePluginName) == 0;
}

template <class T>
[[nodiscard]] T *LookupHelmetToggleForm(const char *a_editorID) {
  auto *form = RE::TESForm::LookupByEditorID<T>(a_editorID);
  return IsHelmetToggleForm(form) ? form : nullptr;
}

[[nodiscard]] bool GlobalEnabled(const RE::TESGlobal *a_global) {
  // Older HT2 builds may omit a category global. In that case the exact HT2
  // shown/hidden signal remains authoritative and the category is accepted.
  return a_global == nullptr || a_global->value != 0.0F;
}

[[nodiscard]] bool HasKeyword(const RE::TESObjectARMO *a_armor,
                              const RE::BGSKeyword *a_keyword) {
  return a_armor != nullptr && a_keyword != nullptr &&
         a_armor->HasKeyword(a_keyword);
}

[[nodiscard]] bool IsManagedHeadgear(const RE::TESObjectARMO *a_armor,
                                     const std::uint32_t a_querySlot) {
  if (!a_armor) {
    return false;
  }

  const bool ignored = HasKeyword(a_armor, g_forms.ignoreHeadgear);
  const bool mask = HasKeyword(a_armor, g_forms.armorMask);
  const bool faceMask = HasKeyword(a_armor, g_forms.armorFaceMask);
  if (a_querySlot == 44) {
    return !ignored && GlobalEnabled(g_forms.enableMask) &&
           (mask || faceMask);
  }
  if (a_querySlot == 55) {
    return !ignored && GlobalEnabled(g_forms.enableMask) && mask;
  }

  const bool helmetAlt = HasKeyword(a_armor, g_forms.armorHelmetAlt);
  if (!helmetAlt &&
      (ignored || HasKeyword(a_armor, g_forms.clothingCirclet))) {
    return false;
  }
  if (!helmetAlt && !HasKeyword(a_armor, g_forms.armorHelmet) && !mask &&
      !HasKeyword(a_armor, g_forms.armorHood) &&
      !HasKeyword(a_armor, g_forms.clothingHead) &&
      !HasKeyword(a_armor, g_forms.armorVisor)) {
    return false;
  }

  // Match HT2's category priority: mask, hood/body, hat, then helmet.
  if (mask) {
    return GlobalEnabled(g_forms.enableMask);
  }
  if (HasKeyword(a_armor, g_forms.armorHood) ||
      HasKeyword(a_armor, g_forms.clothingBody)) {
    return GlobalEnabled(g_forms.enableHood);
  }
  if (HasKeyword(a_armor, g_forms.clothingHead)) {
    return GlobalEnabled(g_forms.enableHat);
  }
  return GlobalEnabled(g_forms.enableHelmet);
}

[[nodiscard]] bool IsPlayer(const RE::Actor *a_actor) {
  const auto *player = RE::PlayerCharacter::GetSingleton();
  return a_actor != nullptr && player != nullptr &&
         a_actor->GetFormID() == player->GetFormID();
}

struct ManagedActualHeadgearSlots {
  std::uint32_t armorMask{0};
  std::uint32_t controllerMask{0};
  bool observedManagedHeadgear{false};
  struct Observation {
    RE::FormID armorFormID{0};
    std::uint32_t armorMask{0};
    std::uint32_t addonMask{0};
    std::uint32_t queryMask{0};
  };
  std::array<Observation, 5> observations{};
  std::size_t observationCount{0};
};

[[nodiscard]] ManagedActualHeadgearSlots
GetManagedActualHeadgearSlots(RE::Actor *a_actor) {
  if (!a_actor) {
    return {};
  }

  constexpr std::array<std::uint32_t, 5> kQuerySlots{30, 31, 42, 44, 55};
  const bool player = IsPlayer(a_actor);
  ManagedActualHeadgearSlots result;
  for (const auto slot : kQuerySlots) {
    if (!player && slot == 55) {
      break;
    }
    const auto queryMask = sfs::armor::GetArmorSlotMask(slot);
    auto *armor = a_actor->GetWornArmor(
        static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(queryMask));
    if (!armor || !IsManagedHeadgear(armor, slot)) {
      continue;
    }
    result.observedManagedHeadgear = true;

    const auto armorMask =
        static_cast<std::uint32_t>(armor->GetSlotMask().underlying());
    result.armorMask |= armorMask;
    ManagedActualHeadgearSlots::Observation *observation = nullptr;
    for (std::size_t index = 0; index < result.observationCount; ++index) {
      if (result.observations[index].armorFormID == armor->GetFormID()) {
        observation = &result.observations[index];
        break;
      }
    }
    if (!observation) {
      if (result.observationCount >= result.observations.size()) {
        continue;
      }
      observation = &result.observations[result.observationCount++];
      observation->armorFormID = armor->GetFormID();
      observation->armorMask = armorMask;
      observation->addonMask = static_cast<std::uint32_t>(
          sfs::armor::GetArmorAddonSlotMask(armor));
    }
    observation->queryMask |= static_cast<std::uint32_t>(queryMask);
  }

  for (std::size_t index = 0; index < result.observationCount; ++index) {
    const auto &observation = result.observations[index];
    result.controllerMask |=
        sfs::native::helmet_toggle::rules::ResolveObservedControllerMask(
            observation.armorMask, observation.queryMask, player);
  }
  return result;
}

void SetActorHiddenState(RE::Actor *a_actor, const bool a_hidden) {
  if (!a_actor || a_actor->GetFormID() == 0) {
    return;
  }
  std::lock_guard lock(g_actorStateMutex);
  if (a_hidden) {
    g_hiddenActors.insert(a_actor->GetFormID());
  } else {
    g_hiddenActors.erase(a_actor->GetFormID());
  }
}

[[nodiscard]] bool IsActorHidden(RE::Actor *a_actor) {
  if (!a_actor || a_actor->GetFormID() == 0) {
    return false;
  }
  std::lock_guard lock(g_actorStateMutex);
  return g_hiddenActors.contains(a_actor->GetFormID());
}

[[nodiscard]] std::uint32_t ResolveActorControllerMask(
    const RE::FormID a_actorFormID, const bool a_hidden,
    const ManagedActualHeadgearSlots &a_observed) {
  std::lock_guard lock(g_controllerMaskMutex);
  const auto cached = g_lastVisibleControllerMasks.find(a_actorFormID);
  const auto cachedMask = cached != g_lastVisibleControllerMasks.end()
                              ? cached->second
                              : 0;
  const auto resolved =
      sfs::native::helmet_toggle::rules::ResolveSignalControllerMask(
          a_hidden, a_observed.controllerMask,
          a_observed.observedManagedHeadgear, cachedMask);

  // A visible observation is authoritative. The same applies when a managed
  // item is still queryable during the hidden transition. A managed pure-31
  // helmet resolves to the semantic registered-headgear controller mask,
  // whereas a non-headgear Hair item never reaches this observation path.
  if (!a_hidden || a_observed.observedManagedHeadgear) {
    if (a_observed.controllerMask != 0) {
      g_lastVisibleControllerMasks.insert_or_assign(
          a_actorFormID, a_observed.controllerMask);
    } else {
      g_lastVisibleControllerMasks.erase(a_actorFormID);
    }
  }
  return resolved;
}

[[nodiscard]] bool HasResolvedMonitorOwnershipForm() {
  for (const auto *spell : g_forms.monitorSpells) {
    if (spell != nullptr) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool HasExplicitMonitorOwnership(RE::Actor *a_actor) {
  if (!a_actor) {
    return false;
  }
  if (g_forms.headgearEquipped &&
      a_actor->HasSpell(g_forms.headgearEquipped)) {
    return true;
  }
  for (auto *spell : g_forms.monitorSpells) {
    if (spell != nullptr && a_actor->HasSpell(spell)) {
      return true;
    }
  }
  // HT2's NPC/follower monitor scripts run from persistent monitor spells,
  // while HT_HeadGearEquipped AddSpell/RemoveSpell calls provide the exact
  // visible-state signal and seed the actor-local cache. Do not inspect the
  // Actor MagicTarget active-effect list here: actor selection can run while
  // that secondary vtable/list is unavailable, and the redundant fallback
  // caused a null-vtable CTD during RefreshArmorFor/SynchronizeActor.
  return false;
}

[[nodiscard]] bool IsManagedNpc(RE::Actor *a_actor) {
  if (!a_actor || IsPlayer(a_actor)) {
    return false;
  }
  const auto actorFormID = a_actor->GetFormID();
  {
    std::lock_guard lock(g_managedActorMutex);
    if (g_managedNpcActors.contains(actorFormID)) {
      return true;
    }
  }
  return HasExplicitMonitorOwnership(a_actor);
}

void ApplyActorState(RE::Actor *a_actor, const bool a_hidden,
                     const bool a_queueRefresh) {
  if (!a_actor) {
    return;
  }

  const auto actorFormID = a_actor->GetFormID();
  const bool wasHidden = IsActorHidden(a_actor);
  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    SetActorHiddenState(a_actor, a_hidden);
    return;
  }

  const bool hasAppearances =
      menu->GetWorkbench().HasRegisteredAppearancesForActor(actorFormID);
  const bool canRenderNow = a_queueRefresh && a_actor->Is3DLoaded();
  const auto displayedBefore =
      canRenderNow && hasAppearances
          ? sfs::native::GetDisplayedFittingSlotMask(a_actor)
          : 0;
  const auto observedHeadgear = GetManagedActualHeadgearSlots(a_actor);
  const auto hairReleaseBefore =
      canRenderNow
          ? sfs::native::helmet_toggle::rules::
                ComputeActualHairSlotReleaseMask(
                    wasHidden, observedHeadgear.armorMask, displayedBefore)
          : 0;
  SetActorHiddenState(a_actor, a_hidden);
  std::uint32_t fittingMask = 0;
  const auto controllerMask = ResolveActorControllerMask(
      actorFormID, a_hidden, observedHeadgear);
  if (a_hidden && hasAppearances) {
    if (controllerMask != 0) {
      fittingMask =
          menu->GetWorkbench().GetHeadgearToggleFittingSlotMaskForActor(
              actorFormID, controllerMask);
    }
  }

  const bool stateChanged =
      sfs::native::ReplaceHeadgearToggleFittingSlotsSuppressed(a_actor,
                                                               fittingMask);
  if (a_queueRefresh) {
    logger::info("HT2 actor-local signal actor={:08X} hidden={} "
                 "observed={} observedManaged={} resolved={} fitting={}",
                 actorFormID, a_hidden, observedHeadgear.controllerMask,
                 observedHeadgear.observedManagedHeadgear, controllerMask,
                 fittingMask);
    for (std::size_t index = 0;
         index < observedHeadgear.observationCount; ++index) {
      const auto &observation = observedHeadgear.observations[index];
      logger::info("HT2 actual headgear actor={:08X} armor={:08X} "
                   "armoSlots={:08X} armaSlots={:08X} queriedSlots={:08X}",
                   actorFormID, observation.armorFormID,
                   observation.armorMask, observation.addonMask,
                   observation.queryMask);
    }
  }
  if (!canRenderNow) {
    return;
  }

  const auto displayedAfter =
      hasAppearances ? sfs::native::GetDisplayedFittingSlotMask(a_actor) : 0;
  const bool fittingDisplayChanged =
      stateChanged && displayedBefore != displayedAfter;
  const auto hairReleaseAfter =
      sfs::native::helmet_toggle::rules::ComputeActualHairSlotReleaseMask(
          a_hidden, observedHeadgear.armorMask, displayedAfter);
  const bool actualHairDisplayChanged =
      hairReleaseBefore != hairReleaseAfter;
  if (fittingDisplayChanged || actualHairDisplayChanged) {
    logger::info("HT2 actor-local fitting refresh actor={:08X} hidden={} "
                 "controller={:08X} fitting={:08X} displayedBefore={:08X} "
                 "displayedAfter={:08X} hairRelease={:08X}",
                 actorFormID, a_hidden, controllerMask, fittingMask,
                 displayedBefore, displayedAfter, hairReleaseAfter);
    sfs::native::QueueArmorRefreshFor(a_actor);
  }
}

void QueueActorState(RE::Actor *a_actor, const bool a_hidden) {
  if (!a_actor) {
    return;
  }
  const auto actorFormID = a_actor->GetFormID();
  auto *taskInterface = SKSE::GetTaskInterface();
  if (actorFormID == 0 || !taskInterface) {
    return;
  }

  const auto generation = ++g_nextSignalTask;
  {
    std::lock_guard lock(g_signalTaskMutex);
    g_latestSignalTask.insert_or_assign(actorFormID, generation);
  }
  taskInterface->AddTask([actorFormID, generation, a_hidden]() {
    {
      std::lock_guard lock(g_signalTaskMutex);
      const auto latest = g_latestSignalTask.find(actorFormID);
      if (latest == g_latestSignalTask.end() || latest->second != generation) {
        return;
      }
      g_latestSignalTask.erase(latest);
    }
    auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
    bool hidden = a_hidden;
    if (hidden && actor && !IsPlayer(actor) &&
        HasResolvedMonitorOwnershipForm() &&
        !HasExplicitMonitorOwnership(actor)) {
      // HT2 removes the marker once more while disabling/finishing its NPC
      // monitor. That cleanup is not a hidden-headgear state. Release only
      // this actor's ownership after the effect has actually gone away.
      std::lock_guard lock(g_managedActorMutex);
      g_managedNpcActors.erase(actorFormID);
      hidden = false;
    }
    ApplyActorState(actor, hidden, true);
  });
}
} // namespace

namespace sfs::native::helmet_toggle {
bool Initialize() {
  if (g_available.load(std::memory_order_acquire)) {
    return true;
  }

  HelmetToggleForms forms{};
  forms.helmetState =
      LookupHelmetToggleForm<RE::TESGlobal>("HT_HelmetState");
  forms.headgearEquipped =
      LookupHelmetToggleForm<RE::SpellItem>("HT_HeadGearEquipped");
  if (!forms.helmetState || !forms.headgearEquipped) {
    logger::info("Helmet Toggle 2 core integration inactive (plugin signals "
                 "not found)");
    return false;
  }

  forms.armorHelmet =
      RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ArmorHelmet");
  forms.clothingHead =
      RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingHead");
  forms.clothingBody =
      RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingBody");
  forms.clothingCirclet =
      RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingCirclet");
  forms.armorHood =
      LookupHelmetToggleForm<RE::BGSKeyword>("HT_ArmorHood");
  forms.armorMask =
      LookupHelmetToggleForm<RE::BGSKeyword>("HT_ArmorMask");
  forms.armorFaceMask =
      LookupHelmetToggleForm<RE::BGSKeyword>("HT_ArmorFaceMask");
  forms.armorVisor =
      LookupHelmetToggleForm<RE::BGSKeyword>("HT_ArmorVisor");
  forms.armorHelmetAlt =
      LookupHelmetToggleForm<RE::BGSKeyword>("HT_ArmorHelmetAlt");
  forms.ignoreHeadgear =
      LookupHelmetToggleForm<RE::BGSKeyword>("HT_IgnoreHeadgear");

  forms.enableHelmet =
      LookupHelmetToggleForm<RE::TESGlobal>("HT_EnableHelmet");
  forms.enableHood =
      LookupHelmetToggleForm<RE::TESGlobal>("HT_EnableHood");
  forms.enableHat = LookupHelmetToggleForm<RE::TESGlobal>("HT_EnableHat");
  forms.enableMask = LookupHelmetToggleForm<RE::TESGlobal>("HT_EnableMask");
  forms.monitorSpells = {
      LookupHelmetToggleForm<RE::SpellItem>("HT_NPCSpellMonitor"),
      LookupHelmetToggleForm<RE::SpellItem>("HT_FollowerSpellMonitor"),
      LookupHelmetToggleForm<RE::SpellItem>("HT_CustomFollowerSpellMonitor")};

  g_forms = forms;
  g_available.store(true, std::memory_order_release);
  if (!sfs::native::external_equipment::
          EnableHelmetToggleSignalObserver()) {
    logger::warn("Helmet Toggle 2 forms were found, but the live signal "
                 "observer could not be enabled");
  }
  logger::info("Helmet Toggle 2 core integration ready (player, NPC, and "
               "follower actor-local signals; no PEX override)");
  return true;
}

bool IsAvailable() {
  return g_available.load(std::memory_order_acquire);
}

void ResetRuntimeState() {
  {
    std::lock_guard lock(g_actorStateMutex);
    g_hiddenActors.clear();
  }
  {
    std::lock_guard lock(g_managedActorMutex);
    g_managedNpcActors.clear();
  }
  {
    std::lock_guard lock(g_controllerMaskMutex);
    g_lastVisibleControllerMasks.clear();
  }
  {
    std::lock_guard lock(g_signalTaskMutex);
    g_latestSignalTask.clear();
  }
  sfs::native::ClearAllHeadgearToggleFittingSlotStates();
}

void ObserveGlobalStateChanged(RE::TESGlobal *a_global) {
  if (!IsAvailable() || a_global != g_forms.helmetState) {
    return;
  }
  QueueActorState(RE::PlayerCharacter::GetSingleton(),
                  a_global->value > 0.0F);
}

void ObserveActorSpellChanged(RE::Actor *a_actor, RE::SpellItem *a_spell) {
  if (!IsAvailable() || !a_actor || a_spell != g_forms.headgearEquipped ||
      IsPlayer(a_actor)) {
    return;
  }
  {
    std::lock_guard lock(g_managedActorMutex);
    g_managedNpcActors.insert(a_actor->GetFormID());
  }
  QueueActorState(a_actor, !a_actor->HasSpell(g_forms.headgearEquipped));
}

void SynchronizeActor(RE::Actor *a_actor, const bool a_queueRefresh) {
  if (!IsAvailable() || !a_actor) {
    return;
  }
  if (IsPlayer(a_actor)) {
    ApplyActorState(a_actor, g_forms.helmetState->value > 0.0F,
                    a_queueRefresh);
    return;
  }
  if (!IsManagedNpc(a_actor)) {
    SetActorHiddenState(a_actor, false);
    sfs::native::ReplaceHeadgearToggleFittingSlotsSuppressed(a_actor, 0);
    return;
  }
  {
    std::lock_guard lock(g_managedActorMutex);
    g_managedNpcActors.insert(a_actor->GetFormID());
  }
  ApplyActorState(a_actor, !a_actor->HasSpell(g_forms.headgearEquipped),
                  a_queueRefresh);
}

void SynchronizePlayer(const bool a_queueRefresh) {
  SynchronizeActor(RE::PlayerCharacter::GetSingleton(), a_queueRefresh);
}

std::uint32_t GetActualHairSlotReleaseMask(
    RE::Actor *a_actor, const std::uint32_t a_displayedFittingSlotMask) {
  if (!IsAvailable() || !IsActorHidden(a_actor)) {
    return 0;
  }
  return rules::ComputeActualHairSlotReleaseMask(
      true, GetManagedActualHeadgearSlots(a_actor).armorMask,
      a_displayedFittingSlotMask);
}
} // namespace sfs::native::helmet_toggle
