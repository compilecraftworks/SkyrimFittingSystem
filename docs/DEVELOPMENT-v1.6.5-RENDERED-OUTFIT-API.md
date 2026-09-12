# v1.6.5 rendered-outfit API — development checkpoint

Historical implementation checkpoint, not a release note. See
`RELEASE-NOTES-v1.6.5.md` for the shipped scope and verification limitations.
The narrow IED BipedSlot bridge and UI work are documented in
`DEVELOPMENT-v1.6.5-IED-AND-UI.md`.

## Implemented boundary

The v1 ABI is in `extras/SkyrimFittingSystemRenderedOutfitAPI.h`.
`SFSCore.dll` exports `SkyrimFittingSystem_GetRenderedOutfitAPIVersion` and
`SkyrimFittingSystem_QueryRenderedOutfit`. The existing exports are retained.
Resolve the already-loaded DLL with GetModuleHandle/GetProcAddress, never
LoadLibrary. Version lookup is independent of game readiness; outfit queries
must run on the SKSE game-task thread. Listen to the exact SKSE sender
`Skyrim Fitting System` and message type `0x53465352`.

The public result is a caller-buffer POD snapshot of **SFS's final display
decision**, not pixel visibility, alpha/occlusion, or a GPU-completion fence.
It does not change actual equipment, keywords, morphs, strip state, or save data.
The API does not rerun BuildDisplaySet, condition evaluation, or strip-link
decisions. The existing display producer supplies the result after its normal
rules have run. A previously prepared result permits first-query initialization
without forcing equipment/3D regeneration.

- Input is the actor reference FormID, not the NPC/base FormID. Zero is invalid.
- All visible ARMO entries are returned, including overlapping slots and SFS
  additions. Registered means an SFS-rendered addition and can also include a
  compatibility-generated addition; it is not proof of saved-card membership.
- Declared ARMO slots and effective SFS slots are separate. Bit 0 means slot 30;
  bit 31 means slot 61. Results sort by FormID/source, not gameplay priority.
- Dynamic original-ARMO provenance is explicitly unknown when not traced;
  the API never invents an original ID from a dynamic form.
- ArmorCuirass/ClothingBody use the same helper as the existing SFS body-keyword
  decision. Ordinary non-body accessories are not inferred to be torso clothing.
- Ready with zero items means managed and empty: do not fall back to worn armor.
  NotManaged permits normal actual-equipment fallback. NotReady means defer.
- BufferTooSmall writes a header/capacity requirement but no partial item array.
  Requery both header and items; revisions can change between the two calls.
- Epoch resets invalidate all consumer caches/subscriptions. Actor deletion,
  unload/reload, display refresh, and observed skinning passes invalidate or
  update the appropriate actor. Same-outfit skinning retains SceneChanged.
- Change callbacks execute outside the provider mutex. Their payload is borrowed
  for Dispatch only. Consumers should queue their own work and coalesce by
  actor/epoch/revision, not recursively rebuild equipment from the callback.

## Performance changes in this checkpoint

1. Removed the subscribed-actor walk on every presentation tick. An idle tick
   uses an atomic pending-work check and schedules no game task.
2. Changed actors enter a deduplicated work set. Ordinary repeated refreshes
   coalesce. Publication checks only those actors, not nearby NPCs or all forms.
3. Process at most 64 changed actors and dispatch at most 64 messages per game
   task. Round-robin cursors prevent continuously changing IDs from starving
   other actors. Epoch reset notifications precede new-epoch actor messages.
   This is a work-count bound, **not a measured frame-time budget**.
4. Canonicalize the item array once, compact duplicates in place, and retain one
   reusable POD scratch buffer per producer thread. Only changed results copy
   into the prepared cache. Scratch capacities above 256 entries are released
   after full publication; no outfit is truncated to that size.
5. Removed the extra additional-armor snapshot copy and the previous-value copy
   during publication. Body-keyword checks use spans over the same armor lists.
6. Unloaded actors lose prepared data even if nobody queried them. Deleted actors
   lose their subscription and caches. Epoch reset releases all actor caches.
7. Query validates only its requested actor's root/availability. Normal object
   events and skinning/refresh notifications drive background publication; there
   is no replacement periodic whole-actor scan.
8. The installed IED bridge enrolls actors from the existing SFS producer, not
   a whole-actor scan. IED workers acquire a shared immutable ID-only view under
   a short lock. Views are copied lazily once per changed value and survive
   cache retirement until their last reader exits. Dirty/unavailable views are
   not served as Ready. The public query's game-thread contract is unchanged.
