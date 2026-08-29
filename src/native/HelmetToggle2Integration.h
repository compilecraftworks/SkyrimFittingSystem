#pragma once

#include <RE/Skyrim.h>

namespace sfs::native::helmet_toggle {

// Resolves Helmet Toggle 2 forms after game data is available and enables the
// narrow Papyrus-native signal observer. No HT2 script or plugin is replaced.
[[nodiscard]] bool Initialize();
[[nodiscard]] bool IsAvailable();

// Clears only actor-local, runtime HT2 observation/suppression state. The
// resolved forms remain valid until Skyrim unloads its data.
void ResetRuntimeState();

// Called after the exact Papyrus native has completed. These functions reject
// every global/spell except the resolved HT2 signal forms.
void ObserveGlobalStateChanged(RE::TESGlobal *a_global);
void ObserveActorSpellChanged(RE::Actor *a_actor, RE::SpellItem *a_spell);

// Returns the real Hair (31) partition which must be released from Skyrim's
// worn mask while HT2 is hiding a managed, still-equipped headgear ARMO.  The
// optional displayed-fitting mask keeps a visible registered slot-31 wig in
// control of Hair.  This is a renderer-only answer: it never changes the
// actor's inventory, ARMO, HT2 variant, or virtual-token catalog.
[[nodiscard]] std::uint32_t GetActualHairSlotReleaseMask(
    RE::Actor *a_actor, std::uint32_t a_displayedFittingSlotMask = 0);

// Reconciles one actor only. queueRefresh=false is used from an already-running
// SFS actor refresh so DAVE/DAV/native is never invoked recursively.
void SynchronizeActor(RE::Actor *a_actor, bool a_queueRefresh);
void SynchronizePlayer(bool a_queueRefresh);

} // namespace sfs::native::helmet_toggle
