# High-heel full-rescan and menu depth-of-field follow-up

Released in v1.7.8 as a follow-up to v1.7.7. No optional patch, save schema, ESL or Papyrus
script changes. The v1.7.7 release archives remain unchanged.

## High-heel rescan

RaceMenu `NiTransformInterface::SetTransforms` rescans HH_OFFSET/SDTA and can
replace `internal` after SFS has completed scene reconciliation. Its producers
include ActorUpdateManager object-load/flush, the native reference-update task,
and UpdateAllReferenceTransforms. These do not necessarily write SexLab's key
or issue another attachment notification. The v1.7.7 position-key observer
therefore does not cover this sequence.

Source reference: expired6978/SKSE64Plugins
`9ebcb733e17be695f994cd2e9cc383043446bc02`, skee64/NiTransformInterface.cpp and
ActorUpdateManager.cpp. Ordinary SexLab option reference remains
eeveelo/SexLab 1.63 `0b1044e83e8aec29cd2344076322f252774eae4d`.

The fix retains the resolved third-person NPC node only for scene participants
whose registered heel is being reconciled. After successful public or legacy
reconciliation it remembers the resulting local Z. The existing Present path
schedules at most one outstanding game-task check; a stable participant requires
only lookup/parent-attachment checks and a scalar comparison. There is no world
actor scan, inventory query, Papyrus dispatch or transform recalculation on a
stable frame. With no watched participant the fast path is an atomic read.

The final audit also removed avoidable check overhead. Only armed watches enable
frame scheduling; waiting for a legacy callback does not. Up to 16 participants
use stack storage for the task's ID snapshot, with an uncapped vector fallback
for larger scenes. This does not cap the actor count or reduce check frequency.
The SKSE task handoff, actor lookup, attachment validation and locks still have
a cost; this is not a zero-cost or allocation-free scheduling claim.

A changed result requests the existing bounded/coalesced reconciliation using
current display state, physical footwear policy and current named transforms.
It never writes the remembered skeleton height back. This avoids overwriting a
new external-mod height and avoids new private RaceMenu ABI hooks. Existing
public v3-compatible and legacy v1/v2 Papyrus transform routes are retained.
Ordinary SexLab, SLU+ and P+ share the existing scene-membership adapter; OStim
scene and strip/redress ownership are unchanged.

Watches are generation-fenced, skip in-flight synchronization, disarm before
requesting repair, and rearm only on success. Provider failure cannot create a
self-retrying reconciliation loop. Scene end, detached nodes, actor unload or
delete, unresolved actors and load/revert release the retained node. Failure
does not disable future event-triggered synchronization.

Failed/no-op completion now releases its unarmed watch immediately. Completion
also covers a disarmed watch from the preceding generation when the actor goes
unavailable before repair starts. The added test first failed at that boundary,
then passed after cleanup was tied to the current completion ticket rather than
requiring the old watch's generation to match. Stale completions still cannot
remove a newer watch. Global save/reset now moves morph and heel root ownership
out of the maps before releasing final references outside both SFS locks, just
like actor-local unload. This is lifecycle cleanup, not removal of live morphs.

Frame-task ownership is fenced by the load epoch, not a bare queued boolean.
Reset cancels that ownership even if the engine discards the old queued task;
an old task delivered later cannot clear a new save's pending-check guard.
Scene/queue cancellation is done under both existing mutexes so a legacy
completion cannot rearm an old watch between those cleanup steps.

This is a scene-scoped per-frame check, not a claim of zero polling. A changed
height is repaired asynchronously by the next available game/VM tasks; a brief
transient before those tasks execute is possible. Tests do not model arbitrary
external scale/rotation composition or prove the reporter's gameplay timing.

## Menu depth of field

