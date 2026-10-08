# Registered high heels and SexLab animation compensation — v1.7.5

## Evidence and cause

The official SexLab actor-alias source at commit
`c3100de5bfd2681d4b5f0d1e9172ccf0015a7aa7` gates its height removal on both
`Config.RemoveHeelEffect` and `ActorRef.GetWornForm(0x00000080)` after stripping.
If actual footwear was removed, that worn-form check can skip the correction
even though SFS registered footwear still supplies automatic height.
The locally inspected SLU+ alias uses the same check and ignores an equipment
removal callback while `IsStripping` is true.

[Official actor-alias source](https://github.com/eeveelo/SexLab/blob/c3100de5bfd2681d4b5f0d1e9172ccf0015a7aa7/scripts/Source/sslActorAlias.psc#L1731)

SexLab writes its correction under `NPC` / `SexLab.esm`; RaceMenu's automatic
equippable position is `NPC` / `internal`. SFS must account for both rather
than overwrite SexLab's key or blindly write zero in the presence of an
existing negative correction.

SFS also previously deferred height removal while a hidden registered root
remained attached. Completion retries could expire without clearing height,
and later node removal did not necessarily queue another update. The unchanged
production functions reproduced both paths in host tests before the fix. This
establishes code-level causes, not the reporter's exact installation or onset.

## Correction and ownership

- Track scene actor membership separately from equipment strip/redress state.
  Ordinary `AnimationStart`, `StageStart`, `AnimationChange`, `PositionChange`,
  `ActorChangeEnd` and `AnimationEnd` SendModEvent names are observed through the
  existing event sink, including the contract retained by P+.
- On an SKSE game task, require a quest originating in `SexLab.esm`, read
  `sslSystemConfig.RemoveHeelEffect` from quest `0xD62`, and enumerate the actual
  reference-alias count with CommonLib's flat-layout accessors. No package-name
  guess, raw VM walk, runtime-specific hardcoded actor-array bound or polling.
- Only SFS-owned automatic height is reconciled. During an enabled scene its Z
  complements the existing SexLab Z (`internal.z = -SexLab.esm.z`), preserving
  the SexLab key and unrelated named offsets/scales. Missing correction yields
  zero automatic Z; existing correction does not produce double lowering.
- After a scene, resolve current display normally. Redress OFF does not restore
  equipment or an appearance. Manual visibility and subsequent resync remain
  usable.
- Hidden registered attachment roots cannot remain height evidence. Exclude
  only roots no longer shown as either registered or actual equipment; preserve
  visible actual HH_OFFSET/SDTA with RaceMenu's metadata traversal order,
  including valid SDTA X/Y components.
- Legacy NiTransform 1/2 uses official NiOverride Papyrus callbacks; public
  NiTransform 3+ uses its existing validated interface prefix. Both share the
  selection policy. Existing callback cancellation/generation guards and
  higher-version compatible-prefix handling are retained.
- Scene reassignment cannot be undone by an old scene's end event. Scene state
  is cleared on end, deletion or save/reset; ordinary 3D unload retains active
  membership so the reloaded actor can be reconciled. No persistent new SFS
  transform key or co-save record is added.

Native/DAV/DAVE attachment routing, BodyMorph observation, dye and actual
equipment ownership are not changed. The change does not add an inventory scan,
world actor scan, forced 3D refresh, equip/unequip command or script replacement.

## Verification boundary

The release is SE/AE-only (`EXCLUSIVE_SKYRIM_FLAT`) and retains the pinned
CommonLibSSE-NG v6.7.0 baseline and explicit supported-runtime table.

38 regression executables and the source gates pass. Focused tests compile
unchanged production synchronization, root-resolution and event-adapter code
against recording engine/VM providers: 236 sync checks, 1,195 root/lifecycle
checks and 19 event checks. Routes include NiTransform 1/2/3/4 and the existing
higher-version prefix policy (version 99 is a test model, not a real binary).

Cases include foot strip ON/OFF, no/existing/stale SexLab correction, flat
footwear, redress OFF/manual display, visible actual gear, SDTA X/Y/Z, hidden
late roots, hide before first SFS sync, third-party named offsets, legacy
callback failure/cancellation, scene ownership cycles, absent SexLab, option
OFF, foreign events, alias clearing at end, actor deletion and load epochs.
Existing morph, dye, strip/P+/DD, actual-slot conflict, IED, UI, API, condition,
kit and performance regressions also pass.

No new-DLL gameplay animation test or complete external-binary matrix was run.
The shared supported-runtime route and tested interface cases are not a claim
that every historical/current/future RaceMenu or SexLab release was executed.
The optional compatibility patches, scripts, ESL, settings and save schema are
unchanged. Temporary diagnosis probe executables/source are excluded from both
the production module and the corresponding source package.
