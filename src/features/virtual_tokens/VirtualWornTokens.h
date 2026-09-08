#pragma once

#include <RE/Skyrim.h>

#include <string_view>

namespace SKSE {
class SerializationInterface;
}

namespace sfs::virtual_tokens {
void InitializeVirtualWornTokens();
bool RegisterVirtualWornTokenPapyrus(RE::BSScript::IVirtualMachine *a_vm);
void UpdateVirtualWornTokenCache();
// Reconciles a generic actor-context wardrobe replacement (for example a
// vanilla/quest jail transfer) without identifying the caller mod. This is
// intentionally actor-local and only evaluates at a location/cell boundary.
void ObserveActorContextWardrobeBoundary(RE::Actor *a_actor);
void RecordActorContextWardrobeSnapshot(RE::Actor *a_actor);
void ResetVirtualWornTokenRuntimeState();
void SerializeVirtualWornTokenState(SKSE::SerializationInterface *a_skse);
void DeserializeVirtualWornTokenState(SKSE::SerializationInterface *a_skse);
void InvalidateVirtualWornTokenAutomationForAppearance(
    RE::FormID a_actorFormID, RE::FormID a_appearanceArmorFormID,
    std::uint32_t a_slotMask, bool a_deleted);
[[nodiscard]] bool IsVirtualWornTokenAutomationBypassed(
    RE::FormID a_actorFormID, RE::FormID a_appearanceArmorFormID,
    std::uint32_t a_slotMask);
[[nodiscard]] bool IsVirtualWornTokenAppearanceSuppressed(
    RE::FormID a_actorFormID, RE::FormID a_appearanceArmorFormID,
    std::uint32_t a_slotMask);
[[nodiscard]] bool IsVirtualWornTokenEventAddedArmor(
    RE::FormID a_actorFormID, RE::FormID a_armorFormID);
[[nodiscard]] bool
IsVirtualWornTokenRecoveryBurstActive(RE::FormID a_actorFormID);
void HandleVirtualWornTokenEquipEvent(RE::Actor *a_actor,
                                      RE::TESObjectARMO *a_armor,
                                      bool a_equipped);
} // namespace sfs::virtual_tokens
