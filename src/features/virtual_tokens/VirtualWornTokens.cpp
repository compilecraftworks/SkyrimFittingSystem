#include "features/virtual_tokens/VirtualWornTokens.h"

#include "ArmorUtils.h"
#include "native/ArmorSkinning.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/FittingSlotState.h"
#include "native/GenitalCompatibility.h"
#include "native/PapyrusObserverInstallRules.h"
#include "native/SexLabPPlusRules.h"
#include "features/devious_devices/DeviousDevicesIntegration.h"
#include "runtime/RuntimeLayouts.h"
#include "ui/Menu.h"
#include "workbench/AppearanceSlotProtection.h"
#include "workbench/AutomaticEquipmentVisibility.h"
#include "workbench/EquipmentRefreshEventSink.h"

#include <RE/N/NativeFunctionBase.h>
#include <RE/O/ObjectTypeInfo.h>
#include <RE/S/ScriptFunction.h>
#include <nlohmann/json.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr auto kTokenPlugin = "SkyrimFittingSystem-VirtualTokens.esl"sv;
constexpr std::uint32_t kFirstSlot = 30;
constexpr std::uint32_t kLastSlot = 61;
constexpr std::size_t kTokenCount = kLastSlot - kFirstSlot + 1;
constexpr std::uint32_t kStateRecordType = 'VWTP';
constexpr std::uint32_t kStateRecordVersion = 2;
constexpr std::uint8_t kPersistentTrustThreshold = 2;
constexpr auto kObservationLifetime = std::chrono::minutes(2);
constexpr auto kPendingMutationLifetime = std::chrono::seconds(3);
constexpr auto kRecoveryProbeLifetime = std::chrono::seconds(3);
constexpr auto kPostRecoveryRefreshDelay = std::chrono::milliseconds(250);
constexpr auto kScriptTypeInspectionDelay = std::chrono::milliseconds(250);
constexpr std::uint8_t kScriptTypeInspectionMaxAttempts = 4;

using RuntimeClock = std::chrono::steady_clock;
using NativeCallResult = RE::BSScript::IFunction::CallResult;
using NativeFunctionBase = RE::BSScript::NF_util::NativeFunctionBase;

[[nodiscard]] bool IsModSettingsStripLinkActive() {
  return sfs::workbench::IsModSettingsStripLinkPolicyActive();
}

[[nodiscard]] bool IsContextWardrobeStripLinkActive() {
  return sfs::workbench::GetExternalModStripLinkMode() !=
         sfs::workbench::ExternalModStripLinkMode::Disabled;
}

std::array<RE::TESObjectARMO *, kTokenCount> g_tokens{};
std::atomic_bool g_tokensReady{false};
std::atomic_bool g_dispatchHookInstalled{false};
std::atomic_bool g_nativeRegistrationHookInstalled{false};
std::atomic_bool g_scriptTypeLoadHookInstalled{false};
std::mutex g_scriptTypeInspectionMutex;
std::unordered_map<std::string, std::uint8_t>
    g_pendingScriptTypeInspections;
bool g_scriptTypeInspectionWorkerScheduled{false};

struct RegisteredAppearance {
  std::string identity;
  RE::FormID actorID{0};
  RE::FormID armorID{0};
  // Visual slots suppressed by SFS when the linked token is stripped.
  std::uint32_t slotMask{0};
  // Slots exposed to external mod queries. Normally identical to slotMask,
  // but DirectSlots can redirect a visual appearance to another extension
  // token without changing its real display slots.
  std::uint32_t tokenSlotMask{0};
  std::uint64_t generation{0};
};

std::mutex g_cacheMutex;
std::unordered_map<RE::FormID, std::vector<RegisteredAppearance>>
    g_registeredAppearances;
std::unordered_map<RE::FormID, std::uint32_t> g_registeredMasks;
std::uint64_t g_nextAppearanceGeneration{1};

struct CallerIdentity {
  std::string key;
  std::string display;

  [[nodiscard]] bool operator==(const CallerIdentity &a_other) const {
    return key == a_other.key;
  }
};

struct TrustProfile {
  std::string display;
  std::uint8_t confirmations{0};
  RE::VMStackID lastConfirmedStack{0};
  bool sessionTrusted{false};
};

struct StackObservation {
  RE::FormID actorID{0};
  RuntimeClock::time_point lastSeen{};
  std::vector<CallerIdentity> callerChain;
  std::unordered_map<RE::FormID, std::uint32_t> returnedForms;
  std::unordered_set<std::uint64_t> completedMutations;
  // Read-only catalog queries must keep their real/null results on a
  // caller's first invocation.  Preserve only the matching appearance
  // identities here so the first confirmed real mutation in the same stack
  // can stage them retrospectively without ever exposing a token early.
  std::unordered_set<std::string> catalogAppearanceCandidates;
  std::uint32_t queriedMask{0};
  bool equippedArrayObserved{false};
  bool catalogCandidatesInitialized{false};
  bool catalogFilterObserved{false};
  bool slotMaskFilterObserved{false};
  bool retrospectiveSelectionStaged{false};
  bool tokenTrustDecisionMade{false};
  bool tokenExposureAuthorized{false};
};

struct StripTransaction {
  std::uint64_t id{0};
  RE::FormID actorID{0};
  RE::VMStackID stackID{0};
  RE::FormID originalOutfitID{0};
  std::unordered_map<RE::FormID, std::uint32_t> originalWornArmor;
  std::unordered_set<RE::FormID> eventAddedArmor;
  std::string source;
  bool originalOutfitRestoreObserved{false};
  bool virtualDisplayRefreshQueued{false};
};

struct ActorContextWardrobeSnapshot {
  std::unordered_map<RE::FormID, std::uint32_t> wornArmor;
  RE::FormID outfitID{0};
};

struct AppearanceTicketRef {
  std::string identity;
  RE::FormID actorID{0};
  RE::FormID armorID{0};
  std::uint64_t generation{0};
  std::uint32_t slotMask{0};
};

struct SuppressionTicket {
  std::uint64_t transactionID{0};
  RE::FormID actorID{0};
  RE::FormID restoreItemID{0};
  std::string source;
  std::vector<AppearanceTicketRef> appearances;
};

struct PendingMutation {
  RE::FormID actorID{0};
  RE::FormID itemID{0};
  bool equipped{false};
  std::uint32_t slotMask{0};
  std::uint64_t transactionID{0};
  std::uint64_t manualGeneration{0};
  std::string source;
  RuntimeClock::time_point expires{};
};

std::mutex g_runtimeMutex;
std::unordered_map<std::string, TrustProfile> g_trustProfiles;
std::unordered_map<RE::VMStackID, StackObservation>
    g_stackObservations;
std::vector<SuppressionTicket> g_suppressionTickets;
std::unordered_map<std::string, AppearanceTicketRef>
    g_manualAutomationOverrides;
std::unordered_map<std::uint64_t, PendingMutation> g_pendingMutations;
std::unordered_map<RE::FormID, std::uint32_t> g_appliedSuppressionMasks;
std::unordered_map<std::uint64_t, StripTransaction> g_stripTransactions;
std::unordered_map<RE::FormID, std::uint64_t> g_actorAutomationGenerations;
std::unordered_map<RE::FormID, std::uint64_t> g_actorManualGenerations;
std::unordered_map<RE::FormID, ActorContextWardrobeSnapshot>
    g_actorContextWardrobeSnapshots;
std::unordered_map<RE::FormID, std::unordered_set<RE::FormID>>
    g_actorContextWardrobeOriginalWornArmor;
std::unordered_map<RE::FormID, std::unordered_set<RE::FormID>>
    g_actorContextWardrobeInitializedActualArmor;
std::unordered_map<RE::FormID, RuntimeClock::time_point>
    g_recoveryProbeExpires;
std::unordered_map<RE::FormID, std::uint64_t>
    g_postRecoveryRefreshGenerations;
std::uint64_t g_nextTransactionID{1};
std::uint64_t g_nextPostRecoveryRefreshGeneration{1};
std::atomic_uint64_t g_runtimeEpoch{1};

std::mutex g_functionHashMutex;
std::unordered_map<const RE::BSScript::IFunction *, std::uint64_t>
    g_functionHashes;

[[nodiscard]] std::uint32_t SlotMask(const std::uint32_t a_slot) {
  return a_slot >= 30 && a_slot <= 61 ? 1U << (a_slot - 30) : 0;
}

[[nodiscard]] constexpr std::size_t
TokenIndexForSlot(const std::uint32_t a_slot) {
  return static_cast<std::size_t>(a_slot - kFirstSlot);
}

[[nodiscard]] constexpr std::uint32_t
TokenLocalFormIDForSlot(const std::uint32_t a_slot) {
  // PoC1-PoC10 used 0x800-0x812 for slots 43-61.  Preserve those
  // FormIDs so an existing co-save or a still-running external catalog never
  // changes meaning.  The newly covered vanilla slots 30-42 are appended at
  // 0x813-0x81F.
  return a_slot >= 43 ? 0x800U + a_slot - 43U
                      : 0x813U + a_slot - 30U;
}

[[nodiscard]] RE::FormID ActorID(const RE::Actor *a_actor) {
  return a_actor ? a_actor->GetFormID() : 0;
}

[[nodiscard]] RE::FormID ResolveOwnerActorID(RE::FormID a_actorID) {
  if (a_actorID != 0) {
    return a_actorID;
  }
  const auto *player = RE::PlayerCharacter::GetSingleton();
  return player ? player->GetFormID() : 0;
}

[[nodiscard]] std::string BuildAppearanceIdentity(
    const RE::FormID a_actorID, const RE::FormID a_armorID,
    const std::uint32_t a_slotMask) {
  // A workbench row follows actual equipment: armor:<form> while worn and
  // slot:<number> after it is stripped.  Neither row key is the identity of
  // the registered appearance.  Keep automation ownership stable across that
  // normal row migration by keying it to actor + appearance ARMO + visual
  // slots instead.
  return std::format("{:08X}|appearance:{:08X}|mask:{:08X}", a_actorID,
                     a_armorID, a_slotMask);
}

[[nodiscard]] std::string RebaseAppearanceIdentity(
    const std::string_view a_savedIdentity,
    const RE::FormID a_resolvedActorID) {
  const auto separator = a_savedIdentity.find('|');
  return separator == std::string_view::npos
             ? std::string(a_savedIdentity)
             : std::format("{:08X}{}", a_resolvedActorID,
                           a_savedIdentity.substr(separator));
}

[[nodiscard]] RE::TESObjectARMO *FindRealWornArmor(
    RE::Actor *a_actor, const std::uint32_t a_mask) {
  if (!a_actor || a_mask == 0) {
    return nullptr;
  }
  for (std::uint32_t slot = 30; slot <= 61; ++slot) {
    const auto mask = SlotMask(slot);
    if ((a_mask & mask) == 0) {
      continue;
    }
    if (auto *armor = a_actor->GetWornArmor(
            static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(mask))) {
      return armor;
    }
  }
  return nullptr;
}

[[nodiscard]] bool IsArmorWornByActor(RE::Actor *a_actor,
                                       RE::TESObjectARMO *a_armor) {
  if (!a_actor || !a_armor) {
    return false;
  }
  const auto mask = static_cast<std::uint32_t>(
      sfs::armor::GetArmorDisplaySlotMask(a_armor));
  return FindRealWornArmor(a_actor, mask) == a_armor;
}

[[nodiscard]] RE::FormID CurrentDefaultOutfitID(const RE::Actor *a_actor) {
  const auto *base = a_actor ? a_actor->GetActorBase() : nullptr;
  return base && base->defaultOutfit ? base->defaultOutfit->GetFormID() : 0;
}

[[nodiscard]] std::unordered_map<RE::FormID, std::uint32_t>
CaptureWornArmorSnapshot(RE::Actor *a_actor) {
  std::unordered_map<RE::FormID, std::uint32_t> snapshot;
  if (!a_actor) {
    return snapshot;
  }
  for (std::uint32_t slot = 30; slot <= 61; ++slot) {
    const auto queryMask = SlotMask(slot);
    auto *armor = a_actor->GetWornArmor(
        static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(queryMask));
    if (!armor) {
      continue;
    }
    if (sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(armor)) {
      continue;
    }
    const auto displayMask = static_cast<std::uint32_t>(
        sfs::armor::GetArmorDisplaySlotMask(armor));
    snapshot[armor->GetFormID()] |= displayMask != 0 ? displayMask : queryMask;
  }
  return snapshot;
}

[[nodiscard]] sfs::native::external_equipment::ContextWardrobeSnapshot
ToActualContextWardrobeSnapshot(
    const std::unordered_map<RE::FormID, std::uint32_t> &a_snapshot) {
  sfs::native::external_equipment::ContextWardrobeSnapshot result;
  result.reserve(a_snapshot.size());
  for (const auto &[armorFormID, slotMask] : a_snapshot) {
    result.emplace(armorFormID, slotMask);
  }
  return result;
}

[[nodiscard]] std::uint64_t FindStripTransactionLocked(
    const RE::FormID a_actorID, const RE::VMStackID a_stackID) {
  if (a_actorID == 0 || a_stackID == 0) {
    return 0;
  }
  for (const auto &[transactionID, transaction] : g_stripTransactions) {
    if (transaction.actorID == a_actorID &&
        transaction.stackID == a_stackID) {
      return transactionID;
    }
  }
  return 0;
}

[[nodiscard]] bool HasActorStripTransactionLocked(
    const RE::FormID a_actorID) {
  return std::ranges::any_of(
      g_stripTransactions, [&](const auto &a_entry) {
        return a_entry.second.actorID == a_actorID;
      });
}

[[nodiscard]] std::uint64_t GetStripTransaction(
    const RE::FormID a_actorID, const RE::VMStackID a_stackID) {
  std::lock_guard lock(g_runtimeMutex);
  return FindStripTransactionLocked(a_actorID, a_stackID);
}

[[nodiscard]] std::uint64_t GetActorStripTransaction(
    const RE::FormID a_actorID) {
  std::lock_guard lock(g_runtimeMutex);
  for (const auto &[transactionID, transaction] : g_stripTransactions) {
    if (transaction.actorID == a_actorID) {
      return transactionID;
    }
  }
  return 0;
}

[[nodiscard]] std::uint64_t CaptureManualGeneration(
    const RE::FormID a_actorID) {
  if (a_actorID == 0) {
    return 0;
  }
  std::lock_guard lock(g_runtimeMutex);
  return g_actorManualGenerations[a_actorID] + 1;
}

[[nodiscard]] std::uint64_t EnsureStripTransaction(
    RE::Actor *a_actor, const RE::VMStackID a_stackID,
    const std::string_view a_source) {
  const auto actorID = ActorID(a_actor);
  if (actorID == 0) {
    return 0;
  }
  {
    std::lock_guard lock(g_runtimeMutex);
    if (const auto existing =
            FindStripTransactionLocked(actorID, a_stackID)) {
      return existing;
    }
  }

  // This is deliberately captured once, before the first real mutation in a
  // causal stack.  Nested RemoveAllItems calls therefore cannot replace the
  // original outfit/worn set with an already-stripped intermediate state.
  auto originalWornArmor = CaptureWornArmorSnapshot(a_actor);
  const auto originalOutfitID = CurrentDefaultOutfitID(a_actor);

  std::lock_guard lock(g_runtimeMutex);
  if (const auto existing = FindStripTransactionLocked(actorID, a_stackID)) {
    return existing;
  }
  // A new strip cycle supersedes any delayed visual rebuild from the
  // preceding cycle. Its own suppression refresh and eventual recovery
  // refresh will establish the correct actor state.
  g_postRecoveryRefreshGenerations.erase(actorID);
  const auto transactionID = g_nextTransactionID++;
  g_stripTransactions.emplace(
      transactionID,
      StripTransaction{.id = transactionID,
                       .actorID = actorID,
                       .stackID = a_stackID,
                       .originalOutfitID = originalOutfitID,
                       .originalWornArmor = std::move(originalWornArmor),
                       .source = std::string(a_source)});
  logger::debug("Opened strip transaction={} actor={:08X} stack={} "
                "worn={} outfit={:08X} source={}",
                transactionID, actorID, a_stackID,
                g_stripTransactions.at(transactionID).originalWornArmor.size(),
                originalOutfitID, a_source);
  return transactionID;
}

// A virtual appearance mutation may be the final operation in a strip, after
// the last real equipment change has already rebuilt the actor. Permit at
// most one explicit refresh for all virtual mutations in the transaction so
// that final state is rendered without restoring the old per-item refresh
// burst that can interrupt an external animation/control hand-off.
[[nodiscard]] bool ShouldQueueVirtualStripRefresh(
    const std::uint64_t a_transactionID) {
  if (a_transactionID == 0) {
    return true;
  }
  std::lock_guard lock(g_runtimeMutex);
  const auto it = g_stripTransactions.find(a_transactionID);
  if (it == g_stripTransactions.end()) {
    return true;
  }
  auto &transaction = it->second;
  if (transaction.virtualDisplayRefreshQueued) {
    return false;
  }
  transaction.virtualDisplayRefreshQueued = true;
  return true;
}

[[nodiscard]] std::uint32_t GetRegisteredMask(const RE::FormID a_actorID) {
  std::lock_guard lock(g_cacheMutex);
  const auto it = g_registeredMasks.find(a_actorID);
  return it != g_registeredMasks.end() ? it->second : 0;
}

[[nodiscard]] std::vector<RegisteredAppearance>
GetRegisteredAppearances(const RE::FormID a_actorID,
                         const std::uint32_t a_mask = 0xFFFFFFFFU) {
  std::lock_guard lock(g_cacheMutex);
  const auto it = g_registeredAppearances.find(a_actorID);
  if (it == g_registeredAppearances.end()) {
    return {};
  }
  std::vector<RegisteredAppearance> result;
  result.reserve(it->second.size());
  for (const auto &appearance : it->second) {
    if ((appearance.tokenSlotMask & a_mask) != 0) {
      result.push_back(appearance);
    }
  }
  return result;
}

[[nodiscard]] std::vector<RegisteredAppearance>
GetRegisteredAppearancesForDisplayMask(
    const RE::FormID a_actorID,
    const std::uint32_t a_mask = 0xFFFFFFFFU) {
  std::lock_guard lock(g_cacheMutex);
  const auto it = g_registeredAppearances.find(a_actorID);
  if (it == g_registeredAppearances.end()) {
    return {};
  }
  std::vector<RegisteredAppearance> result;
  result.reserve(it->second.size());
  for (const auto &appearance : it->second) {
    if ((appearance.slotMask & a_mask) != 0) {
      result.push_back(appearance);
    }
  }
  return result;
}

[[nodiscard]] std::optional<RegisteredAppearance>
FindRegisteredAppearance(const RE::FormID a_actorID,
                         const std::string_view a_identity) {
  std::lock_guard lock(g_cacheMutex);
  const auto actorIt = g_registeredAppearances.find(a_actorID);
  if (actorIt == g_registeredAppearances.end()) {
    return std::nullopt;
  }
  const auto matching = std::ranges::find(actorIt->second, a_identity,
                                          &RegisteredAppearance::identity);
  return matching != actorIt->second.end()
             ? std::optional<RegisteredAppearance>{*matching}
             : std::nullopt;
}

[[nodiscard]] RE::TESObjectARMO *TokenForMask(const std::uint32_t a_mask,
                                               std::uint32_t &a_tokenMask) {
  if (!g_tokensReady.load(std::memory_order_acquire)) {
    return nullptr;
  }
  for (std::uint32_t slot = kFirstSlot; slot <= kLastSlot; ++slot) {
    const auto mask = SlotMask(slot);
    if ((a_mask & mask) != 0) {
      a_tokenMask = mask;
      return g_tokens[TokenIndexForSlot(slot)];
    }
  }
  return nullptr;
}

[[nodiscard]] std::uint32_t MaskForToken(const RE::TESForm *a_form) {
  if (!a_form || !g_tokensReady.load(std::memory_order_acquire)) {
    return 0;
  }
  if (const auto *reference = a_form->As<RE::TESObjectREFR>()) {
    a_form = reference->GetBaseObject();
  }
  for (std::uint32_t slot = kFirstSlot; slot <= kLastSlot; ++slot) {
    if (g_tokens[TokenIndexForSlot(slot)] == a_form) {
      return SlotMask(slot);
    }
  }
  return 0;
}

[[nodiscard]] std::uint64_t HashBytes(std::uint64_t a_hash, const void *a_data,
                                      const std::size_t a_size) {
  constexpr std::uint64_t prime = 1099511628211ULL;
  const auto *bytes = static_cast<const std::byte *>(a_data);
  for (std::size_t index = 0; index < a_size; ++index) {
    a_hash ^= static_cast<std::uint8_t>(bytes[index]);
    a_hash *= prime;
  }
  return a_hash;
}

[[nodiscard]] std::uint64_t
FunctionCodeHash(const RE::BSScript::IFunction *a_function) {
  if (!a_function) {
    return 0;
  }
  {
    std::lock_guard lock(g_functionHashMutex);
    if (const auto it = g_functionHashes.find(a_function);
        it != g_functionHashes.end()) {
      return it->second;
    }
  }

  constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
  auto hash = offsetBasis;
  const auto appendText = [&](const RE::BSFixedString &a_text) {
    const auto *text = a_text.c_str();
    if (text) {
      hash = HashBytes(hash, text, std::strlen(text));
    }
  };
  appendText(a_function->GetSourceFilename());
  appendText(a_function->GetObjectTypeName());
  appendText(a_function->GetStateName());
  appendText(a_function->GetName());

  if (const auto *scriptFunction =
          skyrim_cast<const RE::BSScript::Internal::ScriptFunction *>(
              a_function)) {
    const auto bitCount = scriptFunction->instructions.numInstructionBits;
    const auto byteCount = static_cast<std::size_t>((bitCount + 7U) / 8U);
    constexpr std::size_t maxInstructionBytes = 16U * 1024U * 1024U;
    if (scriptFunction->instructions.instructions &&
        byteCount <= maxInstructionBytes) {
      hash = HashBytes(hash, scriptFunction->instructions.instructions,
                       byteCount);
    }
  }

  std::lock_guard lock(g_functionHashMutex);
  g_functionHashes.emplace(a_function, hash);
  return hash;
}

[[nodiscard]] std::vector<CallerIdentity>
BuildCallerChain(const RE::BSScript::Stack *a_stack) {
  std::vector<CallerIdentity> chain;
  if (!a_stack || !a_stack->top) {
    return chain;
  }
  for (auto *frame = a_stack->top->previousFrame;
       frame && chain.size() < 24; frame = frame->previousFrame) {
    const auto *function = frame->owningFunction.get();
    if (!function || function->GetIsNative()) {
      continue;
    }
    const auto codeHash = FunctionCodeHash(function);
    const auto source = function->GetSourceFilename().c_str();
    const auto object = function->GetObjectTypeName().c_str();
    const auto state = function->GetStateName().c_str();
    const auto name = function->GetName().c_str();
    CallerIdentity identity{
        .key = std::format("{}|{}|{}|{}|{:016X}", source ? source : "",
                           object ? object : "", state ? state : "",
                           name ? name : "", codeHash),
        .display = std::format("{}:{}{}.{}", source ? source : "<unknown>",
                               object ? object : "<script>",
                               state && *state ? std::format("[{}]", state)
                                               : std::string{},
                               name ? name : "<function>")};
    if (std::ranges::find(chain, identity) == chain.end()) {
      chain.push_back(std::move(identity));
    }
  }
  return chain;
}

