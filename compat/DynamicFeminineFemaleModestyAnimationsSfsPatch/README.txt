Dynamic Feminine Female Modesty Animations OAR - SFS Displayed Outfit Patch

Requirements
- Skyrim Fitting System 1.4.4 or newer.
- Open Animation Replacer.
- Dynamic Feminine Female Modesty Animations OAR 4.30.

Install this FOMOD after Dynamic Feminine Female Modesty Animations OAR.
Choose the same Player, NPC, and optional GS Hovering configurations selected
when installing DFFMA.  The patch contains only replacement OAR config.json
files; it has no DLL, ESP, PEX, animation, mesh, or texture files.

What it changes
- Replaces DFFMA's IsWornInSlotHasKeyword checks with
  SFS_IsShownArmorInSlotHasKeyword.
- The existing Slot, Keyword, and negated values are retained exactly.
- DFFMA therefore evaluates the actor-local final displayed outfit, including
  visible SFS registered appearances, instead of only Actor::GetWornArmor().

What it does not change
- No actual equipment, inventory, armor keyword, animation, or save data is
  changed.
- DFFMA's unrelated OAR conditions and all other DFFMA dependencies remain
  under DFFMA's normal control.
- This is a displayed-outfit condition bridge only.  It does not repair
  DFFMA's own animation, OAR-rule, mesh, save, or mod-conflict problems.

If DFFMA is updated, reinstall the matching version of this patch or wait for
an updated patch.  Do not install this over a different DFFMA configuration
version without checking its JSON layout.
