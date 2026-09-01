#include "native/FittingSlotState.h"

#include "native/FittingSlotStateRules.h"

#include <SKSE/SKSE.h>

#include <mutex>
#include <unordered_map>

namespace {
using ActorFittingSlotState =
    sfs::native::fitting_slot_rules::ActorFittingSlotState;

constexpr std::uint32_t kFittingStateSerializationType = 'FSTS';
constexpr std::uint32_t kFittingStateSerializationVersion = 2;

std::mutex g_stateMutex;
std::unordered_map<RE::FormID, ActorFittingSlotState> g_actorStates;

[[nodiscard]] RE::FormID GetActorFormID(const RE::Actor *a_actor) {
  return a_actor ? a_actor->GetFormID() : 0;
}

[[nodiscard]] ActorFittingSlotState GetState(RE::Actor *a_actor) {
  const auto actorFormID = GetActorFormID(a_actor);
  if (actorFormID == 0) {
    return {};
  }
  std::lock_guard lock(g_stateMutex);
  const auto stateIt = g_actorStates.find(actorFormID);
  return stateIt != g_actorStates.end() ? stateIt->second
                                       : ActorFittingSlotState{};
}

} // namespace

namespace sfs::native {
void SetVirtualTokenFittingSlotsSuppressed(RE::Actor *a_actor,
                                           const std::uint32_t a_slotMask,
                                           const bool a_suppressed) {
  const auto actorFormID = GetActorFormID(a_actor);
  if (actorFormID == 0 || a_slotMask == 0) {
    return;
  }
  std::lock_guard lock(g_stateMutex);
  auto &state = g_actorStates[actorFormID];
  fitting_slot_rules::SetVirtualTokenSuppressed(state, a_slotMask,
                                                a_suppressed);
  if (fitting_slot_rules::IsEmpty(state)) {
    g_actorStates.erase(actorFormID);
  }
}

void SetHeadgearToggleFittingSlotsSuppressed(
    RE::Actor *a_actor, const std::uint32_t a_slotMask,
    const bool a_suppressed) {
  const auto actorFormID = GetActorFormID(a_actor);
  if (actorFormID == 0 || a_slotMask == 0) {
    return;
  }
  std::lock_guard lock(g_stateMutex);
  auto &state = g_actorStates[actorFormID];
  fitting_slot_rules::SetHeadgearSuppressed(state, a_slotMask, a_suppressed);
  if (fitting_slot_rules::IsEmpty(state)) {
    g_actorStates.erase(actorFormID);
  }
}

bool ReplaceHeadgearToggleFittingSlotsSuppressed(
    RE::Actor *a_actor, const std::uint32_t a_slotMask) {
  const auto actorFormID = GetActorFormID(a_actor);
  if (actorFormID == 0) {
    return false;
  }

  std::lock_guard lock(g_stateMutex);
  auto stateIt = g_actorStates.find(actorFormID);
  if (stateIt == g_actorStates.end()) {
    if (a_slotMask == 0) {
      return false;
    }
    stateIt = g_actorStates.emplace(actorFormID, ActorFittingSlotState{}).first;
  }

  auto &state = stateIt->second;
  const bool changed =
      fitting_slot_rules::ReplaceHeadgearSuppressed(state, a_slotMask);
  if (fitting_slot_rules::IsEmpty(state)) {
    g_actorStates.erase(stateIt);
  }
  return changed;
}

void ClearHeadgearToggleFittingSlotsSuppressed(RE::Actor *a_actor) {
  const auto actorFormID = GetActorFormID(a_actor);
  if (actorFormID == 0) {
    return;
  }
  std::lock_guard lock(g_stateMutex);
  const auto stateIt = g_actorStates.find(actorFormID);
  if (stateIt == g_actorStates.end()) {
    return;
  }
  stateIt->second.headgearToggleSuppressedSlotMask = 0;
  stateIt->second.headgearToggleManualVisibleSlotMask = 0;
  if (fitting_slot_rules::IsEmpty(stateIt->second)) {
    g_actorStates.erase(stateIt);
  }
}

void ClearAllHeadgearToggleFittingSlotStates() {
  std::lock_guard lock(g_stateMutex);
  for (auto stateIt = g_actorStates.begin(); stateIt != g_actorStates.end();) {
    stateIt->second.headgearToggleSuppressedSlotMask = 0;
    stateIt->second.headgearToggleManualVisibleSlotMask = 0;
    if (fitting_slot_rules::IsEmpty(stateIt->second)) {
      stateIt = g_actorStates.erase(stateIt);
    } else {
      ++stateIt;
    }
  }
}

bool SetHeadgearToggleFittingSlotsManualVisible(
    RE::Actor *a_actor, const std::uint32_t a_slotMask,
    const bool a_visible) {
  const auto actorFormID = GetActorFormID(a_actor);
  if (actorFormID == 0 || a_slotMask == 0) {
    return false;
  }

  std::lock_guard lock(g_stateMutex);
  const auto stateIt = g_actorStates.find(actorFormID);
  if (stateIt == g_actorStates.end()) {
    return false;
  }

  auto &state = stateIt->second;
  const bool changed = fitting_slot_rules::SetHeadgearManualVisible(
      state, a_slotMask, a_visible);
  if (fitting_slot_rules::IsEmpty(state)) {
    g_actorStates.erase(stateIt);
  }
  return changed;
}

void ReconcileFittingSlotState([[maybe_unused]] RE::Actor *a_actor) {}

void ClearFittingSlotState(RE::Actor *a_actor) {
  const auto actorFormID = GetActorFormID(a_actor);
  if (actorFormID == 0) {
    return;
  }
  std::lock_guard lock(g_stateMutex);
  g_actorStates.erase(actorFormID);
}

void ClearAllFittingSlotStates() {
  std::lock_guard lock(g_stateMutex);
  g_actorStates.clear();
}

void SerializeFittingSlotStates(SKSE::SerializationInterface *a_skse) {
  if (!a_skse) {
    return;
  }
  // Keep an empty record so the fixed sequence of co-save records remains
  // compatible with earlier releases that serialized external slot state.
  a_skse->WriteRecord(kFittingStateSerializationType,
                      kFittingStateSerializationVersion, nullptr, 0);
}

void DeserializeFittingSlotStates(SKSE::SerializationInterface *a_skse) {
  ClearAllFittingSlotStates();
  if (!a_skse) {
    return;
  }

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    return;
  }
  if (type != kFittingStateSerializationType) {
    logger::warn("Skipping unexpected SFS fitting-state record type {:X}",
                 type);
    return;
  }

