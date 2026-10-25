# High-heel full-rescan and menu depth-of-field follow-up

## v1.8.0 follow-up: retry scheduling, 2026-10-25

Re-examining the report that *any* physical footwear restores alignment found
two concrete gaps in the HH_OFFSET/SDTA synchronization path:

1. `QueueHighHeelSyncTask` put failed attempts straight back into SKSE's task
   queue. `BSTaskPool::ProcessTasks` drains until that queue is empty, including
   tasks appended by another task. The initial attempt and both "frame" retries
   could therefore finish in one queue turn, before a pending/reparented
   registered attachment became ready. The root could expire and synchronization
   stop before the next rendered frame. The previous recording scheduler drained
   a snapshot of the queue, accidentally making every task handoff look like a
   separate frame.
2. The legacy NiTransform 1/2 Papyrus route ended immediately when a successful
   `HasNodeTransformPosition` query returned false after its first full update.
   The public route retried that same missing-source result. This left older
   providers waiting for a new attachment/equipment/scene notification rather
   than retrying initial readiness.

The SKSE task-loop source was read from
[ianpatt/skse64, Hooks_Threads.cpp](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_Threads.cpp),
Git blob `06dc3bcbcdfdff5e89d48ec5511a85e97000d448` (source evidence only; no
dependency upgrade). Ordinary SexLab 1.63's physical-footwear guard and the
locally installed TuLED SLU+ alias's `OnObjectEquipped`/`UpdateNiOHeelEffect`
were also rechecked. A physical shoe can cause a new equipment/attachment
update; its own HH value need not explain the recovery. This establishes a
matching missed-update mechanism, **not proof that it is the only cause in
the reporter's unobserved installation**.

The existing Present hook now hands deferred retry tickets to the game task.
Only actor IDs, generation tickets and remaining attempt counts are retained in
the retry map, with no additional scene-node ownership. Successful initial
updates still finish immediately. Failures retain the existing initial-plus-two
budget, but retry dispatches require later Present calls. Legacy missing-source
results use that same budget, after removing the neutral bootstrap. Unreadable
Papyrus results and failed dispatches are not mistaken for valid false results.

Retry execution re-resolves current scene membership, actual footwear and
registered visibility. Scene end or hiding an appearance during the wait cannot
restore stripped equipment or reuse an old suppression target. Actor unload,
delete, completion and load/revert clear pending tickets; dispatched stale
generations cannot clear a replacement. No extra inventory/VM/metadata work is
performed on stable frames. The added inactive cost is an atomic flag check;
only a pending bounded retry takes the retry lock/dispatch path. BodyMorph,
strip/redress ownership, backend selection and height arithmetic are unchanged.

Before-fix failures: `build/heel-frame-before-test.log` (same-turn root expiry)
and `build/heel-legacy-before-test.log` (legacy missing-source early completion).
The focused recording-provider tests now cover HH_OFFSET and SDTA, option
ON/OFF, a later graft without a second callback or physical footwear, provider
readiness on the next frame, never-ready exhaustion, scene end/hide while
waiting, 128 unload cycles, stale dispatch after reset and 1,024 idle frames.
Routes 1/2/3/4/99 are API-route fixtures, not all historical RaceMenu DLLs.
No reporter-environment gameplay verification or MO2 deployment was performed
during this audit. The fixes are included in the refreshed v1.8.0 package.

Validation completed: all 45 regression executables and the source-boundary
checks passed (`build/heel-frame-regressions.log`), including 10,988 heel-root,
244 sync, 820 late-transform and 26 scene-event assertions. The SE/AE-only
development DLL built successfully (`build/heel-frame-build.log`, version string
`1.8.0-heel-frame-dev`; SE/AE enabled, VR disabled). The original v1.8.0 archives
were left unchanged at that audit stage; the subsequent release refresh uses
a fresh official-version build.

## v1.8.0 detailed audit: morph, legacy cleanup and dye, 2026-10-25

The follow-up audit found three additional reproducible defects:

- **Late BodyMorph graft:** `QueuePendingMorphSync` had the same recursive
  SKSE-queue retry as the heel path. A full queue drain could expire an observed
  but not-yet-attached morph root before a later graft. The production tracking
  fixture failed before the fix (`build/heel-detail-before-RaceMenuMorphTrackingTests.log`).
  Only retries now wait for Present; ordinary live updates remain immediate.
  Deferred closures capture IDs, tickets and retry counts, not nodes or morph
  values. Reattachment applies current values, and hiding retains the existing
  missed-update intent without writing hidden nodes.
