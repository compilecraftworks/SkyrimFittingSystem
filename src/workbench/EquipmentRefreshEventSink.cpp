#include "workbench/EquipmentRefreshEventSink.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "TngGenitalCoverRules.h"
#include "conditions/Status.h"
#include "conditions/Validation.h"
#include "native/ArmorSkinning.h"
#include "native/DaveIntegration.h"
#include "native/FittingSlotState.h"
#include "native/GenitalCompatibility.h"
#include "native/GenitalArmorResolver.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/SOSStorageSync.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include "features/devious_devices/DeviousDevicesIntegration.h"
#include "ui/Menu.h"
#include "workbench/AutomaticEquipmentVisibility.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
std::mutex g_actorRefreshMutex;
std::unordered_map<RE::FormID, std::uint64_t> g_actorRefreshGeneration;
std::unordered_set<RE::FormID> g_pendingActorStateReconciliations;
std::unordered_set<RE::FormID> g_pendingEquipmentChangeRefreshActors;
std::unordered_set<RE::FormID> g_pendingContextBoundaryRefreshActors;
std::atomic_uint64_t g_eventRefreshGeneration{0};

constexpr std::int64_t kConditionPollIntervalMillis = 500;

[[nodiscard]] std::uint64_t HashConditionTruth(
    std::uint64_t a_hash, std::string_view a_conditionID, bool a_active) {
  constexpr std::uint64_t kOffset = 1469598103934665603ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  if (a_hash == 0) {
    a_hash = kOffset;
  }
  for (const auto character : a_conditionID) {
    a_hash ^= static_cast<std::uint8_t>(character);
    a_hash *= kPrime;
  }
  a_hash ^= static_cast<std::uint8_t>(a_active ? 1 : 0);
  a_hash *= kPrime;
  return a_hash;
}

[[nodiscard]] bool IsGameReady() {
  auto *menu = sfs::Menu::GetSingleton();
  return menu != nullptr && menu->IsGameDataLoaded();
}

[[nodiscard]] std::uint64_t CurrentEventRefreshGeneration() {
  return g_eventRefreshGeneration.load();
}

[[nodiscard]] bool
IsCurrentEventRefreshGeneration(const std::uint64_t a_generation) {
  return g_eventRefreshGeneration.load() == a_generation;
}

[[nodiscard]] std::uint64_t
QueueActorRefreshGeneration(const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_actorRefreshMutex);
  return ++g_actorRefreshGeneration[a_actorFormID];
}

[[nodiscard]] bool
IsLatestActorRefreshGeneration(const RE::FormID a_actorFormID,
                               const std::uint64_t a_generation) {
  std::lock_guard lock(g_actorRefreshMutex);
  const auto generationIt = g_actorRefreshGeneration.find(a_actorFormID);
  return generationIt != g_actorRefreshGeneration.end() &&
         generationIt->second == a_generation;
}

void MarkEquipmentChangeRefreshPending(const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_actorRefreshMutex);
  g_pendingEquipmentChangeRefreshActors.insert(a_actorFormID);
}

void MarkActorStateReconciliationPending(const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_actorRefreshMutex);
  g_pendingActorStateReconciliations.insert(a_actorFormID);
}

void MarkContextBoundaryRefreshPending(const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_actorRefreshMutex);
  g_pendingContextBoundaryRefreshActors.insert(a_actorFormID);
}

[[nodiscard]] bool
ConsumeActorStateReconciliationPending(const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_actorRefreshMutex);
  return g_pendingActorStateReconciliations.erase(a_actorFormID) != 0;
}

[[nodiscard]] bool
ConsumeEquipmentChangeRefreshPending(const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_actorRefreshMutex);
  return g_pendingEquipmentChangeRefreshActors.erase(a_actorFormID) != 0;
}

[[nodiscard]] bool
ConsumeContextBoundaryRefreshPending(const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_actorRefreshMutex);
  return g_pendingContextBoundaryRefreshActors.erase(a_actorFormID) != 0;
}

