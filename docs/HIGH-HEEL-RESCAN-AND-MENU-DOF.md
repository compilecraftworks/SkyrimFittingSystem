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