void PruneRuntimeStateLocked(const RuntimeClock::time_point a_now) {
  std::erase_if(g_stackObservations, [&](const auto &entry) {
    return a_now - entry.second.lastSeen > kObservationLifetime;
  });
  std::erase_if(g_pendingMutations, [&](const auto &entry) {
    return a_now > entry.second.expires;
  });
  std::erase_if(g_recoveryProbeExpires, [&](const auto &entry) {
    return a_now > entry.second;
  });
}

void OpenRecoveryProbe(const RE::FormID a_actorID) {
  if (a_actorID == 0) {
    return;
  }
  std::lock_guard lock(g_runtimeMutex);
  g_recoveryProbeExpires[a_actorID] =
      RuntimeClock::now() + kRecoveryProbeLifetime;
}

[[nodiscard]] bool IsRecoveryProbeOpen(const RE::FormID a_actorID) {
  if (a_actorID == 0) {
    return false;
  }
  std::lock_guard lock(g_runtimeMutex);
  const auto now = RuntimeClock::now();
  PruneRuntimeStateLocked(now);
  return g_recoveryProbeExpires.contains(a_actorID);
}

[[nodiscard]] bool
IsCallerTrusted(const std::vector<CallerIdentity> &a_chain) {
  std::lock_guard lock(g_runtimeMutex);
  for (const auto &identity : a_chain) {
    const auto it = g_trustProfiles.find(identity.key);
    if (it != g_trustProfiles.end() &&
        (it->second.sessionTrusted ||
         it->second.confirmations >= kPersistentTrustThreshold)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool CallerChainsOverlap(
    const std::vector<CallerIdentity> &a_left,
    const std::vector<CallerIdentity> &a_right) {
  return std::ranges::any_of(a_left, [&](const CallerIdentity &a_identity) {
    return std::ranges::find(a_right, a_identity) != a_right.end();
  });
}

// Freeze token exposure at this stack's first catalog query. A caller may
// prove strip intent later in the same stack, but changing a real/null catalog
// into a virtual catalog halfway through produces a partial transaction and
// can disturb the external mod's animation/control hand-off.
[[nodiscard]] bool IsCallerTrustedForTokenExposure(
    const std::vector<CallerIdentity> &a_chain,
    const RE::VMStackID a_stackID) {
  std::lock_guard lock(g_runtimeMutex);
  const auto now = RuntimeClock::now();
  PruneRuntimeStateLocked(now);

  StackObservation *observation = nullptr;
  if (a_stackID != 0) {
    auto &candidate = g_stackObservations[a_stackID];
    if (!candidate.callerChain.empty() &&
        !CallerChainsOverlap(candidate.callerChain, a_chain)) {
      candidate = {};
    }
    if (candidate.callerChain.empty()) {
      candidate.callerChain = a_chain;
    }
    candidate.lastSeen = now;
    if (candidate.tokenTrustDecisionMade) {
      return candidate.tokenExposureAuthorized;
    }
    observation = &candidate;
  }

  bool trustedBeforeStack = false;
  for (const auto &identity : a_chain) {
    const auto it = g_trustProfiles.find(identity.key);
    if (it == g_trustProfiles.end()) {
      continue;
    }
    const auto &profile = it->second;
    if ((profile.sessionTrusted ||
         profile.confirmations >= kPersistentTrustThreshold) &&
        (a_stackID == 0 || profile.lastConfirmedStack != a_stackID)) {
      trustedBeforeStack = true;
      break;
    }
  }

  if (observation) {
    observation->tokenTrustDecisionMade = true;
    observation->tokenExposureAuthorized = trustedBeforeStack;
  }
  return trustedBeforeStack;
}

[[nodiscard]] std::optional<RegisteredAppearance>
ResolveContextualTokenAppearance(
    const RE::VMStackID a_stackID, const RE::TESForm *a_form,
    const std::vector<CallerIdentity> &a_callerChain) {
  const auto tokenMask = MaskForToken(a_form);
  if (a_stackID == 0 || tokenMask == 0 || a_callerChain.empty()) {
    return std::nullopt;
  }

  RE::FormID actorID = 0;
  {
    std::lock_guard lock(g_runtimeMutex);
    const auto observation = g_stackObservations.find(a_stackID);
    if (observation == g_stackObservations.end() ||
        observation->second.actorID == 0 ||
        !observation->second.returnedForms.contains(a_form->GetFormID()) ||
        !CallerChainsOverlap(observation->second.callerChain,
                             a_callerChain)) {
      return std::nullopt;
    }
    actorID = observation->second.actorID;
  }

  const auto appearances = GetRegisteredAppearances(actorID, tokenMask);
  if (appearances.empty()) {
    return std::nullopt;
  }
  // The native fitting resolver guarantees one effective visual source for
  // every occupied display slot.  A multi-slot armor can therefore be found
  // from any one of its token slots without combining keyword sets.
  return appearances.front();
}

[[nodiscard]] RE::TESObjectARMO *GetContextualTokenSourceArmor(
    const RE::VMStackID a_stackID, const RE::TESForm *a_form,
    const std::vector<CallerIdentity> &a_callerChain) {
  const auto appearance = ResolveContextualTokenAppearance(
      a_stackID, a_form, a_callerChain);
  return appearance
             ? RE::TESForm::LookupByID<RE::TESObjectARMO>(
                   appearance->armorID)
             : nullptr;
}

void RecordWornQuery(const RE::VMStackID a_stackID,
                     const RE::FormID a_actorID,
                     const std::vector<CallerIdentity> &a_chain,
                     const std::uint32_t a_requestedMask,
                     const RE::FormID a_returnedFormID) {
  if (a_stackID == 0 || a_actorID == 0 || a_chain.empty()) {
    return;
  }
  const auto now = RuntimeClock::now();
  std::lock_guard lock(g_runtimeMutex);
  PruneRuntimeStateLocked(now);
  auto &observation = g_stackObservations[a_stackID];
  if ((observation.actorID != 0 && observation.actorID != a_actorID) ||
      (!observation.callerChain.empty() &&
       !CallerChainsOverlap(observation.callerChain, a_chain))) {
    observation = {};
  }
  observation.actorID = a_actorID;
  observation.lastSeen = now;
  observation.queriedMask |= a_requestedMask;
  if (observation.callerChain.empty()) {
    observation.callerChain = a_chain;
  }
  if (a_returnedFormID != 0) {
    observation.returnedForms[a_returnedFormID] |= a_requestedMask;
  }
}

void RecordEquippedArrayCatalog(
    const RE::VMStackID a_stackID, const RE::FormID a_actorID,
    const std::vector<CallerIdentity> &a_chain) {
  if (a_stackID == 0 || a_actorID == 0 || a_chain.empty()) {
    return;
  }
  const auto appearances = GetRegisteredAppearances(a_actorID);
  const auto now = RuntimeClock::now();
  std::lock_guard lock(g_runtimeMutex);
  PruneRuntimeStateLocked(now);
  auto &observation = g_stackObservations[a_stackID];
  if ((observation.actorID != 0 && observation.actorID != a_actorID) ||
      (!observation.callerChain.empty() &&
       !CallerChainsOverlap(observation.callerChain, a_chain))) {
    observation = {};
  }
  observation.actorID = a_actorID;
  observation.lastSeen = now;
  observation.equippedArrayObserved = true;
  if (observation.callerChain.empty()) {
    observation.callerChain = a_chain;
  }
  if (!observation.catalogCandidatesInitialized) {
    for (const auto &appearance : appearances) {
      observation.catalogAppearanceCandidates.insert(appearance.identity);
    }
    observation.catalogCandidatesInitialized = true;
  }
}

[[nodiscard]] std::optional<CallerIdentity> FindCommonCaller(
    const std::vector<CallerIdentity> &a_operationChain,
    const std::vector<CallerIdentity> &a_queryChain) {
  for (const auto &operationIdentity : a_operationChain) {
    if (std::ranges::find(a_queryChain, operationIdentity) !=
        a_queryChain.end()) {
      return operationIdentity;
    }
  }
  return std::nullopt;
}

void ConfirmStripCaller(const RE::VMStackID a_stackID,
                        const RE::FormID a_actorID,
                        const std::vector<CallerIdentity> &a_chain,
                        const RE::FormID a_itemID,
                        const bool a_strongWholeActorIntent,
                        const std::string_view a_source) {
  if (a_stackID == 0 || a_actorID == 0 || a_chain.empty()) {
    return;
  }

  const auto now = RuntimeClock::now();
  std::optional<CallerIdentity> confirmedIdentity;
  std::uint8_t confirmations = 0;
  bool becamePersistent = false;
  {
    std::lock_guard lock(g_runtimeMutex);
    PruneRuntimeStateLocked(now);
    const auto observationIt = g_stackObservations.find(a_stackID);
    if (observationIt != g_stackObservations.end() &&
        observationIt->second.actorID == a_actorID) {
      const bool exactReturnedItem =
          a_itemID != 0 &&
          observationIt->second.returnedForms.contains(a_itemID);
      if (exactReturnedItem || a_strongWholeActorIntent) {
        confirmedIdentity =
            FindCommonCaller(a_chain, observationIt->second.callerChain);
      }
    }
    if (!confirmedIdentity && a_strongWholeActorIntent) {
      confirmedIdentity = a_chain.front();
    }
    if (!confirmedIdentity) {
      return;
    }

    auto &profile = g_trustProfiles[confirmedIdentity->key];
    profile.display = confirmedIdentity->display;
    const bool wasPersistent =
        profile.confirmations >= kPersistentTrustThreshold;
    if (profile.lastConfirmedStack != a_stackID) {
      profile.lastConfirmedStack = a_stackID;
      profile.confirmations = static_cast<std::uint8_t>((std::min)(
          static_cast<unsigned>(kPersistentTrustThreshold),
          static_cast<unsigned>(profile.confirmations) + 1U));
    }
    profile.sessionTrusted = true;
    confirmations = profile.confirmations;
    becamePersistent = !wasPersistent &&
                       confirmations >= kPersistentTrustThreshold;
  }

  logger::info("Virtual token caller trust {} caller='{}' confirmations={} "
               "stack={} actor={:08X} source={}",
               becamePersistent ? "persisted" : "confirmed",
               confirmedIdentity->display, confirmations, a_stackID, a_actorID,
               a_source);
}

[[nodiscard]] bool RecordCompletedStripMutation(
    const RE::VMStackID a_stackID, const RE::FormID a_actorID,
    const std::vector<CallerIdentity> &a_chain, const RE::FormID a_itemID,
    const std::uint32_t a_slotMask, const std::string_view a_source) {
  if (a_stackID == 0 || a_actorID == 0 || a_chain.empty() ||
      (a_itemID == 0 && a_slotMask == 0)) {
    return false;
  }
  bool repeatedIntent = false;
  {
    std::lock_guard lock(g_runtimeMutex);
    const auto now = RuntimeClock::now();
    PruneRuntimeStateLocked(now);
    auto &observation = g_stackObservations[a_stackID];
    if ((observation.actorID != 0 && observation.actorID != a_actorID) ||
        (!observation.callerChain.empty() &&
         !CallerChainsOverlap(observation.callerChain, a_chain))) {
      observation = {};
    }
    observation.actorID = a_actorID;
    observation.lastSeen = now;
    if (observation.callerChain.empty()) {
      observation.callerChain = a_chain;
    }
    const auto operationHash =
        static_cast<std::uint32_t>(std::hash<std::string_view>{}(a_source));
    const auto mutationKey =
        (static_cast<std::uint64_t>(a_itemID != 0 ? a_itemID : a_slotMask)
         << 32U) |
        operationHash;
    observation.completedMutations.insert(mutationKey);
    repeatedIntent = observation.completedMutations.size() >= 2;
  }
  if (repeatedIntent) {
    ConfirmStripCaller(a_stackID, a_actorID, a_chain, 0, true, a_source);
  }
  return IsCallerTrusted(a_chain);
}

[[nodiscard]] bool HasCompletedMutationForItem(
    const RE::VMStackID a_stackID, const RE::FormID a_actorID,
    const RE::FormID a_itemID) {
  if (a_stackID == 0 || a_actorID == 0 || a_itemID == 0) {
    return false;
  }
  std::lock_guard lock(g_runtimeMutex);
  const auto observation = g_stackObservations.find(a_stackID);
  if (observation == g_stackObservations.end() ||
      observation->second.actorID != a_actorID) {
    return false;
  }
  return std::ranges::any_of(
      observation->second.completedMutations,
      [&](const std::uint64_t a_key) {
        return static_cast<RE::FormID>(a_key >> 32U) == a_itemID;
      });
}

[[nodiscard]] std::uint32_t
ComputeOwnedMaskLocked(const RE::FormID a_actorID) {
  std::uint32_t mask = 0;
  for (const auto &ticket : g_suppressionTickets) {
    if (ticket.actorID != a_actorID) {
      continue;
    }
    for (const auto &appearance : ticket.appearances) {
      mask |= appearance.slotMask;
    }
  }
  // Include a short-lived pending Unequip before the Papyrus native runs.
  // The engine's own equipment rebuild can then consume the final SFS display
  // state atomically. Success replaces this with a ticket; failure removes it.
  for (const auto &[key, mutation] : g_pendingMutations) {
    static_cast<void>(key);
    if (mutation.actorID == a_actorID && !mutation.equipped) {
      mask |= mutation.slotMask;
    }
  }
  return mask;
}

void QueueDisplayRefresh(const RE::FormID a_actorID) {
  sfs::workbench::EquipmentRefreshEventSink::GetSingleton()->QueueActorRefresh(
      a_actorID, false);
}

void QueuePostRecoveryDisplayRefresh(const RE::FormID a_actorID) {
  if (a_actorID == 0) {
    return;
  }
  std::uint64_t generation = 0;
  {
    std::lock_guard lock(g_runtimeMutex);
    generation = g_nextPostRecoveryRefreshGeneration++;
    g_postRecoveryRefreshGenerations[a_actorID] = generation;
  }
  const auto epoch = g_runtimeEpoch.load(std::memory_order_acquire);
  logger::debug("Queued post-recovery display barrier actor={:08X} "
                "generation={} delay={}ms",
                a_actorID, generation,
                kPostRecoveryRefreshDelay.count());

  std::thread([a_actorID, generation, epoch]() {
    std::this_thread::sleep_for(kPostRecoveryRefreshDelay);
    auto *taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
      std::lock_guard lock(g_runtimeMutex);
      const auto pending =
          g_postRecoveryRefreshGenerations.find(a_actorID);
      if (pending != g_postRecoveryRefreshGenerations.end() &&
          pending->second == generation) {
        g_postRecoveryRefreshGenerations.erase(pending);
      }
      return;
    }
    taskInterface->AddTask([a_actorID, generation, epoch]() {
      {
        std::lock_guard lock(g_runtimeMutex);
        const auto pending =
            g_postRecoveryRefreshGenerations.find(a_actorID);
        if (g_runtimeEpoch.load(std::memory_order_acquire) != epoch ||
            pending == g_postRecoveryRefreshGenerations.end() ||
            pending->second != generation) {
          return;
        }
        if (HasActorStripTransactionLocked(a_actorID)) {
          g_postRecoveryRefreshGenerations.erase(pending);
          return;
        }
        g_postRecoveryRefreshGenerations.erase(pending);
      }
      QueueDisplayRefresh(a_actorID);
      logger::debug("Released post-recovery display barrier actor={:08X} "
                    "generation={}",
                    a_actorID, generation);
    });
  }).detach();
}

void ApplyOwnedMask(const RE::FormID a_actorID,
                    const bool a_queueDisplayRefresh = true) {
  if (a_actorID == 0) {
    return;
  }
  std::uint32_t previousMask = 0;
  std::uint32_t nextMask = 0;
  {
    std::lock_guard lock(g_runtimeMutex);
    previousMask = g_appliedSuppressionMasks[a_actorID];
    nextMask = ComputeOwnedMaskLocked(a_actorID);
    if (nextMask == 0) {
      g_appliedSuppressionMasks.erase(a_actorID);
    } else {
      g_appliedSuppressionMasks[a_actorID] = nextMask;
    }
  }
  if (previousMask == nextMask) {
    return;
  }
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorID);
  if (!actor) {
    return;
  }
  const auto clearMask = previousMask & ~nextMask;
  const auto setMask = nextMask & ~previousMask;
  if (clearMask != 0) {
    sfs::native::SetVirtualTokenFittingSlotsSuppressed(actor, clearMask, false);
  }
  if (setMask != 0) {
    sfs::native::SetVirtualTokenFittingSlotsSuppressed(actor, setMask, true);
  }
  logger::info("Virtual token suppression mask actor={:08X} previous={:08X} "
               "next={:08X} clear={:08X} set={:08X}",
               a_actorID, previousMask, nextMask, clearMask, setMask);
  if (a_queueDisplayRefresh) {
    QueueDisplayRefresh(a_actorID);
  }
}

[[nodiscard]] std::uint64_t AddTicket(
    const RE::FormID a_actorID, const RE::FormID a_restoreItemID,
    const std::span<const RegisteredAppearance> a_appearances,
    const std::string_view a_source, const std::uint64_t a_transactionID = 0,
    const std::uint64_t a_expectedManualGeneration = 0,
    const bool a_queueDisplayRefresh = true) {
  if (a_actorID == 0 || a_appearances.empty()) {
    return 0;
  }
  std::uint64_t transactionID = a_transactionID;
  std::size_t addedCount = 0;
  std::uint32_t addedMask = 0;
  {
    std::lock_guard lock(g_runtimeMutex);
    if (a_expectedManualGeneration != 0 &&
        g_actorManualGenerations[a_actorID] + 1 !=
            a_expectedManualGeneration) {
      return 0;
    }
    if (transactionID == 0) {
      transactionID = g_nextTransactionID++;
    }
    auto ticketIt = std::ranges::find_if(
        g_suppressionTickets, [&](const SuppressionTicket &a_ticket) {
          return a_ticket.actorID == a_actorID &&
                 a_ticket.restoreItemID == a_restoreItemID &&
                 a_ticket.transactionID == transactionID;
        });
    if (ticketIt == g_suppressionTickets.end()) {
      g_suppressionTickets.push_back(
          {.transactionID = transactionID,
           .actorID = a_actorID,
           .restoreItemID = a_restoreItemID,
           .source = std::string(a_source)});
      ticketIt = std::prev(g_suppressionTickets.end());
    }
    for (const auto &appearance : a_appearances) {
      const auto duplicate = std::ranges::find_if(
          ticketIt->appearances,
          [&](const AppearanceTicketRef &a_existing) {
            return a_existing.identity == appearance.identity &&
                   a_existing.generation == appearance.generation;
          });
      if (duplicate == ticketIt->appearances.end()) {
        ticketIt->appearances.push_back(
            {.identity = appearance.identity,
             .actorID = appearance.actorID,
             .armorID = appearance.armorID,
             .generation = appearance.generation,
             .slotMask = appearance.slotMask});
        ++addedCount;
        addedMask |= appearance.slotMask;
      }
    }
  }
  ApplyOwnedMask(a_actorID, a_queueDisplayRefresh);
  if (addedCount != 0) {
    logger::info("Virtual token suppression ticket actor={:08X} "
                 "transaction={} restoreItem={:08X} appearances={} "
                 "mask={:08X} source={}",
                 a_actorID, transactionID, a_restoreItemID, addedCount,
                 addedMask, a_source);
  }
  return transactionID;
}

[[nodiscard]] std::uint64_t
BeginTicketForMask(RE::Actor *a_actor, RE::TESForm *a_restoreItem,
                   const std::uint32_t a_slotMask,
                   const std::string_view a_source,
                   const std::uint64_t a_transactionID = 0,
                   const std::uint64_t a_expectedManualGeneration = 0,
                   const bool a_queueDisplayRefresh = true) {
  const auto actorID = ActorID(a_actor);
  const auto appearances = GetRegisteredAppearances(actorID, a_slotMask);
  const auto itemID = a_restoreItem ? a_restoreItem->GetFormID() : 0;
  return AddTicket(actorID, itemID, appearances, a_source, a_transactionID,
                   a_expectedManualGeneration, a_queueDisplayRefresh);
}

void RecordActorContextWardrobeSnapshot(RE::Actor *a_actor) {
  if (!a_actor || !IsContextWardrobeStripLinkActive()) {
    return;
  }
  const auto actorID = a_actor->GetFormID();
  if (actorID == 0) {
    return;
  }
  ActorContextWardrobeSnapshot snapshot{
      .wornArmor = CaptureWornArmorSnapshot(a_actor),
      .outfitID = CurrentDefaultOutfitID(a_actor)};
  std::unordered_set<RE::FormID> originalWornArmor;
  std::unordered_set<RE::FormID> actualArmorToInitialize;
  const auto newlyContextAddedActualArmor =
      sfs::native::external_equipment::ReconcileContextWardrobeEquipment(
          actorID, ToActualContextWardrobeSnapshot(snapshot.wornArmor));
  {
    std::lock_guard lock(g_runtimeMutex);
    if (const auto original =
            g_actorContextWardrobeOriginalWornArmor.find(actorID);
        original != g_actorContextWardrobeOriginalWornArmor.end()) {
      originalWornArmor = original->second;
      auto &initializedActualArmor =
          g_actorContextWardrobeInitializedActualArmor[actorID];
      for (const auto &[armorID, slotMask] : snapshot.wornArmor) {
        static_cast<void>(slotMask);
        if (!originalWornArmor.contains(armorID) &&
            initializedActualArmor.insert(armorID).second) {
          actualArmorToInitialize.insert(armorID);
        }
      }
    }
    if (!newlyContextAddedActualArmor.empty()) {
      auto &initializedActualArmor =
          g_actorContextWardrobeInitializedActualArmor[actorID];
      for (const auto armorID : newlyContextAddedActualArmor) {
        if (initializedActualArmor.insert(armorID).second) {
          actualArmorToInitialize.insert(armorID);
        }
      }
    }
    g_actorContextWardrobeSnapshots[actorID] = snapshot;
  }

  if (!actualArmorToInitialize.empty()) {
    auto *menu = sfs::Menu::GetSingleton();
    if (menu != nullptr) {
      auto workbenchStateLock = menu->GetWorkbench().AcquireStateLock();
      auto &workbench = menu->GetWorkbench();
      const auto &rows = workbench.GetRows();
      for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const auto &row = rows[rowIndex];
        if (!row.IsOwnedByActor(a_actor) || !row.isEquipped ||
            row.IsSlotRow() || row.equipped.formID == 0 ||
            !actualArmorToInitialize.contains(row.equipped.formID)) {
          continue;
        }
        static_cast<void>(workbench.SetEquippedHiddenForActor(
            actorID, static_cast<int>(rowIndex), false));
      }
    }
  }
}

