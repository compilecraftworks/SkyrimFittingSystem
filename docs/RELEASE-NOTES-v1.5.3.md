# Skyrim Fitting System v1.5.3

## SexLab P+ strip/redress

- Added built-in integration for SexLab P+ v2.12.0 on Skyrim 1.5.97 and
  1.6.1170. No P+ script, ESP, or separate SFS patch is replaced or required.
- P+ `StripByData`, `StripByDataEx`, and `UnequipSlots` operations now enter
  the existing actor-local transaction system. Mod-Configured and
  Direct+Mod-Configured keep the non-inventory virtual-token route, while
  Automatic Vanilla and Direct+Vanilla keep the actual-equipment route.
- Player, NPC, and follower state remains isolated. P+ return arrays,
  NoStrip/AlwaysStrip rules, multi-stage merging, and `EquipItemEx` redress
  order are preserved.

## Catalog, workbench, and conditions

- Added a default-on `Show Only Body-Compatible Items` option for Equipment,
  Outfits, and Kits. Filtering is used only for confidently identified actors;
  uncertain or custom actors fail open and show the full same-sex catalog.
- The registered-appearance workbench is never BodyFamily-filtered, so an item
  already worn or otherwise available can always be registered. Actor changes
  rebuild only catalog presentation state.
- Fixed list scroll position resetting when a scrollbar drag was released.
- Fixed a freeze when a custom condition containing a false clause was itself
  referenced as false. Nested negations are folded before bounded CNF
  materialization without changing the saved condition format or evaluation
  meaning.

## RaceMenu, camera, and Fitting Dye

- Expanded RaceMenu compatibility without assuming one ABI: legacy NiOverride
  transform versions use the Papyrus route, public NiTransform v3+ uses its
  interface, and the public BodyMorph C++ path is used only for compatible
  v4/v5 interfaces. Existing DAVE, DAV, native, and actor-local refresh
  boundaries are preserved.
- Fixed Left/Right character framing on later menu openings with pause enabled
  by committing the pending camera update from the first ordinary ready menu
  frame. Existing FOV restoration and paused/unpaused rotation behavior remain
  unchanged.
- Fitting Dye now excludes compact UBE collision-helper names such as
  `ArmColli`, `FeetColli`, and `ButtLegColli`, while leaving similarly named
  real outfit components available.

## Regression boundary

- Added production-rule regression coverage for SexLab P+, all four strip-link
  policies, DAVE/DAV/native dispatch, actor-local BodyMorph/suppression,
  RaceMenu interface routing, condition lowering, catalog/workbench isolation,
  scroll retention, camera commit timing, and dye helper filtering.
- Actual equipment and inventory semantics, registered appearance ownership,
  locks and dye persistence, DD/Pama/Grid/HT2/OAR compatibility, and the
  IED-compatible `VisitWornItems` chain are unchanged.
