Skyrim Fitting System - Helmet Toggle 2 Compatibility Patch v1.4.4

Requirements
------------
- Skyrim Fitting System v1.4.4 or later.
- Helmet Toggle 2.

Installation
------------
1. Install Skyrim Fitting System.
2. Install Helmet Toggle 2 normally.
3. Install this patch after Helmet Toggle 2 and allow it to replace only:
   - HT_PlayerAlias.pex
   - HT_NPCSpellMonitorAliasEffect.pex
   - HT_FollowerSpellMonitorAliasEffect.pex

What it does
------------
Helmet Toggle 2 remains the owner of real helmet, hood, hair, circlet, and
mask equipment.  The patch sends its shown/hidden transition to SFS, and SFS
then applies the transition only to that actor's matching registered
appearance cards.

The patch forwards only the HT2 slots it actually manages for that transition:
head (30), hair (31), circlet (42), mask/beard (44), and, for the player,
face mask (55).  The SFS core resolves those slots against the actor's current
external-strip link mode (mod-configured slots, automatic vanilla-slot
linking, or direct slot editing).  The resulting refresh uses SFS's normal
DAVE, DAV, or native renderer path.  Player, follower, and NPC events remain
actor-local.

The SFS eye buttons remain usable.  A user show override is temporary and is
cleared by Helmet Toggle 2's next authoritative shown transition; neither
the patch nor Helmet Toggle 2 writes SFS's saved card visibility choices.

Save compatibility
------------------
The patch uses the established Helmet Toggle 2 Papyrus entry points.  SFS
v1.4.4 maps those legacy calls to its current actor-local runtime state, so
saves that previously used the old v1.2.x patch do not retain its former
global suppression state.  No new persistent Helmet Toggle 2 state is added.

Scope
-----
This package contains only the three Helmet Toggle 2 script replacements and
their matching source files.  It does not alter Helmet Toggle 2 MCM settings,
plugin records, equipment, keywords, DAVE/DAV settings, or other SFS features.
It receives only HT2's own `HT_HiddenHelmet*` / `HT_HiddenCirclet*` transition
result and forwards the corresponding managed slots to SFS.  It is not a fix
for Helmet Toggle 2's own toggle, equipment, model, MCM, or mod-conflict
problems.
