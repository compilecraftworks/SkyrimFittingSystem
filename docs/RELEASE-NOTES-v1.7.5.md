# Skyrim Fitting System v1.7.5 SE-AE

## Changes

- Fixed a registered-high-heel height path that could leave actors elevated during SexLab animations after actual footwear was stripped.
- Follow SexLab's existing RemoveHeelEffect option and NPC height correction. SFS-owned automatic heel height now complements that correction rather than defeating it or applying it twice; SexLab's own transform key is not modified.
- Clear stale registered height when its appearance is hidden, even if a display backend has not finished removing the old node. Preserve visible actual-footwear HH_OFFSET/SDTA height, including the same item still displayed as actual gear and SDTA X/Y components.
- Apply the same policy through legacy NiOverride and public NiTransform across native, DAV and DAVE. Keep footwear-strip OFF, flat footwear, scene end with redress OFF, manual appearance visibility and other mods' named transforms covered by regression checks.
- Observe ordinary SexLab/P+ scene events only for scene participants; clear scene ownership on end, deletion and save/reset boundaries. No periodic actor scan, inventory-menu scan, forced 3D refresh, actual equip/unequip or SexLab script replacement was added.

## Updating

Install the full package and keep existing settings, kits and saves. Only SFSCore.dll changes in the runtime package. Requirements, supported game-runtime list, helper ESL, scripts, public API and optional compatibility patches are unchanged. SexLab and RaceMenu have not become new mandatory prerequisites.

The animation compensation follows SexLab's RemoveHeelEffect setting; it does not impose a new heel-removal option on scenes where that setting is disabled. Appearance strip/redress policy and actual-equipment ownership remain unchanged.

## Validation

The SE/AE-only release build, 38 regression executables and source checks passed. Production-source tests include 236 high-heel synchronization checks, 1,195 root/metadata/lifecycle checks and 19 SexLab event-adapter checks. Legacy NiTransform 1/2 and public 3/4 routes are exercised with recording providers; higher-version compatible-prefix handling is tested with a model, not an unreleased binary.

Coverage includes missing/existing/stale SexLab correction, stripped or retained footwear, flat shoes, hidden late roots, actual footwear, named third-party offsets, callback failure/cancellation, redress OFF, scene reassignment, deleted actors and load epochs. Existing BodyMorph, dye, actual-equipment conflicts, strip/DD/P+, IED, UI, API and performance regressions also pass.

No new-DLL in-game animation test was performed. The fix uses the shared supported-runtime adapter and CommonLib's flat-layout alias accessors; this does not mean every game/RaceMenu/SexLab binary or mod combination was individually run.

[Technical evidence and scope](SEXLAB-HIGH-HEEL-FIX-2026-10-08.md)
