#include "native/ExternalEquipmentTransactions.h"

#include "ArmorUtils.h"
#include "native/ExternalEquipmentTransactionRules.h"
#include "native/HelmetToggle2Integration.h"
#include "poc/DeviousDevicesHiderPoC.h"
#include "runtime/RuntimeLayouts.h"
#include "workbench/AutomaticEquipmentVisibility.h"

#include <RE/N/NativeFunctionBase.h>
#include <RE/O/ObjectTypeInfo.h>
#include <RE/S/ScriptFunction.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
using NativeCallResult = RE::BSScript::IFunction::CallResult;
using NativeFunctionBase = RE::BSScript::NF_util::NativeFunctionBase;
using Clock = std::chrono::steady_clock;
using ArmorFormIDSet = std::unordered_set<RE::FormID>;

constexpr std::uint32_t kStateRecordType = 'ESTR';
constexpr std::uint32_t kStateRecordVersion = 3;
constexpr auto kExpectationLifetime = std::chrono::seconds(3);
constexpr auto kRecoveryProbeLifetime = std::chrono::seconds(3);

enum class TargetNative : std::uint8_t {
  None,
  EquipItem,
  UnequipItem,
  UnequipAll,
  UnequipItemSlot,
  EquipItemEx,
  UnequipItemEx,
  EquipItemByID,
  RemoveItem,
  RemoveAllItems,
  DropObject,
  SetOutfit,
  GlobalSetValue,
  ActorAddSpell,
  ActorRemoveSpell,
  SexLabPPlusStripByData,
  SexLabPPlusStripByDataEx,
  SexLabPPlusUnequipSlots
};

struct EventExpectation {
  std::uint64_t id{0};
  RE::FormID actorFormID{0};
  RE::FormID armorFormID{0}; // Zero means any armor for this actor.
  std::uint64_t slotMask{0};
  RE::VMStackID stackID{0};
  TargetNative source{TargetNative::None};
  bool equipped{false};
  std::shared_ptr<const ArmorFormIDSet> originalWornArmor;
  Clock::time_point expires{};
};

struct HookOperation {
  TargetNative target{TargetNative::None};
  RE::Actor *actor{nullptr};
  RE::TESForm *item{nullptr};
  std::shared_ptr<const ArmorFormIDSet> originalWornArmor;
  std::vector<std::uint64_t> expectationIDs;
};

std::mutex g_stateMutex;
std::unordered_map<std::uint64_t, EventExpectation> g_expectations;
std::unordered_map<RE::FormID,
                   std::unordered_map<RE::FormID, std::uint64_t>>
    g_suppressedOriginalEquipment;
std::unordered_map<RE::FormID, ArmorFormIDSet> g_originalWornEquipment;
std::unordered_map<RE::FormID, ArmorFormIDSet>
    g_eventAddedActualEquipment;
struct ContextWardrobeState {
  // Complete pre-replacement set used to distinguish replacement outfit gear
  // from an original item which returns later.
  ArmorFormIDSet originalWornArmor;
  // Only entries inserted by this fallback are released when its context ends;
  // an already-open exact Papyrus transaction keeps ownership of its entries.
  ArmorFormIDSet ownedOriginalWornArmor;
  ArmorFormIDSet suppressedOriginalArmor;
  ArmorFormIDSet eventAddedArmor;
};
std::unordered_map<RE::FormID, ContextWardrobeState>
    g_contextWardrobeStates;
std::unordered_map<RE::FormID, Clock::time_point> g_recoveryProbeExpires;
std::uint64_t g_nextExpectationID{1};

std::atomic_bool g_observerInstalled{false};
std::atomic_bool g_nativeRegistrationHookInstalled{false};
std::atomic_bool g_helmetToggleSignalObserverEnabled{false};
std::atomic<RE::BSScript::IVirtualMachine *> g_observerVm{nullptr};

[[nodiscard]] bool IsAutomaticEquipmentTransactionMode() {
  return sfs::workbench::IsActualEquipmentStripLinkPolicyActive();
}

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

[[nodiscard]] TargetNative
ClassifyNative(const NativeFunctionBase *a_function) {
  if (!a_function) {
    return TargetNative::None;
  }
  const auto *object = a_function->GetObjectTypeName().c_str();
  const auto *name = a_function->GetName().c_str();
  if (EqualNoCase(object, "Actor")) {
    if (EqualNoCase(name, "EquipItem")) return TargetNative::EquipItem;
    if (EqualNoCase(name, "UnequipItem")) return TargetNative::UnequipItem;
    if (EqualNoCase(name, "UnequipAll")) return TargetNative::UnequipAll;
    if (EqualNoCase(name, "UnequipItemSlot")) {
      return TargetNative::UnequipItemSlot;
    }
    if (EqualNoCase(name, "EquipItemEx")) return TargetNative::EquipItemEx;
    if (EqualNoCase(name, "UnequipItemEx")) {
      return TargetNative::UnequipItemEx;
    }
    if (EqualNoCase(name, "EquipItemByID")) {
      return TargetNative::EquipItemByID;
    }
    if (EqualNoCase(name, "SetOutfit")) return TargetNative::SetOutfit;
    if (g_helmetToggleSignalObserverEnabled.load(std::memory_order_acquire)) {
      if (EqualNoCase(name, "AddSpell")) return TargetNative::ActorAddSpell;
      if (EqualNoCase(name, "RemoveSpell")) {
        return TargetNative::ActorRemoveSpell;
      }
    }
  } else if (EqualNoCase(object, "ObjectReference")) {
    if (EqualNoCase(name, "RemoveItem")) return TargetNative::RemoveItem;
    if (EqualNoCase(name, "RemoveAllItems")) {
      return TargetNative::RemoveAllItems;
    }
    if (EqualNoCase(name, "DropObject")) return TargetNative::DropObject;
  } else if (EqualNoCase(object, "GlobalVariable") &&
             g_helmetToggleSignalObserverEnabled.load(
                 std::memory_order_acquire) &&
             EqualNoCase(name, "SetValue")) {
    return TargetNative::GlobalSetValue;
  } else if (EqualNoCase(object, "sslActorAlias")) {
    if (EqualNoCase(name, "StripByData") &&
        MatchesNativeSignature(
            a_function, false,
            RE::BSScript::TypeInfo::RawType::kObjectArray,
            {RE::BSScript::TypeInfo::RawType::kInt,
             RE::BSScript::TypeInfo::RawType::kIntArray,
             RE::BSScript::TypeInfo::RawType::kIntArray})) {
      return TargetNative::SexLabPPlusStripByData;
    }
    if (EqualNoCase(name, "StripByDataEx") &&
        MatchesNativeSignature(
            a_function, false,
            RE::BSScript::TypeInfo::RawType::kObjectArray,
            {RE::BSScript::TypeInfo::RawType::kInt,
             RE::BSScript::TypeInfo::RawType::kIntArray,
             RE::BSScript::TypeInfo::RawType::kIntArray,
             RE::BSScript::TypeInfo::RawType::kObjectArray})) {
      return TargetNative::SexLabPPlusStripByDataEx;
    }
  } else if (EqualNoCase(object, "sslActorLibrary") &&
             EqualNoCase(name, "UnequipSlots") &&
             MatchesNativeSignature(
                 a_function, true,
                 RE::BSScript::TypeInfo::RawType::kObjectArray,
                 {RE::BSScript::TypeInfo::RawType::kObject,
                  RE::BSScript::TypeInfo::RawType::kInt})) {
    return TargetNative::SexLabPPlusUnequipSlots;
  }
  return TargetNative::None;
}

