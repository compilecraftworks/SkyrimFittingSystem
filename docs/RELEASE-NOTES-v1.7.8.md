# Skyrim Fitting System v1.7.8 SE-AE

## Changes

- Addressed late RaceMenu full rescans that could reapply registered high-heel height during SexLab scenes, while respecting the existing heel-removal setting and actual footwear stripping state.
- Temporarily suppress native game and ENB depth of field while the SFS UI is open to reduce character/background blur. Normal behavior returns when the UI closes, and temporary ENB values are restored before preset saves.
- Reduced unnecessary checks while heel synchronization is pending or has failed, and reduced temporary allocations in stable scenes. Stable checks perform no additional inventory queries, Papyrus calls or height recalculations.
- Improved node-reference and queued-task cleanup during actor unload and save transitions so old work cannot block height monitoring in the next scene.
- Preserved existing BodyMorph, visibility, strip/redress, dye and native/DAV/DAVE paths. No new mandatory dependency; existing settings, kits, saves and optional patches remain compatible.

## Updating

Install the full package and keep existing settings, kits and saves. Only
SFSCore.dll changes in the runtime payload. Required mods, supported game
runtimes, scripts, helper ESL, public API and save schema are unchanged.
ENB is optional. Independent ReShade techniques are not controlled by this fix.

The optional OStim and OStim Standalone patches remain v1.0.0. Install only
one matching patch; existing patch users do not need to reinstall it.

## Verification

The SE/AE-exclusive DLL build, 45 regression executables and source-boundary
checks pass. Recording-provider tests cover 3,336 heel-root checks, legacy/public
RaceMenu routes, delayed rescans, scene options, large-scene coverage, stale-task
cancellation, repeated unload/reset, and native/ENB UI restoration.

Only scene participants with SFS-owned heels are monitored. A stable small-scene
check body allocates no temporary actor list, but task scheduling and lifetime
checks still have a cost. No world-actor or inventory polling was added.

These are source/host regressions, not measured in-game FPS or confirmation of
the reporter's exact animation setup. Arbitrary external rotation/scale
composition and every historical RaceMenu binary are not individually verified.
See [implementation and audit](HIGH-HEEL-RESCAN-AND-MENU-DOF.md).