void RunActorRefresh(const RE::FormID a_actorFormID) {
  if (!IsGameReady()) {
    return;
  }

  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
  const bool reconcileStatePending =
      ConsumeActorStateReconciliationPending(a_actorFormID);
  const bool equipmentChangeRefreshPending =
      ConsumeEquipmentChangeRefreshPending(a_actorFormID);
  const bool contextBoundaryRefreshPending =
      ConsumeContextBoundaryRefreshPending(a_actorFormID);

  // A cell/location event can arrive immediately before a prison or wardrobe
  // script removes the actor's equipment. Observe the same boundary again at
  // the coalesced main-thread refresh, before row reconciliation can replace
  // the pre-transition snapshot. This restores the actor-local Pama-style
  // fallback without widening ordinary equipment-event handling.
  if (contextBoundaryRefreshPending) {
    sfs::virtual_tokens::ObserveActorContextWardrobeBoundary(actor);
  }

  const bool hadTrackedFittingState = sfs::native::HasFittingSlotState(actor);
  if (reconcileStatePending && hadTrackedFittingState) {
    sfs::native::dave::ClearHiddenRealEquipment(actor);
  }
  if (reconcileStatePending) {
    sfs::Menu::GetSingleton()->SyncWorkbenchRowsForActor(a_actorFormID);
  }
  // Mod-configured strip/redress links are resolved from the settled worn
  // list, not only from the UI.  Reconcile rows on every equipment-change
  // refresh so an item that was hidden before the event is hidden again (or
  // restored) immediately after the external transaction completes.
  if (equipmentChangeRefreshPending && !reconcileStatePending) {
    sfs::Menu::GetSingleton()->SyncWorkbenchRowsForActor(a_actorFormID);
  }
  if (equipmentChangeRefreshPending) {
    // A real equipment rebuild can discard DAVE's active override even when
    // its JSON payload is unchanged. This applies equally to virtual-token,
    // vanilla-anchor, and direct-editor transactions, so every managed
    // equipment refresh must force the actor-local variant to be reapplied.
    sfs::native::dave::MarkHiddenRealEquipmentDirty(actor);
  }
  sfs::virtual_tokens::UpdateVirtualWornTokenCache();
  // Reconcile context-added real armor before building the display set so a
  // delayed prison/outfit equip starts visibly in this same refresh while
  // remaining under the actor's ordinary eye controls.
  sfs::virtual_tokens::RecordActorContextWardrobeSnapshot(actor);
  sfs::devious_devices::ReconcileDeviousDevicesRenderedDeviceVisibility(actor);
  sfs::native::RefreshArmorFor(
      actor, equipmentChangeRefreshPending
                 ? sfs::native::ArmorRefreshReason::kEquipmentChange
                 : sfs::native::ArmorRefreshReason::kDisplayState);

  // Mod-configured transactions own their settled recovery barrier in the
  // virtual-token runtime. Do not add another task-hop retry here: a second
  // DAVE/DAV/native rebuild can overlap SGO's animation-object hand-off.
}

void InitializeEventAddedActualVisibility(RE::Actor *a_actor,
                                          const RE::FormID a_armorFormID) {
  auto *menu = sfs::Menu::GetSingleton();
  if (!a_actor || a_armorFormID == 0 || !menu) {
    return;
  }

  // Replacement/prison/scene equipment starts in the ordinary displayed
  // state, but remains a normal user-controlled row.  Do not use a persistent
  // force-visible flag: individual and global eye controls must keep working
  // during and after every external transaction.
  auto &workbench = menu->GetWorkbench();
  auto stateLock = workbench.AcquireStateLock();
  const auto &rows = workbench.GetRows();
  for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
    const auto &row = rows[rowIndex];
    if (!row.IsOwnedByActor(a_actor) || !row.isEquipped || row.IsSlotRow() ||
        row.equipped.formID != a_armorFormID) {
      continue;
    }
    static_cast<void>(workbench.SetEquippedHiddenForActor(
        a_actor->GetFormID(), static_cast<int>(rowIndex), false));
  }
}

