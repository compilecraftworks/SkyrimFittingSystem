# Skyrim Fitting System v1.6.7 SE-AE

## Changes

- Fixed registered high-heel `HH_OFFSET` selection being overwritten by another scene branch's zero or different automatic height. The selected registered heel now supplies RaceMenu's automatic position without adding a second persistent height offset.
- Restored actor-local height synchronization after late ordinary-gear or non-heel appearance attachments. This addresses an attachment-order path that could briefly apply the heel height and then clear it.
- Applied the correction to both legacy NiOverride Papyrus calls and the public NiTransform interface. Existing RaceMenu version routing and forward-compatible prefix policy are unchanged; no new version whitelist was added.
- Reduced repeated worn-equipment collection in final-outfit/body-keyword queries and registered-armor membership checks. Snapshots are reused only within one query, so later equipment and condition changes remain visible.
- Removed unused caller tracing from Papyrus WornHasKeyword preparation and deduplicated repeated function identities in other caller chains. Strip/redress, virtual-token, DD and P+ caller tracking remains intact, including trusted callers deeper in the stack.
- Added regression coverage for late attachments, conflicting/zero heights, strip/hide and redress/manual restoration, cleanup, deferred changes, callback failures, and request-local inventory snapshots.

## Compatibility and update notes

DAVE/DAV/native backend ownership, live BodyMorph, Fitting Dye, all stripping-link modes, manual visibility, IED integration and rendered-outfit API v1 are preserved. No real inventory equip/unequip operation, periodic actor scan, new dependency or temporary diagnostic hook was added. Keep existing settings and kits. The three optional compatibility patches are unchanged and are not bundled in the core archive.

## Verification and limits

The SE/AE-only 1.6.7.0 DLL builds successfully. All 23 regression executables and source checks passed; the RaceMenu history audit covers 19 interface-change snapshots, including public transform slots and legacy position-call signatures. These are source/ABI/logic checks, not certification of every distributed RaceMenu binary or in-game mod combination.

The runtime package changes only SFSCore.dll compared with v1.6.6. Scripts, the helper ESL, locales and other runtime assets are unchanged. No in-game tests were performed for this release. The inventory-path changes remove verified redundant work, but the reported SkyUI 3–4-second opening delay has not been measured or confirmed eliminated.

See [the technical audit](HH-OFFSET-AND-INVENTORY-AUDIT-2026-09-19.md) for evidence and boundaries.
