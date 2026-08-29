#include "native/FittingSlotState.h"

#include <SKSE/SKSE.h>

#include <mutex>
#include <unordered_map>

namespace {
struct ActorFittingSlotState {
  std::uint32_t virtualTokenSuppressedSlotMask{0};
  std::uint32_t headgearToggleSuppressedSlotMask{0};
  std::uint32_t headgearToggleManualVisibleSlotMask{0};
};

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

[[nodiscard]] bool IsEmpty(const ActorFittingSlotState &a_state) {
  return a_state.virtualTokenSuppressedSlotMask == 0 &&
         a_state.headgearToggleSuppressedSlotMask == 0 &&
         a_state.headgearToggleManualVisibleSlotMask == 0;
}

[[nodiscard]] std::uint32_t GetEffectiveHeadgearToggleSuppressedSlotMask(
    const ActorFittingSlotState &a_state) {
  return a_state.headgearToggleSuppressedSlotMask &
         ~a_state.headgearToggleManualVisibleSlotMask;
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
  if (a_suppressed) {
    state.virtualTokenSuppressedSlotMask |= a_slotMask;
  } else {
    state.virtualTokenSuppressedSlotMask &= ~a_slotMask;
    if (IsEmpty(state)) {
      g_actorStates.erase(actorFormID);
    }
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
  if (a_suppressed) {
    const auto newlySuppressed =
        a_slotMask & ~state.headgearToggleSuppressedSlotMask;
    state.headgearToggleSuppressedSlotMask |= a_slotMask;
    // A new Helmet Toggle hide transition takes control again.  This leaves
    // the user's saved card visibility intact while resetting only the
    // temporary manual-show override from the preceding hide interval.
    state.headgearToggleManualVisibleSlotMask &= ~newlySuppressed;
  } else {
    state.headgearToggleSuppressedSlotMask &= ~a_slotMask;
    state.headgearToggleManualVisibleSlotMask &= ~a_slotMask;
    if (IsEmpty(state)) {
      g_actorStates.erase(actorFormID);
    }
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
  const auto previousMask = GetEffectiveHeadgearToggleSuppressedSlotMask(state);
  const auto newlySuppressed =
      a_slotMask & ~state.headgearToggleSuppressedSlotMask;
  state.headgearToggleSuppressedSlotMask = a_slotMask;
  state.headgearToggleManualVisibleSlotMask &= a_slotMask;
  state.headgearToggleManualVisibleSlotMask &= ~newlySuppressed;
  const auto currentMask = GetEffectiveHeadgearToggleSuppressedSlotMask(state);
  if (IsEmpty(state)) {
    g_actorStates.erase(stateIt);
  }
  return previousMask != currentMask;
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
  if (IsEmpty(stateIt->second)) {
    g_actorStates.erase(stateIt);
  }
}

void ClearAllHeadgearToggleFittingSlotStates() {
  std::lock_guard lock(g_stateMutex);
  for (auto stateIt = g_actorStates.begin(); stateIt != g_actorStates.end();) {
    stateIt->second.headgearToggleSuppressedSlotMask = 0;
    stateIt->second.headgearToggleManualVisibleSlotMask = 0;
    if (IsEmpty(stateIt->second)) {
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
  const auto previousMask = GetEffectiveHeadgearToggleSuppressedSlotMask(state);
  if (a_visible) {
    state.headgearToggleManualVisibleSlotMask |=
        a_slotMask & state.headgearToggleSuppressedSlotMask;
  } else {
    state.headgearToggleManualVisibleSlotMask &= ~a_slotMask;
  }
  const auto currentMask = GetEffectiveHeadgearToggleSuppressedSlotMask(state);
  const bool changed = previousMask != currentMask;
  if (IsEmpty(state)) {
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
  return GetEffectiveHeadgearToggleSuppressedSlotMask(GetState(a_actor));
}

} // namespace sfs::native
