# Skyrim Fitting System v1.6.6 SE-AE

## Changes

- Added `SkyrimFittingSystem_QueryRenderedOutfitOnGameTask` for external plugins querying SFS's final display state from actual SKSE AddTask callbacks. This avoids erroneous WrongThread rejection when a later serialized task batch runs on a different OS thread from the last SFS publication task, including when the provider is idle.
- Preserved the original query export and ABI v1 layouts, status values, signatures, and caller-owned buffers. Both query entries share argument, actor, readiness, 3D/root, suspension, dirty-state, epoch, revision, and scene validation; no stale-snapshot fallback was added.
- Added regression coverage for 128 actual-torso / registered-torso / managed-empty / no-torso-with-accessory transitions on migrated task threads, including effective slot masks and body-coverage flags. No visible torso is not assumed to mean zero visible items.
- Added checks for task-aware buffer bounds, validation before actor access, and repeated idle reads without forcing an extra SFS publication task.
- Audited temporary diagnostic and analysis code. No live probe or temporary diagnostic DLL was present in the SFS production path to remove. Required error logs, stripping recovery logic, regression tests, and backups were preserved. Added a guard against known analysis-probe code entering the production API/build target.

## Consumer/update notes

Consumers must resolve and prefer the new optional export to use the fix; the original export keeps its existing thread-ID guard for compatibility. The new entry does not schedule a task or validate task context on the caller's behalf. Call it only from an actual `SKSE::TaskInterface::AddTask` callback, never Present, an input callback, or an arbitrary worker. API version remains 1.

This is the SFS-side API fix handed off from BCNG integration testing, not a change to BCNG's ORefit coefficients, preset formulas, XML handling, or UI. BCNG is not bundled or updated by this SFS archive. Existing settings and presets should be kept.

Game/RaceMenu support, BodyMorph ownership, DAVE/DAV/native rendering, stripping-link modes, dye, IED BipedSlot integration, and the three optional compatibility-patch versions are unchanged. No new polling timer, periodic actor scan, or game-memory diagnostic hook was added.

## Verification

The SE/AE-only releasedbg DLL reports 1.6.6.0. All 20 regression targets and source checks passed, as did isolated helper/installer tests for both installed IED 1.7.4 distributions. Export inspection confirms 16 entries, including both query functions. The runtime archive changes only SFSCore.dll compared with v1.6.5; scripts, helper ESL, locales, and other runtime assets are unchanged.

The paired development hotfix was reported working by the user in TuLED. This numbered release has not received new in-game verification across all supported runtimes/mod combinations, and automated tests do not establish CTD-free behavior or measured FPS gains.
