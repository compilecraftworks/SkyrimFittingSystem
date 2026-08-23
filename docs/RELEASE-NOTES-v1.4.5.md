# Skyrim Fitting System v1.4.5

## Grid Inventory Costume integration

- Added built-in support for the public Grid Inventory v1.4.1 Costume-state
  SKSE message. When a Costume is put on, switched, or removed in Grid
  Inventory, SFS receives the current armour FormIDs and updates only the
  player's registered appearances accordingly.
- The first restored Grid Costume state after startup, save load, or revert is
  intentionally ignored. SFS keeps its saved registered appearances until the
  player changes Costume again in Grid Inventory.
- The integration is inside `SFSCore.dll`.  It copies the borrowed Grid data
  during the message callback, then applies the latest state through SFS's
  existing safe processing point.  It does not replace, load, hook, or import
  `GridInventory.dll`.
- Grid Inventory remains the owner of its Costume renderer, loadouts, saves,
  and real equipment.  SFS changes no actual equipment, global/NPC rows,
  condition definitions, or Grid data.
- Remove the old v1.4.4 **SFS Grid Inventory Costume Compatibility Patch**
  when updating.  Grid Inventory v1.4.1+ now needs no separate SFS patch.

## Dynamic Footprints compatibility patch

- Updated the separate Dynamic Footprints patch to v1.4.5.  Its local
  trampoline is now anchored at the exact verified Dynamic Footprints call
  site, preventing the CommonLib `displacement is out of range` edge case on
  affected DLL load layouts.  Its verified-DLL, byte-signature, and read-only
  Feet 37 boundaries are unchanged.

## Regression boundary

- Mod-Configured Slot Linking, Automatic Vanilla-Slot Linking, Direct Editing,
  external strip/redress handling, and DAVE/DAV/Skyrim-native rendering retain
  their existing actor-local state and code paths.
- Completed a full `releasedbg` build and the Kit Generator logic regression
  test suite.
