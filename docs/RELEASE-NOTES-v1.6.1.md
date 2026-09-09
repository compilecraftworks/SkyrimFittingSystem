# Skyrim Fitting System v1.6.1

Fixes a v1.6.0 code path that could prevent registered wigs and clothing from
being attached when another plugin had already installed an unrecognized
custom-skin CALL hook. SFS now preserves the original game's visitor for an
opaque hook and continues its registered-appearance attachment pass.

- Retains full filtering for verified engine routes and the existing safe
  engine-filter/actor-refresh route for verified IED chains.
- Filters hidden actual equipment in opaque CALL chains through the original
  engine visitor's callbacks, preserving its object, vptr, RTTI and fields.
  Thread/visitor-local scopes restore after nested calls and exception unwind;
  inventory, worn flags and unrelated visits are not modified.
- Recognizes longer ENDBR64-prefixed trampolines and complete short trampolines
  at readable page boundaries.
- Adds executable production SE/AE hook tests, including preservation of the
  foreign concrete visitor and registered-appearance attachment arguments.
- Preserves the v1.6.0 save/settings format and existing feature modules.
  No new dependency or companion patch is required. Existing optional patch
  versions remain unchanged.

Startup and compatibility follow-up:

- Retry partially published RaceMenu interfaces/hooks without losing working
  connections. Use the last compatible public prefix for higher interface
  versions, with a logged assumption rather than an upper-version block.
- Retry early DAVE API failure after all DataLoaded listeners before selecting
  fallback ownership. Failed public refresh uses an actor-local engine rebuild
  and retains dye/pose/heel restoration.
- Re-arm saved dye restoration on late registered attachments independently
  of BodyMorph availability.
- Accept Grid Costume's existing prefix with appended fields/unfamiliar ABI
  numbers; retain bounds checks and save-restoration isolation.
- Retry OAR API/condition registration failures at later startup fences without
  repeating successful registrations.
- All 12 fast regression executables pass. RaceMenu history audit covers 19
  source-change snapshots; this is not a binary audit of every distributed ZIP.

Forward-prefix compatibility is an explicit assumption, not a guarantee that
future interfaces cannot reorder methods or change message semantics. See
`RaceMenu-Version-Compatibility-v1.6.1.md` for evidence gaps and exact routes.

The reported user's runtime and mods were unavailable. These defects were
reproduced in production-code tests; the user's specific in-game outcome is
not confirmed. The opaque-chain fix covers calls through the original engine
visitor interface, not arbitrary geometry a foreign plugin renders outside
that interface. Existing vanilla/DAV/DAVE and worn-mask paths remain active.

See `V1.6.1-DISPLAY-REGRESSION-AUDIT.md` for evidence and verification scope.