void ObserveActorContextWardrobeBoundary(RE::Actor *a_actor) {
  if (!a_actor || !IsContextWardrobeStripLinkActive()) {
    return;
  }
  const auto actorID = a_actor->GetFormID();
  if (actorID == 0 || GetRegisteredAppearances(actorID).empty()) {
    return;
  }

  const ActorContextWardrobeSnapshot current{
      .wornArmor = CaptureWornArmorSnapshot(a_actor),
      .outfitID = CurrentDefaultOutfitID(a_actor)};
  std::optional<ActorContextWardrobeSnapshot> previous;
  {
    std::lock_guard lock(g_runtimeMutex);
    const auto it = g_actorContextWardrobeSnapshots.find(actorID);
    if (it != g_actorContextWardrobeSnapshots.end()) {
      previous = it->second;
    }
    g_actorContextWardrobeSnapshots[actorID] = current;
  }
  if (!previous || previous->wornArmor.empty()) {
    return;
  }

  bool hasContextWardrobeTicket = false;
  bool hasContextWardrobeSnapshot = false;
  {
    std::lock_guard lock(g_runtimeMutex);
    hasContextWardrobeTicket = std::ranges::any_of(
        g_suppressionTickets, [&](const SuppressionTicket &a_ticket) {
           return a_ticket.actorID == actorID &&
                  a_ticket.source == "ContextWardrobeReplacement";
         });
    hasContextWardrobeSnapshot =
        g_actorContextWardrobeOriginalWornArmor.contains(actorID);
  }
  const bool hasActualContextWardrobe =
      sfs::native::external_equipment::HasContextWardrobeReplacement(actorID);
  const bool hasContextWardrobeState =
      hasContextWardrobeTicket || hasContextWardrobeSnapshot ||
      hasActualContextWardrobe;
  const bool outfitBoundary = previous->outfitID != 0 &&
                              current.outfitID != previous->outfitID;
  const bool substantialReturn =
      current.wornArmor.size() >= previous->wornArmor.size() + 2;
  if (hasContextWardrobeState && (outfitBoundary || substantialReturn)) {
    std::size_t removedTickets = 0;
    {
      std::lock_guard lock(g_runtimeMutex);
      const auto before = g_suppressionTickets.size();
      std::erase_if(g_suppressionTickets,
                    [&](const SuppressionTicket &a_ticket) {
                      return a_ticket.actorID == actorID &&
                             a_ticket.source ==
                                 "ContextWardrobeReplacement";
                    });
      removedTickets = before - g_suppressionTickets.size();
      g_actorContextWardrobeOriginalWornArmor.erase(actorID);
      g_actorContextWardrobeInitializedActualArmor.erase(actorID);
    }
    sfs::native::external_equipment::ReleaseContextWardrobeReplacement(actorID);
    if (removedTickets != 0) {
      ApplyOwnedMask(actorID);
    }
    logger::info(
        "Context wardrobe recovery actor={:08X} previousWorn={} "
        "currentWorn={} outfitChanged={} releasedTickets={} "
        "releasedActualContext={}",
        actorID, previous->wornArmor.size(), current.wornArmor.size(),
        outfitBoundary, removedTickets, hasActualContextWardrobe);
    return;
  }

  std::size_t removedCount = 0;
  for (const auto &[formID, mask] : previous->wornArmor) {
    if (!current.wornArmor.contains(formID)) {
      ++removedCount;
    }
  }
  const bool substantialRemoval =
      removedCount >= 2 &&
      removedCount * 2 >= previous->wornArmor.size() &&
      current.wornArmor.size() < previous->wornArmor.size();
  const bool outfitChanged = outfitBoundary;
  if (!substantialRemoval && !outfitChanged) {
    return;
  }

  {
    std::lock_guard lock(g_runtimeMutex);
    if (HasActorStripTransactionLocked(actorID)) {
      return;
    }
  }

  const auto appearances = GetRegisteredAppearances(actorID);
  bool committed = false;
  std::uint64_t transactionID = 0;
  if (IsModSettingsStripLinkActive()) {
    transactionID = AddTicket(actorID, 0, appearances,
                              "ContextWardrobeReplacement", 0, 0, true);
    committed = transactionID != 0;
  } else if (sfs::workbench::IsActualEquipmentStripLinkPolicyActive()) {
    auto *menu = sfs::Menu::GetSingleton();
    const auto actualEquipmentLinkedSlotMask =
        menu != nullptr
            ? menu->GetWorkbench().GetActualEquipmentLinkedSlotMaskForActor(
                  actorID)
            : std::uint64_t{0};
    committed =
        sfs::native::external_equipment::BeginContextWardrobeReplacement(
            actorID, ToActualContextWardrobeSnapshot(previous->wornArmor),
            ToActualContextWardrobeSnapshot(current.wornArmor),
            actualEquipmentLinkedSlotMask);
  }
  if (committed) {
    {
      std::lock_guard lock(g_runtimeMutex);
      auto &original = g_actorContextWardrobeOriginalWornArmor[actorID];
      original.clear();
      for (const auto &[armorID, slotMask] : previous->wornArmor) {
        static_cast<void>(slotMask);
        original.insert(armorID);
      }
      g_actorContextWardrobeInitializedActualArmor.erase(actorID);
    }
    logger::info(
        "Context wardrobe replacement actor={:08X} removed={} "
        "previousWorn={} currentWorn={} outfitChanged={} policy={} "
        "committedAppearances={} transaction={}",
        actorID, removedCount, previous->wornArmor.size(),
        current.wornArmor.size(), outfitChanged,
        IsModSettingsStripLinkActive() ? "ModSettings" : "Vanilla",
        IsModSettingsStripLinkActive() ? appearances.size() : 0,
        transactionID);
  }
}

[[nodiscard]] bool HasRestorableTicket(const RE::FormID a_actorID,
                                        const RE::FormID a_itemID) {
  if (a_actorID == 0 || a_itemID == 0) {
    return false;
  }
  std::lock_guard lock(g_runtimeMutex);
  return std::ranges::any_of(
      g_suppressionTickets, [&](const SuppressionTicket &a_ticket) {
        return a_ticket.actorID == a_actorID &&
               a_ticket.restoreItemID == a_itemID;
      });
}

[[nodiscard]] std::uint64_t GetRestorableTicketTransaction(
    const RE::FormID a_actorID, const RE::FormID a_itemID) {
  if (a_actorID == 0 || a_itemID == 0) {
    return 0;
  }
  std::lock_guard lock(g_runtimeMutex);
  const auto ticket = std::ranges::find_if(
      g_suppressionTickets, [&](const SuppressionTicket &a_candidate) {
        return a_candidate.actorID == a_actorID &&
               a_candidate.restoreItemID == a_itemID;
      });
  return ticket != g_suppressionTickets.end() ? ticket->transactionID : 0;
}

void RestoreTickets(const RE::FormID a_actorID, const RE::FormID a_itemID,
                    const std::string_view a_source) {
  if (a_actorID == 0 || a_itemID == 0) {
    return;
  }
  std::size_t removed = 0;
  bool transactionCompleted = false;
  bool actorRecoveryCompleted = false;
  {
    std::lock_guard lock(g_runtimeMutex);
    std::unordered_set<std::uint64_t> affectedTransactions;
    for (const auto &ticket : g_suppressionTickets) {
      if (ticket.actorID == a_actorID &&
          ticket.restoreItemID == a_itemID) {
        affectedTransactions.insert(ticket.transactionID);
      }
    }
    const auto before = g_suppressionTickets.size();
    std::erase_if(g_suppressionTickets,
                  [&](const SuppressionTicket &a_ticket) {
                    return a_ticket.actorID == a_actorID &&
                           a_ticket.restoreItemID == a_itemID;
                  });
    removed = before - g_suppressionTickets.size();
    transactionCompleted = std::ranges::any_of(
        affectedTransactions, [&](const std::uint64_t a_transactionID) {
          return std::ranges::none_of(
              g_suppressionTickets, [&](const SuppressionTicket &a_ticket) {
                return a_ticket.transactionID == a_transactionID;
              });
        });
    std::erase_if(g_stripTransactions, [&](const auto &a_entry) {
      if (a_entry.second.actorID != a_actorID) {
        return false;
      }
      return std::ranges::none_of(
          g_suppressionTickets, [&](const SuppressionTicket &a_ticket) {
            return a_ticket.transactionID == a_entry.first;
          });
    });
    actorRecoveryCompleted =
        transactionCompleted && !HasActorStripTransactionLocked(a_actorID);
    if (actorRecoveryCompleted) {
      g_recoveryProbeExpires.erase(a_actorID);
    }
  }
  if (removed != 0) {
    // Clear ownership immediately, but do not rebuild the actor once per
    // EquipItemEx in a redress burst. The v1.2.1 stable integration rule is
    // source-local cleanup followed by one queued visual refresh after the
    // source transaction has fully completed. This also keeps SFS out of an
    // external framework's event-end input-control window.
    if (actorRecoveryCompleted) {
      QueuePostRecoveryDisplayRefresh(a_actorID);
    }
    ApplyOwnedMask(a_actorID, false);
    logger::info("Virtual token restore consumed actor={:08X} item={:08X} "
                 "tickets={} source={}",
                 a_actorID, a_itemID, removed, a_source);
  }
}

void CompleteSettledTransactions(
    const RE::FormID a_actorID, const std::string_view a_source,
    const std::uint64_t a_expectedGeneration = 0,
    const std::uint64_t a_expectedEpoch = 0) {
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorID);
  if (!actor) {
    return;
  }

  std::vector<StripTransaction> candidates;
  {
    std::lock_guard lock(g_runtimeMutex);
    if (a_expectedGeneration != 0) {
      const auto current = g_actorAutomationGenerations.find(a_actorID);
      if (g_runtimeEpoch.load(std::memory_order_acquire) !=
              a_expectedEpoch ||
          current == g_actorAutomationGenerations.end() ||
          current->second != a_expectedGeneration) {
        return;
      }
    }
    for (const auto &[transactionID, transaction] : g_stripTransactions) {
      if (transaction.actorID == a_actorID) {
        candidates.push_back(transaction);
      }
    }
  }
  if (candidates.empty()) {
    return;
  }

  const auto currentOutfitID = CurrentDefaultOutfitID(actor);
  const auto currentWornArmor = CaptureWornArmorSnapshot(actor);
  std::unordered_map<std::uint64_t, std::string> completed;
  for (const auto &transaction : candidates) {
    const bool exactOriginalOutfit =
        transaction.originalOutfitRestoreObserved &&
        transaction.originalOutfitID != 0 &&
        transaction.originalOutfitID == currentOutfitID;
    const bool exactOriginalWornSet =
        !transaction.originalWornArmor.empty() &&
        std::ranges::all_of(transaction.originalWornArmor,
                            [&](const auto &a_original) {
                              return currentWornArmor.contains(
                                  a_original.first);
                            });
    if (exactOriginalWornSet || exactOriginalOutfit) {
      completed.emplace(transaction.id,
                        exactOriginalWornSet ? "exact-worn-set"
                                            : "exact-original-outfit");
    }
  }
  if (completed.empty()) {
    return;
  }

  std::size_t removedTickets = 0;
  bool actorRecoveryCompleted = false;
  {
    std::lock_guard lock(g_runtimeMutex);
    if (a_expectedGeneration != 0) {
      const auto current = g_actorAutomationGenerations.find(a_actorID);
      if (g_runtimeEpoch.load(std::memory_order_acquire) !=
              a_expectedEpoch ||
          current == g_actorAutomationGenerations.end() ||
          current->second != a_expectedGeneration) {
        return;
      }
    }
    const auto before = g_suppressionTickets.size();
    std::erase_if(g_suppressionTickets,
                  [&](const SuppressionTicket &a_ticket) {
                    return completed.contains(a_ticket.transactionID);
                  });
    removedTickets = before - g_suppressionTickets.size();
    for (const auto &[transactionID, reason] : completed) {
      static_cast<void>(reason);
      g_stripTransactions.erase(transactionID);
    }
    actorRecoveryCompleted = !HasActorStripTransactionLocked(a_actorID);
    if (actorRecoveryCompleted) {
      g_recoveryProbeExpires.erase(a_actorID);
    }
  }
  if (actorRecoveryCompleted) {
    QueuePostRecoveryDisplayRefresh(a_actorID);
  }
  ApplyOwnedMask(a_actorID, false);
  for (const auto &[transactionID, reason] : completed) {
    logger::info("Settled strip transaction={} actor={:08X} reason={} "
                 "tickets={} source={}",
                 transactionID, a_actorID, reason, removedTickets, a_source);
  }
}

void QueueSettledTransactionCheck(const RE::FormID a_actorID,
                                  const std::string_view a_source) {
  if (a_actorID == 0) {
    return;
  }
  std::uint64_t generation = 0;
  {
    std::lock_guard lock(g_runtimeMutex);
    if (!HasActorStripTransactionLocked(a_actorID)) {
      return;
    }
    generation = ++g_actorAutomationGenerations[a_actorID];
  }
  const auto epoch = g_runtimeEpoch.load(std::memory_order_acquire);
  const auto source = std::string(a_source);
  auto *taskInterface = SKSE::GetTaskInterface();
  if (!taskInterface) {
    CompleteSettledTransactions(a_actorID, source, generation, epoch);
    return;
  }

  // Two task-queue turns debounce a burst of EquipItemEx/TESEquipEvent
  // notifications and give SetOutfit/base-outfit evaluation time to settle.
  // Newer equipment or manual UI activity increments the actor generation and
  // makes both queued closures harmless no-ops.
  taskInterface->AddTask([a_actorID, generation, epoch, source]() {
    {
      std::lock_guard lock(g_runtimeMutex);
      const auto current = g_actorAutomationGenerations.find(a_actorID);
      if (g_runtimeEpoch.load(std::memory_order_acquire) != epoch ||
          current == g_actorAutomationGenerations.end() ||
          current->second != generation) {
        return;
      }
    }
    auto *nextTaskInterface = SKSE::GetTaskInterface();
    if (!nextTaskInterface) {
      CompleteSettledTransactions(a_actorID, source, generation, epoch);
      return;
    }
    nextTaskInterface->AddTask(
        [a_actorID, generation, epoch, source]() {
          {
            std::lock_guard lock(g_runtimeMutex);
            const auto current =
                g_actorAutomationGenerations.find(a_actorID);
            if (g_runtimeEpoch.load(std::memory_order_acquire) != epoch ||
                current == g_actorAutomationGenerations.end() ||
                current->second != generation) {
              return;
            }
          }
          CompleteSettledTransactions(a_actorID, source, generation, epoch);
        });
  });
}

void ObserveSuccessfulSetOutfit(const RE::FormID a_actorID,
                                const RE::FormID a_outfitID,
                                const std::string_view a_source) {
  if (a_actorID == 0) {
    return;
  }
  bool hasTransaction = false;
  {
    std::lock_guard lock(g_runtimeMutex);
    for (auto &[transactionID, transaction] : g_stripTransactions) {
      static_cast<void>(transactionID);
      if (transaction.actorID != a_actorID) {
        continue;
      }
      hasTransaction = true;
      if (a_outfitID != 0 &&
          a_outfitID == transaction.originalOutfitID) {
        transaction.originalOutfitRestoreObserved = true;
      }
    }
  }
  if (hasTransaction) {
    QueueSettledTransactionCheck(a_actorID, a_source);
  }
}

void CancelTransaction(const RE::FormID a_actorID,
                       const std::uint64_t a_transactionID) {
  if (a_actorID == 0 || a_transactionID == 0) {
    return;
  }
  {
    std::lock_guard lock(g_runtimeMutex);
    std::erase_if(g_suppressionTickets,
                  [&](const SuppressionTicket &a_ticket) {
                    return a_ticket.actorID == a_actorID &&
                           a_ticket.transactionID == a_transactionID;
                  });
    g_stripTransactions.erase(a_transactionID);
    ++g_actorAutomationGenerations[a_actorID];
    if (!HasActorStripTransactionLocked(a_actorID)) {
      g_recoveryProbeExpires.erase(a_actorID);
    }
  }
  ApplyOwnedMask(a_actorID);
}

[[nodiscard]] std::unordered_map<RE::FormID, std::uint32_t>
GetObservedForms(const RE::VMStackID a_stackID,
                 const RE::FormID a_actorID) {
  std::lock_guard lock(g_runtimeMutex);
  const auto it = g_stackObservations.find(a_stackID);
  return it != g_stackObservations.end() && it->second.actorID == a_actorID
             ? it->second.returnedForms
             : std::unordered_map<RE::FormID, std::uint32_t>{};
}

[[nodiscard]] std::uint64_t BeginWholeActorStrip(
    RE::Actor *a_actor, const RE::VMStackID a_stackID,
    const std::string_view a_source) {
  const auto actorID = ActorID(a_actor);
  const auto appearances = GetRegisteredAppearances(actorID);
  if (appearances.empty()) {
    return 0;
  }
  const auto observedForms = GetObservedForms(a_stackID, actorID);
  std::unordered_map<RE::FormID, std::vector<RegisteredAppearance>> grouped;
  for (const auto &appearance : appearances) {
    RE::FormID ownerItemID = 0;
    for (const auto &[formID, queriedMask] : observedForms) {
      if ((queriedMask & appearance.tokenSlotMask) == 0) {
        continue;
      }
      const auto *form = RE::TESForm::LookupByID(formID);
      if (MaskForToken(form) != 0) {
        ownerItemID = formID;
        break;
      }
      if (const auto *armor = form ? form->As<RE::TESObjectARMO>() : nullptr;
          armor &&
          (static_cast<std::uint32_t>(
               sfs::armor::GetArmorDisplaySlotMask(armor)) &
           appearance.tokenSlotMask) != 0) {
        ownerItemID = formID;
        break;
      }
    }
    if (ownerItemID == 0) {
      if (const auto *worn =
              FindRealWornArmor(a_actor, appearance.tokenSlotMask)) {
        ownerItemID = worn->GetFormID();
      }
    }
    grouped[ownerItemID].push_back(appearance);
  }

  const auto transactionID =
      EnsureStripTransaction(a_actor, a_stackID, a_source);
  if (transactionID == 0) {
    return 0;
  }
  for (const auto &[itemID, itemAppearances] : grouped) {
    static_cast<void>(AddTicket(actorID, itemID, itemAppearances, a_source,
                                transactionID, 0, false));
  }
  logger::info("Virtual token whole-actor strip staged actor={:08X} "
               "appearances={} owners={} stack={} source={}",
               actorID, appearances.size(), grouped.size(), a_stackID,
               a_source);
  return transactionID;
}

[[nodiscard]] std::vector<RegisteredAppearance>
TakeRetrospectiveCatalogSelection(
    const RE::VMStackID a_stackID, const RE::FormID a_actorID,
    const std::vector<CallerIdentity> &a_chain) {
  if (a_stackID == 0 || a_actorID == 0 || a_chain.empty()) {
    return {};
  }

  bool arrayCatalog = false;
  std::uint32_t queriedMask = 0;
  std::unordered_set<std::string> candidateIdentities;
  {
    std::lock_guard lock(g_runtimeMutex);
    const auto observation = g_stackObservations.find(a_stackID);
    if (observation == g_stackObservations.end() ||
        observation->second.actorID != a_actorID ||
        observation->second.retrospectiveSelectionStaged ||
        !CallerChainsOverlap(observation->second.callerChain, a_chain)) {
      return {};
    }
    auto &catalog = observation->second;
    arrayCatalog = catalog.equippedArrayObserved;
    queriedMask = catalog.queriedMask;

    // An explicit catalog filter is enough selection evidence on the first
    // real mutation.  An unfiltered equipped-array reader needs a prior
    // completed mutation, while per-slot queries are already bounded by the
    // exact masks queried before the confirming mutation.
    const bool selectionReady =
        arrayCatalog
            ? catalog.catalogFilterObserved ||
                  !catalog.completedMutations.empty()
            : queriedMask != 0;
    if (!selectionReady) {
      return {};
    }
    candidateIdentities = catalog.catalogAppearanceCandidates;
    catalog.retrospectiveSelectionStaged = true;
    catalog.lastSeen = RuntimeClock::now();
  }

  auto appearances = GetRegisteredAppearances(
      a_actorID, arrayCatalog ? 0xFFFFFFFFU : queriedMask);
  if (arrayCatalog) {
    std::erase_if(appearances, [&](const RegisteredAppearance &a_appearance) {
      return !candidateIdentities.contains(a_appearance.identity);
    });
  }
  return appearances;
}

void StageRetrospectiveCatalogSelection(
    RE::Actor *a_actor, const RE::VMStackID a_stackID,
    const std::vector<CallerIdentity> &a_chain,
    const std::uint64_t a_transactionID,
    const std::string_view a_source) {
  const auto actorID = ActorID(a_actor);
  if (actorID == 0 || a_transactionID == 0) {
    return;
  }
  const auto appearances = TakeRetrospectiveCatalogSelection(
      a_stackID, actorID, a_chain);
  if (appearances.empty()) {
    return;
  }

  std::unordered_map<RE::FormID, std::vector<RegisteredAppearance>> grouped;
  for (const auto &appearance : appearances) {
    RE::FormID ownerItemID = 0;
    if (const auto *worn =
            FindRealWornArmor(a_actor, appearance.tokenSlotMask)) {
      ownerItemID = worn->GetFormID();
    }
    grouped[ownerItemID].push_back(appearance);
  }
  for (const auto &[itemID, itemAppearances] : grouped) {
    static_cast<void>(AddTicket(actorID, itemID, itemAppearances, a_source,
                                a_transactionID, 0, false));
  }
  logger::info("First-call catalog selection staged actor={:08X} "
               "transaction={} appearances={} owners={} stack={} source={}",
               actorID, a_transactionID, appearances.size(), grouped.size(),
               a_stackID, a_source);
}

[[nodiscard]] std::uint64_t MutationKey(const RE::FormID a_actorID,
                                        const RE::FormID a_itemID,
                                        const bool a_equipped) {
  auto key = (static_cast<std::uint64_t>(a_actorID) << 32U) | a_itemID;
  return a_equipped ? key ^ (1ULL << 63U) : key;
}

void StagePendingMutation(const RE::FormID a_actorID,
                          const RE::FormID a_itemID,
                          const bool a_equipped,
                          const std::uint32_t a_slotMask,
                          const std::uint64_t a_transactionID,
                          const std::string_view a_source) {
  if (a_actorID == 0 || a_itemID == 0) {
    return;
  }
  std::lock_guard lock(g_runtimeMutex);
  const auto now = RuntimeClock::now();
  PruneRuntimeStateLocked(now);
  g_pendingMutations[MutationKey(a_actorID, a_itemID, a_equipped)] = {
      .actorID = a_actorID,
      .itemID = a_itemID,
      .equipped = a_equipped,
      .slotMask = a_slotMask,
      .transactionID = a_transactionID,
      .manualGeneration = g_actorManualGenerations[a_actorID] + 1,
      .source = std::string(a_source),
      .expires = now + kPendingMutationLifetime};
}

