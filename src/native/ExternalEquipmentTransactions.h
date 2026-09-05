#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sfs::native::external_equipment {
using ContextWardrobeSnapshot =
    std::unordered_map<RE::FormID, std::uint64_t>;

struct EquipmentEventResult {
  bool accepted{false};
  bool originalStripped{false};
  bool originalItemRestored{false};
  bool originalRecoveryCompleted{false};
  bool eventAddedEquipped{false};
  bool eventAddedRemoved{false};
};

// Installs a selective observer on equipment-mutating Papyrus natives. The
// observer never changes arguments, return values, inventory, or equipment;
// it only marks the TESEquipEvents causally produced by an external script.
bool RegisterPapyrusObserver(RE::BSScript::IVirtualMachine *a_vm);
// Rechecks only the empty-state member-native table of a fully linked
// sslActorAlias. This closes the P+ load-order window without broadening the
// generic post-link scan which intentionally remains global-only.
void InspectFullyLinkedSexLabPPlusAlias(
    RE::BSScript::ObjectTypeInfo *a_type);
// Enables three exact post-call observations used by the built-in HT2 bridge:
// GlobalVariable.SetValue plus Actor.AddSpell/RemoveSpell. The dispatch path
// immediately rejects every receiver/form except HT2's resolved signal forms.
bool EnableHelmetToggleSignalObserver();

// Reports how this exact event changed the actor-local external transaction
// ledger. Manual inventory events are never accepted. Distinguishing original
// strip/redress from event-added scene gear lets the display sink debounce one
// completion refresh without a later prop/tongue event cancelling it.
[[nodiscard]] EquipmentEventResult
ConsumeEquipmentEvent(RE::Actor *a_actor,
                      const RE::TESObjectARMO *a_armor, bool a_equipped,
                      std::uint64_t a_actualEquipmentLinkedSlotMask);

// OR of the vanilla control-slot masks whose original actual equipment is
// absent because of an accepted external strip operation, including exact
// short-lived mutations staged before their Papyrus native enters the engine.
// Pre-staging lets an equipment rebuild caused by that same call consume the
// current SFS display state. A separate actor-local completion barrier handles
// backends which do not rebuild registered appearances for every real item.
[[nodiscard]] std::uint64_t
GetSuppressedActualSlotMask(RE::FormID a_actorFormID);

// True while an actual armor which was not present in the actor's original
// worn set has been equipped by an accepted external strip/redress
// transaction. The display sink uses this actor-local edge to initialize the
// new row to the ordinary visible state; it does not lock either eye control,
// and an ordinary inventory-menu equip can never create it.
[[nodiscard]] bool IsEventAddedActualEquipment(
    RE::FormID a_actorFormID, RE::FormID a_armorFormID);

// Generic location/cell-boundary fallback for wardrobe replacements whose
// individual Papyrus equipment events were not observable (jail transfers,
// RemoveAllItems + SetOutfit scenes, and equivalent quest flows). The caller
// supplies before/after worn snapshots; this ledger remains actor-local and
// projects only onto actual-equipment links owned by the active Vanilla base.
[[nodiscard]] bool BeginContextWardrobeReplacement(
    RE::FormID a_actorFormID,
    const ContextWardrobeSnapshot &a_previousWornArmor,
    const ContextWardrobeSnapshot &a_currentWornArmor,
    std::uint64_t a_actualEquipmentLinkedSlotMask);
[[nodiscard]] std::vector<RE::FormID> ReconcileContextWardrobeEquipment(
    RE::FormID a_actorFormID,
    const ContextWardrobeSnapshot &a_currentWornArmor);
void ReleaseContextWardrobeReplacement(RE::FormID a_actorFormID);
[[nodiscard]] bool
HasContextWardrobeReplacement(RE::FormID a_actorFormID);
[[nodiscard]] bool IsContextWardrobeEventAddedActualEquipment(
    RE::FormID a_actorFormID, RE::FormID a_armorFormID);

void ClearRuntimeState();
void Serialize(SKSE::SerializationInterface *a_skse);
void Deserialize(SKSE::SerializationInterface *a_skse);
} // namespace sfs::native::external_equipment
