# Skyrim Fitting System v1.8.0 SE-AE

## Changes

- Fixed SDTA-only registered high heels being omitted from height selection and SexLab scene correction. HH_OFFSET and SDTA now share metadata selection while preserving SDTA X/Y values.
- Fixed high-heel synchronization retries ending before a registered appearance finished attaching. Legacy and public RaceMenu routes now retry on later frames within the existing retry limit.
- Fixed missed BodyMorph updates on registered appearances that attach later. Current morph values are reapplied when the nodes become ready.
- Fixed saved dye restoration giving up before the actor's 3D became ready.
- Fixed temporary SFS transform values remaining after cancelled legacy RaceMenu synchronization. Pending work is cleared on actor unload and save reset.

## Updating

Refreshed on 2026-10-25. Download v1.8.0 again if you installed the earlier package. The bounded retry fixes preserve existing strip/redress, redress OFF and manual visibility behavior, with no new idle scene scans or Papyrus polling.

Update: Only SFSCore.dll changes from v1.7.9. Keep existing settings, kits and saves. Requirements, supported game runtimes, scripts, helper ESL, public API and save schema are unchanged. Optional OStim / OStim Standalone patches remain v1.0.0; existing patch users do not need to reinstall them.

## Verification

Validation: SE/AE-exclusive DLL build, 45 regression executables and source-boundary checks passed. Coverage includes HH_OFFSET/SDTA, SexLab option ON/OFF, strip/redress, BodyMorph, native/DAV/DAVE paths, 128 unload/reload cycles and idle-work checks. These are production-source tests with recording providers, not confirmation from the reporter's game.

Tests cover both metadata formats, same-node and parent/child priority, full XYZ selection, RemoveHeelEffect ON/OFF, retained/stripped physical footwear, late rescans, bounded failure/recovery, manual redress and 128-cycle unload cleanup. Stable-frame tests count metadata reads and ordinary C++ allocations separately from task scheduling. They do not measure in-game FPS or emulate every historical RaceMenu binary or arbitrary external rotation/scale composition.

The additional v1.7.8 feedback about any real footwear restoring alignment is checked as an event-order scenario, not treated as proof of SDTA use. The existing inverse SexLab-Z compensation remains in place. See [implementation and audit](HIGH-HEEL-RESCAN-AND-MENU-DOF.md).