  // Consume legacy v1 JSON without restoring runtime-only state.  This also
  // deliberately drops pre-v1.4.4 Helmet Toggle suppression masks: the old
  // patch used a fixed controller-slot mask and cannot be safely projected
  // into the user's current ModSettings/Vanilla/Direct linking policy.  The
  // saved workbench eye state is stored separately and remains untouched;
  // Helmet Toggle will rebuild its actor-local temporary state on its next
  // callback.
  if (length != 0) {
    std::string ignored(length, '\0');
    if (!a_skse->ReadRecordData(ignored.data(), length)) {
      logger::warn("Failed to consume legacy SFS fitting-state record");
    }
  }
}

bool HasFittingSlotState(RE::Actor *a_actor) {
  const auto actorFormID = GetActorFormID(a_actor);
  if (actorFormID == 0) {
    return false;
  }
  std::lock_guard lock(g_stateMutex);
  return g_actorStates.contains(actorFormID);
}

std::uint32_t GetSuppressedFittingSlotMask(RE::Actor *a_actor) {
  return GetState(a_actor).virtualTokenSuppressedSlotMask;
}

std::uint32_t
GetVirtualTokenSuppressedFittingSlotMask(RE::Actor *a_actor) {
  return GetState(a_actor).virtualTokenSuppressedSlotMask;
}

std::uint32_t
GetHeadgearToggleSuppressedFittingSlotMask(RE::Actor *a_actor) {
  return fitting_slot_rules::GetEffectiveHeadgearSuppressedMask(
      GetState(a_actor));
}

} // namespace sfs::native