void CancelPendingMutation(const RE::FormID a_actorID,
                           const RE::FormID a_itemID,
                           const bool a_equipped) {
  std::lock_guard lock(g_runtimeMutex);
  g_pendingMutations.erase(MutationKey(a_actorID, a_itemID, a_equipped));
}

void InvalidateAppearanceTickets(const RE::FormID a_actorID,
                                 const std::string_view a_identity,
                                 const std::uint32_t a_slotMask,
                                 const bool a_deleted) {
  if (a_actorID == 0) {
    return;
  }
  {
    std::lock_guard lock(g_runtimeMutex);
    ++g_actorAutomationGenerations[a_actorID];
    ++g_actorManualGenerations[a_actorID];
    g_recoveryProbeExpires.erase(a_actorID);
    g_postRecoveryRefreshGenerations.erase(a_actorID);
    std::erase_if(g_pendingMutations, [&](const auto &a_entry) {
      const auto &mutation = a_entry.second;
      return mutation.actorID == a_actorID &&
             (a_deleted || a_slotMask == 0 || mutation.slotMask == 0 ||
              (mutation.slotMask & a_slotMask) != 0);
    });
    if (a_deleted) {
      g_manualAutomationOverrides.erase(std::string(a_identity));
    }
    for (auto &ticket : g_suppressionTickets) {
      std::erase_if(ticket.appearances,
                    [&](const AppearanceTicketRef &a_appearance) {
                      if (a_deleted) {
                        return a_appearance.identity == a_identity ||
                               (a_slotMask != 0 &&
                                (a_appearance.slotMask & a_slotMask) != 0);
                      }
                      return (a_appearance.slotMask & a_slotMask) != 0;
                    });
    }
    std::erase_if(g_suppressionTickets,
                  [](const SuppressionTicket &a_ticket) {
                    return a_ticket.appearances.empty();
                  });
    std::erase_if(g_stripTransactions, [&](const auto &a_entry) {
      if (a_entry.second.actorID != a_actorID) {
        return false;
      }
      return std::ranges::none_of(
          g_suppressionTickets, [&](const SuppressionTicket &a_ticket) {
            return a_ticket.transactionID == a_entry.first;
          });
    });
  }
  ApplyOwnedMask(a_actorID);
  logger::info("Virtual token automation invalidated actor={:08X} "
               "identity='{}' slotMask={:08X} reason={}",
               a_actorID, a_identity, a_slotMask,
               a_deleted ? "delete" : "manual-visibility");
}

enum class TargetNative {
  None,
  GetWornForm,
  GetEquippedArmorInSlot,
  WornHasKeyword,
  AddAllEquippedItemsToArray,
  FormHasKeyword,
  FormGetKeywords,
  FormGetNumKeywords,
  FormGetNthKeyword,
  FilterFormsByKeywordString,
  FilterFormsByKeyword,
  FilterBySlotMask,
  HasKeywordSubstring,
  FormListContains,
  EquipItem,
  UnequipItem,
  UnequipAll,
  UnequipItemSlot,
  EquipItemEx,
  UnequipItemEx,
  EquipItemByID,
  AddItem,
  RemoveItem,
  RemoveAllItems,
  DropObject,
  SetOutfit,
  DeviousDevicesSyncSetting,
  SexLabPPlusStripByData,
  SexLabPPlusStripByDataEx,
  SexLabPPlusUnequipSlots
};

[[nodiscard]] bool EqualNoCase(const char *a_left, const char *a_right) {
  return a_left && a_right && _stricmp(a_left, a_right) == 0;
}

