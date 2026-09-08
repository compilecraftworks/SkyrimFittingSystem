#pragma once

#include <cstdint>

#define SFS_API extern "C" __declspec(dllexport)

SFS_API bool SkyrimFittingSystem_Open();
SFS_API void SkyrimFittingSystem_Close();
SFS_API bool SkyrimFittingSystem_IsMenuOpen();
SFS_API void SkyrimFittingSystem_SetHotkeyEnabled(bool a_enabled);

// Legacy Grid Inventory Costume bridge (ABI v1).
//
// Kept for the v1.4.4 replacement-DLL compatibility patch. Current Grid
// Inventory v1.4.1+ integration uses Grid's own Costume-state SKSE broadcast
// inside SFSCore and does not call these exports. They remain inert unless SFS
// has finished loading game data and remain safe for older callers.
SFS_API std::uint32_t SkyrimFittingSystem_GetGridInventoryCostumeAPIVersion();
SFS_API bool SkyrimFittingSystem_ApplyGridInventoryCostume(
    const std::uint32_t *a_formIDs, std::uint32_t a_count);
SFS_API bool SkyrimFittingSystem_ClearGridInventoryCostume();

// Optional Dynamic Footprints bridge (ABI v1).
//
// Returns the FormID of the actor's *currently rendered* registered footwear
// (slot 37), or 0 when no visible SFS appearance owns that slot.  This is a
// read-only snapshot: it never changes actual equipment, keywords, saved
// workbench data, conditions, or external strip state.  A caller receiving 0
// must use its normal actual-equipment behavior.
SFS_API std::uint32_t
SkyrimFittingSystem_GetDynamicFootprintsHostAPIVersion();
SFS_API std::uint32_t
SkyrimFittingSystem_GetDisplayedFootwearFormID(std::uint32_t a_actorFormID);
// Optional ABI-v1 extension. Returns true when SFS owns the actor's final
// footwear decision; a true result with FormID 0 explicitly means barefoot.
// False tells older consumers to use their ordinary actual-equipment query.
SFS_API bool SkyrimFittingSystem_TryGetDisplayedFootwearFormID(
    std::uint32_t a_actorFormID, std::uint32_t *a_outFormID);

namespace sfs::api {
[[nodiscard]] bool IsHotkeyEnabled();
void SetMenuInitialized(bool a_initialized);
void SetGameDataLoaded(bool a_loaded);
void SetMenuLifecycleActive(bool a_active);
void ProcessMenuRequests();
} // namespace sfs::api