void QueueContextRefreshForActor(RE::Actor *a_actor) {
  if (!a_actor) {
    return;
  }

  auto *sink = sfs::workbench::EquipmentRefreshEventSink::GetSingleton();
  auto *menu = sfs::Menu::GetSingleton();
  const auto actorFormID = a_actor->GetFormID();
  if (sfs::native::HasFittingSlotState(a_actor) ||
      (menu != nullptr &&
       menu->GetWorkbench().HasRegisteredAppearancesForActor(actorFormID))) {
    sfs::virtual_tokens::ObserveActorContextWardrobeBoundary(a_actor);
    MarkContextBoundaryRefreshPending(actorFormID);
    sink->QueueActorRefresh(a_actor->GetFormID());
  }
}

void QueuePlayerContextRefresh() {
  QueueContextRefreshForActor(RE::PlayerCharacter::GetSingleton());
}

void QueueConditionRefreshForActor(RE::Actor *a_actor) {
  if (!a_actor) {
    return;
  }

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu) {
    return;
  }

  const auto actorFormID = a_actor->GetFormID();
  if (sfs::native::HasFittingSlotState(a_actor) ||
      menu->GetWorkbench().HasRegisteredAppearancesForActor(actorFormID)) {
    // Combat changes condition truth without changing the worn list. Refresh
    // only display state; do not reconcile or rewrite equipment snapshots.
    sfs::workbench::EquipmentRefreshEventSink::GetSingleton()
        ->QueueActorRefresh(actorFormID, false);
  }
}
} // namespace