[[nodiscard]] bool MatchesNativeSignature(
    const NativeFunctionBase *a_function, const bool a_static,
    const RE::BSScript::TypeInfo::RawType a_returnType,
    const std::initializer_list<RE::BSScript::TypeInfo::RawType>
        a_parameterTypes) {
  if (!a_function || a_function->GetIsStatic() != a_static ||
      a_function->GetReturnType().GetUnmangledRawType() != a_returnType ||
      a_function->GetParamCount() != a_parameterTypes.size()) {
    return false;
  }
  std::uint32_t index = 0;
  for (const auto expectedType : a_parameterTypes) {
    RE::BSFixedString name;
    RE::BSScript::TypeInfo type;
    a_function->GetParam(index++, name, type);
    if (type.GetUnmangledRawType() != expectedType) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool MatchesStaticNativeSignature(
    const NativeFunctionBase *a_function,
    const RE::BSScript::TypeInfo::RawType a_returnType,
    const std::initializer_list<RE::BSScript::TypeInfo::RawType>
        a_parameterTypes) {
  return MatchesNativeSignature(a_function, true, a_returnType,
                                a_parameterTypes);
}

[[nodiscard]] bool MatchesMemberNativeSignature(
    const NativeFunctionBase *a_function,
    const RE::BSScript::TypeInfo::RawType a_returnType,
    const std::initializer_list<RE::BSScript::TypeInfo::RawType>
        a_parameterTypes) {
  return MatchesNativeSignature(a_function, false, a_returnType,
                                a_parameterTypes);
}

[[nodiscard]] TargetNative
ClassifyNativeUncached(const NativeFunctionBase *a_function) {
  if (!a_function) {
    return TargetNative::None;
  }
  const auto *object = a_function->GetObjectTypeName().c_str();
  const auto *name = a_function->GetName().c_str();
  if (EqualNoCase(object, "Actor")) {
    if (EqualNoCase(name, "GetWornForm")) {
      return TargetNative::GetWornForm;
    }
    if (EqualNoCase(name, "GetEquippedArmorInSlot")) {
      return TargetNative::GetEquippedArmorInSlot;
    }
    if (EqualNoCase(name, "WornHasKeyword")) {
      return TargetNative::WornHasKeyword;
    }
    if (EqualNoCase(name, "EquipItem")) {
      return TargetNative::EquipItem;
    }
    if (EqualNoCase(name, "UnequipItem")) {
      return TargetNative::UnequipItem;
    }
    if (EqualNoCase(name, "UnequipAll")) {
      return TargetNative::UnequipAll;
    }
    if (EqualNoCase(name, "UnequipItemSlot")) {
      return TargetNative::UnequipItemSlot;
    }
    if (EqualNoCase(name, "EquipItemEx")) {
      return TargetNative::EquipItemEx;
    }
    if (EqualNoCase(name, "UnequipItemEx")) {
      return TargetNative::UnequipItemEx;
    }
    if (EqualNoCase(name, "EquipItemByID")) {
      return TargetNative::EquipItemByID;
    }
    if (EqualNoCase(name, "SetOutfit")) {
      return TargetNative::SetOutfit;
    }
  } else if (EqualNoCase(object, "Form")) {
    if (EqualNoCase(name, "HasKeyword")) {
      return TargetNative::FormHasKeyword;
    }
    if (EqualNoCase(name, "GetKeywords")) {
      return TargetNative::FormGetKeywords;
    }
    if (EqualNoCase(name, "GetNumKeywords")) {
      return TargetNative::FormGetNumKeywords;
    }
    if (EqualNoCase(name, "GetNthKeyword")) {
      return TargetNative::FormGetNthKeyword;
    }
  } else if (EqualNoCase(name, "AddAllEquippedItemsToArray") &&
             MatchesStaticNativeSignature(
                 a_function,
                 RE::BSScript::TypeInfo::RawType::kObjectArray,
                 {RE::BSScript::TypeInfo::RawType::kObject})) {
    return TargetNative::AddAllEquippedItemsToArray;
  } else if (EqualNoCase(name, "FilterFormsByKeywordString") &&
             MatchesStaticNativeSignature(
                 a_function,
                 RE::BSScript::TypeInfo::RawType::kObjectArray,
                 {RE::BSScript::TypeInfo::RawType::kObjectArray,
                  RE::BSScript::TypeInfo::RawType::kStringArray,
                  RE::BSScript::TypeInfo::RawType::kBool,
                  RE::BSScript::TypeInfo::RawType::kBool})) {
    return TargetNative::FilterFormsByKeywordString;
  } else if (EqualNoCase(name, "FilterFormsByKeyword") &&
             MatchesStaticNativeSignature(
                 a_function,
                 RE::BSScript::TypeInfo::RawType::kObjectArray,
                 {RE::BSScript::TypeInfo::RawType::kObjectArray,
                  RE::BSScript::TypeInfo::RawType::kObjectArray,
                  RE::BSScript::TypeInfo::RawType::kBool,
                  RE::BSScript::TypeInfo::RawType::kBool})) {
    return TargetNative::FilterFormsByKeyword;
  } else if (EqualNoCase(name, "FilterBySlotmask") &&
             MatchesStaticNativeSignature(
                 a_function,
                 RE::BSScript::TypeInfo::RawType::kObjectArray,
                 {RE::BSScript::TypeInfo::RawType::kObjectArray,
                  RE::BSScript::TypeInfo::RawType::kInt,
                  RE::BSScript::TypeInfo::RawType::kBool})) {
    // The owning script type is intentionally irrelevant. This common
    // array-filter contract is used by slot-filter strip helpers such as PNO.
    return TargetNative::FilterBySlotMask;
  } else if (EqualNoCase(name, "HasKeywordSub") &&
             MatchesStaticNativeSignature(
                 a_function, RE::BSScript::TypeInfo::RawType::kBool,
                 {RE::BSScript::TypeInfo::RawType::kObject,
                  RE::BSScript::TypeInfo::RawType::kString})) {
    return TargetNative::HasKeywordSubstring;
  } else if (EqualNoCase(name, "FormListHas") &&
             MatchesStaticNativeSignature(
                 a_function, RE::BSScript::TypeInfo::RawType::kBool,
                 {RE::BSScript::TypeInfo::RawType::kObject,
                  RE::BSScript::TypeInfo::RawType::kString,
                  RE::BSScript::TypeInfo::RawType::kObject})) {
    return TargetNative::FormListContains;
  } else if (EqualNoCase(object, "ObjectReference")) {
    if (EqualNoCase(name, "AddItem")) {
      return TargetNative::AddItem;
    }
    if (EqualNoCase(name, "RemoveItem")) {
      return TargetNative::RemoveItem;
    }
    if (EqualNoCase(name, "RemoveAllItems")) {
      return TargetNative::RemoveAllItems;
    }
    if (EqualNoCase(name, "DropObject")) {
      return TargetNative::DropObject;
    }
  } else if (EqualNoCase(object, "ZadNativeFunctions") &&
             EqualNoCase(name, "SyncSetting")) {
    return TargetNative::DeviousDevicesSyncSetting;
  } else if (EqualNoCase(object, "sslActorAlias")) {
    if (EqualNoCase(name, "StripByData") &&
        MatchesMemberNativeSignature(
            a_function, RE::BSScript::TypeInfo::RawType::kObjectArray,
            {RE::BSScript::TypeInfo::RawType::kInt,
             RE::BSScript::TypeInfo::RawType::kIntArray,
             RE::BSScript::TypeInfo::RawType::kIntArray})) {
      return TargetNative::SexLabPPlusStripByData;
    }
    if (EqualNoCase(name, "StripByDataEx") &&
        MatchesMemberNativeSignature(
            a_function, RE::BSScript::TypeInfo::RawType::kObjectArray,
            {RE::BSScript::TypeInfo::RawType::kInt,
             RE::BSScript::TypeInfo::RawType::kIntArray,
             RE::BSScript::TypeInfo::RawType::kIntArray,
             RE::BSScript::TypeInfo::RawType::kObjectArray})) {
      return TargetNative::SexLabPPlusStripByDataEx;
    }
  } else if (EqualNoCase(object, "sslActorLibrary") &&
             EqualNoCase(name, "UnequipSlots") &&
             MatchesStaticNativeSignature(
                 a_function,
                 RE::BSScript::TypeInfo::RawType::kObjectArray,
                 {RE::BSScript::TypeInfo::RawType::kObject,
                  RE::BSScript::TypeInfo::RawType::kInt})) {
    return TargetNative::SexLabPPlusUnequipSlots;
  }
  return TargetNative::None;
}

[[nodiscard]] TargetNative ClassifyNative(
    const NativeFunctionBase *a_function) {
  if (!a_function) {
    return TargetNative::None;
  }
  // Native function objects are process-lifetime registrations. Cache the
  // small target classification per Papyrus worker thread so the global
  // observer adds no repeated string-comparison or locking cost to unrelated
  // native calls.
  thread_local std::unordered_map<const NativeFunctionBase *, TargetNative>
      classifications;
  const auto existing = classifications.find(a_function);
  if (existing != classifications.end()) {
    return existing->second;
  }
  const auto classified = ClassifyNativeUncached(a_function);
  classifications.emplace(a_function, classified);
  return classified;
}

template <class T>
[[nodiscard]] T ReadArgument(const RE::BSScript::StackFrame &a_frame,
                             const std::uint32_t a_index) {
  const auto page = a_frame.GetPageForFrame();
  return a_frame.GetStackFrameVariable(a_index, page).Unpack<T>();
}

struct HookOperation {
  TargetNative target{TargetNative::None};
  RE::Actor *actor{nullptr};
  RE::TESForm *item{nullptr};
  RE::BGSOutfit *outfit{nullptr};
  std::uint32_t requestedMask{0};
  std::uint64_t transactionID{0};
  std::uint64_t manualGeneration{0};
  std::vector<std::int32_t> ddSlotMaskFilters;
  std::vector<RE::TESForm *> filterForms;
  std::vector<RE::BGSKeyword *> filterKeywords;
  std::vector<std::string> filterKeywordStrings;
  std::unordered_set<RE::FormID> pplusMergeInputForms;
  std::string keywordSubstring;
  RE::BGSKeyword *keyword{nullptr};
  std::int32_t keywordIndex{-1};
  std::int32_t ddHiderSetting{0};
  bool filterMatchAll{false};
  bool filterInvert{false};
  bool pendingEquipped{false};
  bool pendingStaged{false};
  bool transactionWasNew{false};
  bool recoveryProbeStaged{false};
  bool learningCandidate{false};
  bool recordMutationEvidence{false};
  std::uint32_t learningMask{0};
  std::vector<CallerIdentity> callerChain;
};

[[nodiscard]] const char *TargetName(const TargetNative a_target) {
  switch (a_target) {
  case TargetNative::GetWornForm: return "GetWornForm";
  case TargetNative::GetEquippedArmorInSlot:
    return "GetEquippedArmorInSlot";
  case TargetNative::WornHasKeyword: return "WornHasKeyword";
  case TargetNative::AddAllEquippedItemsToArray:
    return "AddAllEquippedItemsToArray";
  case TargetNative::FormHasKeyword: return "Form.HasKeyword";
  case TargetNative::FormGetKeywords: return "Form.GetKeywords";
  case TargetNative::FormGetNumKeywords: return "Form.GetNumKeywords";
  case TargetNative::FormGetNthKeyword: return "Form.GetNthKeyword";
  case TargetNative::FilterFormsByKeywordString:
    return "FilterFormsByKeywordString";
  case TargetNative::FilterFormsByKeyword:
    return "FilterFormsByKeyword";
  case TargetNative::FilterBySlotMask: return "FilterBySlotmask";
  case TargetNative::HasKeywordSubstring:
    return "HasKeywordSub";
  case TargetNative::FormListContains:
    return "FormListHas";
  case TargetNative::EquipItem: return "EquipItem";
  case TargetNative::UnequipItem: return "UnequipItem";
  case TargetNative::UnequipAll: return "UnequipAll";
  case TargetNative::UnequipItemSlot: return "UnequipItemSlot";
  case TargetNative::EquipItemEx: return "EquipItemEx";
  case TargetNative::UnequipItemEx: return "UnequipItemEx";
  case TargetNative::EquipItemByID: return "EquipItemByID";
  case TargetNative::AddItem: return "AddItem";
  case TargetNative::RemoveItem: return "RemoveItem";
  case TargetNative::RemoveAllItems: return "RemoveAllItems";
  case TargetNative::DropObject: return "DropObject";
  case TargetNative::SetOutfit: return "SetOutfit";
  case TargetNative::DeviousDevicesSyncSetting: return "DDSyncSetting";
  case TargetNative::SexLabPPlusStripByData:
    return "sslActorAlias.StripByData";
  case TargetNative::SexLabPPlusStripByDataEx:
    return "sslActorAlias.StripByDataEx";
  case TargetNative::SexLabPPlusUnequipSlots:
    return "sslActorLibrary.UnequipSlots";
  default: return "None";
  }
}

void ApplyDisplayedBodyKeywordCompatibility(
    const HookOperation &a_operation, RE::BSScript::Stack &a_stack) {
  if (a_operation.target != TargetNative::WornHasKeyword) {
    return;
  }
  const auto displayedBody = sfs::native::GetDisplayedBodyKeywordState(
      a_operation.actor, a_operation.keyword);
  if (!displayedBody.has_value()) {
    return;
  }
  a_stack.returnValue.SetBool(*displayedBody);
  logger::debug("SFS displayed-body compatibility answered WornHasKeyword "
                "{} actor={:08X} keyword={}",
                *displayedBody ? "true" : "false",
                a_operation.actor ? a_operation.actor->GetFormID() : 0,
                sfs::armor::GetEditorID(a_operation.keyword));
}

[[nodiscard]] HookOperation PrepareOperation(
    const TargetNative a_target, RE::BSScript::Stack *a_stack) {
  HookOperation operation{.target = a_target};
  if (!a_stack || !a_stack->top) {
    return operation;
  }
  auto &frame = *a_stack->top;
  operation.callerChain = BuildCallerChain(a_stack);
  if (a_target == TargetNative::FormHasKeyword ||
      a_target == TargetNative::FormGetKeywords ||
      a_target == TargetNative::FormGetNumKeywords ||
      a_target == TargetNative::FormGetNthKeyword) {
    operation.item = frame.self.Unpack<RE::TESForm *>();
  } else if (a_target == TargetNative::DeviousDevicesSyncSetting ||
             a_target == TargetNative::FilterFormsByKeywordString ||
             a_target == TargetNative::FilterFormsByKeyword ||
             a_target == TargetNative::FilterBySlotMask ||
             a_target == TargetNative::HasKeywordSubstring ||
             a_target == TargetNative::FormListContains) {
    operation.actor = nullptr;
  } else if (a_target == TargetNative::AddAllEquippedItemsToArray) {
    // Equipped-array helpers expose the actor as the first argument, not the
    // Papyrus self object.
    operation.actor = ReadArgument<RE::Actor *>(frame, 0);
  } else if (a_target == TargetNative::SexLabPPlusStripByData ||
             a_target == TargetNative::SexLabPPlusStripByDataEx) {
    auto *alias = frame.self.Unpack<RE::BGSRefAlias *>();
    operation.actor = alias ? alias->GetActorReference() : nullptr;
  } else if (a_target == TargetNative::SexLabPPlusUnequipSlots) {
    operation.actor = ReadArgument<RE::Actor *>(frame, 0);
  } else if (a_target == TargetNative::AddItem ||
      a_target == TargetNative::RemoveItem ||
      a_target == TargetNative::RemoveAllItems ||
      a_target == TargetNative::DropObject) {
    auto *reference = frame.self.Unpack<RE::TESObjectREFR *>();
    operation.actor = reference ? reference->As<RE::Actor>() : nullptr;
  } else {
    operation.actor = frame.self.Unpack<RE::Actor *>();
  }

  switch (a_target) {
  case TargetNative::GetWornForm:
    operation.requestedMask =
        static_cast<std::uint32_t>(ReadArgument<std::int32_t>(frame, 0));
    break;
  case TargetNative::GetEquippedArmorInSlot: {
    const auto slot =
        static_cast<std::uint32_t>(ReadArgument<std::int32_t>(frame, 0));
    operation.requestedMask = SlotMask(slot);
    break;
  }
  case TargetNative::WornHasKeyword:
    operation.keyword = ReadArgument<RE::BGSKeyword *>(frame, 0);
    break;
  case TargetNative::FormHasKeyword:
    operation.keyword = ReadArgument<RE::BGSKeyword *>(frame, 0);
    break;
  case TargetNative::FormGetNthKeyword:
    operation.keywordIndex = ReadArgument<std::int32_t>(frame, 0);
    break;
  case TargetNative::FilterFormsByKeywordString:
    operation.filterForms =
        ReadArgument<std::vector<RE::TESForm *>>(frame, 0);
    operation.filterKeywordStrings =
        ReadArgument<std::vector<std::string>>(frame, 1);
    operation.filterMatchAll = ReadArgument<bool>(frame, 2);
    operation.filterInvert = ReadArgument<bool>(frame, 3);
    break;
  case TargetNative::FilterFormsByKeyword:
    operation.filterForms =
        ReadArgument<std::vector<RE::TESForm *>>(frame, 0);
    operation.filterKeywords =
        ReadArgument<std::vector<RE::BGSKeyword *>>(frame, 1);
    operation.filterMatchAll = ReadArgument<bool>(frame, 2);
    operation.filterInvert = ReadArgument<bool>(frame, 3);
    break;
  case TargetNative::FilterBySlotMask:
    operation.filterForms =
        ReadArgument<std::vector<RE::TESForm *>>(frame, 0);
    operation.requestedMask =
        static_cast<std::uint32_t>(ReadArgument<std::int32_t>(frame, 1));
    operation.filterMatchAll = ReadArgument<bool>(frame, 2);
    break;
  case TargetNative::HasKeywordSubstring:
    operation.item = ReadArgument<RE::TESForm *>(frame, 0);
    operation.keywordSubstring = ReadArgument<std::string>(frame, 1);
    break;
  case TargetNative::FormListContains:
    operation.item = ReadArgument<RE::TESForm *>(frame, 2);
    break;
  case TargetNative::EquipItem:
  case TargetNative::UnequipItem:
  case TargetNative::EquipItemEx:
  case TargetNative::UnequipItemEx:
  case TargetNative::EquipItemByID:
  case TargetNative::AddItem:
  case TargetNative::RemoveItem:
  case TargetNative::DropObject:
    operation.item = ReadArgument<RE::TESForm *>(frame, 0);
    break;
  case TargetNative::UnequipItemSlot: {
    const auto slot =
        static_cast<std::uint32_t>(ReadArgument<std::int32_t>(frame, 0));
    operation.requestedMask = SlotMask(slot);
    break;
  }
  case TargetNative::DeviousDevicesSyncSetting:
    operation.ddSlotMaskFilters =
        ReadArgument<std::vector<std::int32_t>>(frame, 0);
    operation.ddHiderSetting = ReadArgument<std::int32_t>(frame, 1);
    break;
  case TargetNative::SetOutfit:
    operation.outfit = ReadArgument<RE::BGSOutfit *>(frame, 0);
    break;
  case TargetNative::SexLabPPlusStripByData:
  case TargetNative::SexLabPPlusStripByDataEx: {
    const auto stripData = ReadArgument<std::int32_t>(frame, 0);
    const auto defaults =
        ReadArgument<std::vector<std::int32_t>>(frame, 1);
    const auto overwrites =
        ReadArgument<std::vector<std::int32_t>>(frame, 2);
    operation.requestedMask =
        sfs::native::sexlab_pplus::rules::ResolveStripSlotMask(
            stripData, defaults, overwrites);
    if (a_target == TargetNative::SexLabPPlusStripByDataEx) {
      const auto merge =
          ReadArgument<std::vector<RE::TESForm *>>(frame, 3);
      for (auto *form : merge) {
        if (form) {
          operation.pplusMergeInputForms.insert(form->GetFormID());
        }
      }
    }
    break;
  }
  case TargetNative::SexLabPPlusUnequipSlots:
    operation.requestedMask =
        static_cast<std::uint32_t>(ReadArgument<std::int32_t>(frame, 1));
    break;
  default: break;
  }
  return operation;
}

[[nodiscard]] bool HandleVirtualTokenOperation(
    const HookOperation &a_operation, RE::BSScript::Stack &a_stack) {
  const auto tokenMask = MaskForToken(a_operation.item);
  if (tokenMask == 0) {
    return false;
  }
  const auto actorID = ActorID(a_operation.actor);
  const auto itemID = a_operation.item->GetFormID();
  switch (a_operation.target) {
  case TargetNative::UnequipItem:
  case TargetNative::UnequipItemEx:
  case TargetNative::RemoveItem:
  case TargetNative::DropObject: {
    ConfirmStripCaller(a_stack.stackID, actorID, a_operation.callerChain,
                       itemID, false, TargetName(a_operation.target));
    const auto transactionID = EnsureStripTransaction(
        a_operation.actor, a_stack.stackID, TargetName(a_operation.target));
    const bool queueDisplayRefresh =
        ShouldQueueVirtualStripRefresh(transactionID);
    static_cast<void>(BeginTicketForMask(
        a_operation.actor, a_operation.item, tokenMask,
        TargetName(a_operation.target), transactionID, 0,
        queueDisplayRefresh));
    break;
  }
  case TargetNative::EquipItem:
  case TargetNative::EquipItemEx:
  case TargetNative::EquipItemByID:
    RestoreTickets(actorID, itemID, TargetName(a_operation.target));
    QueueSettledTransactionCheck(actorID, TargetName(a_operation.target));
    break;
  case TargetNative::AddItem:
    logger::debug("Quarantined virtual token AddItem token={:08X}", itemID);
    break;
  default: return false;
  }
  a_stack.returnValue.SetNone();
  return true;
}

void PrepareRealMutation(HookOperation &a_operation,
                         RE::BSScript::Stack &a_stack) {
  const auto actorID = ActorID(a_operation.actor);
  if (actorID == 0 || GetRegisteredMask(actorID) == 0) {
    return;
  }
  if (const auto *armor =
          a_operation.item
              ? a_operation.item->As<RE::TESObjectARMO>()
              : nullptr;
      armor &&
      sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(armor)) {
    return;
  }
  a_operation.manualGeneration = CaptureManualGeneration(actorID);
  const auto itemID = a_operation.item ? a_operation.item->GetFormID() : 0;
  const auto source = TargetName(a_operation.target);
  switch (a_operation.target) {
  case TargetNative::UnequipItem:
  case TargetNative::UnequipItemEx: {
    ConfirmStripCaller(a_stack.stackID, actorID, a_operation.callerChain,
                       itemID, false, source);
    auto *armor = a_operation.item
                      ? a_operation.item->As<RE::TESObjectARMO>()
                      : nullptr;
    const auto mask = armor ? static_cast<std::uint32_t>(
                                  sfs::armor::GetArmorDisplaySlotMask(armor))
                            : 0;
    if (mask != 0 && IsArmorWornByActor(a_operation.actor, armor)) {
      a_operation.recordMutationEvidence = true;
      a_operation.learningMask = mask;
      if (IsCallerTrusted(a_operation.callerChain)) {
        a_operation.transactionWasNew =
            GetStripTransaction(actorID, a_stack.stackID) == 0;
        a_operation.transactionID = EnsureStripTransaction(
            a_operation.actor, a_stack.stackID, source);
        StageRetrospectiveCatalogSelection(
            a_operation.actor, a_stack.stackID, a_operation.callerChain,
            a_operation.transactionID, source);
        if ((mask & GetRegisteredMask(actorID)) != 0) {
          StagePendingMutation(actorID, itemID, false, mask,
                               a_operation.transactionID, source);
          a_operation.pendingStaged = true;
          ApplyOwnedMask(actorID, false);
        }
      } else {
        a_operation.learningCandidate = true;
        a_operation.learningMask = mask;
      }
    }
    break;
  }
  case TargetNative::UnequipItemSlot: {
    if (auto *armor =
            FindRealWornArmor(a_operation.actor, a_operation.requestedMask)) {
      if (sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(
              armor)) {
        break;
      }
      a_operation.item = armor;
      a_operation.recordMutationEvidence = true;
      a_operation.learningMask = a_operation.requestedMask;
      ConfirmStripCaller(a_stack.stackID, actorID, a_operation.callerChain,
                         armor->GetFormID(), false, source);
      if (IsCallerTrusted(a_operation.callerChain)) {
        a_operation.transactionWasNew =
            GetStripTransaction(actorID, a_stack.stackID) == 0;
        a_operation.transactionID = EnsureStripTransaction(
            a_operation.actor, a_stack.stackID, source);
        StageRetrospectiveCatalogSelection(
            a_operation.actor, a_stack.stackID, a_operation.callerChain,
            a_operation.transactionID, source);
        StagePendingMutation(actorID, armor->GetFormID(), false,
                             a_operation.requestedMask,
                             a_operation.transactionID, source);
        a_operation.pendingStaged = true;
        ApplyOwnedMask(actorID, false);
      } else {
        a_operation.learningCandidate = true;
        a_operation.learningMask = a_operation.requestedMask;
      }
    } else if (IsCallerTrusted(a_operation.callerChain) &&
               (a_operation.requestedMask & GetRegisteredMask(actorID)) !=
                   0) {
      const auto existing =
          GetStripTransaction(actorID, a_stack.stackID);
      a_operation.transactionWasNew = existing == 0;
      a_operation.transactionID = EnsureStripTransaction(
          a_operation.actor, a_stack.stackID, source);
      a_operation.transactionID = BeginTicketForMask(
          a_operation.actor, nullptr, a_operation.requestedMask, source,
          a_operation.transactionID, 0,
          ShouldQueueVirtualStripRefresh(a_operation.transactionID));
    }
    break;
  }
  case TargetNative::UnequipAll:
  case TargetNative::RemoveAllItems:
    ConfirmStripCaller(a_stack.stackID, actorID, a_operation.callerChain, 0,
                       true, source);
    a_operation.transactionWasNew =
        GetStripTransaction(actorID, a_stack.stackID) == 0;
    a_operation.transactionID =
        BeginWholeActorStrip(a_operation.actor, a_stack.stackID, source);
    break;
  case TargetNative::RemoveItem:
  case TargetNative::DropObject: {
    ConfirmStripCaller(a_stack.stackID, actorID, a_operation.callerChain,
                       itemID, false, source);
    auto *armor = a_operation.item
                      ? a_operation.item->As<RE::TESObjectARMO>()
                      : nullptr;
    if (armor && IsArmorWornByActor(a_operation.actor, armor)) {
      a_operation.recordMutationEvidence = true;
      const auto mask = static_cast<std::uint32_t>(
          sfs::armor::GetArmorDisplaySlotMask(armor));
      a_operation.learningMask = mask;
      if (IsCallerTrusted(a_operation.callerChain)) {
        a_operation.transactionWasNew =
            GetStripTransaction(actorID, a_stack.stackID) == 0;
        a_operation.transactionID = EnsureStripTransaction(
            a_operation.actor, a_stack.stackID, source);
        StageRetrospectiveCatalogSelection(
            a_operation.actor, a_stack.stackID, a_operation.callerChain,
            a_operation.transactionID, source);
        StagePendingMutation(actorID, itemID, false, mask,
                             a_operation.transactionID, source);
        a_operation.pendingStaged = true;
        ApplyOwnedMask(actorID, false);
      } else {
        a_operation.learningCandidate = true;
        a_operation.learningMask = mask;
      }
    } else if (armor && HasCompletedMutationForItem(
                            a_stack.stackID, actorID, itemID)) {
      a_operation.recordMutationEvidence = true;
      a_operation.learningCandidate = true;
      a_operation.learningMask = static_cast<std::uint32_t>(
          sfs::armor::GetArmorDisplaySlotMask(armor));
    }
    break;
  }
  case TargetNative::SetOutfit:
    // SetOutfit is only a possible recovery signal for an already-open strip
    // transaction.  An ordinary outfit maintenance call must never create
    // suppression by itself.
    a_operation.transactionID =
        GetStripTransaction(actorID, a_stack.stackID);
    if (a_operation.transactionID == 0) {
      a_operation.transactionID = GetActorStripTransaction(actorID);
    }
    if (a_operation.transactionID != 0) {
      OpenRecoveryProbe(actorID);
    }
    break;
  case TargetNative::EquipItem:
  case TargetNative::EquipItemEx:
  case TargetNative::EquipItemByID:
    a_operation.recoveryProbeStaged =
        GetActorStripTransaction(actorID) != 0;
    if (a_operation.recoveryProbeStaged) {
      OpenRecoveryProbe(actorID);
    }
    if (HasRestorableTicket(actorID, itemID)) {
      StagePendingMutation(actorID, itemID, true, 0, 0, source);
      a_operation.pendingEquipped = true;
      a_operation.pendingStaged = true;
    }
    break;
  default: break;
  }
}

void CancelFailedOperation(const HookOperation &a_operation) {
  const auto actorID = ActorID(a_operation.actor);
  if (a_operation.pendingStaged && a_operation.item) {
    CancelPendingMutation(actorID, a_operation.item->GetFormID(),
                          a_operation.pendingEquipped);
    ApplyOwnedMask(actorID, false);
  }
  if (a_operation.transactionID != 0 && a_operation.transactionWasNew) {
    CancelTransaction(actorID, a_operation.transactionID);
  }
}

void FinalizeSuccessfulOperation(const HookOperation &a_operation,
                                 RE::BSScript::Stack &a_stack) {
  const auto actorID = ActorID(a_operation.actor);
  const auto source = TargetName(a_operation.target);
  if (a_operation.target == TargetNative::SetOutfit) {
    ObserveSuccessfulSetOutfit(
        actorID, a_operation.outfit ? a_operation.outfit->GetFormID() : 0,
        source);
    return;
  }

  if (a_operation.pendingEquipped && a_operation.item) {
    auto *armor = a_operation.item->As<RE::TESObjectARMO>();
    if (armor && IsArmorWornByActor(a_operation.actor, armor)) {
      CancelPendingMutation(actorID, a_operation.item->GetFormID(), true);
      RestoreTickets(actorID, a_operation.item->GetFormID(), source);
      QueueSettledTransactionCheck(actorID, source);
    }
    return;
  }

  if (a_operation.recoveryProbeStaged) {
    QueueSettledTransactionCheck(actorID, source);
    return;
  }

  if (!a_operation.recordMutationEvidence || !a_operation.actor ||
      !a_operation.item || a_operation.learningMask == 0) {
    return;
  }
  auto *armor = a_operation.item->As<RE::TESObjectARMO>();
  if (!armor || IsArmorWornByActor(a_operation.actor, armor)) {
    return;
  }
  const auto itemID = a_operation.item->GetFormID();
  const bool trusted = RecordCompletedStripMutation(
      a_stack.stackID, actorID, a_operation.callerChain, itemID,
      a_operation.learningMask, source);
  if (trusted) {
    const auto transactionID = EnsureStripTransaction(
        a_operation.actor, a_stack.stackID, source);
    static_cast<void>(BeginTicketForMask(
        a_operation.actor, a_operation.item, a_operation.learningMask,
        source, transactionID, a_operation.manualGeneration, false));
    CancelPendingMutation(actorID, itemID, false);
    ApplyOwnedMask(actorID, false);
  }
}

void HandleGetWornResult(const HookOperation &a_operation,
                         RE::BSScript::Stack &a_stack) {
  const auto actorID = ActorID(a_operation.actor);
  if (actorID == 0 || GetRegisteredMask(actorID) == 0) {
    return;
  }
  auto *returnedForm = a_stack.returnValue.Unpack<RE::TESForm *>();
  const bool tokenExposureAuthorized =
      !returnedForm && IsCallerTrustedForTokenExposure(
                           a_operation.callerChain, a_stack.stackID);
  if (tokenExposureAuthorized) {
    const auto registeredMask = GetRegisteredMask(actorID);
    const auto suppressedMask =
        sfs::native::GetSuppressedFittingSlotMask(a_operation.actor) |
        sfs::devious_devices::GetDeviousDevicesHiderSuppressedFittingSlotMask(
            a_operation.actor);
    const auto availableMask = a_operation.requestedMask & registeredMask &
                               ~suppressedMask;
    std::uint32_t tokenMask = 0;
    if (auto *token = TokenForMask(availableMask, tokenMask)) {
      returnedForm = token;
      a_stack.returnValue.Pack<RE::TESForm *>(token);
      logger::info("Trusted virtual token offered actor={:08X} token={:08X} "
                   "requestedMask={:08X} tokenMask={:08X} stack={}",
                   actorID, token->GetFormID(), a_operation.requestedMask,
                   tokenMask, a_stack.stackID);
    }
  } else if (!returnedForm && IsCallerTrusted(a_operation.callerChain)) {
    // Keep this first stack's real/null return values frozen, but once a real
    // mutation has opened the transaction, shadow later null slots
    // internally so the rest of the first strip is complete.
    const auto transactionID =
        GetStripTransaction(actorID, a_stack.stackID);
    if (transactionID != 0 &&
        (a_operation.requestedMask & GetRegisteredMask(actorID)) != 0) {
      static_cast<void>(BeginTicketForMask(
          a_operation.actor, nullptr, a_operation.requestedMask,
          TargetName(a_operation.target), transactionID,
          a_operation.manualGeneration,
          ShouldQueueVirtualStripRefresh(transactionID)));
      logger::debug("First-call shadow slot actor={:08X} mask={:08X} "
                    "transaction={} stack={}",
                    actorID, a_operation.requestedMask, transactionID,
                    a_stack.stackID);
    }
  }
  RecordWornQuery(a_stack.stackID, actorID, a_operation.callerChain,
                  a_operation.requestedMask,
                  returnedForm ? returnedForm->GetFormID() : 0);
}

void HandleGetEquippedArmorInSlotResult(
    const HookOperation &a_operation, RE::BSScript::Stack &a_stack) {
  const auto actorID = ActorID(a_operation.actor);
  if (actorID == 0 || a_operation.requestedMask == 0 ||
      GetRegisteredMask(actorID) == 0) {
    return;
  }
  auto *returnedArmor =
      a_stack.returnValue.Unpack<RE::TESObjectARMO *>();
  RecordWornQuery(a_stack.stackID, actorID, a_operation.callerChain,
                  a_operation.requestedMask,
                  returnedArmor ? returnedArmor->GetFormID() : 0);
  if (returnedArmor ||
      (a_operation.requestedMask & GetRegisteredMask(actorID)) == 0 ||
      !IsCallerTrustedForTokenExposure(a_operation.callerChain,
                                       a_stack.stackID)) {
    return;
  }

  // Unlike GetWornForm this native keeps its real null result.  A trusted
  // strip path may only create an internal shadow ticket after a real strip
  // transaction already exists in the same causal stack.  Nothing is packed
  // into Papyrus-side lists or the actor inventory.
  const auto transactionID =
      GetStripTransaction(actorID, a_stack.stackID);
  if (transactionID == 0) {
    return;
  }
  static_cast<void>(BeginTicketForMask(
      a_operation.actor, nullptr, a_operation.requestedMask,
      TargetName(a_operation.target), transactionID,
      a_operation.manualGeneration,
      ShouldQueueVirtualStripRefresh(transactionID)));
  logger::debug("Shadow strip intent actor={:08X} mask={:08X} "
                "transaction={} stack={}",
                actorID, a_operation.requestedMask, transactionID,
                a_stack.stackID);
}

void HandleEquippedArrayResult(const HookOperation &a_operation,
                               RE::BSScript::Stack &a_stack) {
  const auto actorID = ActorID(a_operation.actor);
  if (actorID == 0 || GetRegisteredMask(actorID) == 0 ||
      !a_stack.returnValue.IsArray()) {
    return;
  }

  auto forms =
      a_stack.returnValue.Unpack<std::vector<RE::TESForm *>>();
  std::unordered_set<RE::FormID> returnedFormIDs;
  std::uint32_t realWornMask = 0;
  for (auto *form : forms) {
    if (!form) {
      continue;
    }
    returnedFormIDs.insert(form->GetFormID());
    auto *armor = form->As<RE::TESObjectARMO>();
    if (!armor) {
      continue;
    }
    const auto armorMask = static_cast<std::uint32_t>(
        sfs::armor::GetArmorDisplaySlotMask(armor));
    realWornMask |= armorMask;
    // Treat the returned equipped array as the same causal evidence as a
    // sequence of GetWornForm results.  A later mutation of one of these
    // exact forms can trust the caller without trusting a read-only array
    // consumer.
    RecordWornQuery(a_stack.stackID, actorID, a_operation.callerChain,
                    armorMask, form->GetFormID());
  }
  RecordEquippedArrayCatalog(a_stack.stackID, actorID,
                             a_operation.callerChain);

  if (!IsCallerTrustedForTokenExposure(a_operation.callerChain,
                                       a_stack.stackID)) {
    logger::debug("Observed untrusted equipped array actor={:08X} forms={} "
                  "stack={}",
                  actorID, forms.size(), a_stack.stackID);
    return;
  }

  const auto registeredMask = GetRegisteredMask(actorID);
  const auto suppressedMask =
      sfs::native::GetSuppressedFittingSlotMask(a_operation.actor) |
      sfs::devious_devices::GetDeviousDevicesHiderSuppressedFittingSlotMask(
          a_operation.actor);
  const auto availableMask = registeredMask & ~realWornMask & ~suppressedMask;
  std::size_t appended = 0;
  for (std::uint32_t slot = kFirstSlot; slot <= kLastSlot; ++slot) {
    const auto mask = SlotMask(slot);
    if ((availableMask & mask) == 0) {
      continue;
    }
    auto *token = g_tokens[TokenIndexForSlot(slot)];
    if (!token || returnedFormIDs.contains(token->GetFormID())) {
      continue;
    }
    forms.push_back(token);
    returnedFormIDs.insert(token->GetFormID());
    RecordWornQuery(a_stack.stackID, actorID, a_operation.callerChain, mask,
                    token->GetFormID());
    ++appended;
  }
  if (appended == 0) {
    return;
  }

  a_stack.returnValue.Pack(forms);
  logger::info("Trusted equipped array augmented actor={:08X} tokens={} "
               "realMask={:08X} availableMask={:08X} stack={}",
               actorID, appended, realWornMask, availableMask,
               a_stack.stackID);
}

enum class SosTngClassificationKeyword : std::uint8_t {
  kOther,
  kReveal,
  kConceal,
  kUnderwear,
};

[[nodiscard]] SosTngClassificationKeyword
ClassifySosTngClassificationKeyword(const std::string_view a_editorID) {
  if (a_editorID == "SOS_Revealing" || a_editorID == "TNG_Revealing" ||
      a_editorID == "TNG_RevealingOnlyWomen" ||
      a_editorID == "TNG_RevealingOnlyMen") {
    return SosTngClassificationKeyword::kReveal;
  }
  if (a_editorID == "SOS_Concealing" || a_editorID == "TNG_Covering") {
    return SosTngClassificationKeyword::kConceal;
  }
  if (a_editorID == "SOS_Underwear" || a_editorID == "TNG_Underwear") {
    return SosTngClassificationKeyword::kUnderwear;
  }
  return SosTngClassificationKeyword::kOther;
}

void AppendKeywordByEditorID(std::vector<RE::BGSKeyword *> &a_keywords,
                             const std::string_view a_editorID) {
  auto *keyword =
      RE::TESForm::LookupByEditorID<RE::BGSKeyword>(a_editorID.data());
  if (keyword && std::ranges::find(a_keywords, keyword) == a_keywords.end()) {
    a_keywords.push_back(keyword);
  }
}

[[nodiscard]] std::vector<RE::BGSKeyword *>
BuildEffectiveSourceKeywords(RE::TESObjectARMO *a_source) {
  if (!a_source) {
    return {};
  }

  sfs::native::SynchronizeArmorClassificationKeywords(a_source);
  const auto sourceKeywords = a_source->GetKeywords();
  std::vector<RE::BGSKeyword *> effective(sourceKeywords.begin(),
                                          sourceKeywords.end());
  const auto sfsOverride =
      sfs::native::GetArmorGenitalKeywordOverride(a_source);
  if (!sfsOverride.IsActive()) {
    return effective;
  }

  // Keep the source ARMO intact, but remove every conflicting SOS/TNG
  // classification keyword from the contextual token view before applying
  // the single final SFS 32/49 decision.
  std::erase_if(effective, [](const RE::BGSKeyword *a_keyword) {
    return a_keyword &&
           ClassifySosTngClassificationKeyword(
               sfs::armor::GetEditorID(a_keyword)) !=
               SosTngClassificationKeyword::kOther;
  });

  const auto genitalEnvironment =
      sfs::native::genital_compatibility::GetEnvironment();
  if (sfsOverride.disposition ==
      sfs::native::ArmorGenitalKeywordDisposition::kReveal) {
    if (genitalEnvironment.sosInstalled) {
      AppendKeywordByEditorID(effective, "SOS_Revealing");
    }
    if (genitalEnvironment.tngInstalled) {
      AppendKeywordByEditorID(effective, "TNG_Revealing");
    }
  } else {
    if (genitalEnvironment.sosInstalled) {
      AppendKeywordByEditorID(effective, "SOS_Concealing");
    }
    if (genitalEnvironment.tngInstalled) {
      AppendKeywordByEditorID(effective, "TNG_Covering");
    }
    if (sfsOverride.underwear) {
      if (genitalEnvironment.sosInstalled) {
        AppendKeywordByEditorID(effective, "SOS_Underwear");
      }
      if (genitalEnvironment.tngInstalled) {
        AppendKeywordByEditorID(effective, "TNG_Underwear");
      }
    }
  }
  return effective;
}

[[nodiscard]] bool EqualKeywordEditorIDNoCase(
    const RE::BGSKeyword *a_keyword, const std::string_view a_editorID) {
  if (!a_keyword || a_editorID.empty()) {
    return false;
  }
  const auto sourceEditorID = sfs::armor::GetEditorID(a_keyword);
  return sourceEditorID.size() == a_editorID.size() &&
         _strnicmp(sourceEditorID.c_str(), a_editorID.data(),
                   a_editorID.size()) == 0;
}

[[nodiscard]] bool ContainsNoCase(const std::string_view a_text,
                                  const std::string_view a_part) {
  if (a_part.empty() || a_part.size() > a_text.size()) {
    return false;
  }
  for (std::size_t offset = 0; offset + a_part.size() <= a_text.size();
       ++offset) {
    if (_strnicmp(a_text.data() + offset, a_part.data(), a_part.size()) == 0) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool SourceHasKeywordString(
    RE::TESObjectARMO *a_source, const std::string_view a_editorID) {
  const auto keywords = BuildEffectiveSourceKeywords(a_source);
  return std::ranges::any_of(keywords, [&](const auto *keyword) {
    return EqualKeywordEditorIDNoCase(keyword, a_editorID);
  });
}

[[nodiscard]] bool SourceHasKeywordSubstring(
    RE::TESObjectARMO *a_source, const std::string_view a_substring) {
  const auto keywords = BuildEffectiveSourceKeywords(a_source);
  return std::ranges::any_of(keywords, [&](const auto *keyword) {
    return keyword && ContainsNoCase(sfs::armor::GetEditorID(keyword),
                                     a_substring);
  });
}

[[nodiscard]] bool SourceHasRawKeywordSubstring(
    RE::TESObjectARMO *a_source, const std::string_view a_substring) {
  if (!a_source) {
    return false;
  }
  const auto keywords = a_source->GetKeywords();
  return std::ranges::any_of(keywords, [&](const auto *keyword) {
    return keyword && ContainsNoCase(sfs::armor::GetEditorID(keyword),
                                     a_substring);
  });
}

[[nodiscard]] bool IsSexLabPPlusStripTarget(
    const TargetNative a_target) {
  return a_target == TargetNative::SexLabPPlusStripByData ||
         a_target == TargetNative::SexLabPPlusStripByDataEx ||
         a_target == TargetNative::SexLabPPlusUnequipSlots;
}

void HandleSexLabPPlusStripResult(const HookOperation &a_operation,
                                  RE::BSScript::Stack &a_stack) {
  const auto actorID = ActorID(a_operation.actor);
  if (!IsSexLabPPlusStripTarget(a_operation.target) || actorID == 0 ||
      GetRegisteredMask(actorID) == 0 ||
      !a_stack.returnValue.IsArray()) {
    return;
  }

  auto forms = a_stack.returnValue.Unpack<std::vector<RE::TESForm *>>();
  auto effectiveMask = a_operation.requestedMask;
  for (auto *form : forms) {
    if (!form || MaskForToken(form) != 0 ||
        (a_operation.target == TargetNative::SexLabPPlusStripByDataEx &&
         a_operation.pplusMergeInputForms.contains(form->GetFormID()))) {
      continue;
    }
    auto *armor = form->As<RE::TESObjectARMO>();
    if (!armor ||
        sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(
            armor)) {
      continue;
    }
    // StripByData can remove an explicit AlwaysStrip item outside its slot
    // policy. Only newly returned armor contributes this evidence;
    // StripByDataEx's pre-existing merge entries are deliberately ignored.
    effectiveMask |= static_cast<std::uint32_t>(
        sfs::armor::GetArmorDisplaySlotMask(armor));
  }

  std::unordered_map<std::uint32_t, std::vector<RegisteredAppearance>>
      appearancesByRestoreToken;
  for (const auto &appearance : GetRegisteredAppearances(actorID)) {
    auto *source = RE::TESForm::LookupByID<RE::TESObjectARMO>(
        appearance.armorID);
    const bool noStrip = SourceHasRawKeywordSubstring(source, "NoStrip");
    const bool alwaysStrip =
        !noStrip && SourceHasRawKeywordSubstring(source, "AlwaysStrip");
    if (!sfs::native::sexlab_pplus::rules::ShouldStripAppearance(
            effectiveMask, appearance.tokenSlotMask, noStrip,
            alwaysStrip)) {
      continue;
    }
    const auto restoreTokenMask =
        sfs::native::sexlab_pplus::rules::SelectRestoreTokenMask(
            effectiveMask, appearance.tokenSlotMask, alwaysStrip);
    if (restoreTokenMask != 0) {
      // A multi-slot appearance is atomic. Give it one deterministic token
      // owner so one P+ redress call restores the whole card rather than
      // requiring every overlapping slot token to succeed independently.
      appearancesByRestoreToken[restoreTokenMask].push_back(appearance);
    }
  }
  if (appearancesByRestoreToken.empty()) {
    return;
  }

  std::unordered_set<RE::FormID> returnedFormIDs;
  for (auto *form : forms) {
    if (form) {
      returnedFormIDs.insert(form->GetFormID());
    }
  }

  std::uint64_t newTransactionID = 0;
  std::size_t appended = 0;
  std::size_t linkedAppearances = 0;
  const auto sourceName = TargetName(a_operation.target);
  for (std::uint32_t slot = kFirstSlot; slot <= kLastSlot; ++slot) {
    const auto tokenMask = SlotMask(slot);
    const auto group = appearancesByRestoreToken.find(tokenMask);
    if (group == appearancesByRestoreToken.end() || group->second.empty()) {
      continue;
    }
    auto *token = g_tokens[TokenIndexForSlot(slot)];
    if (!token) {
      continue;
    }

    auto transactionID =
        GetRestorableTicketTransaction(actorID, token->GetFormID());
    if (transactionID == 0) {
      if (newTransactionID == 0) {
        newTransactionID = EnsureStripTransaction(
            a_operation.actor, a_stack.stackID, sourceName);
      }
      transactionID = newTransactionID;
    }
    if (transactionID == 0 ||
        AddTicket(actorID, token->GetFormID(), group->second, sourceName,
                  transactionID, 0,
                  ShouldQueueVirtualStripRefresh(transactionID)) == 0) {
      continue;
    }

    linkedAppearances += group->second.size();
    if (returnedFormIDs.insert(token->GetFormID()).second) {
      forms.push_back(token);
      ++appended;
    }
  }
  if (linkedAppearances == 0) {
    return;
  }
  if (appended != 0) {
    a_stack.returnValue.Pack(forms);
  }
  logger::info("SexLab P+ virtual strip linked actor={:08X} source={} "
               "requestedMask={:08X} effectiveMask={:08X} "
               "appearances={} appendedTokens={} stack={}",
               actorID, sourceName, a_operation.requestedMask, effectiveMask,
               linkedAppearances, appended, a_stack.stackID);
}

[[nodiscard]] bool MatchesKeywordFilter(
    RE::TESObjectARMO *a_source,
    const std::span<RE::BGSKeyword *const> a_keywords,
    const bool a_matchAll) {
  if (!a_source) {
    return false;
  }
  const auto effectiveKeywords = BuildEffectiveSourceKeywords(a_source);
  const auto hasKeyword = [&](const RE::BGSKeyword *a_keyword) {
    return a_keyword &&
           std::ranges::find(effectiveKeywords, a_keyword) !=
               effectiveKeywords.end();
  };
  if (a_matchAll) {
    return std::ranges::all_of(a_keywords, hasKeyword);
  }
  return std::ranges::any_of(a_keywords, hasKeyword);
}

[[nodiscard]] bool MatchesKeywordStringFilter(
    RE::TESObjectARMO *a_source,
    const std::span<const std::string> a_keywordStrings,
    const bool a_matchAll) {
  if (!a_source) {
    return false;
  }
  if (a_matchAll) {
    return std::ranges::all_of(a_keywordStrings, [&](const auto &editorID) {
      return SourceHasKeywordString(a_source, editorID);
    });
  }
  return std::ranges::any_of(a_keywordStrings, [&](const auto &editorID) {
    return SourceHasKeywordString(a_source, editorID);
  });
}

template <class MatchPredicate>
void RecordCatalogAppearanceFilter(
    const HookOperation &a_operation, const RE::VMStackID a_stackID,
    MatchPredicate &&a_matches, const bool a_slotMaskFilter) {
  if (a_stackID == 0 || a_operation.callerChain.empty()) {
    return;
  }

  RE::FormID actorID = 0;
  std::unordered_set<std::string> candidates;
  {
    std::lock_guard lock(g_runtimeMutex);
    const auto observation = g_stackObservations.find(a_stackID);
    if (observation == g_stackObservations.end() ||
        !observation->second.equippedArrayObserved ||
        observation->second.actorID == 0 ||
        !CallerChainsOverlap(observation->second.callerChain,
                             a_operation.callerChain)) {
      return;
    }
    actorID = observation->second.actorID;
    candidates = observation->second.catalogAppearanceCandidates;
  }

  const auto appearances = GetRegisteredAppearances(actorID);
  std::unordered_set<std::string> retained;
  for (const auto &appearance : appearances) {
    if (!candidates.contains(appearance.identity)) {
      continue;
    }
    auto *source = RE::TESForm::LookupByID<RE::TESObjectARMO>(
        appearance.armorID);
    if (a_matches(appearance, source)) {
      retained.insert(appearance.identity);
    }
  }

  std::size_t before = 0;
  std::size_t after = 0;
  {
    std::lock_guard lock(g_runtimeMutex);
    const auto observation = g_stackObservations.find(a_stackID);
    if (observation == g_stackObservations.end() ||
        observation->second.actorID != actorID ||
        !CallerChainsOverlap(observation->second.callerChain,
                             a_operation.callerChain)) {
      return;
    }
    auto &catalog = observation->second;
    before = catalog.catalogAppearanceCandidates.size();
    catalog.catalogAppearanceCandidates = std::move(retained);
    catalog.catalogFilterObserved = true;
    catalog.slotMaskFilterObserved |= a_slotMaskFilter;
    catalog.lastSeen = RuntimeClock::now();
    after = catalog.catalogAppearanceCandidates.size();
  }
  logger::debug("Observed provisional catalog filter stack={} actor={:08X} "
                "kind={} candidates={} -> {}",
                a_stackID, actorID,
                a_slotMaskFilter ? "slot-mask" : "keyword", before, after);
}

void RecordCatalogKeywordFilter(const HookOperation &a_operation,
                                const RE::VMStackID a_stackID) {
  if (a_operation.target == TargetNative::FilterFormsByKeywordString) {
    RecordCatalogAppearanceFilter(
        a_operation, a_stackID,
        [&](const RegisteredAppearance &, RE::TESObjectARMO *a_source) {
          const bool matches = MatchesKeywordStringFilter(
              a_source, a_operation.filterKeywordStrings,
              a_operation.filterMatchAll);
          return a_operation.filterInvert != matches;
        },
        false);
  } else if (a_operation.target == TargetNative::FilterFormsByKeyword) {
    RecordCatalogAppearanceFilter(
        a_operation, a_stackID,
        [&](const RegisteredAppearance &, RE::TESObjectARMO *a_source) {
          const bool matches = MatchesKeywordFilter(
              a_source, a_operation.filterKeywords,
              a_operation.filterMatchAll);
          return a_operation.filterInvert != matches;
        },
        false);
  }
}

void RecordCatalogSlotMaskFilter(const HookOperation &a_operation,
                                 const RE::VMStackID a_stackID) {
  if (a_operation.target != TargetNative::FilterBySlotMask ||
      a_operation.requestedMask == 0) {
    return;
  }
  RecordCatalogAppearanceFilter(
      a_operation, a_stackID,
      [&](const RegisteredAppearance &a_appearance, RE::TESObjectARMO *) {
        const auto overlap =
            a_appearance.tokenSlotMask & a_operation.requestedMask;
        return a_operation.filterMatchAll
                   ? overlap == a_operation.requestedMask
                   : overlap != 0;
      },
      true);
}

template <class MatchPredicate>
void PatchContextualTokenFilterResult(const HookOperation &a_operation,
                                      RE::BSScript::Stack &a_stack,
                                      MatchPredicate &&a_matches) {
  if (!a_stack.returnValue.IsArray() || a_operation.filterForms.empty()) {
    return;
  }

  auto filtered =
      a_stack.returnValue.Unpack<std::vector<RE::TESForm *>>();
  std::unordered_set<RE::FormID> processedTokens;
  std::size_t changed = 0;
  for (auto *form : a_operation.filterForms) {
    const auto tokenMask = MaskForToken(form);
    if (!form || tokenMask == 0 ||
        !processedTokens.insert(form->GetFormID()).second) {
      continue;
    }
    auto *source = GetContextualTokenSourceArmor(
        a_stack.stackID, form, a_operation.callerChain);
    if (!source) {
      continue;
    }

    const bool keep = a_operation.filterInvert != a_matches(source);
    const auto before = filtered.size();
    std::erase(filtered, form);
    const bool wasPresent = filtered.size() != before;
    if (keep) {
      filtered.push_back(form);
    }
    changed += wasPresent != keep ? 1U : 0U;
    logger::debug("Contextual token keyword filter token={:08X} "
                  "source={:08X} keep={} invert={} stack={}",
                  form->GetFormID(), source->GetFormID(), keep,
                  a_operation.filterInvert, a_stack.stackID);
  }
  if (changed != 0) {
    a_stack.returnValue.Pack(filtered);
  }
}

void HandleContextualKeywordResult(const HookOperation &a_operation,
                                   RE::BSScript::Stack &a_stack) {
  if (a_operation.target == TargetNative::FilterFormsByKeywordString) {
    RecordCatalogKeywordFilter(a_operation, a_stack.stackID);
    PatchContextualTokenFilterResult(
        a_operation, a_stack, [&](RE::TESObjectARMO *source) {
          return MatchesKeywordStringFilter(
              source, a_operation.filterKeywordStrings,
              a_operation.filterMatchAll);
        });
    return;
  }
  if (a_operation.target == TargetNative::FilterFormsByKeyword) {
    RecordCatalogKeywordFilter(a_operation, a_stack.stackID);
    PatchContextualTokenFilterResult(
        a_operation, a_stack, [&](RE::TESObjectARMO *source) {
          return MatchesKeywordFilter(source, a_operation.filterKeywords,
                                      a_operation.filterMatchAll);
        });
    return;
  }

  auto *source = GetContextualTokenSourceArmor(
      a_stack.stackID, a_operation.item, a_operation.callerChain);
  if (!source) {
    return;
  }

  if (a_operation.target == TargetNative::HasKeywordSubstring) {
    a_stack.returnValue.SetBool(SourceHasKeywordSubstring(
        source, a_operation.keywordSubstring));
    logger::debug("Contextual token keyword substring query token={:08X} "
                  "source={:08X} substring='{}' stack={}",
                  a_operation.item ? a_operation.item->GetFormID() : 0,
                  source->GetFormID(), a_operation.keywordSubstring,
                  a_stack.stackID);
    return;
  }

  const auto effectiveKeywords = BuildEffectiveSourceKeywords(source);

  switch (a_operation.target) {
  case TargetNative::FormHasKeyword:
    a_stack.returnValue.SetBool(
        a_operation.keyword &&
        std::ranges::find(effectiveKeywords, a_operation.keyword) !=
            effectiveKeywords.end());
    break;
  case TargetNative::FormGetKeywords: {
    a_stack.returnValue.Pack(effectiveKeywords);
    break;
  }
  case TargetNative::FormGetNumKeywords:
    a_stack.returnValue.SetSInt(
        static_cast<std::int32_t>(effectiveKeywords.size()));
    break;
  case TargetNative::FormGetNthKeyword:
    if (a_operation.keywordIndex >= 0 &&
        static_cast<std::uint32_t>(a_operation.keywordIndex) <
            effectiveKeywords.size()) {
      auto *keyword = effectiveKeywords[a_operation.keywordIndex];
      a_stack.returnValue.Pack<RE::BGSKeyword *>(std::move(keyword));
    } else {
      a_stack.returnValue.SetNone();
    }
    break;
  default: return;
  }

  logger::debug("Contextual token keyword query target={} token={:08X} "
                "source={:08X} stack={}",
                TargetName(a_operation.target),
                a_operation.item ? a_operation.item->GetFormID() : 0,
                source->GetFormID(), a_stack.stackID);
}

struct ContextualFormArgumentSubstitution {
  RE::BSScript::Variable *argument{nullptr};
  RE::BSScript::Variable original;
  RE::FormID tokenID{0};
  RE::FormID sourceID{0};
};

[[nodiscard]] ContextualFormArgumentSubstitution
SubstituteContextualTokenFormArgument(const HookOperation &a_operation,
                                      RE::BSScript::Stack &a_stack) {
  std::uint32_t argumentIndex = 0;
  if (a_operation.target == TargetNative::FormListContains) {
    argumentIndex = 2;
  } else if (a_operation.target != TargetNative::HasKeywordSubstring) {
    return {};
  }

  auto *source = GetContextualTokenSourceArmor(
      a_stack.stackID, a_operation.item, a_operation.callerChain);
  if (!source || !a_stack.top) {
    return {};
  }
  const auto page = a_stack.top->GetPageForFrame();
  auto &argument =
      a_stack.top->GetStackFrameVariable(argumentIndex, page);
  ContextualFormArgumentSubstitution substitution{
      .argument = &argument,
      .original = argument,
      .tokenID = a_operation.item ? a_operation.item->GetFormID() : 0,
      .sourceID = source->GetFormID()};
  auto *sourceForm = static_cast<RE::TESForm *>(source);
  argument.Pack<RE::TESForm *>(std::move(sourceForm));
  return substitution;
}

void RestoreContextualTokenFormArgument(
    ContextualFormArgumentSubstitution &a_substitution,
    const HookOperation &a_operation, const RE::VMStackID a_stackID) {
  if (!a_substitution.argument) {
    return;
  }
  *a_substitution.argument = std::move(a_substitution.original);
  logger::debug("Contextual token source substitution target={} "
                "token={:08X} source={:08X} stack={}",
                TargetName(a_operation.target), a_substitution.tokenID,
                a_substitution.sourceID, a_stackID);
}

using NativeCallFn = NativeCallResult (*)(
    NativeFunctionBase *,
    const RE::BSTSmartPointer<RE::BSScript::Stack> &,
    RE::BSScript::ErrorLogger *,
    RE::BSScript::Internal::VirtualMachine *, bool);

struct NativeFunctionPatch {
  std::unique_ptr<std::uintptr_t[]> clonedVtableStorage;
  NativeCallFn originalCall{nullptr};
  std::uintptr_t originalVtable{0};
};

std::shared_mutex g_nativeFunctionPatchMutex;
std::unordered_map<NativeFunctionBase *, NativeFunctionPatch>
    g_nativeFunctionPatches;

NativeCallResult CallOriginalNative(
    NativeFunctionBase *a_function,
    const RE::BSTSmartPointer<RE::BSScript::Stack> &a_stack,
    RE::BSScript::ErrorLogger *a_logger,
    RE::BSScript::Internal::VirtualMachine *a_vm, const bool a_arg4) {
  NativeCallFn original = nullptr;
  {
    std::shared_lock lock(g_nativeFunctionPatchMutex);
    const auto patch = g_nativeFunctionPatches.find(a_function);
    if (patch != g_nativeFunctionPatches.end()) {
      original = patch->second.originalCall;
    }
  }
  if (!original) {
    // A patched object must always have its original call target recorded
    // before its vptr is exchanged. Abort one Papyrus call instead of ever
    // attempting an unverified execution address.
    logger::critical(
        "Virtual token selective native hook lost its original call target "
        "function={:X}",
        reinterpret_cast<std::uintptr_t>(a_function));
    return NativeCallResult::kFailedAbort;
  }
  return original(a_function, a_stack, a_logger, a_vm, a_arg4);
}

struct NativeDispatchHook {
  static NativeCallResult thunk(
      NativeFunctionBase *a_function,
      const RE::BSTSmartPointer<RE::BSScript::Stack> &a_stack,
      RE::BSScript::ErrorLogger *a_logger,
      RE::BSScript::Internal::VirtualMachine *a_vm, const bool a_arg4) {
    const auto target = ClassifyNative(a_function);
    if (target == TargetNative::None || !a_stack || !a_stack->top) {
      return CallOriginalNative(a_function, a_stack, a_logger, a_vm, a_arg4);
    }
    const bool displayedBodyKeywordQuery =
        target == TargetNative::WornHasKeyword;
    // The displayed-body compatibility answer is independent of the selected
    // strip-link mode. Token transactions and expanded-slot suppression stay
    // ModSettingsSlots-only, but external naked/body checks must see the
    // final rendered state in every mode.
    if (!IsModSettingsStripLinkActive() && !displayedBodyKeywordQuery) {
      return CallOriginalNative(a_function, a_stack, a_logger, a_vm, a_arg4);
    }

    auto operation = PrepareOperation(target, a_stack.get());
    if (HandleVirtualTokenOperation(operation, *a_stack)) {
      return NativeCallResult::kCompleted;
    }

    PrepareRealMutation(operation, *a_stack);
    auto formSubstitution =
        SubstituteContextualTokenFormArgument(operation, *a_stack);
    const auto result =
        CallOriginalNative(a_function, a_stack, a_logger, a_vm, a_arg4);
    RestoreContextualTokenFormArgument(formSubstitution, operation,
                                       a_stack->stackID);
    if (result == NativeCallResult::kFailedAbort ||
        result == NativeCallResult::kFailedRetry) {
      CancelFailedOperation(operation);
      return result;
    }
    FinalizeSuccessfulOperation(operation, *a_stack);
    if (target == TargetNative::GetWornForm) {
      HandleGetWornResult(operation, *a_stack);
    } else if (target == TargetNative::GetEquippedArmorInSlot) {
      HandleGetEquippedArmorInSlotResult(operation, *a_stack);
    } else if (target == TargetNative::WornHasKeyword) {
      ApplyDisplayedBodyKeywordCompatibility(operation, *a_stack);
    } else if (target == TargetNative::AddAllEquippedItemsToArray) {
      HandleEquippedArrayResult(operation, *a_stack);
    } else if (IsSexLabPPlusStripTarget(target)) {
      HandleSexLabPPlusStripResult(operation, *a_stack);
    } else if (target == TargetNative::FormHasKeyword ||
               target == TargetNative::FormGetKeywords ||
               target == TargetNative::FormGetNumKeywords ||
               target == TargetNative::FormGetNthKeyword ||
               target == TargetNative::FilterFormsByKeywordString ||
               target == TargetNative::FilterFormsByKeyword ||
               target == TargetNative::HasKeywordSubstring) {
      HandleContextualKeywordResult(operation, *a_stack);
    } else if (target == TargetNative::FilterBySlotMask) {
      RecordCatalogSlotMaskFilter(operation, a_stack->stackID);
    } else if (target == TargetNative::DeviousDevicesSyncSetting) {
      static_cast<void>(sfs::devious_devices::UpdateDeviousDevicesHiderSettings(
          operation.ddSlotMaskFilters, operation.ddHiderSetting));
    }
    return result;
  }

};

[[nodiscard]] bool PatchSelectedNativeFunction(
    RE::BSScript::IFunction *a_function) {
  if (!a_function || !a_function->GetIsNative()) {
    return false;
  }
  auto *native = static_cast<NativeFunctionBase *>(a_function);
  const auto target = ClassifyNative(native);
  if (target == TargetNative::None) {
    return false;
  }

  std::unique_lock lock(g_nativeFunctionPatchMutex);
  if (g_nativeFunctionPatches.contains(native)) {
    return false;
  }

  auto **vptrSlot = reinterpret_cast<std::uintptr_t **>(native);
  auto *originalVtable = vptrSlot ? *vptrSlot : nullptr;
  if (!originalVtable) {
    logger::error("Could not selectively hook {}.{}: missing native vtable",
                  native->GetObjectTypeName().c_str(),
                  native->GetName().c_str());
    return false;
  }
  const auto originalCallAddress =
      originalVtable[sfs::runtime::kPapyrusNativeCallVtableIndex];
  const auto replacementAddress =
      reinterpret_cast<std::uintptr_t>(NativeDispatchHook::thunk);
  if (originalCallAddress == 0 || originalCallAddress == replacementAddress) {
    logger::error(
        "Could not selectively hook {}.{}: invalid original call {:X}",
        native->GetObjectTypeName().c_str(), native->GetName().c_str(),
        originalCallAddress);
    return false;
  }

  // Give only this selected native function object a private vtable. Copy the
  // MSVC complete-object locator at [-1] as well so RTTI remains valid. This
  // keeps unrelated natives such as UI.IsMenuOpen entirely outside SFS.
  auto storage = std::make_unique<std::uintptr_t[]>(
      sfs::runtime::kPapyrusNativeFunctionVtableEntryCount + 1);
  storage[0] = originalVtable[-1];
  std::copy_n(originalVtable,
              sfs::runtime::kPapyrusNativeFunctionVtableEntryCount,
              storage.get() + 1);
  auto *replacementVtable = storage.get() + 1;
  replacementVtable[sfs::runtime::kPapyrusNativeCallVtableIndex] =
      replacementAddress;

  auto [patch, inserted] = g_nativeFunctionPatches.emplace(
      native,
      NativeFunctionPatch{
          .clonedVtableStorage = std::move(storage),
          .originalCall = reinterpret_cast<NativeCallFn>(originalCallAddress),
          .originalVtable =
              reinterpret_cast<std::uintptr_t>(originalVtable)});
  if (!inserted) {
    return false;
  }
  replacementVtable = patch->second.clonedVtableStorage.get() + 1;
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(vptrSlot),
                       std::addressof(replacementVtable),
                       sizeof(replacementVtable),
                       std::addressof(originalVtable),
                       sizeof(originalVtable))) {
    g_nativeFunctionPatches.erase(patch);
    logger::error("Could not selectively hook {}.{}: vptr changed",
                  native->GetObjectTypeName().c_str(),
                  native->GetName().c_str());
    return false;
  }
  logger::info("Selectively hooked virtual token native {}.{} ({})",
               native->GetObjectTypeName().c_str(),
               native->GetName().c_str(), TargetName(target));
  return true;
}

void PatchSelectedNativesInType(RE::BSScript::ObjectTypeInfo *a_type,
                                std::size_t &a_patchedCount,
                                const bool a_globalsOnly = false) {
  // A type returned by the loader can report linked before its state/member
  // arrays have finished being published.  Never walk an unlinked type, and
  // let the post-load observer inspect only the stable global-native table.
  // Engine types are process-lifetime residents and are scanned in full once
  // during SFS registration below.
  if (!a_type || !a_type->IsLinked()) {
    return;
  }
  const auto patch = [&](RE::BSScript::IFunction *a_function) {
    if (PatchSelectedNativeFunction(a_function)) {
      ++a_patchedCount;
    }
  };
  if (auto *functions = a_type->GetGlobalFuncIter()) {
    for (std::uint32_t index = 0; index < a_type->GetNumGlobalFuncs();
         ++index) {
      patch(functions[index].func.get());
    }
  }
  if (a_globalsOnly) {
    return;
  }
  if (auto *functions = a_type->GetMemberFuncIter()) {
    for (std::uint32_t index = 0; index < a_type->GetNumMemberFuncs();
         ++index) {
      patch(functions[index].func.get());
    }
  }
  if (auto *states = a_type->GetNamedStateIter()) {
    for (std::uint32_t stateIndex = 0;
         stateIndex < a_type->GetNumNamedStates(); ++stateIndex) {
      auto &state = states[stateIndex];
      auto *functions = state.GetFuncIter();
      for (std::uint32_t functionIndex = 0;
           functions && functionIndex < state.GetNumFuncs();
           ++functionIndex) {
        patch(functions[functionIndex].func.get());
      }
    }
  }
}

[[nodiscard]] std::size_t InspectFullyLinkedSexLabPPlusAliasMembers(
    RE::BSScript::ObjectTypeInfo *a_type) {
  if (!a_type || !a_type->IsLinked()) {
    return 0;
  }

  std::size_t discovered = 0;
  std::size_t patched = 0;
  if (auto *functions = a_type->GetMemberFuncIter()) {
    for (std::uint32_t index = 0; index < a_type->GetNumMemberFuncs();
         ++index) {
      auto *function = functions[index].func.get();
      if (!function || !function->GetIsNative()) {
        continue;
      }
      const auto target =
          ClassifyNative(static_cast<NativeFunctionBase *>(function));
      if (target != TargetNative::SexLabPPlusStripByData &&
          target != TargetNative::SexLabPPlusStripByDataEx) {
        continue;
      }
      ++discovered;
      if (PatchSelectedNativeFunction(function)) {
        ++patched;
      }
    }
  }
  logger::info("SexLab P+ post-link virtual-token observer scan "
               "type=sslActorAlias discovered={} newlyHooked={}",
               discovered, patched);
  return discovered;
}

void QueuePostLinkTypeInspection(
    RE::BSScript::IVirtualMachine *a_vm, const std::string_view a_className,
    const std::uint8_t a_attempt = 0) {
  if (!a_vm || a_className.empty()) {
    return;
  }

  bool startWorker = false;
  {
    std::lock_guard lock(g_scriptTypeInspectionMutex);
    auto [entry, inserted] = g_pendingScriptTypeInspections.try_emplace(
        std::string(a_className), a_attempt);
    if (!inserted) {
      entry->second = (std::min)(entry->second, a_attempt);
    }
    if (!g_scriptTypeInspectionWorkerScheduled) {
      g_scriptTypeInspectionWorkerScheduled = true;
      startWorker = true;
    }
  }
  if (!startWorker) {
    return;
  }

  // GetScriptObjectType1 runs inside the Papyrus loader.  Accessing the type's
  // function/state arrays from that call caused the PoC13 startup CTD while
  // FormArray.pex was still being linked.  The loader hook now records only a
  // class name; all inspection happens after a short post-link barrier on the
  // game task queue.
  std::thread([a_vm]() {
    std::this_thread::sleep_for(kScriptTypeInspectionDelay);
    auto *task = SKSE::GetTaskInterface();
    if (!task) {
      std::lock_guard lock(g_scriptTypeInspectionMutex);
      g_pendingScriptTypeInspections.clear();
      g_scriptTypeInspectionWorkerScheduled = false;
      return;
    }
    task->AddTask([a_vm]() {
      std::unordered_map<std::string, std::uint8_t> pending;
      {
        std::lock_guard lock(g_scriptTypeInspectionMutex);
        pending.swap(g_pendingScriptTypeInspections);
        g_scriptTypeInspectionWorkerScheduled = false;
      }

      for (const auto &[className, attempt] : pending) {
        RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
        if (!a_vm->GetScriptObjectTypeNoLoad(RE::BSFixedString(className),
                                             type) ||
            !type || !type->IsLinked()) {
          if (attempt + 1 < kScriptTypeInspectionMaxAttempts) {
            QueuePostLinkTypeInspection(a_vm, className, attempt + 1);
          } else {
            logger::debug("Skipped generic Papyrus type '{}' after {} "
                          "post-link inspection attempts",
                          className, kScriptTypeInspectionMaxAttempts);
          }
          continue;
        }

        std::size_t patchedCount = 0;
        PatchSelectedNativesInType(type.get(), patchedCount, true);
        if (patchedCount != 0) {
          logger::info("Generic post-link Papyrus observer attached {} "
                       "equipment native(s) from '{}'",
                       patchedCount, className);
        }
        if (sfs::native::papyrus_observer::rules::
                ShouldInspectPostLinkMembers(className)) {
          // The generic safety boundary remains unchanged: every other type
          // is globals-only here. P+ declares its real strip entry points as
          // sslActorAlias empty-state member natives, so revisit only that
          // exact, now fully linked member table. Both observers are
          // independently idempotent.
          const auto discovered =
              InspectFullyLinkedSexLabPPlusAliasMembers(type.get());
          sfs::native::external_equipment::
              InspectFullyLinkedSexLabPPlusAlias(type.get());
          if (sfs::native::papyrus_observer::rules::
                  ShouldRetryPostLinkMemberInspection(
                      className, discovered, attempt,
                      kScriptTypeInspectionMaxAttempts)) {
            QueuePostLinkTypeInspection(a_vm, className, attempt + 1);
          }
        }
      }
    });
  }).detach();
}

struct ScriptTypeLoadHook {
  using Fn = bool (*)(
      RE::BSScript::IVirtualMachine *, const RE::BSFixedString &,
      RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> &);

  static bool thunk(
      RE::BSScript::IVirtualMachine *a_vm,
      const RE::BSFixedString &a_className,
      RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> &a_type) {
    const auto original = func.load(std::memory_order_acquire);
    if (!original) {
      return false;
    }
    const bool loaded = original(a_vm, a_className, a_type);
    if (loaded) {
      // The loader can return a linked type before its member/state arrays are
      // safe to traverse. Global native functions are already published at
      // this point, though, and must be patched synchronously so the script's
      // first equipment call cannot run ahead of the observer. Keep the
      // delayed inspection below as a post-link fallback for types whose
      // global table is not ready yet.
      std::size_t patchedCount = 0;
      PatchSelectedNativesInType(a_type.get(), patchedCount, true);
      if (patchedCount != 0) {
        logger::info("Generic immediate Papyrus observer attached {} "
                     "equipment native(s) from '{}'",
                     patchedCount, a_className.c_str());
      }
      QueuePostLinkTypeInspection(a_vm, a_className.c_str());
    }
    return loaded;
  }

  static inline std::atomic<Fn> func{nullptr};
};

[[nodiscard]] bool InstallScriptTypeLoadHook(
    RE::BSScript::IVirtualMachine *a_vm) {
  if (g_scriptTypeLoadHookInstalled.load(std::memory_order_acquire)) {
    return true;
  }
  auto *vtable = a_vm ? *reinterpret_cast<std::uintptr_t **>(a_vm) : nullptr;
  if (!vtable) {
    return false;
  }
  auto *slot = std::addressof(
      vtable[sfs::runtime::kPapyrusGetScriptObjectTypeVtableIndex]);
  const auto originalAddress = *slot;
  const auto replacementAddress =
      reinterpret_cast<std::uintptr_t>(ScriptTypeLoadHook::thunk);
  if (originalAddress == 0) {
    return false;
  }
  if (originalAddress == replacementAddress) {
    g_scriptTypeLoadHookInstalled.store(true, std::memory_order_release);
    return true;
  }
  ScriptTypeLoadHook::func.store(
      reinterpret_cast<ScriptTypeLoadHook::Fn>(originalAddress),
      std::memory_order_release);
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(slot),
                       std::addressof(replacementAddress),
                       sizeof(replacementAddress),
                       std::addressof(originalAddress),
                       sizeof(originalAddress))) {
    ScriptTypeLoadHook::func.store(nullptr, std::memory_order_release);
    return false;
  }
  g_scriptTypeLoadHookInstalled.store(true, std::memory_order_release);
  return true;
}

