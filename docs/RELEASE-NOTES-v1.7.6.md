# Skyrim Fitting System v1.7.6 SE-AE

## Changes

- Added official optional OStim and OStim Standalone Strip Link patches v1.0.0, requiring SFS v1.7.6 or later.
- Connect Mod Settings and Vanilla Slots strip/redress linking, including both Direct Edit bases. Preserve slot protection, NoStrip and each framework's wig policy.
- Support full, partial and animated appearance redress with actor/session-scoped ownership. Keep actual equipment and OStim's equipment arrays under OStim's control.
- Move automatic vanilla wig linking to LongHair slot 41 without changing 31+42 helmet handling or explicit saved mappings.
- Hide pure registered slot-41 wigs under visible hair-covering helmets and restore them when the helmet is hidden or removed, including Helmet Toggle 2. Preserve manual hiding and locked appearances.
- Retain existing native/DAV/DAVE rendering, BodyMorph, high heels, dye, conditions, IED and public API behavior. No periodic actor/inventory scan or new mandatory dependency was added.

## Updating and optional patches

Install the full SFS package; keep existing settings, kits and saves. Required
mods and supported game-runtime list are unchanged. OStim is not a prerequisite
for SFS. The core runtime changes SFSCore.dll and the third-party notice only;
its helper ESL, existing PEX scripts and other runtime assets remain unchanged.

For OStim integration, install exactly ONE separate v1.0.0 patch after SFS
v1.7.6 or later and the matching framework:

- **SFS - OStim Strip Link Patch:** archived OStim's Quest-based OUndress contract.
- **SFS - OStim Standalone Strip Link Patch:** Standalone's static OUndress contract.

The patch must win OUndress.pex conflicts in MO2/Vortex. Restart or reload after
enabling/removing it; OStim caches UsePapyrusUndressing once per game load.
If another mod replaces OUndress, merge its changes rather than enabling two
overrides. No OStim DLL replacement, new ESP/quest or compile-only runtime
imports are included. Each patch includes installation guidance, license,
modified source and SFSOStimBridge. See the
[patch README](../compat/OStimSfsPatch/README.txt).

Vanilla wig linking changes only the automatic appearance anchor. Actual
equipment masks, helmet 31+42 handling, explicitly saved slot-31 mappings and
Do Not Link remain unchanged. Pure slot-41 wig occlusion is a temporary final
display decision, not a mutation of saved rows or manual visibility.

## Verification

SE/AE-exclusive core build, both Papyrus variants and 42 regression executables
plus source-boundary checks pass. The consumed public API and Papyrus contracts
were compared against immutable upstream snapshots. Production-source checks
cover NoStrip/wigs, mappings, protection, ownership, partial redress, saved
session identity, transaction rebasing, late redress, removed NPCs and 128
scene transitions. The full scene adapter verifies exact int32 IDs, patch
activation and absence gates, membership and no duplicate listener growth.
Vanilla wig linking adds 3,367 checks and 128 transitions; long-hair projection
covers player/NPC helmets, visibility, locks and mixed-slot exclusions.

These are source/host checks with recording engine boundaries, not in-game
scene emulation. Real animation timing, scene save/load and every external
OStim/add-on or game/RaceMenu binary were not individually exercised.
The official packages are not labeled test builds; this validation boundary
does not assert universal gameplay proof.

[Detailed implementation and pinned contracts](OStim-Strip-Link-Implementation-2026-10-09.md)
