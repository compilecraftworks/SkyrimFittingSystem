#pragma once

#include <RE/Skyrim.h>

namespace SKSE {
class SerializationInterface;
}

namespace sfs::native {
void SetVirtualTokenFittingSlotsSuppressed(RE::Actor *a_actor,
                                           std::uint32_t a_slotMask,
                                           bool a_suppressed);
// Helmet Toggle 2 uses this separate, runtime-only suppression layer.  It
// never edits the saved fitting cards or the user's eye-button choices.
void SetHeadgearToggleFittingSlotsSuppressed(RE::Actor *a_actor,
                                             std::uint32_t a_slotMask,
                                             bool a_suppressed);
// Helmet Toggle sends a complete "shown" transition for its managed headgear
// set. Clear every temporary mask for that actor so a removed/remapped card
// cannot leave stale runtime suppression behind.
void ClearHeadgearToggleFittingSlotsSuppressed(RE::Actor *a_actor);
// A user who presses an SFS eye button while Helmet Toggle is hiding a slot
// can temporarily show that slot again.  The override is cleared with the
// corresponding Helmet Toggle show transition, never serialized.
bool SetHeadgearToggleFittingSlotsManualVisible(RE::Actor *a_actor,
                                                std::uint32_t a_slotMask,
                                                bool a_visible);
void ReconcileFittingSlotState(RE::Actor *a_actor);
void ClearFittingSlotState(RE::Actor *a_actor);
void ClearAllFittingSlotStates();
void SerializeFittingSlotStates(SKSE::SerializationInterface *a_skse);
void DeserializeFittingSlotStates(SKSE::SerializationInterface *a_skse);

[[nodiscard]] bool HasFittingSlotState(RE::Actor *a_actor);
[[nodiscard]] std::uint32_t GetSuppressedFittingSlotMask(RE::Actor *a_actor);
[[nodiscard]] std::uint32_t
GetVirtualTokenSuppressedFittingSlotMask(RE::Actor *a_actor);
[[nodiscard]] std::uint32_t
GetHeadgearToggleSuppressedFittingSlotMask(RE::Actor *a_actor);
} // namespace sfs::native
