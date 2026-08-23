# Skyrim Fitting System v1.4.6

## Grid Inventory Costume preservation

- Grid Inventory v1.4.1+ Costume synchronization now updates the player's SFS
  registered appearances only for a non-empty Costume containing player-owned
  armour forms.
- Selecting no Costume, clearing a Costume, or sending a layout without a
  usable armour form now leaves the player's saved SFS registered appearances
  unchanged. It no longer clears them.
- The restored state sent after startup, save load, or revert remains ignored;
  the next later non-empty Costume change is the only Grid event that updates
  SFS.
- `SFSCore.dll` still uses only Grid Inventory's public SKSE message. It never
  replaces, loads, hooks, or writes `GridInventory.dll`, Grid loadouts, Grid
  saves, or actual equipment.

## GPL-3 corresponding source

- Starting with v1.4.6, the source download contains the preferred source form
  for this exact release, real `xmake.lua` and lock files, the exact audited
  CommonLibSSE-NG source revision, test sources, and third-party notices.
- The runtime archive also carries `LICENSE` and `THIRD_PARTY_NOTICES.md`.

## Regression boundary

- Mod-Configured Slot Linking, Automatic Vanilla-Slot Linking, Direct Editing,
  external strip/redress handling, and DAVE/DAV/Skyrim-native rendering retain
  their existing actor-local state and code paths.
- The v1.4.6 `releasedbg` build and integrated Kit Generator logic regression
  suite passed.