- **Legacy bootstrap cancellation:** NiTransform 1/2 could accept the neutral
  `NPC/SFS_HH_SYNC` scale and then skip removal when unload/reset invalidated
  the chain. The cancellation fixture failed before the fix
  (`build/heel-detail-before-RaceMenuHighHeelRootTests.log`). Removal now follows
  creation immediately, before the scan or any position query. Cleanup still
  runs for a resolvable actor after cancellation; subsequent scan/write work
  remains ticket- and 3D-guarded. This reorders the same successful-chain calls,
  rather than adding a polling or per-frame Papyrus path. It never removes
  RaceMenu's `internal` or SexLab's named key as cancellation cleanup.
- **Dye 3D readiness:** saved tint restoration likewise exhausted all readiness
  attempts in one queue turn when the actor had no loaded 3D. The production
  queue fixture failed before the fix (`build/heel-detail-dye-before-test.log`).
  These retries now cross Present with the existing initial-plus-two budget.
  Present only hands off a closure; actor lookup, shape selection and restoration
  remain on the original game-task path. Exact component matching, color recipes
  and GPU binding/restoration logic are unchanged.

For the legacy reorder, upstream component-removal and full-scan implementations
were inspected in both `87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc`
(`skee/NiTransformInterface.cpp`) and
`9ebcb733e17be695f994cd2e9cc383043446bc02`
(`skee64/NiTransformInterface.cpp`). Removing a scale component erases that
variant but retains the actor storage entry required by the full scan. The
change does not depend on keeping the neutral scale through the scan.

Additional lifecycle coverage includes 128 morph and dye unload/reload cycles,
retry handoff before cancellation, reuse of the same actor ID, notification-burst
coalescing, hidden/reactivated morphs, legacy cancellation at ten VM boundaries
with and without a replacement request, save reset, and 1,024 idle-frame checks.
Pending closures clear on unload/reset; tickets reject work already handed off.
The dye fixture counts shared resource ownership and preserves another actor's
targets and saved colors. This is a fake engine/provider test, not a GPU driver
or in-game measurement. Each inactive dispatcher only reads an atomic flag.

Source review also covered equipment-event refresh ordering, Native/DAV/DAVE
follow-up scheduling, SexLab membership and option reads, OStim patch contracts,
and scene-resource cleanup. No additional confirmed defect was found in those
reviewed paths. All 45 regression executables and source-boundary checks passed
(`build/heel-detail-regressions.log`), including 11,388 heel-root, 244 sync and
820 late-transform checks. Runtime layout, actual equipment conflict, HT2 hair,
strip/redress, condition/UI, IED, final-rendered API and dye checks are included
in that suite; they are not evidence that every external mod binary or gameplay
combination was exercised. Reporter-environment gameplay remains unverified.

The SE/AE-only development DLL also compiled and linked successfully
(`build/heel-detail-build.log`, version string `1.8.0-heel-detail-dev`, PE version
1.8.0.0, SHA-256
`9E2D36125C48BFE16C11B7CE986BE86C616BC71AC61C1B030A52523BCEC9CD44`).
The development DLL above is audit evidence, not the release binary. The user
subsequently requested these changes in v1.8.0: rebuild with the official
`1.8.0` version string, preserve the original packages locally, and refresh the
runtime/source ZIPs, checksums, release tag and bilingual notes. `VERSION`
remains 1.8.0. No MO2 deployment is part of this refresh.

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

## Registered SDTA position selection — 2026-10-25

The next report still describes raised actors after v1.7.9. The reporter's
footwear and metadata format are unknown. A separate, reproducible source gap
is confirmed: registered roots were enrolled and selected only by HH_OFFSET,
although cleanup already recognized actual-equipment SDTA NPC positions. An
SDTA-only registered heel therefore never reached the existing scene correction.
The production-source regression fails before the fix at registered-height
restoration (`build/sdta-before-test.log`). This is evidence of that missing
path, not proof of the unknown reporter's exact configuration.

Registration, selected position and remaining actual sources now share one
metadata reader. It reads finite HH_OFFSET or the SDTA NPC `pos` array, keeps
SDTA X/Y, and preserves RaceMenu's same-object SDTA-then-HH and
parent-before-child ordering. Explicit zero and negative values remain valid.
Other-node, rotation-only and scale-only SDTA records do not enroll heel roots.
The previous HH-only walker is removed; there is no second persistent metadata
cache, extra polling loop, actor scan, or new engine/VM interface.