struct NativeRegistrationHook {
  using Fn = bool (*)(RE::BSScript::IVirtualMachine *,
                      RE::BSScript::IFunction *);

  static bool thunk(RE::BSScript::IVirtualMachine *a_vm,
                    RE::BSScript::IFunction *a_function) {
    const auto original = func.load(std::memory_order_acquire);
    if (!original) {
      return false;
    }
    const bool registered = original(a_vm, a_function);
    if (registered) {
      static_cast<void>(PatchSelectedNativeFunction(a_function));
    }
    return registered;
  }

  static inline std::atomic<Fn> func{nullptr};
};

[[nodiscard]] bool InstallNativeRegistrationHook(
    RE::BSScript::IVirtualMachine *a_vm) {
  if (g_nativeRegistrationHookInstalled.load(std::memory_order_acquire)) {
    return true;
  }
  auto *vtable = a_vm ? *reinterpret_cast<std::uintptr_t **>(a_vm) : nullptr;
  if (!vtable) {
    return false;
  }
  auto *slot = std::addressof(
      vtable[sfs::runtime::kPapyrusBindNativeMethodVtableIndex]);
  const auto originalAddress = *slot;
  const auto replacementAddress =
      reinterpret_cast<std::uintptr_t>(NativeRegistrationHook::thunk);
  if (originalAddress == 0) {
    return false;
  }
  if (originalAddress == replacementAddress) {
    g_nativeRegistrationHookInstalled.store(true,
                                            std::memory_order_release);
    return true;
  }
  NativeRegistrationHook::func.store(
      reinterpret_cast<NativeRegistrationHook::Fn>(originalAddress),
      std::memory_order_release);
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(slot),
                       std::addressof(replacementAddress),
                       sizeof(replacementAddress),
                       std::addressof(originalAddress),
                       sizeof(originalAddress))) {
    NativeRegistrationHook::func.store(nullptr, std::memory_order_release);
    return false;
  }
  g_nativeRegistrationHookInstalled.store(true, std::memory_order_release);
  return true;
}