9. Ready/NotManaged state or availability changes enqueue the existing
   actor-local IED.Evaluate task outside the provider mutex. SceneChanged alone
   does not enqueue IED and therefore does not create a skinning/evaluation loop.
   Without IED there is no bridge observer, hook, or IED evaluation work.

There is still work in an existing display build: projecting/filtering its
already-collected equipped armors, building item records, keyword checks,
sorting, and a short cache comparison. Queries copy the requested item array.
The implementation therefore does not claim zero overhead. Consumer callbacks,
BCNG morph work, and IED condition evaluation have their own costs.

## Verification

Pinned repository baseline: XMake 3.1.0, MSVC 14.51.36231, vendored
CommonLibSSE-NG 6.7.0 with the repository's existing flat-layout correction.
Configuration remains SE/AE with VR disabled (`EXCLUSIVE_SKYRIM_FLAT`).

Run:

```powershell
& 'C:/Users/yunha/.codex/external-tools/xmake/3.1.0/xmake/xmake.exe' -y SkyrimFittingSystem
& './tests/run-fast-regressions.ps1' -Xmake 'C:/Users/yunha/.codex/external-tools/xmake/3.1.0/xmake/xmake.exe'
```

The 20-target suite includes the real provider/exports against fake engine/task
boundaries and verbatim extracted production visibility, body-keyword, and item
producer functions. Measured operation counts in that harness:

| Scenario | Result |
| --- | --- |
| 1,000 subscribed actors, 10,000 idle ticks | 0 queued game tasks, 0 actor lookups |
| 1,000 repeated refresh requests for one actor before pumping | 1 task, 1 actor lookup |
| 1,000 initially changed actors | 64 maximum actor checks per task; all covered in 16 tasks |
| 1,000 unchanged calls to the production item producer | No new publication task or revision |

Additional checks cover caller-buffer bounds, effective/declared slots, dynamic
provenance, protected/force-visible actual equipment, internal genital-cover
exclusion, managed-empty versus unmanaged, preview/hide cycles, actor isolation,
same-outfit skinning, late producer input, root changes, disabled/re-enabled
actors, unload/delete cleanup, reentrant queries, reentrant epoch reset, message
coalescing, message burst limits, and queue fairness.
IED observer tests also cover unload with an uncleared old root, trailing
skinning notifications, fresh-root reload, actor deletion, and pending
publication canceled by an epoch reset. Verbatim production IED queue tests
cover duplicate requests, deleted/disabled/missing/unloaded actors, load
cancellation, same-ID/new-token reuse, fresh actor resolution, and VM teardown.

Existing tests also cover DAVE/RaceMenu initialization, SE/AE custom-skin visitor
routing, BodyMorph tracking through helmet/dye/camera/strip/backend changes,
manual visibility, condition UI/input, and compatibility packaging guards.
These are not in-game tests or evidence of every installed mod combination.

## Remaining integration/release gates

- **Implemented IED scope is BipedSlot ARMO conditions**, including form/keyword
  predicates attached to those conditions, in four equipment dispatcher
  variants and the node-override dispatcher. Both installed IED 1.7.4 runtime
  distributions passed actual helper/installer tests in an isolated process.
  Broad equipped-form/keyword/type, inventory collectors, and armor-default
  paths are not converted. Do not advertise universal IED condition support.
- Existing custom-skin visitor-chain safety is unchanged. Confirm new-game
  Helgen escape, cell changes, and repeated save/load with the condition bridge
  active. Fake-engine lifecycle and isolated helper tests do not prove a live
  modded game is CTD-free. IED still owns its real-equipment selection, presets,
  node side effects, inventory, and save data.
- Verify scene/publication ordering in DAVE, DAV, and native in-game. An observed
  skinning generation is not proof that all parallel attachment passes finished.
  Consumers must retain this distinction even with a versioned v1 ABI.
- Compare v1.6.4/v1.6.5 frame times on identical city routes, populated cells,
  player/NPC hide/show bursts, and strip/redress sequences, including consumer
  callbacks and IED enabled/disabled. Check frame-time spikes and cache/memory
  behavior after repeated loads, not just average FPS.
- Published notes must disclose these outstanding checks; fake-engine counters
  alone do not establish complete in-game validation of v1.6.5.
