# SexLab height and hidden actual-gear redress follow-up — v1.7.7

## Evidence

- Ordinary SexLab 1.63 source: eeveelo/SexLab tag 1.63, commit
  `0b1044e83e8aec29cd2344076322f252774eae4d`.
- TuLED: base framework 1.63 plus enabled SLU+ 2024.4.7, with effective
  Hentairim/SLSO/Hakkey script overrides. This is not an unmodified 1.63 setup.
  The effective alias writes/removes the NPC / SexLab.esm position from
  UpdateNiOHeelEffect and equipment callbacks, as well as scene cleanup.
- TAKEALOOK: effective P+ DLL/scripts 2.17.1, not an inferred 2.18 package.
  StripByData/StripByDataEx and QueueNiNodeUpdate are preserved; the ordinary
  scene-event and RemoveHeelEffect property contracts remain available.
- RaceMenu's official NiOverride AddNodeTransformPosition / RemoveNodeTransformPosition
  static contracts are observed through the existing selective native observer.
  Exact owner, return and parameter types are checked; calls/results are not replaced.

SFS previously read the SexLab position during reconciliation but did not
observe subsequent writes. A production-source test first completes a correct
ON reconciliation, then performs an unobserved late SexLab removal: the actor
becomes raised again with no queued repair. Observing the completed original
native rearms one actor-local sync and makes that same test pass. This proves a
code-level stale-writer mechanism, not the reporter's exact in-game sequence.

## Scope

- Resolve the option through the engine VM property accessor. Keep scene
  membership even when the property is temporarily unavailable, preserving a
  known policy and avoiding an invented ON setting for new unknown scenes.
- When OFF, distinguish physical slot-37 footwear from virtual worn tokens or
  renderer hiding. Only an existing non-initializing inventory lookup is used,
  and only for scene actors with SFS-owned heel state. ON does not depend on
  proving physical barefoot state.
- Rearm after foot equipment changes and successful third-person NPC /
  SexLab.esm position writes, independently of the selected strip-link mode.
  Other keys, first-person transforms and non-SFS heels do not enroll work.
- Reuse bounded/coalesced heel work and current-data follow-ups for legacy
  callbacks. No new polling, caller-chain tracing, actual gear mutation,
  SexLab transform-key writes, BodyMorph ownership or framework script override.
- Include saved actual-only eye state when enrolling equipment refreshes, and
  reapply it after redress. Dirty the existing DAVE payload even when identical;
  DAV/native receive the existing equipment-change refresh reason. Keep the
  no-extra-rebuild path for event-added scene props and DD/SGO hand-offs.
- No save schema, ESL, runtime script, public API layout, optional patch or
  dependency version changes.

## Verification boundary

The SE/AE-only DLL builds with the pinned dependency closure. All 44 regression
executables and source-boundary checks pass. Focused production-source tests:
236 heel-sync checks, 1,255 root/lifecycle checks, 820 late-transform observer
checks, 26 scene-event checks and 148 equipment-dispatch checks.

Tests cover ON/OFF, missing/existing/late correction, retained/stripped feet,
redress OFF, actual-only hidden helmets, actor isolation, pending recovery,
legacy/public transform routes, 128-cycle cleanup, unload/revert and original
native result preservation. OStim script/session contracts and existing
BodyMorph, dye, DD, P+, UI, API, runtime-layout and performance gates also pass.

Providers at engine/VM/render boundaries are recording fakes. No new-DLL
in-game animation run, complete external-binary matrix or reporter-specific
reproduction is claimed. Tests/analysis artifacts are not linked into the DLL.
