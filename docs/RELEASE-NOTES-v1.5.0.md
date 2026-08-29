# Skyrim Fitting System v1.5.0

## Fitting Dye

- Added Fitting Dye to every registered-appearance card in the workbench base
  and condition areas. Open it with the dedicated dye action or the card's
  right-click context menu.
- The popup lists only rendered components belonging to that exact registered
  ARMO. Selecting a component makes it pulse briefly on the actor before a dye
  color is applied, and Restore original color removes that component's saved
  tint.
- Dye is stored as actor FormID -> registered appearance ARMO -> exact rendered
  component identity. The same mesh or diffuse texture on another actor or
  appearance remains original, while zero or multiple exact matches fail
  closed instead of tinting a guessed component.
- RGB tinting uses a private GPU texture and a geometry-scoped draw-time
  substitution. Original DDS/NIF files, shader materials, actual ARMO,
  inventory, keywords, equipment state, workbench rows, conditions, and kits
  are never modified.
- Saved colors return only for the same actor, registered appearance, and exact
  component after save loading or an actor 3D refresh. Mod-Configured virtual
  tokens, Automatic Vanilla-Slot Linking, Direct Slot Editing, external
  strip/redress state, and the DAVE/DAV/native display paths remain isolated.

## Helmet Toggle 2 compatibility

- Fixed an SFSCore CTD when switching the workbench from the player to a new
  NPC while HT2 was installed. The redundant active-magic-effect ownership
  fallback was removed; NPC/follower ownership continues to use HT2's resolved
  monitor spells, exact `HT_HeadGearEquipped` AddSpell/RemoveSpell signals, and
  the actor-local FormID cache.
- Replaced the separate Helmet Toggle 2 PEX patch with built-in SFSCore
  integration. SFS observes only HT2's exact player state global and its exact
  actor-local NPC/follower spell transitions; it never replaces or edits HT2
  scripts, calls, real equipment, or DAVE variants.
- The integration derives SFS visibility transitions from each real helmet's
  full ARMO slot mask instead of the array position retained by HT2 after
  deduplication.
- A still-equipped real headgear can keep Hair 31 set in Skyrim's worn mask
  after HT2 replaces its mesh with a hidden DAVE variant. SFS now mirrors the
  exact actor-local hidden state into the skinning pass and releases only that
  real Hair partition, fixing bald actors with real 31+42 helmets. The ARMO,
  inventory, keywords, HT2 calls, and DAVE variant remain untouched.
- Pure registered slot-31 appearances are still not suppressed by HT2. A
  visible registered slot-31 wig retains Hair ownership, while a registered
  31+42 card continues to follow through slot 42.
- Player, follower, and NPC transitions remain actor-local, with no periodic
  polling or surrounding-actor scan. An unloaded NPC records only its state
  and is rendered when its own 3D is later rebuilt. Mod-Configured
  Slot Linking, Automatic Vanilla-Slot Linking, Direct Slot Editing, manual
  eye controls, external strip/redress handling, and the DAVE/DAV/native
  renderer paths retain their existing behavior. Mod-Configured headgear
  virtual tokens and their strip/redress transactions are unchanged.

The main SFS runtime is rebuilt as v1.5.0. Completely remove any older
**SFS Helmet Toggle 2 Compatibility Patch**; install Helmet Toggle 2 itself
normally. No separate SFS patch is required.
