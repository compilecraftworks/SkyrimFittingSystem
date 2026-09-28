# SkyUI-related query-cost reduction — 2026-09-29

Implemented in the existing v1.7.2 working tree. This is the first stage of
[the alternative design](SKYUI-QUERY-ALTERNATIVE-PLAN-2026-09-29.md), not a claim
that the reporter's approximately four-second menu delay is fully resolved.
No in-game timing or reporter-environment reproduction was performed.

## Implementation

- Hidden-real slot queries, native/DAV/DAVE worn-mask projection and the custom
  skinning visitor now share their existing request-local EquippedArmorSnapshot
  with BuildDisplaySet. The IED visitor route and mask combination rules are
  unchanged. The snapshot is destroyed after the request; no later query can
  reuse its inventory state. RefreshArmorFor's separate, potentially mutating
  backend work was not made to reuse a pre-mutation snapshot.
- Final-outfit and body-keyword queries also move the visible-actual list
  already computed by their own BuildDisplaySet/PrepareOutfitValue invocation,
  avoiding a second visibility filter/vector construction. This is a transient
  local output, not a read from the public provider cache. No output (unmanaged,
  inactive or nested production) retains the original fresh fallback; a valid
  empty list stays distinct from missing output. Each build resets the output.
- AcquireExecutableConditionById returns only the owned TESCondition pointer
  from the same materialization cache. The UI accessor still returns the full
  signature, display CNF and refresh targets. Native display, workbench, DD and
  condition polling use the lightweight accessor without changing condition
  eligibility, evaluation counts, error handling or polling cadence. Failed
  materialization still retries on the same invalidation boundaries as before.
- Papyrus global-type inspection uses four-way LRU buckets with the same total
  128 retained types. Pointer ownership, VM/table/revision stamps, first-call
  synchronous installation, post-link scanning, late binding and P+ member
  retries remain intact. Eviction and reset release references outside the
  memo lock. No per-frame actor scan, logging or new timer was added.

Internal nudity detection still computes SFS display state directly. It does
not query BCNG, another mod, or SFS's public AcquirePublished endpoint. The
rendered-outfit public ABI/provider, NotReady contract and IED routing were not
modified. No suppression of WornHasKeyword during inventory/dialogue was added.

## Reproducible host-test evidence

| Query/test | Before | After |
| --- | --- | --- |
| 1,000 hidden-slot queries | 2,000 worn collections | 1,000 |
| 1,000 native worn-mask queries | 2,000 collections | 1,000 |
| 1,000 DAV/DAVE worn-mask queries | 3,000 collections | 1,000 |
| 1,000 Wet-style hidden-slot + displayed-slot pairs | 3,000 collections | 2,000 |
| 1,000 active body/final-state queries | 2,000 visible-actual list constructions | 1,000; still a fresh decision per query |
| 10,000 warm executable condition accesses | full UI metadata returned/copied per access | zero display-CNF copies, zero emission/target rebuilds |
| 1,024 alternating queries over four settled colliding types | direct-mapped collisions can repeatedly rescan | zero native rescans in this fixture |

These are production-function host tests with controlled engine boundaries,
not Skyrim menu milliseconds. The condition fixture explicitly counts metadata
copies and checks full UI reads, dependent invalidation, failed-emission retry,
deletion, owning engine strings and concurrent cache reset. Type tests cover
LRU overflow/recheck, invalid neighbors, 4,096 transient types with at most 128
retained owners and final release outside the lock. Worn tests cover next-query
equip/hide/strip changes, shared slots, HT2 hair release and unmanaged/null guards.

All **35** targets in tests/run-fast-regressions.ps1 plus source-wiring checks
passed after the accompanying [heel fix](HH-PENDING-ATTACHMENT-FIX-2026-09-29.md).
The StateBoundary path test initially returned Access Denied in the sandbox;
the exact same read-only test and full suite passed outside that sandbox. No
production path validation was weakened to make that test pass.

Build: pinned xmake 3.1.0, MSVC 14.51.36231, vendored CommonLibSSE-NG v6.7.0 and
its existing dependency closure; SE/AE flat configuration, skyrim_vr=false.
Main target SkyrimFittingSystem built successfully in releasedbg mode.
Final combined DLL SHA-256:
`51B3B1086CC3CFF53AA0B8728C0A46199A513017EE73EF0A419514767C0FA93C`.

## Not implemented / not claimed

The second-stage cross-query display read view is **not implemented**. Each
body-keyword query still evaluates the current display decision. In particular,
unnotified engine condition changes, random/self-referential conditions and
pre-task equipment changes must not be hidden by a stale published API result.
No TTL/frame cache or vanilla-answer shortcut was substituted. Thus repeated
body queries may still scale with item-list work; the first-stage tests do not
meet the second-stage goal of eliminating all per-item full recomputation.

The separate heel request is documented independently. No new version number,
MO2 installation, release/source ZIP, GitHub publication or Nexus update was
performed by this implementation turn. Existing unrelated UI/kit changes remain.
