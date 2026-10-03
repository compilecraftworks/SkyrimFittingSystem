# Hidden actual equipment: preserve vanilla slot conflicts

## Scope

Actual gloves, necklaces, rings and other physical armor must remain equipped
for vanilla conflict checks when SFS hides their rendering. This change does
not introduce an equip policy, directly call EquipObject/UnequipObject, change
ARMO/ARMA masks or worn flags, or change registered-appearance slot rules.

## Evidence

Read-only decoded code was obtained at the main menu, without a save load,
in-process patching or injected diagnostics:

| Runtime | Address Library function | RVA | Installed backend |
| --- | --- | --- | --- |
| SE 1.5.97 | 36979 | 0x60CA30 | DAVE; its outer conflict predicate is hooked |
| AE 1.6.1170 | 38004 | 0x6A0AA0 | DAV; outer predicate retains the engine CALL |

Both functions obtain the actor's rendered BipedAnim and iterate its first 32
armor objects to find candidate conflicts. The load at +0xC9 is
`mov r12, [rax+r13+0x10]`, with actor=rdi, requested physical slot=ebx and
inner object-byte index=r13. Hidden actual armor can be absent from this
render list. The later native path still selects worn inventory ExtraData,
checks quest restrictions and performs its own unequip. Thus changing worn
flags or adding a second unequip implementation is unnecessary.

The AE GetArmorInSlot function (16113, RVA 0x234000) was also inspected. It
uses an inventory visitor and accepts physical slots 30..61, not render data.
The AE TestBodyPartByIndex function (14119) tests the original biped-form mask.

## Narrow correction

Only the first candidate read for a requested occupied slot can be supplemented:

1. Replay the original load. Later inner iterations return immediately.
2. Require loaded SFS data and actor-owned appearance/hide/condition/preview
   state. Idle actors do not query inventory.
3. Query existing inventory without initialization, once for the requested
   physical slot. Do not evaluate display conditions or rebuild API snapshots.
4. If that actually worn ARMO already occurs in the native 32-object render
   list, keep the original candidate. Otherwise supply that actual ARMO.
5. Resume the original native dynamic cast, mask checks, inventory-instance
   lookup, quest handling, unequip and scene cleanup.

No biped field is written. There is no persistent actor/item cache, timer,
background worker or inventory-menu callback. All other registers, volatile
XMM state and flags are preserved around the five-byte load replacement.
This prevents new hidden-slot conflicts from being missed; it is not a
save-data cleanup pass for already stacked equipment from an older build.

## Backends and versions

- Native and DAV use the same engine input correction.
- DAVE's +0x97 predicate hook is not replaced or called a second time. If it
  resolves a conflict and skips the native inner loop, the SFS resolver is not
  entered. If native processing continues, the same correction is available.
- IED skinning visitor chains are untouched.
- No RaceMenu/NiOverride ABI, BodyMorph, heel, dye or strip/redress code changes.
  The correction does not require RaceMenu or branch on its package version.
- Existing SE/AE runtime profiles supply distinct relocation IDs. Each runtime
  must match the decoded register/load/32-entry loop contract before patching;
  another mod's incompatible instruction is never overwritten.
- Only 1.5.97 and 1.6.1170 were directly observed in running games here. The
  other existing supported profiles share the guarded adapter, not a claim of
  individual historical-binary or in-game verification. No new runtime support
  is added; an allowlist entry alone does not supply SKSE support to Epic.

## Verification and limits

`ActualEquipmentConflictTests` executes the production resolver and x64 stub,
checks GP/XMM/flag preservation and untouched biped bytes, inactive/player/NPC/
legacy-player/preview/condition ownership, one lookup per first candidate,
and all 32 physical slots over 128 cycles. Replacement/quest outcomes are an
explicit native-decision model, not in-game test results. Optional captured
code inputs verify the production installation contract against both live
functions without distributing executable bytes in the repository.

In-game hidden gloves/ring/necklace replacement and retained registered looks
still need testing with the new DLL, including native, DAV and DAVE. Main-menu
code inspection cannot establish the final gameplay result. This change is
packaged as v1.7.4; MO2 installation is not changed by the release operation.

The SE/AE-exclusive release build and all 37 regression executables plus source
boundary checks pass. The production installation contract also passes against
both read-only captured functions. RaceMenu interface/morph/heel, strip/redress,
dye, IED and native/DAV/DAVE skinning regression coverage is retained. These
results do not imply an in-game test of every historical runtime/provider DLL.
