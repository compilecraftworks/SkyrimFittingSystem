# Skyrim Fitting System v1.6.3

## Changes

- Fixed text-based condition arguments being treated as numbers. GetGraphVariableInt and GetGraphVariableFloat now accept variable names without numeric conversion.
- Added text input and native string-argument handling for GetVMScriptVariable and GetVMQuestVariable. Reference/Quest targets remain separate from variable names.
- Kept condition strings alive until the last evaluator releases them, including after draft changes, condition expansion and cache invalidation.
- Added dynamically sized text fields, preserving long names and leading zeros. Embedded NUL characters now produce a localized validation error.
- Unified numeric validation across input, saving and evaluation. Trailing garbage such as 12abc, overflow, NaN and infinity are rejected instead of silently changing the value.
- Fixed invalid Axis values silently becoming Z and unknown ActorValue names being passed as -1. Invalid values remain editable with an error.
- Added English, Korean and Chinese validation messages and VM-variable naming guidance.
- Expanded regression coverage for string lifetime, numeric/selection validation and the requested SE/AE runtime branches. All 16 regression targets passed.

## Compatibility and update notes

No new game runtime or RaceMenu package support is introduced. Skyrim 1.6.678 is the Epic edition and is not supported by official SKSE; an existing SFS routing entry is not a support guarantee. Skyrim 1.7.x and VR are not supported by this release.

RaceMenu integration, BodyMorph refresh routing, DAVE/DAV/native display backends, stripping links, dye and the three optional compatibility patches are unchanged. Keep existing settings, presets and user kits.

VM conditions require exact names of engine-accessible conditional variables. Legacy GetScriptVariable/GetQuestVariable are not converted into Papyrus conditions. Automated checks and ABI/source inspection do not certify every game/RaceMenu combination in gameplay.
