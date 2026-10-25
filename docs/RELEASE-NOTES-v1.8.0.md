# Skyrim Fitting System v1.8.0 SE-AE

## Changes

- Fixed SDTA-only registered high heels being omitted from height selection and SexLab scene correction.
- Unified HH_OFFSET and SDTA handling, including mixed-source priority, child-node metadata, and valid zero or negative offsets. SDTA X/Y values are preserved while scene compensation adjusts height.
- Preserved existing strip/redress behavior, including redress OFF and manual visibility controls, across native/DAV/DAVE and legacy/public RaceMenu routes. Actual equipment and BodyMorph ownership are unchanged.
- Removed the redundant HH_OFFSET-only reader and reused the existing synchronization and cleanup paths. Stable-frame checks add no metadata parsing, inventory queries or Papyrus calls.

## Updating

Update: Only SFSCore.dll changes from v1.7.9. Keep existing settings, kits and saves. Requirements, supported game runtimes, scripts, helper ESL, public API and save schema are unchanged. Optional OStim / OStim Standalone patches remain v1.0.0; existing patch users do not need to reinstall them.

## Verification

Validation: SE/AE-exclusive DLL build, 45 regression executables and source-boundary checks passed. The SDTA gap is verified in production-source tests; the reporter's exact footwear format is unknown, so this does not confirm resolution of that particular in-game report.

Tests cover both metadata formats, same-node and parent/child priority, full XYZ selection, RemoveHeelEffect ON/OFF, retained/stripped physical footwear, late rescans, bounded failure/recovery, manual redress and 128-cycle unload cleanup. Stable-frame tests count metadata reads and ordinary C++ allocations separately from task scheduling. They do not measure in-game FPS or emulate every historical RaceMenu binary or arbitrary external rotation/scale composition.

The additional v1.7.8 feedback about any real footwear restoring alignment is checked as an event-order scenario, not treated as proof of SDTA use. The existing inverse SexLab-Z compensation remains in place. See [implementation and audit](HIGH-HEEL-RESCAN-AND-MENU-DOF.md).