The engine ImageSpaceEffectDepthOfField IsActive slot (6 in CommonLibSSE-NG
6.7.0's flat SE/AE layout) retains its original call and returns inactive while
SFS is open. No engine setting or image-space data is overwritten. Hide, game
load transition and window shutdown clear the gate before early returns.
Camera side, rotation, FOV, pause and SmoothCam restoration are unchanged.

ENB is optional, discovered once by its exported SDK entry points. The adapter
uses the 1000/1001 compatible C ABI (24-byte parameter, 32-bit BOOL data, SDK
family 1xxx) and only EFFECT/EnableDepthOfField. Declaration pin:
[ENBSeriesSDK.h](https://github.com/clayne/skyrim-ao-toggler/blob/ee16b0b5bd317d91ed73a283c4f301328cbe208d/include/API/ENBSeriesSDK.h),
commit `ee16b0b5bd317d91ed73a283c4f301328cbe208d`. Original SDK by Boris
Vorontsov; this project declares only the required ABI, without copying the
wrapper, loading another DLL or changing build dependencies.

Parameter access occurs only in ENB callbacks: BeginFrame temporarily suppresses,
EndFrame restores, and PreSave/PreReset/OnExit restore before serialization or
teardown. PostLoad discards stale restoration when a new preset replaces it.
Disabled settings remain disabled, invalid/missing parameters are untouched,
and failed restorations retry on a later callback even after SFS closes. SFS
does not write a preset or INI file. Closed stable menus issue no parameter calls.

This covers native game DoF and ENB's documented DoF switch. Independent ReShade
techniques or blur embedded in an arbitrary external shader are not controlled.
The reporter's graphics provider has not been identified, so their visual
result still requires in-game confirmation.

## Validation

Production-source recording-provider tests exercise late full rescans without
an attachment/SexLab-key event, ON/OFF and stripped/retained footwear, current
external keys, stable-frame cost, coalescing, repeated scene/unload cycles and
revert. Interface routes 1, 2, 3, 4 and compatible-prefix 99 use the same tests.
These are route tests, not proof of all historical RaceMenu binaries.

Menu tests execute the production engine dispatch and ENB parameter callbacks
through recording providers: 128 open/close cycles, frame restoration, disabled
settings, preset save/load, renderer teardown, failed writes/reads, invalid
parameter type, and no closed-menu API traffic. The SE/AE-only releasedbg DLL
build and all 45 regression executables plus source-boundary checks passed on
2026-10-17. The log is `build/high-heel-menu-dof-regressions.log` (local only).
No in-game test is claimed.

### Detailed follow-up audit, 2026-10-17

- Final full-suite log: `build/high-heel-audit-regressions.log` (local only).
  All 45 executables and source-boundary checks pass. Heel-root checks: 3,336;
  additional synchronization checks: 236; late-transform observer: 820;
  SexLab event adapter: 26. These are assertions against recording providers,
  not thousands of distinct game configurations.
- The actual stable check body makes zero ordinary C++ allocations for 256
  consecutive small-scene frames per tested interface route. The fake scheduler's
  task allocation is excluded explicitly. No inventory query, Papyrus dispatch
  or transform rebuild occurs in those checks.
- Legacy-wait, failed-provider, and 1,024 inactive-frame cases enqueue no new
  frame checks. Thirty-three simultaneous participants prove the inline storage
  capacity is not a scan limit. Missing/unloaded participants release their own
  watches without stopping the others; repeated unload/reset releases references.
- A final-reference callback probes both SFS mutexes from another thread and
  verifies that morph roots, heel roots and watched skeletons are released
  outside the locks on global reset. Allocation counters and lock probes exist
  only in the test executable, not in `SFSCore.dll`.
- Re-reviewed the attachment callback's independent consumers: native/DAV
  before/after capture, late DAVE/RaceMenu attachment, live morph requests, saved
  dye restoration and heel selection. No new coupling, forced actor-3D rebuild,
  equipment mutation or additional inventory-menu handler was introduced.
- The broad suite covers physical slot conflicts, actual/registered visibility,
  HT2 hair masks, IED routing, strip/redress including DD/P+ and OStim ownership,
  rendered-outfit API, dye lifecycle, condition input/transactions, kit grouping,
  list-only filters, keyboard/gamepad routing, UI restoration and runtime/ABI
  boundaries. Those subsystems retain their existing behavior; their entire
  source trees and every external binary combination were not re-proven.

The neutral RaceMenu bootstrap, public/legacy routes, late-attachment observer,
actor-lifetime checks, generation cancellation and required vtable declarations
are functional compatibility code, not disposable diagnostics. They remain.
No arbitrary polling delay, global transform reset, scene actor cap, or unrelated
feature disablement was added as a performance shortcut.

Remaining limits: no measured in-game frame times, reporter save/animation run,
arbitrary external rotation/scale composition, or complete historical RaceMenu
binary matrix. This audit does not turn the earlier timing/visual limits into a
claim of complete gameplay verification. The audit was performed before the
v1.7.8 version bump; release builds repeat the regression suite. Packaging is
checked by `scripts/verify-v1.7.8-release.ps1`. Existing v1.7.7 archives and
optional patch bytes are preserved. MO2 deployment is not part of this release task.

## v1.7.9 initial-application verification — 2026-10-24

The v1.7.8 watch recorded whatever NPC local Z existed at completion. A newly
added production-source regression interleaves a RaceMenu full rescan after
the selected internal position is applied but before that baseline is recorded.
The old code accepts the raised result as stable, so subsequent unchanged-frame
checks cannot correct it. The failing run is in
`build/heel-initial-before-test.log`. This is a reproducible code-level sequence,
not proof that the reporter's installed binaries used this exact interleaving.

The follow-up reads back the selected internal X/Y/Z after the existing node
update and, when scene suppression is required, verifies the current SexLab Z
still matches its complement. The NPC node is sampled before those readbacks;
the watch publishes that sample only if its identity and finite Z are still
consistent at completion. It no longer takes a fresh, unverified height as its
baseline after the reads. Failed removal is also distinguished from successful
internal-position cleanup. An invalid legacy Boolean result is not interpreted
as evidence that the position was removed.

Public v3-compatible and legacy v1/v2 routes use the same positional tolerance
and the existing initial-plus-two-retry budget, on the same generation ticket.
Retry resolves current metadata, physical footwear and scene policy; it does
not replay an old target or begin an unlimited chain of fresh requests. Ordinary
events remain able to retry after failure. Stable frame checks are unchanged:
no added inventory reads, transform readbacks, writes or Papyrus calls.

Legacy completion samples scene identity on a game task and uses the existing
documented NiOverride position queries. The request holds only a non-dereferenced
identity pointer and scalar sample, not another owning node reference. Load,
unload and actor generation cancellation still fence pending callbacks. The
bootstrap is removed before verification, including a failed verification.

This relies on RaceMenu's synchronous local-update contract (pinned source
`9ebcb733e17be695f994cd2e9cc383043446bc02`,
`NiTransformInterface::Impl_UpdateNodeTransforms`): the queued task refreshes
world data, not HH_OFFSET selection. It verifies accepted position values and
the corresponding stable local-node sample; it does not independently emulate
all external rotation/scale composition, the renderer or an arbitrary modified
provider that violates that contract. No direct skeleton write, private ABI,
external transform-key edit or additional frame polling was introduced.

Regression additions cover initial overwrite, lost position writes, changes
between apply/readback, scene end, strip during verification, redress OFF,
manual redress, bounded persistent failure, later recovery, stable-frame cost
and unload with legacy verification outstanding. Existing body-morph,
native/DAV/DAVE attachment, footwear policy and strip/redress owners are unchanged.
The two root-based test targets now have separate generated-source directories
to avoid parallel builders overwriting the same test include; production
build/layout configuration is unchanged.

Final verification: all 45 regression executables and source-boundary checks
pass (`build/heel-initial-final-regressions.log`), including 3,466 heel-root,
244 synchronization, 820 late-transform and 26 scene-event assertions. The
SE/AE-only development DLL also builds (`build/heel-initial-plugin-build.log`),
with diagnostic version label `1.7.8-height-verification-dev`. Its SHA-256 is
`EBC5B33DDD041530A930100BB9917B3A78E6C589C0E53963032E0F448AF7953F`.
The extra reads occur during synchronization; stable-frame counters remain
unchanged. This is not an in-game FPS benchmark or an animation reproduction.

The v1.7.9 release repeats the full regression suite and SE/AE-only build with
the final version label. Release logs are `build/v1.7.9-regressions.log` and
`build/v1.7.9-build.log`; `scripts/verify-v1.7.9-release.ps1` checks corresponding
source, package hashes and the core-DLL-only runtime change. Published v1.7.8
archives and OStim v1.0.0 patch bytes remain unchanged. MO2 deployment is not
part of this release task.
