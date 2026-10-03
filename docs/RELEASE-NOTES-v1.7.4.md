# Skyrim Fitting System v1.7.4 SE-AE

## Changes

- Fixed hidden actual equipment being omitted from vanilla equipment-conflict checks, which could allow multiple items in the same physical armor slot.
- Restored the missing actually worn item as input to the original engine check. Skyrim still owns slot validation, quest restrictions and equipment replacement; SFS does not add a separate equip/unequip policy or change inventory, worn flags or armor slot masks.
- Used the same guarded engine-input correction for native and DAV paths while preserving DAVE's existing conflict-processing hook. Registered-appearance slot rules and rendering remain unchanged.
- Kept RaceMenu/NiOverride interfaces, BodyMorph, high heels, dye, stripping/redress, IED and the rendered-outfit API unchanged. No inventory-menu scan, periodic actor scan or retained actor/item cache was added.

## Updating

Install the full package and retain existing settings, kits and saves. Requirements, game-runtime support, helper ESL, scripts and optional compatibility patches are unchanged. Only SFSCore.dll changes in the runtime package.

This is not an automatic cleanup of already stacked equipment from an older build. If a slot already contains duplicates, unequip those items once before retesting normal replacement.

## Validation

The SE/AE-only release build, 37 regression executables and source checks passed. Read-only main-menu inspection verified the relevant engine code on SE 1.5.97 and AE 1.6.1170. Production resolver and x64-stub tests cover register/flag preservation, actor ownership, unchanged biped data and 4,096 physical-slot transitions. Existing native/DAV/DAVE and RaceMenu regressions also pass.

Other supported runtimes use the same guarded adapter; they were not individually run in game. Actual gameplay replacement with the new DLL remains unverified. Automated tests and main-menu code inspection are not a guarantee for every mod combination.

[Technical evidence and scope](ACTUAL-EQUIPMENT-CONFLICT-FIX-2026-10-03.md)
