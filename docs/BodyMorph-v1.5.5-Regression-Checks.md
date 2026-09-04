# v1.5.5 live BodyMorph regression scope

## Implementation boundary

Production changes are limited to RaceMenuBodyMorph, its private header, the
shared morph rules, and the morph tracking/initial-application fields and call
sites in ArmorSkinning. No changes to equipment ownership, workbench rows,
saved conditions/kits, BodyFamily catalogs, external strip resolution, dye,
RaceMenu ABI definitions, hook slots, or DAVE/DAV/native backend selection.

Preview roots participate in subsequent live updates. Initial preview morphing
still belongs to RaceMenu, and the high-heel attachment observer retains its
previous non-replacement-preview scope. Native initial fallback still skips
already processed SHAPEDATA. No new dependency or external patch is needed.

Actor-local node records survive inactive display transitions. Only current
displayed ARMO roots belonging to that actor's corresponding 1P/3P scene can be
written. Completion tasks coalesce per actor, allow at most two additional
attempts for pending attachments, and release persistently detached roots.
Bursts of immediate updates do not consume those completion attempts. No
world scan, per-frame polling, or stored morph-value copy is introduced.

Requested-update intent remains available for a later attachment event or
reactivation, even if an earlier completion had zero nodes. Forget/load/revert
invalidates queued tickets; new tickets do not reuse identifiers from an old
save. Nodes are never rediscovered by guessing names, DDS paths, or another
actor's body family.

Node resolution copies records under the node mutex, then releases it before
consulting the workbench. Updating/pruning records checks observation identity
so a newer callback cannot be erased by an older snapshot.

## Automated checks

Run `tests/run-fast-regressions.ps1` with the pinned xmake v3.1.0 executable.
All nine executables passed during the v1.5.5 implementation:

- RuntimeLayoutTests: verified runtime-layout boundaries.
- KitGeneratorLogicTests, BodyFamilyLogicTests, ConditionCnfLogicTests,
  FittingDyeRulesTests, KitListNavigationTests: existing logic suites.
- CoreBehaviorRegressionTests: actor-local state, virtual tokens,
  ModSettings/Vanilla/Direct+both suppression rules, backend dispatch, rotation,
  plus preview initial/live separation and stale morph-task ticket checks.
- RaceMenuInterfaceTests: independent ActorUpdateManager v0/v1/v2 provider
  layouts and BodyMorph v4/v5 dispatch, alongside existing transform checks.
- RaceMenuMorphTrackingTests: actual production tracking function definitions,
  generated into a test-only include and compiled with a fake engine/recording
  morph interface. Includes direct/deferred updates, retained preview nodes,
  hidden updates/reactivation, delayed grafting, burst updates, detached-node
  cleanup, late callbacks after an empty pass, actor ownership, unregistered
  actors, stale queued work after forget, initial preview/native separation,
  SHAPEDATA duplication protection, and first/third-person roots.

Mutation validation also rejected each deliberately reintroduced fault:
preview tracking disabled; erase on inactivity; lost update intent; duplicate
initial preview application; missing task invalidation; premature pruning
during a burst of direct updates. The unmodified test baseline passed.

The existing public menu exports were verified in the built DLL. ABI profile
definitions and backend selection code are unchanged, not reinterpreted for
this fix. The test fake does not execute a real RaceMenu shader/vertex update
or simulate the full Skyrim engine. Passing these checks is NOT proof of every
in-game combination or universal support for all historical RaceMenu releases.

## Required in-game matrix (not performed by these tests)

Use each available DAVE, DAV, and native setup separately. For a player and an
NPC with a morph-enabled registered outfit, test:

1. OBody preset changes and one real-time NiOverride producer with SFS closed.
2. The same changes during Gear/Outfits/Kits preview and after closing it.
3. Hide/show and condition swaps; confirm the current morph after restoration.
4. BodyFamily option ON/OFF, then reopening SFS; catalog filtering must not
   change workbench registration or morph eligibility.
5. ModSettings, Vanilla, Direct+ModSettings, Direct+Vanilla strip/redress;
   locked appearances, HT2, and ordinary inventory actions retain their rules.
6. Dye remains actor/component-local after previews, morph changes, and reload.
7. Registered high heels, first person, actor switches, save/reload, and loading
   a different save. Check no delayed update reaches another actor/save.

The checkbox-specific causal explanation for the original external report
remains unproven. v1.5.5 addresses the independently reproduced tracking gaps
without disabling the catalog filter or changing its semantics.