namespace sfs::workbench {

EquipmentRefreshEventSink *EquipmentRefreshEventSink::GetSingleton() {
  static EquipmentRefreshEventSink singleton;
  return &singleton;
}

void EquipmentRefreshEventSink::Register() {
  static bool registered = false;
  if (registered) {
    return;
  }

  auto *eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
  if (!eventSourceHolder) {
    logger::warn("Failed to register equipment refresh event sink");
    return;
  }

  eventSourceHolder->AddEventSink<RE::TESEquipEvent>(GetSingleton());
  eventSourceHolder->AddEventSink<RE::TESActorLocationChangeEvent>(
      GetSingleton());
  eventSourceHolder->AddEventSink<RE::TESCellFullyLoadedEvent>(GetSingleton());
  eventSourceHolder->AddEventSink<RE::TESCombatEvent>(GetSingleton());
  if (auto *modCallbackSource = SKSE::GetModCallbackEventSource()) {
    modCallbackSource->AddEventSink(GetSingleton());
  } else {
    logger::warn("Failed to register GenderBender compatibility event sink");
  }
#if defined(EXCLUSIVE_SKYRIM_FLAT)
  eventSourceHolder->AddEventSink<RE::TESFastTravelEndEvent>(GetSingleton());
#endif
  registered = true;
  logger::info("Registered equipment refresh event sink");
}

void EquipmentRefreshEventSink::CancelQueuedRefreshes() {
  ++g_eventRefreshGeneration;
  ClearAutomaticEquipmentStateEvents();
  {
    std::lock_guard lock(g_actorRefreshMutex);
    g_actorRefreshGeneration.clear();
    g_pendingActorStateReconciliations.clear();
    g_pendingEquipmentChangeRefreshActors.clear();
    g_pendingContextBoundaryRefreshActors.clear();
  }
  GetSingleton()->refreshWork_.Cancel();
  GetSingleton()->conditionPollWork_.Cancel();
  GetSingleton()->nextConditionPollMillis_.store(0);
  GetSingleton()->actorConditionSignatures_.clear();
  GetSingleton()->hasWorldConditionSignature_ = false;
}

RE::BSEventNotifyControl EquipmentRefreshEventSink::ProcessEvent(
    const RE::TESEquipEvent *a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESEquipEvent> *a_eventSource) {
  if (!IsGameReady()) {
    return RE::BSEventNotifyControl::kContinue;
  }

  if (!a_event || a_event->baseObject == 0) {
    return RE::BSEventNotifyControl::kContinue;
  }

  const auto *armor =
      RE::TESForm::LookupByID<RE::TESObjectARMO>(a_event->baseObject);
  auto *actorRef = a_event->actor.get();
  auto *actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
  if (!actor) {
    return RE::BSEventNotifyControl::kContinue;
  }
  // Weapon equip/unequip can change IsWeaponOut and arbitrary custom
  // condition functions without changing the worn-armor list. Keep this
  // branch display-only; the normal armor transaction path remains below.
  if (!armor) {
    QueueConditionRefreshForActor(actor);
    return RE::BSEventNotifyControl::kContinue;
  }

  // TNG equips its non-playable TNG_GenitalCover as an invisible slot-52
  // renderer blocker. It is neither user gear nor the genital addon. Never
  // publish it to the workbench, virtual-token, DD, or actual-equipment strip
  // pipelines. A display-only refresh is sufficient when this actor already
  // has SFS state and keeps the event actor-local for players and NPCs.
  if (sfs::armor::IsTngGenitalCoverArmor(armor)) {
    auto *menu = sfs::Menu::GetSingleton();
    const auto actorFormID = actor->GetFormID();
    const bool hasWorkbenchDisplayState =
        menu != nullptr && menu->IsGameDataLoaded() &&
        menu->GetWorkbench().HasDisplayStateForActor(actorFormID);
    const auto *player = RE::PlayerCharacter::GetSingleton();
    const bool isPlayer =
        player != nullptr && player->GetFormID() == actorFormID;
    if (sfs::armor::rules::ShouldRefreshForGenitalCoverEvent(
            sfs::native::genital_compatibility::IsTngInstalled(), isPlayer,
            sfs::native::HasFittingSlotState(actor),
            hasWorkbenchDisplayState)) {
      QueueActorRefresh(actorFormID, false);
    }
    logger::debug("Ignored TNG internal genital-cover equipment event "
                  "actor={:08X} armor={:08X} equipped={}",
                  actorFormID, armor->GetFormID(), a_event->equipped);
    return RE::BSEventNotifyControl::kContinue;
  }

  sfs::virtual_tokens::HandleVirtualWornTokenEquipEvent(
      actor, const_cast<RE::TESObjectARMO *>(armor), a_event->equipped);
  sfs::devious_devices::ObserveDeviousDevicesRenderedDeviceEquipEvent(
      actor, const_cast<RE::TESObjectARMO *>(armor), a_event->equipped);
  sfs::native::SynchronizeArmorClassificationKeywords(
      const_cast<RE::TESObjectARMO *>(armor));

  const bool isGenitalArmor = sfs::armor::IsSosTngGenitalArmor(armor);
  if (isGenitalArmor) {
    if (a_event->equipped) {
      sfs::native::RememberGenitalArmor(actor, armor);
    } else {
      sfs::native::ForgetGenitalArmor(actor);
    }
    sfs::native::RequestSOSStorageSync();
  }

  const auto activeFittingSlotMask =
      sfs::native::GetActiveFittingSlotMask(actor);
  const bool hasTrackedFittingState = sfs::native::HasFittingSlotState(actor);
  const bool hasSfsDisplayState =
      activeFittingSlotMask != 0 || hasTrackedFittingState;
  const bool untrackedGenitalArmorEvent = !hasSfsDisplayState && isGenitalArmor;
  auto *menu = sfs::Menu::GetSingleton();
  const auto actualEquipmentLinkedSlotMask =
      menu != nullptr
          ? menu->GetWorkbench().GetActualEquipmentLinkedSlotMaskForActor(
                actor->GetFormID())
          : std::uint64_t{0};
  const bool actorHasActualEquipmentLinks =
      actualEquipmentLinkedSlotMask != 0;
  const auto externalEquipmentEvent =
      sfs::native::external_equipment::ConsumeEquipmentEvent(
          actor, armor, a_event->equipped,
          actualEquipmentLinkedSlotMask);
  const bool eventAddedArmor =
      a_event->equipped &&
      (sfs::virtual_tokens::IsVirtualWornTokenEventAddedArmor(
           actor->GetFormID(), armor->GetFormID()) ||
       sfs::native::external_equipment::IsEventAddedActualEquipment(
           actor->GetFormID(), armor->GetFormID()));
  if (eventAddedArmor) {
    InitializeEventAddedActualVisibility(actor, armor->GetFormID());
  }
  const bool hasAutomaticAppearanceTracking =
      externalEquipmentEvent.accepted &&
      IsActualEquipmentStripLinkPolicyActive() &&
      actorHasActualEquipmentLinks;

  if (hasAutomaticAppearanceTracking) {
    RecordAutomaticEquipmentStateEvent(actor->GetFormID(), armor->GetFormID(),
                                       a_event->equipped);
    const auto armorName = sfs::armor::GetDisplayName(armor);
    const auto armorSlotMask = sfs::armor::GetArmorDisplaySlotMask(armor);
    const auto controlSlotMask =
        GetAutomaticEquipmentControlSlotMask(armor);
    logger::info("Actual-equipment event actor={:08X} armor={:08X} name=\"{}\" "
                 "slots={:016X} controlSlots={:016X} equipped={}",
                 actor->GetFormID(), armor->GetFormID(), armorName,
                 armorSlotMask, controlSlotMask, a_event->equipped);
  }

  if (!hasSfsDisplayState && !untrackedGenitalArmorEvent &&
      !hasAutomaticAppearanceTracking) {
    return RE::BSEventNotifyControl::kContinue;
  }

  if (hasAutomaticAppearanceTracking) {
    // Restore the v1.3.0 actual-equipment display boundary: publish the
    // actor-local event state first, coalesce every same-frame mutation with
    // QueueActorRefreshGeneration, then rebuild once on the SKSE main thread.
    // The v1.4 transaction ledger remains the authority, so manual inventory
    // changes are still ignored and SetOutfit/event-added scene gear cannot
    // leak into another actor. Only original gear leaving or returning needs
    // an SFS display rebuild; props and replacement outfits use the engine's
    // own equip update. Their row was initialized visible above, while both
    // the individual and global eye controls remain authoritative afterward.
    if (externalEquipmentEvent.originalStripped ||
        externalEquipmentEvent.originalItemRestored) {
      MarkEquipmentChangeRefreshPending(actor->GetFormID());
      QueueActorRefresh(actor->GetFormID());
    }
    logger::debug("Observed external actual-equipment boundary "
                  "actor={:08X} armor={:08X} equipped={} originalStrip={} "
                  "originalItemRestore={} recoveryComplete={} "
                  "eventAddedEquip={} eventAddedRemove={}",
                  actor->GetFormID(), armor->GetFormID(), a_event->equipped,
                  externalEquipmentEvent.originalStripped,
                  externalEquipmentEvent.originalItemRestored,
                  externalEquipmentEvent.originalRecoveryCompleted,
                  externalEquipmentEvent.eventAddedEquipped,
                  externalEquipmentEvent.eventAddedRemoved);
    return RE::BSEventNotifyControl::kContinue;
  }
  MarkEquipmentChangeRefreshPending(actor->GetFormID());
  if (sfs::virtual_tokens::IsVirtualWornTokenRecoveryBurstActive(
          actor->GetFormID()) &&
      !eventAddedArmor) {
    logger::debug("Deferred equipment refresh inside strip transaction "
                  "actor={:08X} armor={:08X} equipped={}",
                  actor->GetFormID(), armor->GetFormID(), a_event->equipped);
    return RE::BSEventNotifyControl::kContinue;
  }
  QueueActorRefresh(actor->GetFormID());
  return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EquipmentRefreshEventSink::ProcessEvent(
    const RE::TESActorLocationChangeEvent *a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESActorLocationChangeEvent>
        *a_eventSource) {
  if (!IsGameReady() || !a_event || a_event->oldLoc == a_event->newLoc) {
    return RE::BSEventNotifyControl::kContinue;
  }

  auto *actorRef = a_event->actor.get();
  auto *actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
  QueueContextRefreshForActor(actor);
  return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EquipmentRefreshEventSink::ProcessEvent(
    [[maybe_unused]] const RE::TESCellFullyLoadedEvent *a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESCellFullyLoadedEvent>
        *a_eventSource) {
  if (IsGameReady()) {
    QueuePlayerContextRefresh();
  }
  return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EquipmentRefreshEventSink::ProcessEvent(
    const RE::TESCombatEvent *a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESCombatEvent> *a_eventSource) {
  if (!IsGameReady() || !a_event) {
    return RE::BSEventNotifyControl::kContinue;
  }

  auto queueActor = [](RE::TESObjectREFR *a_reference) {
    auto *actor = a_reference != nullptr ? a_reference->As<RE::Actor>()
                                          : nullptr;
    QueueConditionRefreshForActor(actor);
  };
  queueActor(a_event->actor.get());
  if (a_event->targetActor.get() != a_event->actor.get()) {
    queueActor(a_event->targetActor.get());
  }

  return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EquipmentRefreshEventSink::ProcessEvent(
    const SKSE::ModCallbackEvent *a_event,
    [[maybe_unused]] RE::BSTEventSource<SKSE::ModCallbackEvent>
        *a_eventSource) {
  if (!IsGameReady() || !a_event || !a_event->eventName.c_str()) {
    return RE::BSEventNotifyControl::kContinue;
  }

  const auto *eventName = a_event->eventName.c_str();
  if (std::strcmp(eventName, "ReSchlongify") != 0 || !a_event->strArg.c_str()) {
    return RE::BSEventNotifyControl::kContinue;
  }

  const bool goToActive =
      std::strcmp(a_event->strArg.c_str(), "GoToActive") == 0;
  const bool goToSchlongless =
      std::strcmp(a_event->strArg.c_str(), "GoToSchlongless") == 0;
  if (!goToActive && !goToSchlongless) {
    return RE::BSEventNotifyControl::kContinue;
  }

  const auto actorFormID = static_cast<RE::FormID>(
      std::llround(static_cast<double>(a_event->numArg)));
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
  if (!actor) {
    logger::warn("SFS SOS compatibility: ignored ReSchlongify {} for "
                 "unresolved actor {:08X}",
                 a_event->strArg.c_str(), actorFormID);
    return RE::BSEventNotifyControl::kContinue;
  }

  if (goToActive) {
    sfs::native::RequestGenitalArmorResolution(actor, true);
  } else {
    sfs::native::ForgetGenitalArmor(actor);
  }
  logger::debug("SFS SOS compatibility: queued actor-local refresh after {} "
                "actor={:08X}",
                a_event->strArg.c_str(), actorFormID);
  QueueActorRefresh(actorFormID);

  return RE::BSEventNotifyControl::kContinue;
}

#if defined(EXCLUSIVE_SKYRIM_FLAT)
RE::BSEventNotifyControl EquipmentRefreshEventSink::ProcessEvent(
    [[maybe_unused]] const RE::TESFastTravelEndEvent *a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESFastTravelEndEvent>
        *a_eventSource) {
  if (IsGameReady()) {
    QueuePlayerContextRefresh();
  }
  return RE::BSEventNotifyControl::kContinue;
}
#endif

void EquipmentRefreshEventSink::QueueRefresh() {
  const auto generation = CurrentEventRefreshGeneration();
  const auto ticket = refreshWork_.Start();
  if (!ticket.has_value()) {
    return;
  }

  auto *taskInterface = SKSE::GetTaskInterface();
  if (!taskInterface) {
    refreshWork_.Finish(*ticket);
    logger::warn(
        "Skipped equipment refresh because SKSE task interface is unavailable");
    return;
  }

  taskInterface->AddTask([generation, ticket = *ticket]() {
    auto *sink = EquipmentRefreshEventSink::GetSingleton();
    // Finish only this callback's enrollment, never a post-load replacement.
    if (!sink->refreshWork_.Finish(ticket) ||
        !IsCurrentEventRefreshGeneration(generation)) {
      return;
    }
    sink->RunRefresh();
  });
}

void EquipmentRefreshEventSink::QueueActorRefresh(RE::FormID a_actorFormID,
                                                  const bool a_reconcileState) {
  if (a_actorFormID == 0 || !IsGameReady()) {
    return;
  }

  if (a_reconcileState) {
    MarkActorStateReconciliationPending(a_actorFormID);
  }

  auto *taskInterface = SKSE::GetTaskInterface();
  if (!taskInterface) {
    logger::warn("Skipped actor equipment refresh because SKSE task interface "
                 "is unavailable");
    return;
  }

  const auto eventGeneration = CurrentEventRefreshGeneration();
  const auto actorRefreshGeneration =
      QueueActorRefreshGeneration(a_actorFormID);
  taskInterface->AddTask(
      [a_actorFormID, eventGeneration, actorRefreshGeneration]() {
        if (!IsCurrentEventRefreshGeneration(eventGeneration)) {
          return;
        }
        if (!IsLatestActorRefreshGeneration(a_actorFormID,
                                            actorRefreshGeneration)) {
          return;
        }
        RunActorRefresh(a_actorFormID);
      });
}

void EquipmentRefreshEventSink::RunRefresh() {
  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return;
  }

  menu->GetWorkbench().RefreshNativeArmorOverrides(menu->GetConditions(), 0,
                                                   true);
}

void EquipmentRefreshEventSink::TickConditionState() {
  const auto generation = CurrentEventRefreshGeneration();
  if (!IsGameReady()) {
    return;
  }

  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  if (now < nextConditionPollMillis_.load(std::memory_order_relaxed)) {
    return;
  }
  nextConditionPollMillis_.store(now + kConditionPollIntervalMillis,
                                 std::memory_order_relaxed);

  const auto ticket = conditionPollWork_.Start();
  if (!ticket.has_value()) {
    return;
  }

  auto *taskInterface = SKSE::GetTaskInterface();
  if (!taskInterface) {
    conditionPollWork_.Finish(*ticket);
    return;
  }

  taskInterface->AddTask([this, generation, ticket = *ticket]() {
    if (conditionPollWork_.Finish(ticket) &&
        IsCurrentEventRefreshGeneration(generation) && IsGameReady()) {
      PollConditionState();
    }
  });
}

void EquipmentRefreshEventSink::PollConditionState() {
  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    actorConditionSignatures_.clear();
    hasWorldConditionSignature_ = false;
    return;
  }

  const auto *player = RE::PlayerCharacter::GetSingleton();
  const auto playerFormID = player != nullptr ? player->GetFormID() : 0;
  if (playerFormID == 0) {
    return;
  }

  // Copy only condition assignments that can affect a registered actor. The
  // workbench lock is released before TESCondition::IsTrue is evaluated so
  // this fallback cannot block UI edits or equipment reconciliation.
  std::unordered_map<RE::FormID, std::unordered_set<std::string>> tracked;
  {
    auto workbenchLock = menu->GetWorkbench().AcquireStateLock();
    for (const auto &row : menu->GetWorkbench().GetRows()) {
      if (!row.conditionId.has_value() || row.conditionId->empty()) {
        continue;
      }
      const auto actorFormID = row.ownerActorFormID != 0
                                   ? row.ownerActorFormID
                                   : playerFormID;
      tracked[actorFormID].insert(*row.conditionId);
    }
    for (const auto &rule :
         menu->GetWorkbench().GetConditionalVisibilityRules()) {
      if (rule.conditionId.empty()) {
        continue;
      }
      const auto actorFormID = rule.ownerActorFormID != 0
                                   ? rule.ownerActorFormID
                                   : playerFormID;
      tracked[actorFormID].insert(rule.conditionId);
    }
  }

  if (tracked.empty()) {
    actorConditionSignatures_.clear();
    hasWorldConditionSignature_ = false;
    return;
  }

  // Materialization is independent of the actor passed to TESCondition::IsTrue.
  // Snapshot each distinct condition once per poll, then release the editor
  // state lock before touching actor state. This keeps a shared condition used
  // by several NPCs from repeating cache/status work and prevents engine
  // evaluation from holding up condition editing in the UI.
  std::unordered_set<std::string> distinctConditionIDs;
  for (const auto &[_, conditionIDs] : tracked) {
    distinctConditionIDs.insert(conditionIDs.begin(), conditionIDs.end());
  }
  std::unordered_map<std::string, std::shared_ptr<RE::TESCondition>>
      materializedConditions;
  materializedConditions.reserve(distinctConditionIDs.size());
  {
    auto conditionStateLock = menu->AcquireConditionStateLock();
    auto &conditions = menu->GetConditions();
    for (const auto &conditionID : distinctConditionIDs) {
      std::shared_ptr<RE::TESCondition> condition;
      if (const auto *definition =
              sfs::conditions::FindDefinitionById(conditions, conditionID);
          definition != nullptr &&
          sfs::conditions::IsWorkbenchSelectable(*definition) &&
          sfs::conditions::EvaluateDefinitionStatus(*definition, conditions)
              .IsActive()) {
        if (const auto materialized =
                sfs::conditions::MaterializeConditionById(conditionID,
                                                           conditions);
            materialized.has_value()) {
          condition = materialized->condition;
        }
      }
      materializedConditions.emplace(conditionID, std::move(condition));
    }
  }

  std::unordered_set<RE::FormID> observedActors;
  observedActors.reserve(tracked.size());
  for (auto &[actorFormID, conditionIDs] : tracked) {
    auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
    if (!actor) {
      continue;
    }
    observedActors.insert(actorFormID);

    ActorConditionSignature signature{};
    signature.inCombat = actor->IsInCombat();
    signature.sneaking = actor->IsSneaking();
    if (const auto *actorState = actor->AsActorState()) {
      signature.weaponDrawn = actorState->IsWeaponDrawn();
      signature.swimming = actorState->IsSwimming();
    }

    std::vector<std::string> sortedConditionIDs(conditionIDs.begin(),
                                                conditionIDs.end());
    std::ranges::sort(sortedConditionIDs);
    for (const auto &conditionID : sortedConditionIDs) {
      bool active = false;
      if (const auto materialized = materializedConditions.find(conditionID);
          materialized != materializedConditions.end() &&
          materialized->second) {
        active = materialized->second->IsTrue(actor, actor);
      }
      signature.conditionTruthHash = HashConditionTruth(
          signature.conditionTruthHash, conditionID, active);
    }

    const auto previous = actorConditionSignatures_.find(actorFormID);
    if (previous != actorConditionSignatures_.end() &&
        previous->second != signature) {
      // The poll never rebuilds armor on every interval; only a signature
      // transition queues the existing actor-local refresh path.
      QueueActorRefresh(actorFormID, false);
    }
    actorConditionSignatures_.insert_or_assign(actorFormID, signature);
  }

  for (auto it = actorConditionSignatures_.begin();
       it != actorConditionSignatures_.end();) {
    if (!observedActors.contains(it->first)) {
      it = actorConditionSignatures_.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace sfs::workbench
