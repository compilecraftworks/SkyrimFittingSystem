// Execute the production eye setter, ticket invalidation and settled redress
// functions. Engine boundaries are fakes; no equip/unequip API is provided.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RE {
using FormID = std::uint32_t;
struct Actor {
  FormID id;
  std::unordered_map<FormID, std::uint32_t> worn;
  FormID outfit{0};
};
std::unordered_map<FormID, Actor> actors;
struct TESForm {
  template <class T> static T *LookupByID(FormID id) {
    auto found = actors.find(id);
    return found == actors.end() ? nullptr : &found->second;
  }
};
struct PlayerCharacter {
  static Actor *GetSingleton() { return TESForm::LookupByID<Actor>(0x14); }
};
}
namespace logger { template <class... T> void info(T&&...) {} }
struct AppearanceTicketRef {
  std::string identity;
  RE::FormID actorID, armorID;
  std::uint32_t slotMask;
};
struct SuppressionTicket {
  std::uint64_t transactionID;
  RE::FormID actorID;
  std::vector<AppearanceTicketRef> appearances;
};
struct StripTransaction {
  std::uint64_t id;
  RE::FormID actorID, originalOutfitID{0};
  std::unordered_map<RE::FormID, std::uint32_t> originalWornArmor;
  bool originalOutfitRestoreObserved{false};
};
struct Mutation { RE::FormID actorID; std::uint32_t slotMask; };
std::mutex g_runtimeMutex;
std::atomic_uint64_t g_runtimeEpoch{1};
std::unordered_map<RE::FormID, std::uint64_t> g_actorAutomationGenerations,
    g_actorManualGenerations, g_recoveryProbeExpires, g_postRecoveryRefreshGenerations;
std::unordered_map<std::uint64_t, Mutation> g_pendingMutations;
std::unordered_map<std::string, AppearanceTicketRef> g_manualAutomationOverrides;
std::vector<SuppressionTicket> g_suppressionTickets;
std::unordered_map<std::uint64_t, StripTransaction> g_stripTransactions;
std::unordered_map<RE::FormID, std::uint32_t> masks, ddMasks;
bool modSettings = true;
int refreshes = 0;
RE::FormID ResolveActor(RE::FormID id) { return id ? id : 0x14; }
void ApplyOwnedMask(RE::FormID id, bool = true) {
  masks[id] = 0;
  for (const auto &ticket : g_suppressionTickets) {
    if (ticket.actorID != id) continue;
    for (const auto &ref : ticket.appearances) masks[id] |= ref.slotMask;
  }
}
bool HasActorStripTransactionLocked(RE::FormID id) {
  return std::ranges::any_of(g_stripTransactions,
      [id](const auto &entry) { return entry.second.actorID == id; });
}
RE::FormID CurrentDefaultOutfitID(RE::Actor *actor) { return actor->outfit; }
auto CaptureWornArmorSnapshot(RE::Actor *actor) { return actor->worn; }
void QueuePostRecoveryDisplayRefresh(RE::FormID) { ++refreshes; }
#include "ManualVisibilityTickets.production.inc"

namespace sfs::native {
std::uint32_t GetVirtualTokenSuppressedFittingSlotMask(RE::Actor *actor) {
  return masks[actor->id];
}
}
namespace sfs::devious_devices {
std::uint32_t GetDeviousDevicesHiderSuppressedFittingSlotMask(RE::Actor *actor) {
  return ddMasks[actor->id];
}
}
namespace sfs::virtual_tokens {
bool IsVirtualWornTokenAppearanceSuppressed(RE::FormID id, RE::FormID armor,
                                           std::uint32_t) {
  return std::ranges::any_of(g_suppressionTickets, [&](const auto &ticket) {
    return ticket.actorID == ResolveActor(id) &&
        std::ranges::any_of(ticket.appearances,
            [armor](const auto &ref) { return ref.armorID == armor; });
  });
}
}
namespace sfs::workbench {
bool IsModSettingsStripLinkPolicyActive() { return modSettings; }
struct Item { RE::FormID formID; std::uint32_t mask; bool hidden{false}, locked{false}; };
struct Row {
  RE::FormID ownerActorFormID;
  std::vector<Item> overrides;
  std::uint32_t GetOverrideVisualSlotMask(const Item &item) const { return item.mask; }
};
struct VariantWorkbench {
  std::vector<Row> rows_;
  std::recursive_mutex mutex;
  int changes{0};
  auto AcquireStateLock() { return std::unique_lock(mutex); }
  void MarkChanged() { ++changes; }
  void InvalidateAppearanceAutomation(RE::FormID id, RE::FormID armor,
                                     std::uint32_t mask, bool deleted) {
    id = ResolveActor(id);
    ddMasks[id] &= ~mask;
    InvalidateAppearanceTickets(id, std::to_string(armor), mask, deleted);
  }
  bool SetOverrideHidden(int row, int item, bool hidden);
};
#include "ManualVisibilitySetter.production.inc"
}

