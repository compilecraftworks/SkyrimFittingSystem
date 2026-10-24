# Skyrim Fitting System v1.7.9 SE-AE

## Changes

- Fixed a path where a RaceMenu height overwrite immediately after initial heel correction could be accepted as the stable height during a SexLab scene.
- Verify the applied automatic heel offset, current SexLab correction and node state before accepting synchronization. If they change during application, resolve the current appearance and scene state again.
- Apply the same verification and bounded retries to legacy and public RaceMenu routes. Persistent failure cannot create an endless retry loop; later normal events can retry.
- Preserve strip/redress behavior, including redress OFF, scene exit and actor unload cleanup. Actual equipment, BodyMorph and native/DAV/DAVE ownership are unchanged.
- Run the additional verification only during height synchronization. Stable-frame checks add no inventory queries, Papyrus calls or height recalculations.

## Updating

Update: Only SFSCore.dll changes from v1.7.8. Keep existing settings, kits and saves. Requirements, supported game runtimes, scripts, helper ESL, public API and save schema are unchanged. Optional OStim / OStim Standalone patches remain v1.0.0; install only the matching one, with no reinstall required for existing patch users.

## Verification

Validation: SE/AE-exclusive DLL build, 45 regression executables and source-boundary checks passed. This covers source/host regression scenarios, not an in-game FPS measurement or reproduction of the reporter's exact animation setup.

Recording-provider tests include 3,466 heel-root, 244 synchronization, 820 late-transform and 26 scene-event assertions. New cases cover first-apply overwrites, ignored writes, changes between application and verification, scene exit, stripping during verification, redress OFF, manual redress, bounded failure/recovery, stable-frame cost and pending legacy callbacks on unload.

The synchronization path performs additional reads, and legacy completion uses a game-task handoff. This is not a claim of zero execution cost. No world-actor scan or additional idle inventory/VM polling was introduced. The code verifies accepted transform values and a consistent finite local-node sample using RaceMenu's update contract; arbitrary external rotation/scale composition, modified providers and every historical RaceMenu binary are not individually verified. See [implementation and audit](HIGH-HEEL-RESCAN-AND-MENU-DOF.md).