[[nodiscard]] const char *TargetName(const TargetNative a_target) {
  switch (a_target) {
  case TargetNative::EquipItem: return "Actor.EquipItem";
  case TargetNative::UnequipItem: return "Actor.UnequipItem";
  case TargetNative::UnequipAll: return "Actor.UnequipAll";
  case TargetNative::UnequipItemSlot: return "Actor.UnequipItemSlot";
  case TargetNative::EquipItemEx: return "Actor.EquipItemEx";
  case TargetNative::UnequipItemEx: return "Actor.UnequipItemEx";
  case TargetNative::EquipItemByID: return "Actor.EquipItemByID";
  case TargetNative::RemoveItem: return "ObjectReference.RemoveItem";
  case TargetNative::RemoveAllItems:
    return "ObjectReference.RemoveAllItems";
  case TargetNative::DropObject: return "ObjectReference.DropObject";
  case TargetNative::SetOutfit: return "Actor.SetOutfit";
  case TargetNative::GlobalSetValue: return "GlobalVariable.SetValue";
  case TargetNative::ActorAddSpell: return "Actor.AddSpell";
  case TargetNative::ActorRemoveSpell: return "Actor.RemoveSpell";
  case TargetNative::SexLabPPlusStripByData:
    return "sslActorAlias.StripByData";
  case TargetNative::SexLabPPlusStripByDataEx:
    return "sslActorAlias.StripByDataEx";
  case TargetNative::SexLabPPlusUnequipSlots:
    return "sslActorLibrary.UnequipSlots";
  default: return "None";
  }
}

[[nodiscard]] bool IsHelmetToggleSignalTarget(const TargetNative a_target) {
  return a_target == TargetNative::GlobalSetValue ||
         a_target == TargetNative::ActorAddSpell ||
         a_target == TargetNative::ActorRemoveSpell;
}

template <class T>
[[nodiscard]] T ReadArgument(const RE::BSScript::StackFrame &a_frame,
                             const std::uint32_t a_index) {
  const auto page = a_frame.GetPageForFrame();
  return a_frame.GetStackFrameVariable(a_index, page).Unpack<T>();
}

[[nodiscard]] std::uint64_t ArmorControlSlotMask(
    const RE::TESObjectARMO *a_armor) {
  return sfs::workbench::GetAutomaticEquipmentControlSlotMask(a_armor);
}

[[nodiscard]] bool IsDedicatedDeviousDevicesArmor(
    const RE::TESObjectARMO *a_armor) {
  return sfs::poc::IsDeviousDevicesEquipmentTransactionArmor(a_armor);
}

[[nodiscard]] std::unordered_map<RE::FormID, std::uint64_t>
CaptureWornArmor(RE::Actor *a_actor) {
  std::unordered_map<RE::FormID, std::uint64_t> worn;
  if (!a_actor) {
    return worn;
  }
  for (std::uint32_t slot = 30; slot <= 61; ++slot) {
    const auto queryMask = sfs::armor::GetArmorSlotMask(slot);
    auto *armor = a_actor->GetWornArmor(
        static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(queryMask));
    if (!armor) {
      continue;
    }
    if (IsDedicatedDeviousDevicesArmor(armor)) {
      continue;
    }
    const auto controlMask = ArmorControlSlotMask(armor);
    worn[armor->GetFormID()] |= controlMask != 0 ? controlMask : queryMask;
  }
  return worn;
}

void EnsureOriginalWornSnapshot(HookOperation &a_operation) {
  if (!a_operation.actor || a_operation.originalWornArmor) {
    return;
  }
  ArmorFormIDSet originalWornArmor;
  for (const auto &[armorFormID, _] : CaptureWornArmor(a_operation.actor)) {
    originalWornArmor.insert(armorFormID);
  }
  a_operation.originalWornArmor =
      std::make_shared<const ArmorFormIDSet>(std::move(originalWornArmor));
}

[[nodiscard]] RE::TESObjectARMO *FindWornArmor(
    RE::Actor *a_actor, const std::uint64_t a_slotMask) {
  if (!a_actor || a_slotMask == 0) {
    return nullptr;
  }
  for (std::uint32_t slot = 30; slot <= 61; ++slot) {
    const auto queryMask = sfs::armor::GetArmorSlotMask(slot);
    if ((queryMask & a_slotMask) == 0) {
      continue;
    }
    if (auto *armor = a_actor->GetWornArmor(
            static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(queryMask))) {
      return armor;
    }
  }
  return nullptr;
}

