# Skyrim Fitting System v1.3.0

## Main Changes

- Replaced per-mod strip bridges with actor-local automatic appearance hiding based on each actor's final worn equipment state.
- Added the Strip Link popup with No Linking, Vanilla Auto-Match, Include Mod Slots, and per-slot fixed exceptions.
- Added actor- and save-local persistence with backward-compatible v1.2.2 co-save record ordering. Official `AEVS` v2 begins with linking disabled for saves without an `AEVS` record and does not migrate global `settings.json` or the discarded development-only `AEVS` v1.
- Added customizable special-effect slot protection with default slots 50, 51, and 61, plus an optional slot-39 shield appearance mode that is OFF by default.
- Preserved the verified RaceMenu `ApplyBodyMorphs` path used by OBody. For NiOverride Papyrus callers such as FHU and SGO, SFS resolves RaceMenu's internal deferred `UpdateModelWeight` task and rebuilds only that actor's SFS appearances through the existing DAVE, DAV, or native display path after RaceMenu finishes. No global actor scan or periodic polling is used.
- Added Equipment Ctrl multi-selection, batch registration, a full-height actor selector, safer UTF-8 kit loading, and invalid JSON isolation.
- Improved actor-local DAVE, DAV, and native refresh handling after actual equipment changes and retained SOS/TNG and optional Helmet Toggle 2/Wet Function Redux compatibility.
- Calculated Strip Link actual occupancy from the union of ARMO BOD and all ARMA slots, with the popup preview and runtime automatic suppression sharing the same normalized control mask.
- Added the stable external menu C API: `SkyrimFittingSystem_Open`, `SkyrimFittingSystem_Close`, `SkyrimFittingSystem_IsMenuOpen`, and runtime-only `SkyrimFittingSystem_SetHotkeyEnabled`. Disabling the native shortcut does not alter its saved binding, does not affect the other API calls, and resets to enabled on every game launch.
- Removed the main ESP, SEQ, and retired per-mod bridge PEX/PSC files. The main package now runs without a main ESP.

## Update Instructions

- Replace the old v1.2.2 main mod folder instead of merging the v1.3.0 files over it. This prevents retired ESP, SEQ, and bridge scripts from remaining installed.
- Preserve or back up `Interface/SkyrimFittingSystem/user`, especially personal Fitting Kits.
- Keep Helmet Toggle 2 and Wet Function Redux compatibility patches as separate optional installations and install only the patches for mods you use.
- Every actor starts with Strip Link set to No Linking. Global `settings.json` and the discarded development-only `AEVS` v1 are not migration sources.

## Package and Compatibility Notes

- The main package is centered on `SkyrimFittingSystem.dll`, UI resources, and `SkyrimFittingSystemNative.pex/psc`.
- SOS StorageUtil synchronization now runs inside the DLL and no longer needs the retired bridge ESP or PEX.
- RaceMenu does not require a source patch for BodyMorph compatibility.
- Existing registered appearances, original slots, conditions, manual visibility, actor ownership, and Fitting Kit JSON are preserved by the new temporary strip-link state.

## Validation Status

- The v1.3.0.0 development build, direct MO2 archive layout, three locale JSON files, UTF-8 files, external C exports, and standalone API consumer path have passed static/build verification.
- Final DLL SHA-256: `4821E5DE28B6D03D56AB0860723EF9073759C42D783DE617361403476902E681`. The MO2 development installation matched all 11 payload hashes, contained no legacy payload files, and had no empty `Seq` folder.
- The official v1.2.2 co-save record prefix order was compared directly before appending the new final `AEVS` record.
- Final in-game regression testing is being handled separately; this document does not claim that testing is complete.

For the full detailed list, use the separate v1.3.0 Nexus update document.
