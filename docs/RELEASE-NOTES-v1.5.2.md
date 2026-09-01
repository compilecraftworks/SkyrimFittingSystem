# Skyrim Fitting System v1.5.2

## Registered appearance locks

- Registered appearance cards can now be locked or unlocked from their
  right-click context menu. A small lock icon is shown after the name and the
  actor-local state is stored in the SKSE co-save.
- The right-click `Lock`, `Unlock`, and `Dye` labels are consistent across
  English, Korean, and Simplified Chinese.
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

## BodyMorph regression fix

- Live RaceMenu BodyMorph now updates registered appearances while they are
  visible, with actor-local attachment state for the player, NPCs, and
  followers.
- The same behavior is preserved across DAVE, DAV, native refreshes, and
  deferred `UpdateModelWeight` processing. Hide/show and actor switching do
  not leak morph state between actors.

## Input, dye, and packaging

- Workbench kit-list navigation accepts W/S alongside the existing directional
  controls. These navigation keys remain inactive while a real SFS text field
  owns keyboard focus and shows its editing caret.
- Fitting Dye excludes base-body and helper geometry for CBBE/3BA/3BBB,
  UNP/BHUNP, UBE, HIMBO, SAM, and Vanilla body families while keeping actual
  outfit components available.
- Runtime and MO2 packages no longer include the generated PDB. Debug symbols
  remain available in the local build tree for opt-in diagnostics.

## Menu camera and SMP

- Character framing and FOV are now identical with menu pause enabled or
  disabled. SFS uses FOV 70 only while its menu is open and restores the
  original camera and FOV on close.
- SmoothCam control is acquired and returned through its public API boundary.
- While the game is paused, right-drag orbits the camera without moving the
  actor or its SMP roots. This prevents FSMP garments from stretching toward
  their last simulated world position. With pause disabled, right-drag keeps
  the existing live actor rotation and SMP follow behavior.

## Compatibility boundary

- Actor ownership, DAVE/DAV/native separation, BodyMorph and dye state,
  Mod-Configured/Vanilla/Direct strip linking, DD, Pama, Grid Inventory, OAR,
  and Helmet Toggle 2 remain isolated from the new lock and high-heel state.
- The IED-compatible `VisitWornItems` chain and actual inventory/equipment
  semantics are unchanged.
- Added fast regression coverage sharing production decision rules for
  actor-local BodyMorph and suppression state, virtual-token and actual-item
  transactions, all four strip-link policies, and DAVE/DAV/native refresh
  dispatch.
