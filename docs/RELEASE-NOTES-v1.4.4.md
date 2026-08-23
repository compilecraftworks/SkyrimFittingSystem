# Skyrim Fitting System v1.4.4

## Open Animation Replacer visual-equipment conditions

- Added an optional OAR public-API integration. When Open Animation Replacer
  is present, SFS registers four read-only custom conditions that evaluate
  each actor's final rendered outfit rather than only technically worn armor.
- Added `SFS_IsShownArmorEquipped`, `SFS_ShownArmorHasKeyword`,
  `SFS_IsShownArmorInSlotHasKeyword`, and `SFS_IsShownBodyNaked` for OAR
  animation packs and small config-only compatibility patches.
- Final displayed equipment includes visible actual armor and visible
  registered appearances. Actual armor and registered appearances hidden by
  SFS are excluded, so a hidden body item with no shown body appearance is
  correctly treated as naked.
- SFS does not hook OAR's `GetWornArmor`, mutate ARMO keywords, change actual
  equipment, or load OAR. If OAR is absent or its Conditions API is not
  available, SFS skips registration and retains its normal behavior.

See [OpenAnimationReplacer-Conditions.md](OpenAnimationReplacer-Conditions.md)
for condition names and OAR JSON examples.

## Dynamic Feminine Female Modesty Animations OAR compatibility patch

- Added a separate FOMOD patch for DFFMA OAR 4.30. It replaces only the
  slot-specific visual-equipment checks in the selected Player/NPC/GS Hovering
  configuration JSON files.
- The patch retains every original slot, keyword, negation, and unrelated OAR
  condition. It does not include or replace DFFMA animations, meshes, scripts,
  DLLs, or plugins.

## Separate compatibility patches

- Added a Grid Inventory Costume replacement-DLL patch for its verified
  upstream revision. A selected Grid Costume now replaces only the player's
  SFS registered base appearances with that Costume's armour layout; switching
  Costume off clears those player appearances. Without SFS, Grid Inventory
  keeps its ordinary behavior.
- Updated the Helmet Toggle 2 script patch to derive its mask from HT2's own
  final managed-headgear array. It forwards only the actor-local HT2 slots:
  Head 30, Hair 31, Circlet 42, Beard/Mask 44, and the player-only Face/Mask
  55 when present. Shown transitions clear only HT2's temporary SFS
  suppression; saved SFS manual-eye state is not changed.
- The HT2 bridge resolves that controller mask through the actor's current
  Mod-Configured (virtual-token), Automatic Vanilla-Slot, or Direct override
  mapping. It does not take ownership of real equipment or alter DAVE, DAV,
  or Skyrim-native rendering.
- Added a Dynamic Footprints SKSE BASE v3.0 patch. It reads a visible SFS
  slot-37 footwear appearance only for Dynamic Footprints' own footwear
  classification and falls back to actual footwear otherwise. The patch is
  disabled automatically unless the exact verified Dynamic Footprints DLL hash
  and code signature match.

## Final validation boundary

- The v1.4.4 core completed a full `releasedbg` build and Kit Generator logic
  regression tests. Static review confirmed actor-keyed transaction, temporary
  visibility, preview, and refresh state across Mod-Configured, Vanilla, and
  Direct linking paths.
- DAVE, DAV, and Skyrim-native backends share that actor-local state but still
  require in-game coverage in every installed backend. The optional patches
  also require their respective in-game smoke tests before a public upload.
