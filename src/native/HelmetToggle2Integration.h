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

// Reconciles one actor only. queueRefresh=false is used from an already-running
// SFS actor refresh so DAVE/DAV/native is never invoked recursively.
void SynchronizeActor(RE::Actor *a_actor, bool a_queueRefresh);
void SynchronizePlayer(bool a_queueRefresh);

// Returns the renderer-only Hair (31) release used by the proven v1.5.0 HT2
// path. It never equips, unequips, or edits the actor's real armor.
[[nodiscard]] std::uint32_t GetActualHairSlotReleaseMask(
    RE::Actor *a_actor, std::uint32_t a_displayedFittingSlotMask = 0);

} // namespace sfs::native::helmet_toggle