This follows the already-audited RaceMenu source commit
`9ebcb733e17be695f994cd2e9cc383043446bc02`:
`SkeletonExtenderInterface::ReadTransforms` and
`NiTransformInterface::SetTransforms`. The existing legacy/public synchronization
routes carry full XYZ positions; only the Z component is replaced by the scene
correction. Actual equipment, SexLab's key and options, morph/dye consumers,
strip/redress ownership and the bounded retry/generation cancellation rules are
unchanged. Cleanup applies the same metadata precedence when preserving actual
equipment, excluding only registered roots whose actual visibility also ended.

The existing scene-policy, first-apply verification, late rescan, failed-provider
and 128-cycle unload/reset checks run for both formats on interface routes
1/2/3/4/compatible-prefix 99. Additional checks cover native/DAV before/after
capture and the attachment callback with SDTA on a child shape, pre-graft
completion, different actual footwear, nonzero X/Y, current metadata edits,
manual redress, hidden stale attachments, mixed-source precedence and invalid
metadata. Expected transform-provider scan results are supplied separately from
the production metadata parser, so parser output is not its own oracle.

Stable-frame assertions include metadata-read counters: no additional metadata
walk/JSON parse, inventory query, transform operation or Papyrus call, and no
ordinary C++ allocation within the small-scene check body. Counters and reference
probes exist only in test executables. This does not measure in-game frame times
or prove every RaceMenu binary or external rotation/scale composition.

Verification: all 45 regression executables and source-boundary checks pass
(`build/sdta-regressions.log`), including 6,058 heel-root, 244 synchronization,
820 late-transform and 26 SexLab event assertions. The SE/AE-only development
DLL builds with label `1.7.9-sdta-height-dev` (`build/sdta-plugin-build.log`);
SE/AE defines with VR disabled select `EXCLUSIVE_SKYRIM_FLAT`. Its SHA-256 is
`6B0C0D1ED0E4A6B9934CB475068EC25A37C141906A4583D519F00E9DF2A960DA`.
Published v1.7.9 release/source ZIPs are unchanged. This development build has
not been deployed to MO2 or published; no in-game reproduction is claimed.

## v1.8.0 release and additional v1.7.8 feedback — 2026-10-25

The added feedback says that stripping physical footwear leaves the registered
heel's actor raised with RemoveHeelEffect ON or OFF. Hiding the registered heel
or equipping any real footwear, including flat shoes, restores alignment;
retaining footwear from scene start avoids the problem. This is consistent
with reconciliation timing/ownership trouble, but does not identify the shoe's
metadata format or prove which missing update occurred.

The proposed `internal Z = -SexLab.esm Z` compensation was already present in
v1.7.8. Reintroducing that formula alone cannot explain a fix. Two independently
verified gaps are distinguished: v1.7.8 could accept a height overwritten
between initial application and baseline capture (fixed in v1.7.9), and
SDTA-only registered roots were not selected at all (fixed in v1.8.0). The old
initial-overwrite failure remains recorded in `build/heel-initial-before-test.log`.
Neither gap is asserted to be the exact cause on the reporter's machine.

An additional production-source regression now executes the real slot-37
equipment event body, rather than substituting a direct queue request for it.
For both metadata formats, ON/OFF, initially retained/stripped footwear and
legacy/public interface routes, it verifies initial correction before any
manual hide/equip, subsequent flat/heeled real-shoe equip and strip, registered
hide/show, scene exit and reference/task cleanup. ON compensates regardless
of real footwear; OFF retains the existing visible-heel policy while real
footwear is worn and compensates when physical feet are bare. No game animation
or external framework binary is emulated by these recording providers.

Final v1.8.0 verification: the SE/AE-only release build and all 45 regression
executables plus source-boundary checks pass. Logs are `build/v1.8.0-build.log`
and `build/v1.8.0-regressions.log`; heel-root assertions increase to 6,818,
with 244 synchronization, 820 late-transform and 26 scene-event assertions.
The final `1.8.0` DLL SHA-256 is
`888352C440309C872FA85E87C320B5DEDD3781CAD31B62308F48EACAFC4B4A32`.
`scripts/verify-v1.8.0-release.ps1` checks corresponding source, release documents,
ZIP hashes and the core-DLL-only runtime change against v1.7.9. Published
v1.7.9 archives and optional OStim v1.0.0 patch bytes are preserved. MO2
deployment is not included in this release task.
