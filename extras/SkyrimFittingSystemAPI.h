#pragma once

#include <cstdint>

// Skyrim Fitting System v1.3.0+ runtime menu API.
// Resolve these functions with GetProcAddress; no import library is required.
// v1.4.0 renamed the native module to SFSCore.dll. Consumers that also support
// v1.3.x may fall back to the legacy module name below.
inline constexpr wchar_t SkyrimFittingSystem_ModuleName[] = L"SFSCore.dll";
inline constexpr wchar_t SkyrimFittingSystem_LegacyModuleName[] =
    L"SkyrimFittingSystem.dll";

using SkyrimFittingSystem_Open_t = bool (*)();
using SkyrimFittingSystem_Close_t = void (*)();
using SkyrimFittingSystem_IsMenuOpen_t = bool (*)();
using SkyrimFittingSystem_SetHotkeyEnabled_t = void (*)(bool);

// Optional Dynamic Footprints consumer API (ABI v1).
// A 0 result from GetDisplayedFootwearFormID means that SFS has no visible
// registered footwear for this actor and the consumer must retain its normal
// technically-equipped-footwear behavior.
using SkyrimFittingSystem_GetDynamicFootprintsHostAPIVersion_t =
    std::uint32_t (*)();
using SkyrimFittingSystem_GetDisplayedFootwearFormID_t =
    std::uint32_t (*)(std::uint32_t a_actorFormID);

inline constexpr auto SkyrimFittingSystem_Open_Name =
    "SkyrimFittingSystem_Open";
inline constexpr auto SkyrimFittingSystem_Close_Name =
    "SkyrimFittingSystem_Close";
inline constexpr auto SkyrimFittingSystem_IsMenuOpen_Name =
    "SkyrimFittingSystem_IsMenuOpen";
inline constexpr auto SkyrimFittingSystem_SetHotkeyEnabled_Name =
    "SkyrimFittingSystem_SetHotkeyEnabled";
inline constexpr auto SkyrimFittingSystem_GetDynamicFootprintsHostAPIVersion_Name =
    "SkyrimFittingSystem_GetDynamicFootprintsHostAPIVersion";
inline constexpr auto SkyrimFittingSystem_GetDisplayedFootwearFormID_Name =
    "SkyrimFittingSystem_GetDisplayedFootwearFormID";
