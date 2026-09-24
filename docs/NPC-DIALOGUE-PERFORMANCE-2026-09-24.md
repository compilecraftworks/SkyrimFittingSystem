# NPC dialogue performance correction — 2026-09-24

Correction developed on the SFS 1.7.0 source baseline and prepared for release
as SFS 1.7.1 at the user's subsequent request. MO2 deployment is not part of
this release task. Initial local-build evidence is recorded separately below.

## Scope

Remove two confirmed sources of redundant work without disabling final-rendered
body keyword answers or external strip/redress observation. The report of a
brief pause before NPC dialogue has not been reproduced/timed in-game. These
changes must not be described as proof that every reported dialogue stall is
resolved.

The report that an older ESL-free version avoids the stall is a version-boundary
clue, not an isolated ESL experiment: the DLL changed too. The current helper
ESL has only a TES4 header and 32 ARMO records (4,720 bytes; no VMAD, dialogue,
or quest records). Removing it from existing saves is not the proposed fix.

## Changes

### Script type installation inspection

The name-based Papyrus type getter previously traversed every returned linked
type's global function table on every successful lookup and re-enrolled it in
the delayed inspection queue after previous work completed.

The new installation memo retains at most 128 type owners in a fixed direct-
mapped array. It compares VM, live type identity, global table address/count,
and the native-registration/reset revision. Eviction causes another inspection,
not a skipped feature. Retaining the owner prevents a freed type's address from
being mistaken for an inspected object. Load/revert resets release the owners;
native/engine work and retired owner destruction occur outside the memo lock.

- First lookup still installs global hooks synchronously before returning.
- The first post-link barrier always scans again, even with the same table.
- Changed types/tables/counts, newly bound natives, and resets invalidate reuse.
- A successful native bind still patches that function immediately.
- Unlinked types, null entries, missing tables, and failed selected hooks are
  not recorded as completed; bounded retries and subsequent natural lookups
  can recover.
- P+ member observation is **not** certified by the global-table memo. Both
  independent member observers retain their original delayed/natural-lookup
  reinspection, including after retry exhaustion. Generic types still never
  gain an unsafe early member/state-table walk.
- No persistent actor, inventory, appearance, or condition-result cache was
  introduced. No runtime offsets, vtable slots, or collection bounds changed.

### Ordinary Form keyword calls

`Form.HasKeyword`, `GetKeywords`, `GetNumKeywords`, and `GetNthKeyword` now
resolve their Form self before deciding whether to build a caller chain. Only
non-token/null Form reads skip the unused identity work. Actual tokens retain
the existing contextual source resolution. Worn queries, mutation evidence,
catalog/array filters, DD, P+, and strip/redress handling retain their routing.

`WornHasKeyword` and the engine final-rendered body decision are unchanged.
No stale body-result cache, new caller depth cutoff, mode restriction, or
time-based suppression of gameplay checks was added.

## Reproducible verification

Use the repository-pinned xmake 3.1.0 and existing dependency closure. New
`PapyrusObserverInspectionTests` compiles the actual production traversal,
memo, queue, type hook, and registration hook against controlled VM/native
patch/task boundaries. The test worker delay is shortened to 1 ms; this is
not a game-timing benchmark. `CallerChainPerformanceTests` compiles actual
operation preparation/caller routing with controlled Form and frame values.

Controlled workload: one already-linked type with 12 globals, 256 lookups in
two batches separated by queue draining:

| Work | Before | After |
| --- | ---: | ---: |
| Function entry patch probes | 3,096 | 24 |
| Immediate full table inspections | 256 | 1 |
| Deferred full table inspections | 2 | 1 |
| Deferred task batches | 2 | 1 |

The after case still performs constant-time signature checks on each lookup.
Four ordinary Form keyword queries, each repeated 256 times, no longer build
caller identities; their token counterparts still do. The existing deep
trusted-caller and 24-distinct-identity behavior is retained.

Additional cases cover failed/missing original lookup, new type with same name,
table growth/relocation, in-place late native binding, failed native binding,
unlinked/missing-table recovery, failed selected-hook recovery, late null-entry
publication, P+ retry exhaustion/recovery, empty types acquiring globals,
different VMs, reset during scanning, stale completion, and 4,096 transient
types with bounded ownership plus complete reset release.

`tests/run-fast-regressions.ps1` includes the new executable and guards the
production load/reset connection. Runtime ABI, actual mod combinations, and
the dialogue timing itself still require an in-game comparison using the same
save, mod configuration, and NPC interaction before/after the correction.

## Completed local checks

- Windows x64 release DLL built successfully with the pinned dependency closure.
  Compile metadata enables SE and AE only (the CommonLib flat-exclusive layout);
  no VR build or ABI layout changes.
- All 29 regression executables and the fast runner's static wiring/package
  checks passed on the final source. This includes rendered-outfit API, IED,
  DAVE/RaceMenu initialization, morph/high-heel ownership, manual visibility,
  condition UI, strip-link rules, and save/resource lifecycle coverage.
- The initial sandbox run rejected ordinary nonexistent kit paths with Windows
  `Access is denied`. The identical `StateBoundaryTests` executable passed
  outside the sandbox; the complete final suite also passed there. No kit path
  production code or test assertion was weakened to obtain a pass.
- The DLL retains all 16 existing exports. Local DLL SHA-256:
  `2BF6E0A262E2787AC9B60580FB15F0BB370C42ABA5E21E0C41FFF82D1825F028`.
- The published-version runtime ZIP is unchanged (SHA-256
  `7F9FA86BCFDB98D4DAAA939113B760E34ACC7D863FEBBF4B11F2BB88423BA0BE`).
  At that initial verification milestone, MO2 was not updated and no version
  bump or publication had been performed. The subsequent 1.7.1 package uses
  its own freshly versioned DLL; the old 1.7.0 ZIP is retained unchanged.
- Xmake dropped an existing Xbyak recipe commit field while regenerating lock
  metadata; the original exact commit pin was restored. No dependency version
  or source baseline was upgraded/downgraded.

## SFS 1.7.1 release verification

Both freshly versioned `release` and `releasedbg` DLLs built as 1.7.1.0.
All 29 regression executables and source checks passed in both configurations.
The two modes retain the same 16 exports and SE/AE-only compiler settings.
The packaged `releasedbg` DLL SHA-256 is
`6F9F9D3BFB88E438C038348749AD46FFD57468CA42FED33DA05F87CE85954653`.
PDBs remain local. Package verification is reproducible with
`scripts/verify-v1.7.1-release.ps1`: it compares the runtime against 1.7.0,
checks corresponding source/build/release files, and validates ZIP checksums
and dist copies. These release checks do not add in-game timing evidence.
