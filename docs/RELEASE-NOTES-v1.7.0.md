# Skyrim Fitting System v1.7.0 SE-AE

## Changes

- Fixed an initialization path that skipped worn-mask and registered-appearance hooks when an existing plugin had replaced a supported five-byte CALL site with an E9 inline jump. This could leave the UI functional while appearances and armor hiding did not work, as reported with SFS 1.6.6 on Skyrim SE 1.5.97. The skip was also present in 1.6.9.
- Added actor-selective inline-detour handling for SE and AE custom skinning, worn masks and vanilla armor attachment. Actors without active SFS skinning retain the previous jump, original stack, volatile registers and flags. Active SFS actors use the original engine call plus SFS visibility and attachment logic.
- Retained ordinary E8 provider routing and its concrete-visitor protection. When IED is installed, the active E9 custom-skin path requests evaluation through the existing coalesced, load-canceled actor queue. No IED-specific work is requested when IED is absent.

## Compatibility and scope

**The E9 fallback gives SFS display priority on actors with active SFS skinning.** It does not execute an unknown prior inline body for those actors, and cannot promise to retain all behavior of another appearance mod controlling the same actor. It never calls an inline-jump body as though it were a normal C++ function, guesses its return address, or rewrites that provider's code. Passive actors retain that provider's own continuation. The existing supported runtime table and DAVE/DAV/native backend selection are unchanged; this is not expanded Skyrim runtime support.

Compared with 1.6.9, only SFSCore.dll changes in the runtime package. The source package's license notice is updated to 1.7.0. Scripts, helper ESL, locales, settings/save formats, API exports/ABI, RaceMenu interfaces and optional compatibility patches are unchanged. No actual equipment operations, new scan/poll loop, background worker, growing actor history or runtime diagnostic probe were added. Existing settings, kits, saves and optional patches can be retained.

The Wet Function Redux Visual Effect Patch v1.2.0 contains a Papyrus script override and a README, not a native DLL. It does not directly install the native E9 hooks in the report. The available log does not identify their actual provider; do not treat Wet Function as the established cause.

## Verification

The original E9 skip failed a production-stub reproduction before the fix. The expanded test executes the actual emitted SE/AE hook code and production visitor routing against controlled engine/provider objects: 2,600 cases, including 128-cycle active/passive and hidden/visible transitions per route. It checks registered attachment, mask output, vanilla armor arguments, IED present/absent behavior, E8 routes, and exact passive E9 stack/volatile-register/flag preservation with a deliberately clobbering predicate.

All 28 regression executables and source checks passed in release and releasedbg. The SE/AE-only DLL built successfully, reports 1.7.0.0 and retains all 16 export names. Details and package checks are recorded in [the technical note](V1.7.0-INLINE-DETOUR-FIX.md). These are host tests, not an in-game reproduction with the reporter's unidentified hook provider. No universal inter-mod compatibility or zero-regression claim is made.
