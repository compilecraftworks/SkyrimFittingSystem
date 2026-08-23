Legacy Skyrim Fitting System - Grid Inventory Costume Compatibility Patch v1.4.4

Status
------
This replacement-DLL patch is retired.  Do not install it with SFS v1.4.5 or
later: Grid Inventory v1.4.1+ broadcasts its public Costume state directly and
SFSCore receives that message without replacing GridInventory.dll.

Keep this source only when reproducing an older SFS v1.4.4 installation with
the exact historical Grid Inventory revision below.

Requirements
------------
- Skyrim Fitting System v1.4.4 only.
- Grid Inventory built from upstream commit:
  799b9c680d448b524d8e122a37def9870ccae97e

Installation
------------
1. Install Grid Inventory normally.
2. Install this patch after Grid Inventory and allow only
   SKSE/Plugins/GridInventory.dll to overwrite the original DLL.
3. Keep SFS installed normally.  If SFS is absent, the replacement DLL keeps
   Grid Inventory's ordinary Costume behavior and performs no SFS calls.

Behavior
--------
When Grid Inventory has finished applying a selected Costume tab, this patch
passes that tab's current armour FormIDs to SFS through the optional SFS Core
API.  SFS then replaces the player's registered base appearances with the
Costume's armour layout.  Entries that are not armour are ignored.

When the Costume is switched off, or its selected tab is removed, SFS clears
the player's registered base appearances.  Grid Inventory's own Costume
renderer, real equipment, anchors, loadout data, and save records remain
owned by Grid Inventory and are not changed by SFS.

Safety boundary
---------------
- The bridge uses GetModuleHandleW/GetProcAddress only.  It never loads SFS.
- Missing SFS, an incompatible SFS API, or an early plugin-load order makes
  the bridge a no-op; it retries on the next Costume application.
- Repeated Grid re-dress passes for the same Costume do not rewrite SFS rows.
- The bridge affects only the player.  It does not alter global SFS rows or
  any NPC/follower registration.

Version scope
-------------
This is a replacement-DLL patch for the exact upstream Grid Inventory commit
listed above.  Do not use it with another Grid Inventory DLL revision.  When
Grid Inventory updates, disable this patch until a matching rebuild is made.
It connects only the completed Costume layout to SFS.  It is not a fix for
Grid Inventory's own Costume UI, rendering, loadout, save, or mod-conflict
problems.

Source
------
Grid Inventory is GPL-3.0.  The exact upstream commit and the complete source
change used for this build are included under source/.  Apply
source/grid-inventory-sfs-costume.patch to that revision to reproduce the
replacement DLL.
