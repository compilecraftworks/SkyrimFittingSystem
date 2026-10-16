# Skyrim Fitting System v1.7.7 SE-AE

## Changes

- Fixed additional registered high-heel alignment cases during SexLab scenes, including when RemoveHeelEffect is ON and actual footwear has been stripped.
- Reconcile late SexLab height-correction changes and footwear strip/redress without overwriting SexLab's transform key or other mods' named transforms.
- Fixed hidden actual helmets becoming visible again after redress until the SFS UI was reopened. Reapply the saved visibility state across native, DAV and DAVE.
- Preserve footwear-strip OFF, redress OFF, manual visibility, locked/protected appearances and existing SexLab, SLU+, P+, OStim and DD strip/redress behavior.
- No actual-equipment equip/unequip commands, periodic actor/inventory scans, replacement framework scripts or new mandatory dependencies were added.

## Updating

Install the full package and keep existing settings, kits and saves. Required
mods, supported game runtimes, scripts, helper ESL and save schema are unchanged.
Only SFSCore.dll changes in the runtime package. No separate SexLab patch is
needed for this correction.

The existing optional OStim and OStim Standalone Strip Link patches stay at
v1.0.0 and continue to require SFS v1.7.6 or later. Install exactly one patch
matching your framework; existing patch users do not need to reinstall it.

## Verification

The SE/AE-exclusive build and 44 regression executables plus source-boundary
checks pass. The ordinary SexLab 1.63 source and the effective installed SLU+
2024.4.7 and P+ 2.17.1 contracts were compared. Tests compile production
functions against recording engine/VM/render boundaries; they cover late
correction writes, ON/OFF policy, physical footwear, actual-only hidden gear,
coalesced recovery, actor isolation, 128-cycle cleanup, legacy/public RaceMenu
routes, and existing morph/dye/strip/API/UI behavior.

These checks are not in-game scene emulation or proof that the reporter's
specific animation setup, every game binary or every external mod version was
executed. See [implementation and evidence](SEXLAB-HEIGHT-AND-REDRESS-FOLLOWUP-v1.7.7.md).
