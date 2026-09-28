# Registered heel attachment lifetime fix — 2026-09-29

Implemented in the existing v1.7.2 working tree, following
[the read-only follow-up](HH-OFFSET-AND-INVENTORY-FOLLOWUP-2026-09-28.md).

## Confirmed failure and narrow repair

The former resolver deleted any recorded HH_OFFSET root whose parent chain did
not yet reach the actor. If OnAttach preceded scene grafting, the first queued
sync erased that valid source and reported Complete. Grafting the same root
later could not recover it without a new capture. Temporary same-node
detach/reparent had the same problem for previously active heels.

Registered heel records now retain a displayed pending root for the existing
two additional completion attempts. Pending metadata is **not applied while
detached**, and an already active height is not cleared during that short
pending interval. Once attached, selection uses the original finite HH_OFFSET
reader and original public/legacy transform synchronization. On the third
unsuccessful check the record expires; an undisplayed detached root expires
immediately. There is no permanent polling or unbounded retry extension.

A recapture renews the same record without duplicating ownership. Its observation
number prevents an older resolution pass from deleting or expiring the new
capture. A generation check after display resolution prevents an unload/reload
that occurred during that query from issuing obsolete transform writes.
Snapshots keep references alive while records are removed, so final node release
occurs outside g_nodeMutex. Existing unload/delete/reset cleanup remains shared.

No real equipment is equipped/unequipped. BodyMorph vertex application, dye
ownership, backend selection and public API ABI were not changed. Preview scope,
first-person exclusion, finite-value validation and RaceMenu's existing
equippable-transform acceptance checks remain. Public v3+ and legacy v1/v2
dispatch paths are still distinct; no new RaceMenu version block was introduced.

The transform semantics were checked against pinned primary upstream sources:
[SkeletonExtender](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/SkeletonExtender.cpp)
and [NiTransformInterface](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/NiTransformInterface.cpp).
The existing automatic `internal` position, temporary neutral bootstrap and
preservation of named third-party transforms are unchanged by this repair.

## Regression coverage

New RaceMenuHighHeelRootTests compiles actual production root record/capture,
metadata reader, resolver, native before/after capture, official attachment
observer, synchronization, queue and unload/reset functions. Only the engine
scene and transform/Papyrus providers are fixtures; selected offset is **not**
injected into the resolver. Fixture NiPointer counts retained scene references.

Before the production edit, the first pre-graft regression failed. It now passes
for interface routes 1, 2, 3, 4 and forward-compatible 99. These are interface
contract fixtures, **not** verification of every distributed RaceMenu binary.

Coverage includes:

- HH=9 observed before graft, first task during the gap, later graft with no
  actual boots and no second capture; temporary same-node reattachment.
- No actual heel source, matching source, different source (HH=17 in either
  scan order), flat source, ordinary late attachment and named third-party +3.
- Recapture during the last expiry check; no duplicate owner or stale lowering.
- Strip/removal while an actual HH=7 source remains; redress-off idle without
  automatic restoration; a later manual display/redress observation.
- Bounded expiry of never-grafted and never-reattached records, with no pending
  actor task or retained node reference after completion.
- Native/DAV before/after scene capture with HH_OFFSET on a child shape, with
  BodyMorph unavailable. DAVE's official callback uses the separately exercised
  observer path; this is source wiring coverage, not an in-game DAVE simulation.
- Explicit HH=0, non-finite data, first-person exclusion, stale observation
  rejection and unload during display resolution; hide/strip and save/revert
  while a pre-graft source is pending.
- 128 pending-attachment unload/reload cycles **per tested interface route**.

The new harness reports 870 checks (including queue-quiescence checks). The
existing 232 synchronization checks and full 35-target regression suite pass.
The normal SE/AE DLL build also passes; its combined SkyUI/HH hash is recorded
in [query-cost verification](SKYUI-QUERY-COST-FIX-2026-09-29.md).

## Limits / deployment

This fixes a reproduced source lifecycle defect. The reporter's exact meshes,
attachment ordering and RaceMenu binary have not been reproduced in game. A
branch grafted only after all completion attempts still needs a fresh attachment
observation; abandoned roots are intentionally not retained indefinitely. Loss
of metadata on a discarded NIF wrapper is not established by the supplied
evidence and is not claimed fixed. The existing transform-acceptance behavior
was not bypassed to force an unverified height.

No gameplay, MO2 replacement, release ZIP or GitHub publishing was performed
by the implementation turn. Subsequent v1.7.2 packaging/publication is a separate
user-authorized step; it does not extend the in-game verification described here.
