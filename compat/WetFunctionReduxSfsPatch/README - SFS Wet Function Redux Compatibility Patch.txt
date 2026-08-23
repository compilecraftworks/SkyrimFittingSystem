Skyrim Fitting System - Wet Function Redux Visual Effect Patch v1.2.0

Requires:
- Skyrim Fitting System v1.1.0 or newer with the SFS visual equipment query API.
- Wet Function Redux SE.

For Wet Function's Naked auto-apply check to follow SFS's final displayed
body state without replacing WetFunctionMCM.pex, use SFS v1.4.0 or newer.

Install after Wet Function Redux SE so this patch overwrites only
WetFunctionEffect.pex.

Do NOT install or overwrite WetFunctionMCM.pex.  Wet Function Redux's own
current MCM script must remain in control of its RaceMenu/NiOverride checks
and its saved settings.

If Wet Function reports that RaceMenu/NiOverride is "broken", that warning
comes from Wet Function's older built-in version-check path, not from SFS
visual equipment handling.  Keep Wet Function Redux's current MCM files
installed and troubleshoot that warning, including any UBE/RaceMenu setup
issue, with the Wet Function/RaceMenu setup rather than SFS.

What this patch does:
- Wet Function can apply body/hand/feet skin overrides on slots that SFS has
  visually exposed, even when the real equipment is still technically worn.

Current SFS releases supply the final displayed body answer for Wet Function's
normal WornHasKeyword naked check.  No WetFunctionMCM override is required.

This patch does not unequip equipment and does not change armor stats,
keywords, enchantments, RaceMenu files, or other gameplay effects.