[[nodiscard]] bool InstallNativeDispatchHook(
    RE::BSScript::IVirtualMachine *a_vm) {
  if (!a_vm) {
    return false;
  }
  if (g_dispatchHookInstalled.load(std::memory_order_acquire)) {
    return true;
  }
  if (!InstallScriptTypeLoadHook(a_vm)) {
    logger::error("Virtual tokens unavailable: generic Papyrus type-load "
                  "observer could not be installed");
    return false;
  }
  if (!InstallNativeRegistrationHook(a_vm)) {
    logger::error("Virtual tokens unavailable: selective Papyrus native "
                  "registration hook could not be installed");
    return false;
  }

  // Engine types are already resident when SKSE asks SFS to register. Every
  // other Papyrus type is inspected lazily by ScriptTypeLoadHook when the
  // game or a newly installed mod naturally loads it; no mod or PEX name is
  // enumerated here.
  constexpr std::array<std::string_view, 5> targetTypes{
      "Actor", "Form", "ObjectReference", "sslActorAlias",
      "sslActorLibrary"};
  std::size_t patchedCount = 0;
  for (const auto typeName : targetTypes) {
    RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
    if (a_vm->GetScriptObjectTypeNoLoad(RE::BSFixedString(typeName), type)) {
      PatchSelectedNativesInType(type.get(), patchedCount);
    }
  }
  if (patchedCount == 0) {
    logger::error("Virtual tokens unavailable: no selected Papyrus native "
                  "function objects were available");
    return false;
  }
  g_dispatchHookInstalled.store(true, std::memory_order_release);
  logger::warn("Virtual-token selective native observer installed for {} "
               "engine function object(s); future script types are inspected "
               "generically and unrelated Papyrus natives are untouched",
               patchedCount);
  return true;
}
} // namespace

namespace sfs::virtual_tokens {
void ObserveActorContextWardrobeBoundary(RE::Actor *a_actor) {
  ::ObserveActorContextWardrobeBoundary(a_actor);
}

void RecordActorContextWardrobeSnapshot(RE::Actor *a_actor) {
  ::RecordActorContextWardrobeSnapshot(a_actor);
}

void UpdateVirtualWornTokenCache() {
  // The appearance catalog also gates the actor-local context-wardrobe
  // fallback. Keep it populated for Vanilla and Direct+Vanilla modes even
  // though those policies project the boundary onto actual anchors instead of
  // creating a virtual-token ticket.
  if (!IsContextWardrobeStripLinkActive()) {
    std::lock_guard lock(g_cacheMutex);
    g_registeredAppearances.clear();
    g_registeredMasks.clear();
    return;
  }

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu) {
    return;
  }
  auto workbenchStateLock = menu->GetWorkbench().AcquireStateLock();

  std::unordered_map<RE::FormID, std::vector<RegisteredAppearance>> rebuilt;
  std::unordered_map<RE::FormID, std::uint32_t> rebuiltMasks;
  const auto *player = RE::PlayerCharacter::GetSingleton();
  const auto playerID = player ? player->GetFormID() : RE::FormID{0};
  const auto protectedMask = static_cast<std::uint32_t>(
      sfs::workbench::GetEffectiveAppearanceProtectedSlotMask());

  std::unordered_set<RE::FormID> actorIDs;
  if (playerID != 0) {
    actorIDs.insert(playerID);
  }
  for (const auto &row : menu->GetWorkbench().GetRows()) {
    const auto actorID = row.ownerActorFormID != 0 ? row.ownerActorFormID
                                                   : playerID;
    if (actorID != 0) {
      actorIDs.insert(actorID);
    }
  }

  for (const auto actorID : actorIDs) {
    auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
    if (!actor) {
      continue;
    }
    std::unordered_set<std::uint64_t> seenSources;
    for (std::uint32_t slot = 30; slot <= 61; ++slot) {
      const auto queryMask = SlotMask(slot);
      const auto active = sfs::native::GetActiveFittingAppearanceForSlot(
          actor, queryMask, true);
      const auto slotMask = active ? active->slotMask : 0;
      // Multi-slot appearances are atomic for protection. Do not trim only the
      // protected bits and accidentally publish the remainder as a token.
      if (!active || !active->armor || slotMask == 0 ||
          (slotMask & protectedMask) != 0 ||
          menu->GetWorkbench().IsRegisteredAppearanceLockedForActor(
              actorID, active->armor->GetFormID(), slotMask) ||
          !sfs::workbench::IsExternalModStripLinkAppearanceEnabled(
              slotMask)) {
        continue;
      }
      auto tokenSlotMask = slotMask;
      if (sfs::workbench::GetExternalModStripLinkMode() ==
              sfs::workbench::ExternalModStripLinkMode::Custom &&
          sfs::workbench::GetCustomDirectStripLinkAutomaticBaseMode() ==
              sfs::workbench::ExternalModStripLinkMode::ModSettingsSlots) {
        const auto directTokenMask =
            sfs::workbench::ResolveCustomDirectStripLinkTokenSlotMask(
                slotMask);
        if (directTokenMask.has_value()) {
          tokenSlotMask = static_cast<std::uint32_t>(*directTokenMask);
        }
      }
      // Explicit "Do Not Link" targets are omitted. Every concrete direct
      // target remains in the mod-settings catalog; catalog readers retain a
      // real worn result and receive a virtual token only for an empty slot.
      if (tokenSlotMask == 0) {
        continue;
      }
      const auto sourceKey =
          (static_cast<std::uint64_t>(active->armor->GetFormID()) << 32U) |
          slotMask;
      if (!seenSources.insert(sourceKey).second) {
        continue;
      }
      rebuilt[actorID].push_back(
          {.identity = BuildAppearanceIdentity(
               actorID, active->armor->GetFormID(), slotMask),
           .actorID = actorID,
           .armorID = active->armor->GetFormID(),
           .slotMask = slotMask,
           .tokenSlotMask = tokenSlotMask});
      rebuiltMasks[actorID] |= tokenSlotMask;
    }
  }

  {
    std::lock_guard lock(g_cacheMutex);
    for (auto &[actorID, appearances] : rebuilt) {
      const auto oldIt = g_registeredAppearances.find(actorID);
      for (auto &appearance : appearances) {
        if (oldIt != g_registeredAppearances.end()) {
          const auto matching = std::ranges::find(
              oldIt->second, appearance.identity,
              &RegisteredAppearance::identity);
          if (matching != oldIt->second.end()) {
            appearance.generation = matching->generation;
          }
        }
        if (appearance.generation == 0) {
          appearance.generation = g_nextAppearanceGeneration++;
        }
      }
    }
    g_registeredAppearances = std::move(rebuilt);
    g_registeredMasks = std::move(rebuiltMasks);
  }
}

void InitializeVirtualWornTokens() {
  ResetVirtualWornTokenRuntimeState();
  auto *data = RE::TESDataHandler::GetSingleton();
  bool valid = data != nullptr;
  for (std::uint32_t slot = kFirstSlot; slot <= kLastSlot; ++slot) {
    auto *token = data ? data->LookupForm<RE::TESObjectARMO>(
                             TokenLocalFormIDForSlot(slot),
                             kTokenPlugin)
                       : nullptr;
    g_tokens[TokenIndexForSlot(slot)] = token;
    valid = valid && token != nullptr &&
            static_cast<std::uint32_t>(token->GetSlotMask().underlying()) ==
                SlotMask(slot);
  }
  g_tokensReady.store(valid, std::memory_order_release);
  UpdateVirtualWornTokenCache();
  if (valid && g_dispatchHookInstalled.load()) {
    logger::warn("Virtual Worn Tokens active: 32 non-inventory "
                 "forms loaded for slots 30-61; generic worn-form, equipped-"
                 "array, filter, and equipment-mutation observation enabled");
  } else if (!valid) {
    logger::error("Virtual tokens unavailable: {} is missing or its records "
                  "failed validation",
                  kTokenPlugin);
  }
}

bool RegisterVirtualWornTokenPapyrus(RE::BSScript::IVirtualMachine *a_vm) {
  return InstallNativeDispatchHook(a_vm);
}

void ResetVirtualWornTokenRuntimeState() {
  g_runtimeEpoch.fetch_add(1, std::memory_order_acq_rel);
  std::vector<std::pair<RE::FormID, std::uint32_t>> applied;
  {
    std::lock_guard lock(g_runtimeMutex);
    for (const auto &[actorID, mask] : g_appliedSuppressionMasks) {
      applied.emplace_back(actorID, mask);
    }
    g_trustProfiles.clear();
    g_stackObservations.clear();
    g_suppressionTickets.clear();
    g_manualAutomationOverrides.clear();
    g_pendingMutations.clear();
    g_appliedSuppressionMasks.clear();
    g_stripTransactions.clear();
    g_actorAutomationGenerations.clear();
    g_actorManualGenerations.clear();
    g_actorContextWardrobeSnapshots.clear();
    g_actorContextWardrobeOriginalWornArmor.clear();
    g_actorContextWardrobeInitializedActualArmor.clear();
    g_recoveryProbeExpires.clear();
    g_postRecoveryRefreshGenerations.clear();
    g_nextTransactionID = 1;
    g_nextPostRecoveryRefreshGeneration = 1;
  }
  for (const auto &[actorID, mask] : applied) {
    if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorID)) {
      sfs::native::SetVirtualTokenFittingSlotsSuppressed(actor, mask, false);
    }
  }
}