[[nodiscard]] bool IsArmorWornByActor(
    RE::Actor *a_actor, const RE::TESObjectARMO *a_armor) {
  if (!a_actor || !a_armor) {
    return false;
  }
  const auto armorFormID = a_armor->GetFormID();
  for (std::uint32_t slot = 30; slot <= 61; ++slot) {
    const auto queryMask = sfs::armor::GetArmorSlotMask(slot);
    const auto *worn = a_actor->GetWornArmor(
        static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(queryMask));
    if (worn && worn->GetFormID() == armorFormID) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool HasSuppressedOriginalEquipment(
    const RE::FormID a_actorFormID, const RE::FormID a_armorFormID = 0) {
  std::lock_guard lock(g_stateMutex);
  const auto actorState =
      g_suppressedOriginalEquipment.find(a_actorFormID);
  if (actorState == g_suppressedOriginalEquipment.end()) {
    return false;
  }
  return a_armorFormID == 0 || actorState->second.contains(a_armorFormID);
}

void PruneRecoveryProbesLocked(const Clock::time_point a_now) {
  std::vector<RE::FormID> expiredActors;
  for (const auto &[actorFormID, expires] : g_recoveryProbeExpires) {
    if (expires <= a_now) {
      expiredActors.push_back(actorFormID);
    }
  }
  for (const auto actorFormID : expiredActors) {
    g_recoveryProbeExpires.erase(actorFormID);
    if (!g_suppressedOriginalEquipment.contains(actorFormID) &&
        !g_eventAddedActualEquipment.contains(actorFormID)) {
      g_originalWornEquipment.erase(actorFormID);
    }
  }
}

[[nodiscard]] bool HasRecoveryProbeLocked(
    const RE::FormID a_actorFormID, const Clock::time_point a_now) {
  PruneRecoveryProbesLocked(a_now);
  return g_recoveryProbeExpires.contains(a_actorFormID);
}

[[nodiscard]] bool HasOpenEquipmentTransaction(
    const RE::FormID a_actorFormID) {
  if (a_actorFormID == 0) {
    return false;
  }
  std::lock_guard lock(g_stateMutex);
  const auto now = Clock::now();
  return g_suppressedOriginalEquipment.contains(a_actorFormID) ||
         HasRecoveryProbeLocked(a_actorFormID, now);
}

void OpenRecoveryProbe(const RE::FormID a_actorFormID) {
  if (a_actorFormID == 0) {
    return;
  }
  std::lock_guard lock(g_stateMutex);
  const auto now = Clock::now();
  PruneRecoveryProbesLocked(now);
  g_recoveryProbeExpires[a_actorFormID] = now + kRecoveryProbeLifetime;
}

void PruneExpiredExpectationsLocked(const Clock::time_point a_now) {
  std::erase_if(g_expectations, [&](const auto &a_entry) {
    return a_entry.second.expires <= a_now;
  });
}

[[nodiscard]] bool HasExpectationForActorLocked(
    const RE::FormID a_actorFormID) {
  return std::ranges::any_of(g_expectations, [&](const auto &a_entry) {
    return a_entry.second.actorFormID == a_actorFormID;
  });
}

[[nodiscard]] bool EraseArmorTransactionStateLocked(
    const RE::FormID a_actorFormID, const RE::FormID a_armorFormID) {
  if (a_actorFormID == 0 || a_armorFormID == 0) {
    return false;
  }

  bool erased = false;
  if (auto actor = g_suppressedOriginalEquipment.find(a_actorFormID);
      actor != g_suppressedOriginalEquipment.end()) {
    erased |= actor->second.erase(a_armorFormID) != 0;
    if (actor->second.empty()) {
      g_suppressedOriginalEquipment.erase(actor);
    }
  }
  if (auto actor = g_eventAddedActualEquipment.find(a_actorFormID);
      actor != g_eventAddedActualEquipment.end()) {
    erased |= actor->second.erase(a_armorFormID) != 0;
    if (actor->second.empty()) {
      g_eventAddedActualEquipment.erase(actor);
    }
  }
  if (auto actor = g_originalWornEquipment.find(a_actorFormID);
      actor != g_originalWornEquipment.end()) {
    erased |= actor->second.erase(a_armorFormID) != 0;
    if (actor->second.empty()) {
      g_originalWornEquipment.erase(actor);
    }
  }
  erased |= std::erase_if(g_expectations, [&](const auto &a_entry) {
              return a_entry.second.actorFormID == a_actorFormID &&
                     a_entry.second.armorFormID == a_armorFormID;
            }) != 0;

  if (!g_suppressedOriginalEquipment.contains(a_actorFormID) &&
      !g_eventAddedActualEquipment.contains(a_actorFormID) &&
      !HasExpectationForActorLocked(a_actorFormID)) {
    g_originalWornEquipment.erase(a_actorFormID);
    g_recoveryProbeExpires.erase(a_actorFormID);
  }
  return erased;
}

void ReleaseContextWardrobeStateLocked(const RE::FormID a_actorFormID) {
  const auto context = g_contextWardrobeStates.find(a_actorFormID);
  if (context == g_contextWardrobeStates.end()) {
    return;
  }

  if (auto suppressed = g_suppressedOriginalEquipment.find(a_actorFormID);
      suppressed != g_suppressedOriginalEquipment.end()) {
    for (const auto armorFormID : context->second.suppressedOriginalArmor) {
      suppressed->second.erase(armorFormID);
    }
    if (suppressed->second.empty()) {
      g_suppressedOriginalEquipment.erase(suppressed);
    }
  }
  if (auto eventAdded = g_eventAddedActualEquipment.find(a_actorFormID);
      eventAdded != g_eventAddedActualEquipment.end()) {
    for (const auto armorFormID : context->second.eventAddedArmor) {
      eventAdded->second.erase(armorFormID);
    }
    if (eventAdded->second.empty()) {
      g_eventAddedActualEquipment.erase(eventAdded);
    }
  }
  if (auto original = g_originalWornEquipment.find(a_actorFormID);
      original != g_originalWornEquipment.end()) {
    for (const auto armorFormID : context->second.ownedOriginalWornArmor) {
      const bool stillSuppressed =
          g_suppressedOriginalEquipment.contains(a_actorFormID) &&
          g_suppressedOriginalEquipment.at(a_actorFormID).contains(armorFormID);
      const bool stillEventAdded =
          g_eventAddedActualEquipment.contains(a_actorFormID) &&
          g_eventAddedActualEquipment.at(a_actorFormID).contains(armorFormID);
      if (!stillSuppressed && !stillEventAdded) {
        original->second.erase(armorFormID);
      }
    }
    if (original->second.empty()) {
      g_originalWornEquipment.erase(original);
    }
  }
  g_contextWardrobeStates.erase(context);
}

void PruneDeviousDevicesTransactionStateLocked(
    const std::optional<RE::FormID> a_actorFormID = std::nullopt) {
  std::unordered_map<RE::FormID, ArmorFormIDSet> candidates;
  const auto collect = [&](const RE::FormID a_actorID,
                           const RE::FormID a_armorID) {
    if ((!a_actorFormID || *a_actorFormID == a_actorID) && a_actorID != 0 &&
        a_armorID != 0) {
      candidates[a_actorID].insert(a_armorID);
    }
  };
  for (const auto &[actorID, equipment] : g_suppressedOriginalEquipment) {
    for (const auto &[armorID, _] : equipment) {
      collect(actorID, armorID);
    }
  }
  for (const auto &[actorID, equipment] : g_eventAddedActualEquipment) {
    for (const auto armorID : equipment) {
      collect(actorID, armorID);
    }
  }
  for (const auto &[actorID, equipment] : g_originalWornEquipment) {
    for (const auto armorID : equipment) {
      collect(actorID, armorID);
    }
  }
  for (const auto &[_, expectation] : g_expectations) {
    collect(expectation.actorFormID, expectation.armorFormID);
  }

  for (const auto &[actorID, armorIDs] : candidates) {
    for (const auto armorID : armorIDs) {
      const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(armorID);
      if (armor && IsDedicatedDeviousDevicesArmor(armor)) {
        static_cast<void>(
            EraseArmorTransactionStateLocked(actorID, armorID));
      }
    }
  }
}

[[nodiscard]] std::uint64_t StageExpectation(const RE::FormID a_actorFormID,
                                             const RE::FormID a_armorFormID,
                                             const std::uint64_t a_slotMask,
                                             const bool a_equipped,
                                             const RE::VMStackID a_stackID,
                                             const TargetNative a_source,
                                             std::shared_ptr<
                                                 const ArmorFormIDSet>
                                                 a_originalWornArmor = {}) {
  if (a_actorFormID == 0) {
    return 0;
  }
  std::lock_guard lock(g_stateMutex);
  const auto now = Clock::now();
  PruneExpiredExpectationsLocked(now);
  const auto id = g_nextExpectationID++;
  g_expectations.emplace(
      id, EventExpectation{.id = id,
                           .actorFormID = a_actorFormID,
                           .armorFormID = a_armorFormID,
                           .slotMask = a_slotMask,
                           .stackID = a_stackID,
                           .source = a_source,
                           .equipped = a_equipped,
                           .originalWornArmor =
                               std::move(a_originalWornArmor),
                           .expires = now + kExpectationLifetime});
  return id;
}

void StageArmorExpectation(HookOperation &a_operation,
                           RE::TESObjectARMO *a_armor,
                           const bool a_equipped,
                           const RE::VMStackID a_stackID) {
  if (!a_operation.actor || !a_armor ||
      IsDedicatedDeviousDevicesArmor(a_armor)) {
    return;
  }
  // PoC18 transaction rule: an unequip candidate must be genuinely worn. An
  // equip candidate must either restore a suppressed original or occur inside
  // the short recovery window of an already confirmed external strip. This
  // admits event-only scene equipment without allowing a standalone no-op
  // Papyrus call to leave a marker for a later inventory-menu action.
  if ((!a_equipped &&
       !IsArmorWornByActor(a_operation.actor, a_armor)) ||
      (a_equipped &&
       !HasSuppressedOriginalEquipment(a_operation.actor->GetFormID(),
                                       a_armor->GetFormID()) &&
       !HasOpenEquipmentTransaction(a_operation.actor->GetFormID()))) {
    return;
  }
  const auto slotMask = ArmorControlSlotMask(a_armor);
  if (slotMask == 0) {
    return;
  }
  if (a_equipped) {
    OpenRecoveryProbe(a_operation.actor->GetFormID());
  } else {
    EnsureOriginalWornSnapshot(a_operation);
  }
  if (const auto id = StageExpectation(a_operation.actor->GetFormID(),
                                       a_armor->GetFormID(), slotMask,
                                       a_equipped,
                                       a_stackID, a_operation.target,
                                       a_operation.originalWornArmor)) {
    a_operation.expectationIDs.push_back(id);
  }
}

void StageCurrentWornUnequipExpectations(HookOperation &a_operation,
                                         const RE::VMStackID a_stackID) {
  EnsureOriginalWornSnapshot(a_operation);
  for (const auto &[armorFormID, slotMask] :
       CaptureWornArmor(a_operation.actor)) {
    if (const auto id = StageExpectation(a_operation.actor->GetFormID(),
                                         armorFormID, slotMask, false,
                                         a_stackID,
                                         a_operation.target,
                                         a_operation.originalWornArmor)) {
      a_operation.expectationIDs.push_back(id);
    }
  }
}

[[nodiscard]] HookOperation PrepareOperation(
    const TargetNative a_target, RE::BSScript::Stack *a_stack) {
  HookOperation operation{.target = a_target};
  if (!a_stack || !a_stack->top || !IsAutomaticEquipmentTransactionMode()) {
    return operation;
  }

  auto &frame = *a_stack->top;
  if (a_target == TargetNative::SexLabPPlusStripByData ||
      a_target == TargetNative::SexLabPPlusStripByDataEx) {
    auto *alias = frame.self.Unpack<RE::BGSRefAlias *>();
    operation.actor = alias ? alias->GetActorReference() : nullptr;
  } else if (a_target == TargetNative::SexLabPPlusUnequipSlots) {
    operation.actor = ReadArgument<RE::Actor *>(frame, 0);
  } else if (a_target == TargetNative::RemoveItem ||
      a_target == TargetNative::RemoveAllItems ||
      a_target == TargetNative::DropObject) {
    auto *reference = frame.self.Unpack<RE::TESObjectREFR *>();
    operation.actor = reference ? reference->As<RE::Actor>() : nullptr;
  } else {
    operation.actor = frame.self.Unpack<RE::Actor *>();
  }
  if (!operation.actor) {
    return operation;
  }

  switch (a_target) {
  case TargetNative::EquipItem:
  case TargetNative::UnequipItem:
  case TargetNative::EquipItemEx:
  case TargetNative::UnequipItemEx:
  case TargetNative::EquipItemByID:
  case TargetNative::RemoveItem:
  case TargetNative::DropObject:
    operation.item = ReadArgument<RE::TESForm *>(frame, 0);
    break;
  default: break;
  }

  const auto stageItem = [&](const bool a_equipped) {
    auto *armor = operation.item
                      ? operation.item->As<RE::TESObjectARMO>()
                      : nullptr;
    StageArmorExpectation(operation, armor, a_equipped, a_stack->stackID);
  };
  switch (a_target) {
  case TargetNative::EquipItem:
  case TargetNative::EquipItemEx:
  case TargetNative::EquipItemByID:
    stageItem(true);
    break;
  case TargetNative::UnequipItem:
  case TargetNative::UnequipItemEx:
  case TargetNative::RemoveItem:
  case TargetNative::DropObject:
    stageItem(false);
    break;
  case TargetNative::UnequipItemSlot: {
    const auto slot =
        static_cast<std::uint32_t>(ReadArgument<std::int32_t>(frame, 0));
    StageArmorExpectation(operation,
                          FindWornArmor(operation.actor,
                                        sfs::armor::GetArmorSlotMask(slot)),
                          false, a_stack->stackID);
    break;
  }
  case TargetNative::UnequipAll:
  case TargetNative::RemoveAllItems:
  case TargetNative::SexLabPPlusStripByData:
  case TargetNative::SexLabPPlusStripByDataEx:
  case TargetNative::SexLabPPlusUnequipSlots:
    // P+ performs its inventory loop and ActorEquipManager calls wholly
    // inside these native functions. Stage the actor's current equipment
    // before entering P+ so the existing actor-local event ledger can accept
    // only the forms that P+ actually removes. This is active exclusively for
    // Vanilla and Direct+Vanilla policies.
    StageCurrentWornUnequipExpectations(operation, a_stack->stackID);
    break;
  case TargetNative::SetOutfit:
    // Match PoC18: SetOutfit is recovery evidence only. An ordinary outfit
    // maintenance call must never open a new strip transaction.
    if (HasSuppressedOriginalEquipment(operation.actor->GetFormID())) {
      OpenRecoveryProbe(operation.actor->GetFormID());
      if (const auto id = StageExpectation(operation.actor->GetFormID(), 0, 0,
                                           true, a_stack->stackID,
                                           operation.target)) {
      operation.expectationIDs.push_back(id);
      }
    }
    break;
  default: break;
  }
  return operation;
}

void CancelOperation(const HookOperation &a_operation) {
  if (a_operation.expectationIDs.empty()) {
    return;
  }
  std::lock_guard lock(g_stateMutex);
  for (const auto id : a_operation.expectationIDs) {
    g_expectations.erase(id);
  }
}

void ObserveHelmetToggleSignal(const TargetNative a_target,
                               RE::BSScript::Stack *a_stack) {
  if (!a_stack || !a_stack->top || !IsHelmetToggleSignalTarget(a_target)) {
    return;
  }
  auto &frame = *a_stack->top;
  if (a_target == TargetNative::GlobalSetValue) {
    sfs::native::helmet_toggle::ObserveGlobalStateChanged(
        frame.self.Unpack<RE::TESGlobal *>());
    return;
  }
  auto *actor = frame.self.Unpack<RE::Actor *>();
  auto *spell = ReadArgument<RE::SpellItem *>(frame, 0);
  sfs::native::helmet_toggle::ObserveActorSpellChanged(actor, spell);
}

using NativeCallFn = NativeCallResult (*)(
    NativeFunctionBase *, const RE::BSTSmartPointer<RE::BSScript::Stack> &,
    RE::BSScript::ErrorLogger *, RE::BSScript::Internal::VirtualMachine *,
    bool);

struct NativeFunctionPatch {
  std::unique_ptr<std::uintptr_t[]> clonedVtableStorage;
  NativeCallFn originalCall{nullptr};
};

std::shared_mutex g_patchMutex;
std::unordered_map<NativeFunctionBase *, NativeFunctionPatch> g_patches;

[[nodiscard]] NativeCallResult CallOriginal(
    NativeFunctionBase *a_function,
    const RE::BSTSmartPointer<RE::BSScript::Stack> &a_stack,
    RE::BSScript::ErrorLogger *a_logger,
    RE::BSScript::Internal::VirtualMachine *a_vm, const bool a_arg4) {
  NativeCallFn original = nullptr;
  {
    std::shared_lock lock(g_patchMutex);
    const auto found = g_patches.find(a_function);
    if (found != g_patches.end()) {
      original = found->second.originalCall;
    }
  }
  if (!original) {
    logger::critical("External equipment observer lost native call target");
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
    if (IsHelmetToggleSignalTarget(target)) {
      const auto result =
          CallOriginal(a_function, a_stack, a_logger, a_vm, a_arg4);
      if (result != NativeCallResult::kFailedAbort &&
          result != NativeCallResult::kFailedRetry) {
        ObserveHelmetToggleSignal(target, a_stack.get());
      }
      return result;
    }
    auto operation = PrepareOperation(target, a_stack.get());
    const auto result =
        CallOriginal(a_function, a_stack, a_logger, a_vm, a_arg4);
    if (result == NativeCallResult::kFailedAbort ||
        result == NativeCallResult::kFailedRetry) {
      CancelOperation(operation);
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

  std::unique_lock lock(g_patchMutex);
  if (g_patches.contains(native)) {
    return false;
  }
  auto **vptrSlot = reinterpret_cast<std::uintptr_t **>(native);
  auto *originalVtable = vptrSlot ? *vptrSlot : nullptr;
  if (!originalVtable) {
    return false;
  }
  const auto originalCallAddress =
      originalVtable[sfs::runtime::kPapyrusNativeCallVtableIndex];
  const auto replacementAddress =
      reinterpret_cast<std::uintptr_t>(NativeDispatchHook::thunk);
  if (originalCallAddress == 0 || originalCallAddress == replacementAddress) {
    return false;
  }

  auto storage = std::make_unique<std::uintptr_t[]>(
      sfs::runtime::kPapyrusNativeFunctionVtableEntryCount + 1);
  storage[0] = originalVtable[-1];
  std::copy_n(originalVtable,
              sfs::runtime::kPapyrusNativeFunctionVtableEntryCount,
              storage.get() + 1);
  auto *replacementVtable = storage.get() + 1;
  replacementVtable[sfs::runtime::kPapyrusNativeCallVtableIndex] =
      replacementAddress;

  const auto [patch, inserted] = g_patches.emplace(
      native, NativeFunctionPatch{
                  .clonedVtableStorage = std::move(storage),
                  .originalCall =
                      reinterpret_cast<NativeCallFn>(originalCallAddress)});
  if (!inserted) {
    return false;
  }
  replacementVtable = patch->second.clonedVtableStorage.get() + 1;
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(vptrSlot),
                       std::addressof(replacementVtable),
                       sizeof(replacementVtable),
                       std::addressof(originalVtable),
                       sizeof(originalVtable))) {
    g_patches.erase(patch);
    return false;
  }
  logger::info("External equipment observer hooked {}", TargetName(target));
  return true;
}

void PatchSelectedNativesInType(RE::BSScript::ObjectTypeInfo *a_type,
                                std::size_t &a_count) {
  if (!a_type) {
    return;
  }
  const auto patch = [&](RE::BSScript::IFunction *a_function) {
    if (PatchSelectedNativeFunction(a_function)) {
      ++a_count;
    }
  };
  if (!a_type->IsLinked()) {
    for (auto *entry = a_type->GetUnlinkedFunctionIter(); entry;
         entry = entry->next) {
      patch(entry->func.get());
    }
    return;
  }
  if (auto *functions = a_type->GetGlobalFuncIter()) {
    for (std::uint32_t index = 0; index < a_type->GetNumGlobalFuncs();
         ++index) {
      patch(functions[index].func.get());
    }
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
    g_nativeRegistrationHookInstalled.store(true, std::memory_order_release);
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
} // namespace

namespace sfs::native::external_equipment {
bool RegisterPapyrusObserver(RE::BSScript::IVirtualMachine *a_vm) {
  if (!a_vm) {
    return false;
  }
  if (g_observerInstalled.load(std::memory_order_acquire)) {
    g_observerVm.store(a_vm, std::memory_order_release);
    return true;
  }
  if (!InstallNativeRegistrationHook(a_vm)) {
    logger::error("External equipment observer could not install the Papyrus "
                  "native registration hook");
    return false;
  }

  g_observerVm.store(a_vm, std::memory_order_release);

  constexpr std::array<std::string_view, 4> kTargetTypes{
      "Actor", "ObjectReference", "sslActorAlias", "sslActorLibrary"};
  std::size_t patchedCount = 0;
  for (const auto typeName : kTargetTypes) {
    RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
    if (a_vm->GetScriptObjectTypeNoLoad(RE::BSFixedString(typeName), type)) {
      PatchSelectedNativesInType(type.get(), patchedCount);
    }
  }
  g_observerInstalled.store(true, std::memory_order_release);
  logger::info("External equipment observer ready ({} native functions "
               "available immediately)",
               patchedCount);
  return true;
}

bool EnableHelmetToggleSignalObserver() {
  auto *vm = g_observerVm.load(std::memory_order_acquire);
  if (!vm || !g_observerInstalled.load(std::memory_order_acquire)) {
    return false;
  }

  g_helmetToggleSignalObserverEnabled.store(true, std::memory_order_release);
  constexpr std::array<std::string_view, 2> kSignalTypes{
      "Actor", "GlobalVariable"};
  std::size_t patchedCount = 0;
  for (const auto typeName : kSignalTypes) {
    RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
    if (vm->GetScriptObjectTypeNoLoad(RE::BSFixedString(typeName), type)) {
      PatchSelectedNativesInType(type.get(), patchedCount);
    }
  }
  logger::info("Helmet Toggle 2 signal observer ready ({} exact native "
               "functions added immediately)",
               patchedCount);
  return true;
}

EquipmentEventResult
ConsumeEquipmentEvent(RE::Actor *a_actor, const RE::TESObjectARMO *a_armor,
                      const bool a_equipped,
                      const std::uint64_t a_actualEquipmentLinkedSlotMask) {
  if (!a_actor || !a_armor || !IsAutomaticEquipmentTransactionMode()) {
    return {};
  }

  const auto actorFormID = a_actor->GetFormID();
  const auto armorFormID = a_armor->GetFormID();
  if (IsDedicatedDeviousDevicesArmor(a_armor)) {
    // DD removals represent the device lifecycle, not an original outfit
    // which SFS should wait to see redressed.  Also heal any entry written by
    // an older build before returning control to the dedicated DD Hider.
    std::lock_guard lock(g_stateMutex);
    if (EraseArmorTransactionStateLocked(actorFormID, armorFormID)) {
      logger::info("Removed DD armor from generic equipment transaction "
                   "state actor={:08X} armor={:08X}",
                   actorFormID, armorFormID);
    }
    return {};
  }
  EquipmentEventResult result{};
  bool accepted = false;
  bool eventAddedArmorMarked = false;
  bool eventAddedArmorRemoved = false;
  RE::VMStackID acceptedStackID = 0;
  TargetNative acceptedSource = TargetNative::None;
  std::shared_ptr<const ArmorFormIDSet> acceptedOriginalWornArmor;
  {
    std::lock_guard lock(g_stateMutex);
    const auto now = Clock::now();
    PruneExpiredExpectationsLocked(now);
    PruneRecoveryProbesLocked(now);

    // An event-added item may also be removed manually. Removing the durable
    // scene-equipment marker on every matching unequip is safe: a bare equip
    // event can never create this state, and a later inventory-menu equip must
    // remain ordinary user-controlled equipment.
    if (!a_equipped) {
      const auto eventAddedState =
          g_eventAddedActualEquipment.find(actorFormID);
      if (eventAddedState != g_eventAddedActualEquipment.end()) {
        eventAddedArmorRemoved =
            eventAddedState->second.erase(armorFormID) != 0;
        if (eventAddedState->second.empty()) {
          g_eventAddedActualEquipment.erase(eventAddedState);
          if (!g_suppressedOriginalEquipment.contains(actorFormID) &&
              !g_recoveryProbeExpires.contains(actorFormID)) {
            g_originalWornEquipment.erase(actorFormID);
          }
        }
      }
    }

    for (auto expectation = g_expectations.begin();
         expectation != g_expectations.end();) {
      const auto &value = expectation->second;
      if (value.actorFormID == actorFormID &&
          value.equipped == a_equipped &&
          (value.armorFormID == armorFormID ||
           (value.armorFormID == 0 && a_equipped &&
            HasRecoveryProbeLocked(actorFormID, now)))) {
        accepted = true;
        acceptedStackID = value.stackID;
        acceptedSource = value.source;
        acceptedOriginalWornArmor = value.originalWornArmor;
        // SetOutfit may emit several equip events for one Papyrus call.  Its
        // wildcard expectation must therefore remain alive for the rest of
        // the short event burst; exact item expectations are single-use.
        if (value.armorFormID != 0) {
          expectation = g_expectations.erase(expectation);
        }
        break;
      }
      ++expectation;
    }
    if (!accepted) {
      return {};
    }
    result.accepted = true;
    result.eventAddedRemoved = eventAddedArmorRemoved;

    if (a_actualEquipmentLinkedSlotMask == 0) {
      // Observing Papyrus equipment natives must never turn an arbitrary NPC
      // or an actor without actual-equipment links into managed
      // actual-equipment state. Consume the short-lived origin marker, then
      // discard any stale ledger left after that actor's last actual link was
      // removed.
      g_suppressedOriginalEquipment.erase(actorFormID);
      g_originalWornEquipment.erase(actorFormID);
      g_eventAddedActualEquipment.erase(actorFormID);
      g_recoveryProbeExpires.erase(actorFormID);
    } else {
      // Old saves and a previous transaction can contain equipment from slots
      // that no longer control any registered appearance for this actor. Keep
      // only the actor-local linked bits before deciding whether this event
      // completes recovery. Otherwise an unrelated SGO prop or DD/rendered
      // device can schedule a DAVE/DAV/native rebuild in the middle of its
      // animation merely because it once appeared in the original worn set.
      if (auto actorState =
              g_suppressedOriginalEquipment.find(actorFormID);
          actorState != g_suppressedOriginalEquipment.end()) {
        for (auto equipment = actorState->second.begin();
             equipment != actorState->second.end();) {
          equipment->second &= a_actualEquipmentLinkedSlotMask;
          if (equipment->second == 0) {
            equipment = actorState->second.erase(equipment);
          } else {
            ++equipment;
          }
        }
        if (actorState->second.empty()) {
          g_suppressedOriginalEquipment.erase(actorState);
        }
      }
    }

    if (a_actualEquipmentLinkedSlotMask != 0 && a_equipped) {
      bool restoredOriginal = false;
      bool recoveryCompleted = false;
      const auto actorState =
          g_suppressedOriginalEquipment.find(actorFormID);
      if (actorState != g_suppressedOriginalEquipment.end()) {
        restoredOriginal = actorState->second.erase(armorFormID) != 0;
        if (actorState->second.empty()) {
          g_suppressedOriginalEquipment.erase(actorState);
          recoveryCompleted = restoredOriginal;
        }
      }
      result.originalItemRestored = restoredOriginal;
      result.originalRecoveryCompleted = recoveryCompleted;
      bool presentAtTransactionStart = restoredOriginal;
      if (const auto originalWorn =
              g_originalWornEquipment.find(actorFormID);
          originalWorn != g_originalWornEquipment.end()) {
        presentAtTransactionStart =
            presentAtTransactionStart ||
            originalWorn->second.contains(armorFormID);
      }
      if (!presentAtTransactionStart &&
          HasRecoveryProbeLocked(actorFormID, now)) {
        eventAddedArmorMarked =
            g_eventAddedActualEquipment[actorFormID]
                .insert(armorFormID)
                .second;
        result.eventAddedEquipped = eventAddedArmorMarked;
      }
    } else if (a_actualEquipmentLinkedSlotMask != 0) {
      // An item which entered during this event is not an original stripped
      // item when it leaves again. It only loses its scene-equipment marker.
      if (!eventAddedArmorRemoved) {
        const auto slotMask = ArmorControlSlotMask(a_armor) &
                              a_actualEquipmentLinkedSlotMask;
        if (slotMask != 0) {
          if (!g_originalWornEquipment.contains(actorFormID)) {
            auto &originalWorn = g_originalWornEquipment[actorFormID];
            if (acceptedOriginalWornArmor) {
              originalWorn = *acceptedOriginalWornArmor;
            } else {
              originalWorn.insert(armorFormID);
            }
          }
          g_suppressedOriginalEquipment[actorFormID].insert_or_assign(
              armorFormID, slotMask);
          g_recoveryProbeExpires[actorFormID] =
              now + kRecoveryProbeLifetime;
          result.originalStripped = true;
        }
      }
    }
  }

  logger::info("Accepted external equipment transaction actor={:08X} "
               "armor={:08X} slots={:016X} equipped={} stack={} source={}",
               actorFormID, armorFormID, ArmorControlSlotMask(a_armor),
               a_equipped, acceptedStackID, TargetName(acceptedSource));
  if (eventAddedArmorMarked) {
    logger::info("Event-added actual armor initialized visible actor={:08X} "
                 "armor={:08X} source={}",
                 actorFormID, armorFormID, TargetName(acceptedSource));
  }
  return result;
}

std::uint64_t GetSuppressedActualSlotMask(const RE::FormID a_actorFormID) {
  if (a_actorFormID == 0 || !IsAutomaticEquipmentTransactionMode()) {
    return 0;
  }
  std::lock_guard lock(g_stateMutex);
  const auto now = Clock::now();
  PruneExpiredExpectationsLocked(now);
  PruneDeviousDevicesTransactionStateLocked(a_actorFormID);

  // Match the ModSettings transaction boundary: publish an exact Papyrus
  // equipment mutation before the native call reaches the engine. DAVE, DAV,
  // and the native backend can then read the current suppression state from
  // an equipment rebuild caused by that same call. The event sink still owns
  // one debounced completion refresh because DAVE does not necessarily rebuild
  // registered appearance attachments for every real-item mutation.
  // A confirmed TESEquipEvent replaces the pending entry with the durable
  // ledger under this same mutex; a failed call removes its expectation.
  std::vector<rules::PendingEquipmentMutation> pending;
  pending.reserve(g_expectations.size());
  for (const auto &[_, expectation] : g_expectations) {
    pending.push_back({.sequence = expectation.id,
                       .actorFormID = expectation.actorFormID,
                       .armorFormID = expectation.armorFormID,
                       .slotMask = expectation.slotMask,
                       .equipped = expectation.equipped});
  }
  return rules::ResolveSuppressedSlotMask(
      a_actorFormID, g_suppressedOriginalEquipment, pending);
}

bool IsEventAddedActualEquipment(const RE::FormID a_actorFormID,
                                 const RE::FormID a_armorFormID) {
  if (a_actorFormID == 0 || a_armorFormID == 0 ||
      !IsAutomaticEquipmentTransactionMode()) {
    return false;
  }
  if (const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(a_armorFormID);
      armor && IsDedicatedDeviousDevicesArmor(armor)) {
    std::lock_guard lock(g_stateMutex);
    static_cast<void>(
        EraseArmorTransactionStateLocked(a_actorFormID, a_armorFormID));
    return false;
  }
  std::lock_guard lock(g_stateMutex);
  const auto actorState =
      g_eventAddedActualEquipment.find(a_actorFormID);
  return actorState != g_eventAddedActualEquipment.end() &&
         actorState->second.contains(a_armorFormID);
}

bool BeginContextWardrobeReplacement(
    const RE::FormID a_actorFormID,
    const ContextWardrobeSnapshot &a_previousWornArmor,
    const ContextWardrobeSnapshot &a_currentWornArmor,
    const std::uint64_t a_actualEquipmentLinkedSlotMask) {
  if (a_actorFormID == 0 || a_previousWornArmor.empty() ||
      a_actualEquipmentLinkedSlotMask == 0 ||
      !IsAutomaticEquipmentTransactionMode()) {
    return false;
  }

  std::size_t suppressedCount = 0;
  std::size_t eventAddedCount = 0;
  std::uint64_t suppressedSlotMask = 0;
  {
    std::lock_guard lock(g_stateMutex);
    if (g_contextWardrobeStates.contains(a_actorFormID)) {
      return false;
    }

    ContextWardrobeState context;
    auto &originalWorn = g_originalWornEquipment[a_actorFormID];
    for (const auto &[armorFormID, _] : a_previousWornArmor) {
      if (armorFormID == 0) {
        continue;
      }
      context.originalWornArmor.insert(armorFormID);
      if (originalWorn.insert(armorFormID).second) {
        context.ownedOriginalWornArmor.insert(armorFormID);
      }
      if (a_currentWornArmor.contains(armorFormID)) {
        continue;
      }

      const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID);
      if (!armor || IsDedicatedDeviousDevicesArmor(armor)) {
        continue;
      }
      const auto slotMask =
          ArmorControlSlotMask(armor) & a_actualEquipmentLinkedSlotMask;
      if (slotMask == 0) {
        continue;
      }
      auto &suppressed = g_suppressedOriginalEquipment[a_actorFormID];
      const auto existing = suppressed.find(armorFormID);
      if (existing == suppressed.end()) {
        suppressed.emplace(armorFormID, slotMask);
        context.suppressedOriginalArmor.insert(armorFormID);
        ++suppressedCount;
      } else {
        existing->second |= slotMask;
      }
      suppressedSlotMask |= slotMask;
    }

    if (context.suppressedOriginalArmor.empty()) {
      for (const auto armorFormID : context.ownedOriginalWornArmor) {
        originalWorn.erase(armorFormID);
      }
      if (originalWorn.empty()) {
        g_originalWornEquipment.erase(a_actorFormID);
      }
      return false;
    }

    for (const auto &[armorFormID, _] : a_currentWornArmor) {
      if (armorFormID == 0 ||
          context.originalWornArmor.contains(armorFormID)) {
        continue;
      }
      const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID);
      if (!armor || IsDedicatedDeviousDevicesArmor(armor)) {
        continue;
      }
      if (g_eventAddedActualEquipment[a_actorFormID]
              .insert(armorFormID)
              .second) {
        context.eventAddedArmor.insert(armorFormID);
        ++eventAddedCount;
      }
    }
    g_contextWardrobeStates.emplace(a_actorFormID, std::move(context));
  }

  logger::info("Actual-equipment context wardrobe replacement actor={:08X} "
               "suppressedOriginals={} suppressedSlots={:016X} "
               "eventAdded={}",
               a_actorFormID, suppressedCount, suppressedSlotMask,
               eventAddedCount);
  return true;
}

std::vector<RE::FormID> ReconcileContextWardrobeEquipment(
    const RE::FormID a_actorFormID,
    const ContextWardrobeSnapshot &a_currentWornArmor) {
  if (a_actorFormID == 0 || !IsAutomaticEquipmentTransactionMode()) {
    return {};
  }

  std::vector<RE::FormID> newlyEventAdded;
  std::size_t restoredCount = 0;
  std::size_t eventAddedCount = 0;
  std::size_t eventRemovedCount = 0;
  {
    std::lock_guard lock(g_stateMutex);
    const auto context = g_contextWardrobeStates.find(a_actorFormID);
    if (context == g_contextWardrobeStates.end()) {
      return {};
    }

    if (auto suppressed = g_suppressedOriginalEquipment.find(a_actorFormID);
        suppressed != g_suppressedOriginalEquipment.end()) {
      for (auto owned = context->second.suppressedOriginalArmor.begin();
           owned != context->second.suppressedOriginalArmor.end();) {
        if (!a_currentWornArmor.contains(*owned)) {
          ++owned;
          continue;
        }
        suppressed->second.erase(*owned);
        owned = context->second.suppressedOriginalArmor.erase(owned);
        ++restoredCount;
      }
      if (suppressed->second.empty()) {
        g_suppressedOriginalEquipment.erase(suppressed);
      }
    }

    ArmorFormIDSet currentEventAdded;
    for (const auto &[armorFormID, _] : a_currentWornArmor) {
      if (armorFormID == 0 ||
          context->second.originalWornArmor.contains(armorFormID)) {
        continue;
      }
      const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID);
      if (!armor || IsDedicatedDeviousDevicesArmor(armor)) {
        continue;
      }
      currentEventAdded.insert(armorFormID);
      if (g_eventAddedActualEquipment[a_actorFormID]
              .insert(armorFormID)
              .second) {
        context->second.eventAddedArmor.insert(armorFormID);
        newlyEventAdded.push_back(armorFormID);
        ++eventAddedCount;
      }
    }

    if (auto eventAdded = g_eventAddedActualEquipment.find(a_actorFormID);
        eventAdded != g_eventAddedActualEquipment.end()) {
      for (auto owned = context->second.eventAddedArmor.begin();
           owned != context->second.eventAddedArmor.end();) {
        if (currentEventAdded.contains(*owned)) {
          ++owned;
          continue;
        }
        eventAdded->second.erase(*owned);
        owned = context->second.eventAddedArmor.erase(owned);
        ++eventRemovedCount;
      }
      if (eventAdded->second.empty()) {
        g_eventAddedActualEquipment.erase(eventAdded);
      }
    }
  }

  if (restoredCount != 0 || eventAddedCount != 0 ||
      eventRemovedCount != 0) {
    logger::info("Reconciled actual-equipment context wardrobe actor={:08X} "
                 "restoredOriginals={} eventAdded={} eventRemoved={}",
                 a_actorFormID, restoredCount, eventAddedCount,
                 eventRemovedCount);
  }
  return newlyEventAdded;
}

void ReleaseContextWardrobeReplacement(const RE::FormID a_actorFormID) {
  if (a_actorFormID == 0) {
    return;
  }
  std::size_t suppressedCount = 0;
  std::size_t eventAddedCount = 0;
  {
    std::lock_guard lock(g_stateMutex);
    const auto context = g_contextWardrobeStates.find(a_actorFormID);
    if (context == g_contextWardrobeStates.end()) {
      return;
    }
    suppressedCount = context->second.suppressedOriginalArmor.size();
    eventAddedCount = context->second.eventAddedArmor.size();
    ReleaseContextWardrobeStateLocked(a_actorFormID);
  }
  logger::info("Released actual-equipment context wardrobe actor={:08X} "
               "suppressedOriginals={} eventAdded={}",
               a_actorFormID, suppressedCount, eventAddedCount);
}

bool HasContextWardrobeReplacement(const RE::FormID a_actorFormID) {
  if (a_actorFormID == 0 || !IsAutomaticEquipmentTransactionMode()) {
    return false;
  }
  std::lock_guard lock(g_stateMutex);
  return g_contextWardrobeStates.contains(a_actorFormID);
}

bool IsContextWardrobeEventAddedActualEquipment(
    const RE::FormID a_actorFormID, const RE::FormID a_armorFormID) {
  if (a_actorFormID == 0 || a_armorFormID == 0 ||
      !IsAutomaticEquipmentTransactionMode()) {
    return false;
  }
  std::lock_guard lock(g_stateMutex);
  const auto context = g_contextWardrobeStates.find(a_actorFormID);
  return context != g_contextWardrobeStates.end() &&
         context->second.eventAddedArmor.contains(a_armorFormID);
}

void ClearRuntimeState() {
  std::lock_guard lock(g_stateMutex);
  g_expectations.clear();
  g_suppressedOriginalEquipment.clear();
  g_originalWornEquipment.clear();
  g_eventAddedActualEquipment.clear();
  g_contextWardrobeStates.clear();
  g_recoveryProbeExpires.clear();
  g_nextExpectationID = 1;
}

void Serialize(SKSE::SerializationInterface *a_skse) {
  if (!a_skse) {
    return;
  }
  nlohmann::json root;
  root["actors"] = nlohmann::json::array();
  {
    std::lock_guard lock(g_stateMutex);
    PruneDeviousDevicesTransactionStateLocked();
    std::vector<RE::FormID> actorFormIDs;
    actorFormIDs.reserve(g_suppressedOriginalEquipment.size() +
                         g_eventAddedActualEquipment.size() +
                         g_contextWardrobeStates.size());
    for (const auto &[actorFormID, _] : g_suppressedOriginalEquipment) {
      actorFormIDs.push_back(actorFormID);
    }
    for (const auto &[actorFormID, _] : g_eventAddedActualEquipment) {
      actorFormIDs.push_back(actorFormID);
    }
    for (const auto &[actorFormID, _] : g_contextWardrobeStates) {
      actorFormIDs.push_back(actorFormID);
    }
    std::ranges::sort(actorFormIDs);
    actorFormIDs.erase(std::unique(actorFormIDs.begin(), actorFormIDs.end()),
                       actorFormIDs.end());
    for (const auto actorFormID : actorFormIDs) {
      nlohmann::json equipment = nlohmann::json::array();
      if (const auto suppressed =
              g_suppressedOriginalEquipment.find(actorFormID);
          suppressed != g_suppressedOriginalEquipment.end()) {
        for (const auto &[armorFormID, slotMask] : suppressed->second) {
          equipment.push_back(
              {{"formID", armorFormID}, {"slotMask", slotMask}});
        }
      }
      nlohmann::json eventAdded = nlohmann::json::array();
      if (const auto added = g_eventAddedActualEquipment.find(actorFormID);
          added != g_eventAddedActualEquipment.end()) {
        for (const auto armorFormID : added->second) {
          eventAdded.push_back(armorFormID);
        }
      }
      nlohmann::json originalWorn = nlohmann::json::array();
      if (const auto original = g_originalWornEquipment.find(actorFormID);
          original != g_originalWornEquipment.end()) {
        for (const auto armorFormID : original->second) {
          originalWorn.push_back(armorFormID);
        }
      }
      nlohmann::json context = nullptr;
      if (const auto savedContext = g_contextWardrobeStates.find(actorFormID);
          savedContext != g_contextWardrobeStates.end()) {
        const auto toJsonArray = [](const ArmorFormIDSet &a_forms) {
          nlohmann::json result = nlohmann::json::array();
          for (const auto formID : a_forms) {
            result.push_back(formID);
          }
          return result;
        };
        context = {{"originalWorn",
                    toJsonArray(savedContext->second.originalWornArmor)},
                   {"ownedOriginalWorn",
                    toJsonArray(savedContext->second.ownedOriginalWornArmor)},
                   {"suppressedOriginal",
                    toJsonArray(
                        savedContext->second.suppressedOriginalArmor)},
                   {"eventAdded",
                    toJsonArray(savedContext->second.eventAddedArmor)}};
      }
      root["actors"].push_back({{"formID", actorFormID},
                                  {"equipment", std::move(equipment)},
                                  {"eventAdded", std::move(eventAdded)},
                                  {"originalWorn", std::move(originalWorn)},
                                  {"contextWardrobe", std::move(context)}});
    }
  }
  const auto payload = root.dump();
  a_skse->WriteRecord(kStateRecordType, kStateRecordVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

void Deserialize(SKSE::SerializationInterface *a_skse) {
  ClearRuntimeState();
  if (!a_skse) {
    return;
  }
  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    return;
  }
  if (type != kStateRecordType || version < 1 ||
      version > kStateRecordVersion) {
    logger::warn("Skipped unsupported external equipment state record "
                 "type={:X} version={}",
                 type, version);
    return;
  }
  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    return;
  }
  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object() ||
      !root["actors"].is_array()) {
    return;
  }
  if (!IsAutomaticEquipmentTransactionMode()) {
    logger::info(
        "Discarded saved actual-equipment transaction state because the "
        "active strip-link policy does not use actual-equipment anchors");
    return;
  }

  std::unordered_map<RE::FormID,
                     std::unordered_map<RE::FormID, std::uint64_t>> loaded;
  std::unordered_map<RE::FormID, ArmorFormIDSet> loadedOriginal;
  std::unordered_map<RE::FormID, std::unordered_set<RE::FormID>>
      loadedEventAdded;
  std::unordered_map<RE::FormID, ContextWardrobeState> loadedContexts;
  for (const auto &actorState : root["actors"]) {
    const auto savedActorFormID = actorState.value("formID", RE::FormID{0});
    RE::FormID actorFormID = 0;
    if (savedActorFormID == 0 ||
        !a_skse->ResolveFormID(savedActorFormID, actorFormID) ||
        actorFormID == 0 || !actorState["equipment"].is_array()) {
      continue;
    }
    for (const auto &equipment : actorState["equipment"]) {
      const auto savedArmorFormID =
          equipment.value("formID", RE::FormID{0});
      RE::FormID armorFormID = 0;
      const auto slotMask = equipment.value("slotMask", std::uint64_t{0});
      if (savedArmorFormID != 0 && slotMask != 0 &&
          a_skse->ResolveFormID(savedArmorFormID, armorFormID) &&
          armorFormID != 0) {
        const auto *armor =
            RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID);
        if (!armor || IsDedicatedDeviousDevicesArmor(armor)) {
          continue;
        }
        loaded[actorFormID].insert_or_assign(armorFormID, slotMask);
      }
    }
    const auto eventAdded =
        actorState.value("eventAdded", nlohmann::json::array());
    if (version >= 2 && eventAdded.is_array()) {
      for (const auto &savedArmor : eventAdded) {
        if (!savedArmor.is_number_unsigned()) {
          continue;
        }
        RE::FormID armorFormID = 0;
        if (a_skse->ResolveFormID(savedArmor.get<RE::FormID>(), armorFormID) &&
            armorFormID != 0 &&
            RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID) &&
            !IsDedicatedDeviousDevicesArmor(
                RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID))) {
          loadedEventAdded[actorFormID].insert(armorFormID);
        }
      }
    }
    const auto originalWorn =
        actorState.value("originalWorn", nlohmann::json::array());
    if (version >= 2 && originalWorn.is_array()) {
      for (const auto &savedArmor : originalWorn) {
        if (!savedArmor.is_number_unsigned()) {
          continue;
        }
        RE::FormID armorFormID = 0;
        if (a_skse->ResolveFormID(savedArmor.get<RE::FormID>(), armorFormID) &&
            armorFormID != 0 &&
            RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID) &&
            !IsDedicatedDeviousDevicesArmor(
                RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID))) {
          loadedOriginal[actorFormID].insert(armorFormID);
        }
      }
    }
    if (!loadedOriginal.contains(actorFormID)) {
      if (const auto suppressed = loaded.find(actorFormID);
          suppressed != loaded.end()) {
        for (const auto &[armorFormID, _] : suppressed->second) {
          loadedOriginal[actorFormID].insert(armorFormID);
        }
      }
    }
    const auto savedContext =
        actorState.value("contextWardrobe", nlohmann::json{});
    if (version >= 3 && savedContext.is_object()) {
      const auto resolveArmorSet = [&](const std::string_view a_field) {
        ArmorFormIDSet result;
        const auto savedForms =
            savedContext.value(std::string(a_field), nlohmann::json::array());
        if (!savedForms.is_array()) {
          return result;
        }
        for (const auto &savedArmor : savedForms) {
          if (!savedArmor.is_number_unsigned()) {
            continue;
          }
          RE::FormID armorFormID = 0;
          if (!a_skse->ResolveFormID(savedArmor.get<RE::FormID>(),
                                     armorFormID) ||
              armorFormID == 0) {
            continue;
          }
          const auto *armor =
              RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID);
          if (armor && !IsDedicatedDeviousDevicesArmor(armor)) {
            result.insert(armorFormID);
          }
        }
        return result;
      };

      ContextWardrobeState context{
          .originalWornArmor = resolveArmorSet("originalWorn"),
          .ownedOriginalWornArmor =
              resolveArmorSet("ownedOriginalWorn"),
          .suppressedOriginalArmor =
              resolveArmorSet("suppressedOriginal"),
          .eventAddedArmor = resolveArmorSet("eventAdded")};
      std::erase_if(context.ownedOriginalWornArmor,
                    [&](const RE::FormID a_formID) {
                      return !loadedOriginal[actorFormID].contains(a_formID);
                    });
      std::erase_if(context.suppressedOriginalArmor,
                    [&](const RE::FormID a_formID) {
                      return !loaded[actorFormID].contains(a_formID);
                    });
      std::erase_if(context.eventAddedArmor,
                    [&](const RE::FormID a_formID) {
                      return !loadedEventAdded[actorFormID].contains(a_formID);
                    });
      if (!context.originalWornArmor.empty() &&
          (!context.suppressedOriginalArmor.empty() ||
           !context.eventAddedArmor.empty())) {
        loadedContexts.emplace(actorFormID, std::move(context));
      }
    }
  }
  std::lock_guard lock(g_stateMutex);
  g_suppressedOriginalEquipment = std::move(loaded);
  g_originalWornEquipment = std::move(loadedOriginal);
  g_eventAddedActualEquipment = std::move(loadedEventAdded);
  g_contextWardrobeStates = std::move(loadedContexts);
}
} // namespace sfs::native::external_equipment