void Require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
void Reset() {
  RE::actors = {{0x14, {0x14, {}, 0}}, {0x42, {0x42, {}, 0}}};
  g_suppressionTickets = {
      {1, 0x14, {{"100", 0x14, 100, 4}, {"101", 0x14, 101, 8}}},
      {2, 0x42, {{"200", 0x42, 200, 4}}}};
  g_stripTransactions = {{1, {1, 0x14, 0, {{900, 4}, {901, 8}}, false}},
                         {2, {2, 0x42, 0, {{902, 4}}, false}}};
  g_actorAutomationGenerations.clear(); g_actorManualGenerations.clear();
  g_pendingMutations = {{1, {0x14, 4}}, {2, {0x42, 4}}, {3, {0x14, 8}}};
  g_recoveryProbeExpires.clear(); g_postRecoveryRefreshGenerations.clear();
  g_manualAutomationOverrides.clear(); masks.clear(); ddMasks.clear();
  modSettings = true; refreshes = 0;
  ApplyOwnedMask(0x14); ApplyOwnedMask(0x42);
}
int main() {
  try {
    using sfs::workbench::VariantWorkbench;
    Reset();
    VariantWorkbench bench;
    bench.rows_ = {{0, {{100, 4}, {101, 8}}}}; // legacy owner=0 player row
    CompleteSettledTransactions(0x14, "no-redress");
    Require(masks[0x14] == 12 && g_stripTransactions.size() == 2 && refreshes == 0,
            "No actual redress must leave appearances hidden without auto-recovery");
    Require(bench.SetOverrideHidden(0, 0, false),
            "First individual show must release runtime hide even when saved hidden=false");
    Require(masks[0x14] == 8 && masks[0x42] == 4 &&
                g_suppressionTickets[1].appearances.size() == 1 &&
                g_stripTransactions.contains(2) && g_pendingMutations.contains(2) &&
                g_pendingMutations.contains(3) && !g_pendingMutations.contains(1),
            "Manual show must retain other actor and non-overlapping appearance state");
    Require(bench.SetOverrideHidden(0, 0, true) && bench.rows_[0].overrides[0].hidden &&
                bench.SetOverrideHidden(0, 0, false) && !bench.rows_[0].overrides[0].hidden,
            "Repeated hide/show must remain usable after no-redress");
    for (int item = 0; item < 2; ++item) bench.SetOverrideHidden(0, item, false);
    Require(masks[0x14] == 0 && !g_stripTransactions.contains(1) &&
                masks[0x42] == 4 && RE::actors[0x14].worn.empty(),
            "Global show must release this actor's cards only and never equip actual gear");
    Require(!bench.SetOverrideHidden(0, 0, false) && !bench.SetOverrideHidden(-1, 0, false),
            "Unchanged visible cards and invalid indices must remain no-ops");

    Reset();
    // Provider, not SFS, restores actual gear. Partial restoration is not completion.
    RE::actors[0x14].worn = {{900, 4}};
    CompleteSettledTransactions(0x14, "partial-redress");
    Require(masks[0x14] == 12, "Partial actual redress must preserve pending tickets");
    RE::actors[0x14].worn.emplace(901, 8);
    CompleteSettledTransactions(0x14, "actual-redress");
    Require(masks[0x14] == 0 && masks[0x42] == 4 && refreshes == 1 &&
                RE::actors[0x14].worn.size() == 2,
            "Actual redress must retain existing automatic appearance recovery");

    Reset();
    g_actorAutomationGenerations[0x14] = 10;
    RE::actors[0x14].worn = {{900, 4}, {901, 8}};
    CompleteSettledTransactions(0x14, "stale", 9, 1);
    Require(masks[0x14] == 12, "Stale recovery callbacks must not clear current state");
    InvalidateAppearanceTickets(0x14, "100", 4, true);
    Require(masks[0x14] == 8 && g_suppressionTickets[1].appearances.size() == 1,
            "Deleting a card must also preserve another actor's same-slot ticket");

    Reset();
    masks[0x14] = 0; // exact-appearance fallback after row/mask migration
    Require(bench.SetOverrideHidden(0, 0, false), "Exact ticket fallback must permit manual show");
    Reset();
    modSettings = false; ddMasks[0x14] = 4;
    Require(bench.SetOverrideHidden(0, 0, false) && ddMasks[0x14] == 0,
            "Existing DD latch manual release must remain available");
    Reset();
    bench.rows_[0].overrides[0].locked = true;
    Require(!bench.SetOverrideHidden(0, 0, false) && masks[0x14] == 12,
            "A locked already-visible card must not release automation as a no-op");
    Reset();
    bench.rows_ = {{0x42, {{200, 4}}}};
    Require(bench.SetOverrideHidden(0, 0, false) && masks[0x42] == 0 &&
                masks[0x14] == 12 && g_suppressionTickets.front().appearances.size() == 2,
            "An NPC's manual show must not release the player's same-slot state");
    Reset();
    bench.rows_ = {{0x14, {{100, 4}, {101, 8}}}};
    for (int cycle = 0; cycle < 128; ++cycle) {
      // A fresh provider strip must hide again after the previous manual show.
      if (cycle != 0) {
        g_suppressionTickets.push_back({1, 0x14,
            {{"100", 0x14, 100, 4}, {"101", 0x14, 101, 8}}});
        g_stripTransactions.emplace(1, StripTransaction{1, 0x14, 0,
            {{900, 4}, {901, 8}}, false});
        ApplyOwnedMask(0x14);
      }
      CompleteSettledTransactions(0x14, "no-redress");
      Require(masks[0x14] == 12, "A new strip must not inherit permanent manual visibility");
      Require(bench.SetOverrideHidden(0, 0, false) &&
                  bench.SetOverrideHidden(0, 1, false) && masks[0x14] == 0 &&
                  g_suppressionTickets.size() == 1 && g_stripTransactions.size() == 1 &&
                  RE::actors[0x14].worn.empty(),
              "Repeated no-redress/manual-show cycles must clean target tickets without gear mutation");
    }
    std::cout << "Manual visibility production regressions passed (fake engine, no equip API)\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
