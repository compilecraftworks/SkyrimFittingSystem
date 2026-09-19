# Skyrim Fitting System v1.6.9 SE-AE

## Changes

- Grid Inventory Costume changes replace only the player's base appearances. Conditional appearance cards, NPC/global registrations and locked appearances are preserved. The normal kit projection validates the incoming layout before clearing anything; an unusable layout leaves existing appearances intact.
- Fixed rejected condition-action drops consuming their source or changing only the target's show/hide value. Validate the requested armor before staging either field, then commit target and source together. Valid moves, polarity-only edits, and duplicate/stale conflict handling remain available.
- Equipment refresh and condition polling callbacks now own unique, constant-size queue tickets and capture the load generation before enrollment. An old callback cannot run in a new world or release a replacement task's pending state. Normal coalescing and legitimate follow-up refreshes remain enabled.

## Additional corrections

- Fixed stale SOS resolver callbacks overwriting newer armor results after loading. Both callback stages reject obsolete requests; normal requests, explicit retry and actor isolation remain available.
- UI close restores only controls SFS disabled and respects current engine stored-control locks. Window teardown also releases SFS control ownership.
- Kit saving validates final destinations before creating directories or truncating files. Nested/Unicode collections and ordinary overwrites remain supported; traversal, rooted paths and outside-directory junctions are rejected. Writes retain the original virtual Data path.
- Condition JSON rejects duplicate/reserved definition IDs and overflowing ID counters before store publication. Import staging no longer queues a premature actor refresh. Built-in references, EditorID arguments and unknown external function names are preserved.
- Restored missing Korean/Chinese protected-appearance and condition-not-applied tooltips. Added localized invalid-path messages and locale key-parity checks.

Compared with v1.6.8, only SFSCore.dll and the three locale JSON files change in the runtime package. Keep existing settings, kits, saves and optional patches.

## Scope

No save/settings format, API ABI, supported runtime table, dependency, optional patch, actual inventory operation, RaceMenu routing, or DAVE/DAV/native renderer ownership change. No new world scan, background thread, polling loop or persistent actor/ticket history. Existing morph/high-heel and dye resource cleanup is retained.

These fixes prevent the identified state-loss paths; they cannot reconstruct conditional cards already removed by an earlier version. Restore those from a prior save/export or register them again if needed.

## Verification

All 28 regression executables and source wiring checks passed in release and releasedbg configurations. The SE/AE DLL built successfully, reports 1.6.9.0, and retains all 16 export names.

New production-function host tests cover rejected/valid action drops, unchanged-target polarity edits, conflict rollback, 128 Grid transitions, 128 load boundaries, concurrent queue producers, cancellation during enrollment, unavailable SKSE, and legitimate reentrant follow-ups. Engine layout projection and task execution are controlled test boundaries, not actual gameplay. See [the technical note](V1.6.9-TRANSACTION-REGRESSION-FIXES.md) for evidence and verification limits.

No in-game test, real GPU-memory measurement or SkyUI opening-time benchmark was performed. The separately flagged input-event pointer lifetime risk is not changed in this release.

Additional production-function tests cover 128 SOS reload cycles, both stale callback stages, normal retries, control ownership and condition-store integrity. Internal/external Windows junction checks passed. Engine/VM/ImGui boundaries are fakes. A native plugin disabling an already-disabled control without updating stored state leaves no observable ownership information; this case is not claimed solved. See [state-boundary verification](V1.6.9-STATE-BOUNDARY-FIXES.md).
