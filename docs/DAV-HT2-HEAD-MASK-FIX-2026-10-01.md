# DAV / DAVE head-mask parity

## Scope

Included in v1.7.3, requested after a face-disappearance report on v1.7.2.
The inspected C:\TAKEALOOK profile uses Skyrim 1.6.1170, DAV 1.0.5 and Helmet
Toggle 2. Its HT2 JSON requests `overrideHead: showAll`. No SFSCore.dll was
found in that installation during inspection. The shared current SFS log was
from TuLED, not TAKEALOOK. No in-game reproduction or deployment was performed.

DAV's official GetWornMask visitor consumes GetBipedObjectSlots, which applies
the active variant's head/hair policy. The corresponding manager implementation
at upstream commit `7789a6e63e670355fd5a38b232066f0583eb17b4` clears the race's
head/hair slots for ShowAll. This source comparison is not a binary execution
test of the installed DLL.

## Confirmed defect and correction

SFS's API-ready DAVE path already preserved the provider's resolved worn mask.
Its API-unavailable DAV path instead ORed the raw visible actual-armor masks
back into that result. A real helmet could therefore restore Head after HT2
had cleared it. The existing HT2 release only removes Hair. The native NPC
preservation helper bypasses players and NPCs with registered headgear, so it
did not protect those cases.

GetDisplayWornMask now uses one provider-result merge for DAV and DAVE:

1. Keep the incoming, variant-resolved mask.
2. Remove only slots exclusively owned by actual armor SFS explicitly hides;
   a visible actual item sharing a slot retains that ownership.
3. Add the currently displayed registered-appearance slots.
4. Apply the unchanged actor-local HT2 Hair release rule.

The helper is renamed MergeVariantResolvedWornMask. The existing DAVE late-API
query remains, but API availability no longer selects a different mask formula.
Native-only reconstruction and its NPC preservation helper remain unchanged.
DAV 3D refresh, DAVE public refresh, real-equipment filtering, hook routing and
backend initialization have not been replaced or disabled. No Equip/Unequip,
inventory mutation, new cache, retained node, task, polling loop or new log site
is introduced. The existing debug mask message now identifies DAV/DAVE.

This is a display-result correction, not SFS taking ownership of HT2's actual
equipment toggle. Existing user-requested real-equipment rendering hide remains.
The independent OCF/SOS classification report is not modified by this change.

## Verification

The new test fails on the original v1.7.2 implementation before the fix:
player, API unavailable, no head fitting, resolved base 0x100C yields 0x100D
instead of 0x100C (Head bit reintroduced). It passes with the common merge.

WornSnapshotRegressionTests now extracts the real
PreserveUnmanagedHeadgearWornMask instead of replacing it with an identity stub.
The real GetDisplayWornMask, collectors and merge rule are exercised against
engine/display-state fixtures. Across 128 cycles, 107,520 mask queries cover:

- player/NPC, DAV -> DAVE -> unavailable API transitions;
- shown and head/hair-cleared provider results;
- no fitting, body, Hair, Hair+Circlet, Head+Circlet, genital and face-jewelry fits;
- actual-equipment hidden/shown and slots shared with another visible item;
- the unchanged HT2 Hair release and registered Hair independence;
- one request-local equipment collection and no actual inventory changes.

Additional controls cover inactive, unmanaged, null and native-only actors.
All 35 regression executables and source-wiring checks passed, including
BodyMorph, heels, dye, strip/redress, conditions, IED and resource lifecycle.
The SE/AE-only releasedbg main DLL build passed with the existing dependency
pins and VR disabled. Local DLL SHA-256:
`BDBAE54A27A8CCC4C53C17652C67B5911C4A7FD607815855A346395EC054E6B7`.

These are host/source tests, not an in-game face/mesh test or proof of every
mod combination. Existing v1.7.2 runtime/source ZIPs and GitHub release remain
unchanged; neither MO2 installation was modified by this fix.

The final v1.7.3 build also includes the separate OCF/SOS classifier correction
and passes all 36 regression executables/source checks. Final release DLL
SHA-256: `391213BB4E89E0A115F1A02E665E1386D83F51C57A0979605B84327613355EEE`.
