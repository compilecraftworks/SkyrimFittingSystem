# Skyrim Fitting System v1.6.8 SE-AE

## Changes

- Release actor-local registered-appearance morph and high-heel scene references on actual object unload or form deletion. Cancel that scene's pending work by FormID even when the actor is no longer resolvable.
- Release the unloaded actor's dye GPU targets and component preview without deleting saved color recipes. Restore saved colors through the existing bounded load/attachment path.
- Give high-heel and dye restore work unique tickets so an old callback cannot finish or cancel a replacement request for the same actor. Fence in-flight dye/preview builds against cleanup while allowing independent components and actors to continue.
- Preserve morph eligibility across unload so DAVE/RaceMenu reattachment can resume before another SFS refresh. Ordinary hide/show, preview, HT2, camera, strip/redress and backend-refresh paths do not invoke this cleanup.
- Remove the unused partial morph-only forget helper; lifecycle cleanup uses the tested FormID-based entry.

## Compatibility and verification

All 24 regression executables and source wiring checks passed. New coverage includes 128 unload/reload cycles, 128 distinct deleted actors, same-ID replacement, legacy NiOverride callbacks, actor isolation, saved colors, counted resource release, and concurrent build cancellation. These are production-function host tests with fake engine/GPU ownership, not in-game or actual GPU memory measurements.

Only SFSCore.dll changes in the runtime archive compared with v1.6.7. Keep existing settings, kits, saves and optional compatibility patches. Runtime support, RaceMenu routing, DAVE/DAV/native ownership, API v1 and existing exports are unchanged. No new dependency, polling, world scan, forced actor rebuild, or real equipment operation was added.

No in-game testing was performed. This fixes the two lifecycle omissions found after the v1.6.7 audit, not every possible leak/regression or the separately reported SkyUI opening delay.

See [the lifecycle technical note](V1.6.8-ACTOR-RESOURCE-LIFECYCLE.md).
