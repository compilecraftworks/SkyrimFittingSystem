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
// OStim's explicit Papyrus override reuses the existing identity/generation
// tickets. It never returns tokens or source appearances to OStim's real-gear
// cache, and never equips/unequips actual equipment.
void BeginOStimEquipmentPass(RE::VMStackID a_stackID, RE::Actor *a_actor);
void EndOStimEquipmentPass(RE::VMStackID a_stackID);
void StripOStimAppearances(RE::Actor *a_actor, std::int32_t a_threadID,
                          std::uint32_t a_mask, bool a_undressWigs);
void RestoreOStimAppearances(RE::Actor *a_actor, std::int32_t a_threadID,
                            std::uint32_t a_mask);
RE::BSFixedString GetOStimRedressSession(RE::Actor *a_actor);
std::uint32_t GetOStimSessionMask(RE::Actor *a_actor, RE::BSFixedString a_session);
void RestoreOStimSession(RE::Actor *a_actor, RE::BSFixedString a_session,
                         std::uint32_t a_mask);
void ObserveOStimSceneEnd(RE::Actor *a_actor, std::int32_t a_threadID);
void ObserveOStimSceneStart(RE::Actor *a_actor);
void ObserveRegisteredAppearanceWig(RE::Actor *a_actor,
                                    RE::TESObjectARMO *a_armor,
                                    RE::NiAVObject *a_object);
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
