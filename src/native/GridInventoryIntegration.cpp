#include "native/GridInventoryIntegration.h"
#include "native/IntegrationCompatibilityRules.h"

#include "third_party/gridinventory/GridInventoryCostumeAPI.h"
#include "ui/Menu.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace sfs::native::grid_inventory {
namespace {
constexpr std::uint32_t kPlayerFormID = 0x00000014;

struct CostumeSnapshot {
  std::int32_t tab{-1};
  std::vector<std::uint32_t> armorFormIDs;
  std::uint64_t generation{0};
};

std::mutex g_stateLock;
std::optional<CostumeSnapshot> g_pendingSnapshot;
std::optional<CostumeSnapshot> g_lastAppliedSnapshot;
std::uint64_t g_nextGeneration{0};
bool g_gameDataLoaded{false};
// Grid Inventory announces its current Costume once after a save/load or
// revert.  That is a state restoration, not a user Costume change: SFS keeps
// the co-save's registered appearances at that boundary and starts following
// Grid only from the next transition the player makes in Grid Inventory.
bool g_discardNextRestoredState{true};

[[nodiscard]] bool SameCostumeState(const CostumeSnapshot &a_left,
                                    const CostumeSnapshot &a_right) {
  return a_left.tab == a_right.tab &&
         a_left.armorFormIDs == a_right.armorFormIDs;
}

void OnGridInventoryMessage(SKSE::MessagingInterface::Message *a_message) {
  // This listener is deliberately unfiltered.  SKSE lifecycle message values
  // and plugin message values have different namespaces, so never inspect any
  // non-Grid 4CC here.
  if (a_message == nullptr ||
      a_message->type != GridInvAPI::kMsgCostumeState) {
    return;
  }
  if (a_message->data == nullptr ||
      a_message->dataLen < sizeof(GridInvAPI::CostumeState)) {
    logger::warn("[GRID COSTUME] ignored malformed CostumeState message");
    return;
  }

  const auto *state =
      static_cast<const GridInvAPI::CostumeState *>(a_message->data);
  if (!integration_rules::CanReadCostumePrefix(
          a_message->dataLen, state->structSize, state->pieceCount,
          state->pieces != nullptr)) {
    logger::warn("[GRID COSTUME] ignored malformed CostumeState message "
                 "(abi={}, size={}, pieces={})",
                 state->abiVersion, state->structSize, state->pieceCount);
    return;
  }
  if (state->abiVersion != GridInvAPI::kABIVersion) {
    logger::warn("[GRID COSTUME] ABI {} is not individually audited; using the v1 message/ItemKey prefix (compatibility assumed)", state->abiVersion);
  }

  CostumeSnapshot snapshot;
  snapshot.tab = state->tab;
  snapshot.armorFormIDs.reserve(state->pieceCount);
  for (std::uint32_t index = 0; index < state->pieceCount; ++index) {
    const auto &piece = state->pieces[index];
    // Grid Inventory's public contract sends player-owned, plain-form armor
    // keys.  Preserve the contract boundary here and copy only the FormID.
    if (piece.owner == kPlayerFormID && piece.uid == 0 && piece.base != 0) {
      snapshot.armorFormIDs.push_back(piece.base);
    }
  }
  std::sort(snapshot.armorFormIDs.begin(), snapshot.armorFormIDs.end());
  snapshot.armorFormIDs.erase(
      std::unique(snapshot.armorFormIDs.begin(), snapshot.armorFormIDs.end()),
      snapshot.armorFormIDs.end());

  {
    std::scoped_lock lock(g_stateLock);
    if (g_discardNextRestoredState) {
      g_discardNextRestoredState = false;
      g_pendingSnapshot.reset();
      logger::info("[GRID COSTUME] ignored restored Costume state for tab {}",
                   snapshot.tab);
      return;
    }

    // No selected Costume, no armor pieces, or no player-owned armor means
    // Grid Inventory is not supplying an appearance layout for SFS.  It is
    // never an instruction to clear the player's saved SFS registrations.
    if (snapshot.tab < 0 || snapshot.armorFormIDs.empty()) {
      g_pendingSnapshot.reset();
      logger::info(
          "[GRID COSTUME] ignored empty Costume state for tab {}; "
          "SFS appearances unchanged",
          snapshot.tab);
      return;
    }
    snapshot.generation = ++g_nextGeneration;
    g_pendingSnapshot = std::move(snapshot);
  }

  logger::info("[GRID COSTUME] received tab {} ({} armor form(s))",
               state->tab, state->pieceCount);
}
} // namespace

void RegisterMessageListener(const SKSE::MessagingInterface *a_messaging) {
  if (a_messaging == nullptr) {
    logger::warn("[GRID COSTUME] SKSE messaging interface unavailable");
    return;
  }

  // nullptr is intentional.  RegisterListener(callback) filters to the SKSE
  // lifecycle sender and would never receive Grid Inventory plugin messages.
  const bool registered =
      a_messaging->RegisterListener(nullptr, OnGridInventoryMessage);
  logger::info("[GRID COSTUME] Grid Inventory message listener {}",
               registered ? "registered" : "failed to register");
}

void SetGameDataLoaded(const bool a_loaded) {
  std::scoped_lock lock(g_stateLock);
  g_gameDataLoaded = a_loaded;
  if (!a_loaded) {
    // A pending Costume belongs to the outgoing save.  Grid Inventory emits a
    // fresh restored state after its own load/revert flow; that first state is
    // deliberately ignored so only subsequent Grid Costume changes drive SFS.
    g_pendingSnapshot.reset();
    g_lastAppliedSnapshot.reset();
    g_discardNextRestoredState = true;
  }
}

void ProcessPendingCostumeState() {
  CostumeSnapshot snapshot;
  {
    std::scoped_lock lock(g_stateLock);
    if (!g_gameDataLoaded || !g_pendingSnapshot.has_value()) {
      return;
    }
    if (g_lastAppliedSnapshot.has_value() &&
        SameCostumeState(*g_lastAppliedSnapshot, *g_pendingSnapshot)) {
      g_pendingSnapshot.reset();
      return;
    }
    snapshot = *g_pendingSnapshot;
  }

  // This runs from SFS's established UI/game processing point rather than the
  // Grid callback.  It keeps the borrowed Grid pointer out of deferred work
  // and waits safely for SFS game data/player state to be ready.
  // Keep a second non-destructive guard here in case a future Grid ABI or a
  // legacy caller ever queues an empty state outside the callback boundary.
  if (snapshot.tab < 0 || snapshot.armorFormIDs.empty()) {
    std::scoped_lock lock(g_stateLock);
    if (g_pendingSnapshot.has_value() &&
        g_pendingSnapshot->generation == snapshot.generation) {
      g_pendingSnapshot.reset();
    }
    return;
  }

  const bool applied = Menu::GetSingleton()->ApplyGridInventoryCostume(
      snapshot.armorFormIDs.data(),
      static_cast<std::uint32_t>(snapshot.armorFormIDs.size()));
  if (!applied) {
    // Keep the latest state only.  The existing safe processing point retries
    // while SFS is still loading; no actor scan or periodic work happens once
    // the state has been accepted.
    return;
  }

  std::scoped_lock lock(g_stateLock);
  if (!g_pendingSnapshot.has_value() ||
      g_pendingSnapshot->generation != snapshot.generation) {
    return;
  }
  g_lastAppliedSnapshot = snapshot;
  g_pendingSnapshot.reset();
  logger::info("[GRID COSTUME] applied player Costume tab {} ({} armor form(s))",
               snapshot.tab, snapshot.armorFormIDs.size());
}

} // namespace sfs::native::grid_inventory
