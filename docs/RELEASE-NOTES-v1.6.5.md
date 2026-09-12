# Skyrim Fitting System v1.6.5 SE-AE

## Changes

- Added the read-only Rendered Outfit API v1 for external SKSE plugins. It reports SFS's final display decision for visible actual equipment and registered appearances, effective slots, and body-coverage flags. Actor-local change messages distinguish display changes, scene changes, availability, and save/load epochs. Managed-empty is distinct from unmanaged/unavailable.
- Added a built-in IED 1.7.4 BipedSlot condition bridge for the verified pre-629 and post-629 distributions. Covered equipment and node-override conditions now read SFS's final-visible ARMO slots 30-61, including hidden real equipment and shown/hidden registered appearances. No separate SFS patch is required for this bridge.
- Kept IED's original form/keyword predicates within BipedSlot conditions and node match bookkeeping. Inventory, actual equipment, presets, scripts, and IED files are not replaced. Weapon/quiver/race-sentinel slots, explicit skin queries, and other condition families remain owned by IED.
- When IED is absent, no bridge hooks, observer, polling, or IED evaluation tasks are installed. Unrecognized or modified IED binaries retain original IED behavior and core SFS rendering, with a diagnostic rather than guessed hook offsets.
- Preserved the previous Helgen/custom-skin visitor safety route. IED reevaluation is actor-local and deduplicated; tasks resolve actors afresh, skip missing/deleted/disabled/unloaded actors, and are canceled across save/load transitions. Scene-only notifications do not trigger an IED reevaluation loop.
- Added LT + horizontal RS character/camera rotation through the existing right-mouse rotation path, with analog deadzone, frame-time-based speed, and focus/device reset. Existing paused camera-only behavior is retained.
- Added a right-aligned rotation hint beside the main window's close button in English, Korean, and Simplified Chinese.
- Keyboard and gamepad Cancel now follow the current game MenuMode mapping, including remapped keys. Existing one-level cancellation is retained: close the active popup/editor first, or close the main SFS window when none is active.
- Set first-run character position to Left. Pause Game remains unchecked by default. Existing saved settings and presets are preserved.
- Bounded API publication to changed actors, coalesced duplicate requests, and provided immutable ID-only views for IED workers without a periodic whole-actor scan.

## Update and integration notes

Game/RaceMenu support, save/settings formats, BodyMorph ownership, DAVE/DAV/native refresh dispatch, and the three optional compatibility patches are unchanged. IED is optional. This bridge does not convert every IED equipped-form/keyword/type or inventory condition; presets must use the covered BipedSlot conditions to read SFS final-display state.

For API consumers, use `extras/SkyrimFittingSystemRenderedOutfitAPI.h` and the included API documentation. Resolve the already-loaded SFSCore.dll, query on the SKSE game-task thread, and invalidate consumer caches on epoch changes. Ready describes SFS's display decision, not completion of all parallel 3D attachment work or pixel visibility. The API does not itself install or update a consumer such as BCNG.

## Verification

SE/AE-only 1.6.5.0 build, all 20 fast regression targets, and source/package checks passed. Both installed IED 1.7.4 distributions passed actual helper and installer checks in an isolated process without changing installed DLLs. Tests include the prior 40 SE/AE custom-skin routes, BodyMorph/strip/backend regressions, API lifecycle, unload/delete/load cancellation, and ImGui title/close behavior.

Live Helgen/cell-transition gameplay, physical-controller input, and matched city-route frame-time/memory comparisons have not been completed for this release. These tests do not establish CTD-free behavior, measured FPS gains, or compatibility with every mod combination.
