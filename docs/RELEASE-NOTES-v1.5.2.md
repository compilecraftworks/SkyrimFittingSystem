# Skyrim Fitting System v1.5.2

## Registered appearance locks

- Registered appearance cards can now be locked or unlocked from their
  right-click context menu. A small lock icon is shown after the name and the
  actor-local state is stored in the SKSE co-save.
- Locked appearances remain in place while previewing or applying Gear,
  Outfits, Conditions, Kits, and Kit Generator results. An incoming multi-slot
  appearance is omitted atomically when any of its displayed slots overlaps a
  locked appearance.
- Automatic Mod-Configured, Vanilla, Direct, DD Hider, and Helmet Toggle 2
  suppression does not hide a locked appearance. Manual eye state and
  condition results retain their existing behavior.

## Registered high heels

- Registered appearances now synchronize mesh `HH_OFFSET` values through
  RaceMenu's public NiTransform interface for the player, NPCs, and followers.
- Synchronization is actor-local and event driven across DAVE, DAV, and native
  display refreshes, including late DAVE attachment callbacks. It does not
  scan nearby actors, equip a proxy item, or alter actual equipment.
- SFS removes only its temporary transform bootstrap and preserves RaceMenu's
  handling of genuinely equipped high heels.

## Input, dye, and packaging

- Workbench kit-list navigation accepts W/S alongside the existing directional
  controls. These navigation keys remain inactive while a real SFS text field
  owns keyboard focus and shows its editing caret.
- Fitting Dye excludes base-body and helper geometry for CBBE/3BA/3BBB,
  UNP/BHUNP, UBE, HIMBO, SAM, and Vanilla body families while keeping actual
  outfit components available.
- Runtime and MO2 packages no longer include the generated PDB. Debug symbols
  remain available in the local build tree for opt-in diagnostics.

## Compatibility boundary

- Actor ownership, DAVE/DAV/native separation, BodyMorph and dye state,
  Mod-Configured/Vanilla/Direct strip linking, DD, Pama, Grid Inventory, OAR,
  and Helmet Toggle 2 remain isolated from the new lock and high-heel state.
- The IED-compatible `VisitWornItems` chain and actual inventory/equipment
  semantics are unchanged.