void SerializeVirtualWornTokenState(SKSE::SerializationInterface *a_skse) {
  if (!a_skse) {
    return;
  }
  nlohmann::json root;
  root["trust"] = nlohmann::json::array();
  root["transactions"] = nlohmann::json::array();
  root["tickets"] = nlohmann::json::array();
  root["manualOverrides"] = nlohmann::json::array();
  {
    std::lock_guard lock(g_runtimeMutex);
    for (const auto &[key, profile] : g_trustProfiles) {
      // One confirmation grants trust only for the current game session.
      // Persist only callers independently confirmed by two stack instances.
      if (profile.confirmations < kPersistentTrustThreshold) {
        continue;
      }
      root["trust"].push_back(
          {{"key", key},
           {"display", profile.display},
           {"confirmations", profile.confirmations}});
    }
    for (const auto &ticket : g_suppressionTickets) {
      nlohmann::json serializedTicket{
          {"transaction", ticket.transactionID},
          {"actor", ticket.actorID},
          {"item", ticket.restoreItemID},
          {"source", ticket.source},
          {"appearances", nlohmann::json::array()}};
      for (const auto &appearance : ticket.appearances) {
        serializedTicket["appearances"].push_back(
            {{"identity", appearance.identity},
             {"armor", appearance.armorID},
             {"slotMask", appearance.slotMask}});
      }
      root["tickets"].push_back(std::move(serializedTicket));
    }
    for (const auto &[transactionID, transaction] : g_stripTransactions) {
      const bool ownsTickets = std::ranges::any_of(
          g_suppressionTickets, [&](const SuppressionTicket &a_ticket) {
            return a_ticket.transactionID == transactionID;
          });
      if (!ownsTickets) {
        continue;
      }
      nlohmann::json serializedTransaction{
          {"id", transactionID},
          {"actor", transaction.actorID},
          {"outfit", transaction.originalOutfitID},
          {"outfitRestoreObserved",
           transaction.originalOutfitRestoreObserved},
          {"source", transaction.source},
          {"worn", nlohmann::json::array()},
          {"eventAdded", nlohmann::json::array()}};
      for (const auto &[armorID, slotMask] :
           transaction.originalWornArmor) {
        serializedTransaction["worn"].push_back(
            {{"armor", armorID}, {"slotMask", slotMask}});
      }
      for (const auto armorID : transaction.eventAddedArmor) {
        serializedTransaction["eventAdded"].push_back(armorID);
      }
      root["transactions"].push_back(
          std::move(serializedTransaction));
    }
    for (const auto &[identity, appearance] : g_manualAutomationOverrides) {
      root["manualOverrides"].push_back(
          {{"actor", appearance.actorID},
           {"identity", identity},
           {"armor", appearance.armorID},
           {"slotMask", appearance.slotMask}});
    }
  }
  const auto payload = root.dump();
  a_skse->WriteRecord(kStateRecordType, kStateRecordVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

void DeserializeVirtualWornTokenState(SKSE::SerializationInterface *a_skse) {
  if (!a_skse) {
    return;
  }
  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    return;
  }
  if (type != kStateRecordType ||
      (version != 1 && version != kStateRecordVersion)) {
    logger::warn("Ignored unexpected virtual token state record type={:X} "
                 "version={}",
                 type, version);
    if (length != 0) {
      std::vector<std::byte> ignored(length);
      static_cast<void>(a_skse->ReadRecordData(ignored.data(), length));
    }
    return;
  }
  std::string payload(length, '\0');
  if (length != 0 && !a_skse->ReadRecordData(payload.data(), length)) {
    logger::warn("Failed to read virtual token state record");
    return;
  }
  if (!IsModSettingsStripLinkActive()) {
    logger::info(
        "Discarded saved virtual-token transaction state because the active "
        "strip-link policy does not use mod-configured slots");
    return;
  }

  try {
    const auto root = nlohmann::json::parse(payload);
    std::unordered_map<std::string, TrustProfile> restoredProfiles;
    for (const auto &entry : root.value("trust", nlohmann::json::array())) {
      const auto key = entry.value("key", std::string{});
      if (key.empty()) {
        continue;
      }
      const auto confirmations = static_cast<std::uint8_t>((std::min)(
          static_cast<unsigned>(kPersistentTrustThreshold),
          entry.value("confirmations", 0U)));
      restoredProfiles[key] = {
          .display = entry.value("display", std::string{}),
          .confirmations = confirmations,
          .sessionTrusted = confirmations >= kPersistentTrustThreshold};
    }

    std::vector<SuppressionTicket> restoredTickets;
    std::unordered_map<std::uint64_t, StripTransaction>
        restoredTransactions;
    std::unordered_map<std::uint64_t, std::uint64_t>
        restoredTransactionIDs;
    std::unordered_map<std::string, AppearanceTicketRef>
        restoredManualOverrides;
    std::unordered_set<RE::FormID> affectedActors;
    std::uint64_t restoredNextTransactionID = 1;
    if (version >= 2) {
      for (const auto &entry :
           root.value("transactions", nlohmann::json::array())) {
        const auto savedTransactionID =
            entry.value("id", std::uint64_t{0});
        const auto savedActorID = entry.value("actor", RE::FormID{0});
        RE::FormID resolvedActorID = 0;
        if (savedTransactionID == 0 || savedActorID == 0 ||
            !a_skse->ResolveFormID(savedActorID, resolvedActorID)) {
          continue;
        }
        RE::FormID resolvedOutfitID = 0;
        const auto savedOutfitID =
            entry.value("outfit", RE::FormID{0});
        if (savedOutfitID != 0) {
          static_cast<void>(
              a_skse->ResolveFormID(savedOutfitID, resolvedOutfitID));
        }
        std::unordered_map<RE::FormID, std::uint32_t> originalWorn;
        for (const auto &wornEntry :
             entry.value("worn", nlohmann::json::array())) {
          const auto savedArmorID =
              wornEntry.value("armor", RE::FormID{0});
          RE::FormID resolvedArmorID = 0;
          if (savedArmorID == 0 ||
              !a_skse->ResolveFormID(savedArmorID, resolvedArmorID) ||
              !RE::TESForm::LookupByID<RE::TESObjectARMO>(
                  resolvedArmorID) ||
              sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(
                  RE::TESForm::LookupByID<RE::TESObjectARMO>(
                      resolvedArmorID))) {
            continue;
          }
          originalWorn[resolvedArmorID] |=
              wornEntry.value("slotMask", std::uint32_t{0});
        }
        std::unordered_set<RE::FormID> eventAddedArmor;
        for (const auto &savedArmor :
             entry.value("eventAdded", nlohmann::json::array())) {
          RE::FormID resolvedArmorID = 0;
          if (savedArmor.is_number_unsigned() &&
              a_skse->ResolveFormID(savedArmor.get<RE::FormID>(),
                                    resolvedArmorID) &&
              RE::TESForm::LookupByID<RE::TESObjectARMO>(resolvedArmorID) &&
              !sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(
                  RE::TESForm::LookupByID<RE::TESObjectARMO>(
                      resolvedArmorID))) {
            eventAddedArmor.insert(resolvedArmorID);
          }
        }
        const auto transactionID = restoredNextTransactionID++;
        restoredTransactionIDs.emplace(savedTransactionID, transactionID);
        restoredTransactions.emplace(
            transactionID,
            StripTransaction{
                .id = transactionID,
                .actorID = resolvedActorID,
                .stackID = 0,
                .originalOutfitID = resolvedOutfitID,
                .originalWornArmor = std::move(originalWorn),
                .eventAddedArmor = std::move(eventAddedArmor),
                .source = entry.value("source", std::string{"load"}),
                .originalOutfitRestoreObserved =
                    entry.value("outfitRestoreObserved", false)});
      }
    }
    for (const auto &entry : root.value("tickets", nlohmann::json::array())) {
      auto actorID = entry.value("actor", RE::FormID{0});
      auto itemID = entry.value("item", RE::FormID{0});
      RE::FormID resolvedActorID = 0;
      if (actorID == 0 || !a_skse->ResolveFormID(actorID, resolvedActorID)) {
        continue;
      }
      RE::FormID resolvedItemID = 0;
      if (itemID != 0 && !a_skse->ResolveFormID(itemID, resolvedItemID)) {
        continue;
      }
      if (const auto *restoreArmor =
              RE::TESForm::LookupByID<RE::TESObjectARMO>(resolvedItemID);
          restoreArmor &&
          sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(
              restoreArmor)) {
        continue;
      }
      const auto liveAppearances = GetRegisteredAppearances(resolvedActorID);
      std::uint64_t transactionID = 0;
      if (version >= 2) {
        const auto savedTransactionID =
            entry.value("transaction", std::uint64_t{0});
        const auto restored =
            restoredTransactionIDs.find(savedTransactionID);
        if (restored != restoredTransactionIDs.end()) {
          transactionID = restored->second;
        }
      }
      if (transactionID == 0) {
        transactionID = restoredNextTransactionID++;
      }
      SuppressionTicket ticket{
          .transactionID = transactionID,
          .actorID = resolvedActorID,
          .restoreItemID = resolvedItemID,
          .source = entry.value("source", std::string{"load"})};
      for (const auto &serializedAppearance :
           entry.value("appearances", nlohmann::json::array())) {
        const auto identity = RebaseAppearanceIdentity(
            serializedAppearance.value("identity", std::string{}),
            resolvedActorID);
        const auto savedSlotMask = serializedAppearance.value(
            "slotMask", std::uint32_t{0});
        auto matching = std::ranges::find(
            liveAppearances, identity, &RegisteredAppearance::identity);
        // PoC8/early PoC9 saved row-dependent identities.  Migrate them by
        // their serialized visual slots; only one active appearance can own a
        // visual slot for an actor at a time.
        if (matching == liveAppearances.end() && savedSlotMask != 0) {
          matching = std::ranges::find_if(
              liveAppearances, [&](const RegisteredAppearance &a_live) {
                return (a_live.slotMask & savedSlotMask) != 0;
              });
        }
        if (matching == liveAppearances.end()) {
          continue;
        }
        if (std::ranges::find(ticket.appearances, matching->identity,
                              &AppearanceTicketRef::identity) !=
            ticket.appearances.end()) {
          continue;
        }
        ticket.appearances.push_back(
            {.identity = matching->identity,
             .actorID = resolvedActorID,
             .armorID = matching->armorID,
             .generation = matching->generation,
             .slotMask = matching->slotMask});
      }
      if (!ticket.appearances.empty()) {
        affectedActors.insert(resolvedActorID);
        restoredTickets.push_back(std::move(ticket));
      }
    }
    for (const auto &entry :
         root.value("manualOverrides", nlohmann::json::array())) {
      const auto savedActorID = entry.value("actor", RE::FormID{0});
      const auto savedIdentity = entry.value("identity", std::string{});
      RE::FormID resolvedActorID = 0;
      if (savedActorID == 0 || savedIdentity.empty() ||
          !a_skse->ResolveFormID(savedActorID, resolvedActorID)) {
        continue;
      }
      const auto identity =
          RebaseAppearanceIdentity(savedIdentity, resolvedActorID);
      auto matching = FindRegisteredAppearance(resolvedActorID, identity);
      const auto savedSlotMask =
          entry.value("slotMask", std::uint32_t{0});
      if (!matching && savedSlotMask != 0) {
        const auto liveAppearances =
            GetRegisteredAppearancesForDisplayMask(resolvedActorID,
                                                   savedSlotMask);
        if (!liveAppearances.empty()) {
          matching = liveAppearances.front();
        }
      }
      if (matching) {
        restoredManualOverrides.emplace(
            matching->identity,
            AppearanceTicketRef{.identity = matching->identity,
                                .actorID = resolvedActorID,
                                .armorID = matching->armorID,
                                .generation = matching->generation,
                                .slotMask = matching->slotMask});
      }
    }
    std::size_t restoredProfileCount = 0;
    std::size_t restoredTicketCount = 0;
    std::size_t restoredTransactionCount = 0;
    std::erase_if(restoredTransactions, [&](const auto &a_entry) {
      return std::ranges::none_of(
          restoredTickets, [&](const SuppressionTicket &a_ticket) {
            return a_ticket.transactionID == a_entry.first;
          });
    });
    {
      std::lock_guard lock(g_runtimeMutex);
      g_trustProfiles = std::move(restoredProfiles);
      g_suppressionTickets = std::move(restoredTickets);
      g_stripTransactions = std::move(restoredTransactions);
      g_manualAutomationOverrides = std::move(restoredManualOverrides);
      g_nextTransactionID = restoredNextTransactionID;
      restoredProfileCount = g_trustProfiles.size();
      restoredTicketCount = g_suppressionTickets.size();
      restoredTransactionCount = g_stripTransactions.size();
    }
    for (const auto actorID : affectedActors) {
      ApplyOwnedMask(actorID);
      QueueSettledTransactionCheck(actorID, "load");
    }
    logger::info("Restored virtual token state trustProfiles={} tickets={} "
                 "transactions={}",
                 restoredProfileCount, restoredTicketCount,
                 restoredTransactionCount);
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse virtual token state: {}", exception.what());
  }
}

void InvalidateVirtualWornTokenAutomationForAppearance(
    const RE::FormID a_actorFormID,
    const RE::FormID a_appearanceArmorFormID,
    const std::uint32_t a_slotMask, const bool a_deleted) {
  const auto actorID = ResolveOwnerActorID(a_actorFormID);
  if (actorID == 0 || a_appearanceArmorFormID == 0) {
    return;
  }
  const auto liveAppearances =
      GetRegisteredAppearancesForDisplayMask(actorID, a_slotMask);
  const auto exactAppearance = std::ranges::find_if(
      liveAppearances, [&](const RegisteredAppearance &a_appearance) {
        return a_appearance.armorID == a_appearanceArmorFormID;
      });
  const auto identity = exactAppearance != liveAppearances.end()
                            ? exactAppearance->identity
                            : BuildAppearanceIdentity(
                                  actorID, a_appearanceArmorFormID,
                                  a_slotMask);
  sfs::devious_devices::ReleaseDeviousDevicesHiderSuppressionForAppearanceMask(
      actorID, a_slotMask);
  if (!a_deleted && a_slotMask != 0) {
    auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
    const bool ddHiderControlled =
        actor != nullptr &&
        (sfs::devious_devices::GetDeviousDevicesHiderSuppressedFittingSlotMask(
             actor) &
         a_slotMask) != 0;
    if (ddHiderControlled) {
      if (exactAppearance != liveAppearances.end()) {
        std::lock_guard lock(g_runtimeMutex);
        g_manualAutomationOverrides[identity] =
            {.identity = identity,
             .actorID = actorID,
             .armorID = exactAppearance->armorID,
             .generation = exactAppearance->generation,
             .slotMask = exactAppearance->slotMask};
      }
    }
  }
  InvalidateAppearanceTickets(actorID, identity, a_slotMask, a_deleted);
}

bool IsVirtualWornTokenAutomationBypassed(
    const RE::FormID a_actorFormID,
    const RE::FormID a_appearanceArmorFormID,
    const std::uint32_t a_slotMask) {
  if (!IsModSettingsStripLinkActive()) {
    return false;
  }
  const auto actorID = ResolveOwnerActorID(a_actorFormID);
  if (actorID == 0 || a_appearanceArmorFormID == 0 || a_slotMask == 0) {
    return false;
  }
  const auto appearances =
      GetRegisteredAppearancesForDisplayMask(actorID, a_slotMask);
  const auto liveAppearance = std::ranges::find_if(
      appearances, [&](const RegisteredAppearance &a_appearance) {
        return a_appearance.armorID == a_appearanceArmorFormID;
      });
  if (liveAppearance == appearances.end()) {
    return false;
  }
  std::lock_guard lock(g_runtimeMutex);
  const auto it =
      g_manualAutomationOverrides.find(liveAppearance->identity);
  return it != g_manualAutomationOverrides.end() &&
         it->second.actorID == actorID &&
         it->second.generation == liveAppearance->generation;
}

bool IsVirtualWornTokenAppearanceSuppressed(
    const RE::FormID a_actorFormID,
    const RE::FormID a_appearanceArmorFormID,
    const std::uint32_t a_slotMask) {
  if (!IsModSettingsStripLinkActive() ||
      !sfs::workbench::IsExternalModStripLinkAppearanceEnabled(a_slotMask)) {
    return false;
  }
  const auto actorID = ResolveOwnerActorID(a_actorFormID);
  if (actorID == 0 || a_appearanceArmorFormID == 0) {
    return false;
  }
  std::lock_guard lock(g_runtimeMutex);
  return std::ranges::any_of(
      g_suppressionTickets, [&](const SuppressionTicket &a_ticket) {
        if (a_ticket.actorID != actorID) {
          return false;
        }
        return std::ranges::any_of(
            a_ticket.appearances,
            [&](const AppearanceTicketRef &a_appearance) {
              if (a_appearance.armorID != a_appearanceArmorFormID) {
                return false;
              }
              // The exact mask is preferred, but the armor identity remains
              // authoritative while a worn-equipment row migrates to an empty
              // slot row during stripping.  Manual eye/trash actions remove
              // the owning ticket before this fallback can apply.
              return true;
            });
      });
}

bool IsVirtualWornTokenEventAddedArmor(
    const RE::FormID a_actorFormID, const RE::FormID a_armorFormID) {
  const auto actorID = ResolveOwnerActorID(a_actorFormID);
  if (actorID == 0 || a_armorFormID == 0) {
    return false;
  }
  if (const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(a_armorFormID);
      armor &&
      sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(armor)) {
    return false;
  }
  std::lock_guard lock(g_runtimeMutex);
  if (!IsModSettingsStripLinkActive()) {
    return false;
  }
  if (const auto contextAdded =
          g_actorContextWardrobeInitializedActualArmor.find(actorID);
      contextAdded !=
          g_actorContextWardrobeInitializedActualArmor.end() &&
      contextAdded->second.contains(a_armorFormID)) {
    return true;
  }
  const StripTransaction *earliestTransaction = nullptr;
  for (const auto &[transactionID, transaction] : g_stripTransactions) {
    if (transaction.actorID != actorID) {
      continue;
    }
    if (!earliestTransaction || transactionID < earliestTransaction->id) {
      earliestTransaction = &transaction;
    }
  }
  return earliestTransaction != nullptr &&
         earliestTransaction->eventAddedArmor.contains(a_armorFormID);
}

bool IsVirtualWornTokenRecoveryBurstActive(
    const RE::FormID a_actorFormID) {
  if (!IsModSettingsStripLinkActive()) {
    return false;
  }
  const auto actorID = ResolveOwnerActorID(a_actorFormID);
  if (actorID == 0) {
    return false;
  }
  std::lock_guard lock(g_runtimeMutex);
  const auto now = RuntimeClock::now();
  PruneRuntimeStateLocked(now);
  // The refresh barrier covers the complete strip transaction, not only its
  // redress probe.  Per-item DAVE/DAV/native rebuilds during the initial
  // unequip burst can swallow SGO animation-object events. The engine's own
  // equipment rebuild carries the pre-staged suppression; SFS performs at
  // most one virtual display refresh and one settled recovery refresh.
  return HasActorStripTransactionLocked(actorID) ||
         g_postRecoveryRefreshGenerations.contains(actorID);
}

void HandleVirtualWornTokenEquipEvent(RE::Actor *a_actor,
                                      RE::TESObjectARMO *a_armor,
                                      const bool a_equipped) {
  if (!IsModSettingsStripLinkActive() || !a_actor || !a_armor) {
    return;
  }
  const auto actorID = a_actor->GetFormID();
  const auto itemID = a_armor->GetFormID();
  if (sfs::devious_devices::IsDeviousDevicesEquipmentTransactionArmor(
          a_armor)) {
    bool removedStaleTicket = false;
    {
      std::lock_guard lock(g_runtimeMutex);
      g_pendingMutations.erase(MutationKey(actorID, itemID, false));
      g_pendingMutations.erase(MutationKey(actorID, itemID, true));
      for (auto &[_, transaction] : g_stripTransactions) {
        if (transaction.actorID == actorID) {
          transaction.originalWornArmor.erase(itemID);
          transaction.eventAddedArmor.erase(itemID);
        }
      }
      removedStaleTicket =
          std::erase_if(g_suppressionTickets,
                        [&](const SuppressionTicket &a_ticket) {
                          return a_ticket.actorID == actorID &&
                                 a_ticket.restoreItemID == itemID;
                        }) != 0;
    }
    if (removedStaleTicket) {
      ApplyOwnedMask(actorID, false);
      logger::info("Removed DD armor from ModSettings transaction state "
                   "actor={:08X} armor={:08X}",
                   actorID, itemID);
    }
    return;
  }
  std::optional<PendingMutation> mutation;
  bool eventAddedArmorMarked = false;
  {
    std::lock_guard lock(g_runtimeMutex);
    const auto now = RuntimeClock::now();
    PruneRuntimeStateLocked(now);
    const auto key = MutationKey(actorID, itemID, a_equipped);
    const auto it = g_pendingMutations.find(key);
    if (it != g_pendingMutations.end()) {
      mutation = std::move(it->second);
      g_pendingMutations.erase(it);
    }
    if (a_equipped && g_recoveryProbeExpires.contains(actorID)) {
      StripTransaction *earliestTransaction = nullptr;
      for (auto &[transactionID, transaction] : g_stripTransactions) {
        if (transaction.actorID != actorID ||
            transaction.originalWornArmor.contains(itemID)) {
          continue;
        }
        if (!earliestTransaction || transactionID < earliestTransaction->id) {
          earliestTransaction = &transaction;
        }
      }
      if (earliestTransaction) {
        eventAddedArmorMarked =
            earliestTransaction->eventAddedArmor.insert(itemID).second;
      }
    } else if (!a_equipped) {
      for (auto &[transactionID, transaction] : g_stripTransactions) {
        static_cast<void>(transactionID);
        if (transaction.actorID == actorID) {
          transaction.eventAddedArmor.erase(itemID);
        }
      }
    }
  }
  if (mutation && a_equipped) {
    sfs::devious_devices::ReleaseDeviousDevicesHiderSuppressionForSourceMask(
        actorID, mutation->slotMask);
    RestoreTickets(actorID, itemID, mutation->source);
  } else if (mutation) {
    static_cast<void>(BeginTicketForMask(a_actor, a_armor,
                                         mutation->slotMask,
                                         mutation->source,
                                         mutation->transactionID,
                                         mutation->manualGeneration,
                                         false));
  }
  if (mutation) {
    logger::debug("Confirmed marked equipment mutation actor={:08X} "
                  "item={:08X} equipped={} source={}",
                  actorID, itemID, a_equipped, mutation->source);
  }
  if (eventAddedArmorMarked) {
    logger::info("Event-added actual armor initialized visible actor={:08X} "
                 "armor={:08X}",
                 actorID, itemID);
  }

  // Bare TESEquipEvent notifications are ignored during ordinary gameplay.
  // For three seconds after an observed Papyrus EquipItem*/SetOutfit recovery
  // call, however, late engine outfit events may refresh the debounced scan.
  // This covers delayed base-outfit evaluation without making a long-term
  // inventory UI equip into an automatic restoration signal.
  if (a_equipped && (mutation || IsRecoveryProbeOpen(actorID))) {
    QueueSettledTransactionCheck(
        actorID, mutation ? mutation->source : "outfit-recovery-event");
  }
}
} // namespace sfs::virtual_tokens
