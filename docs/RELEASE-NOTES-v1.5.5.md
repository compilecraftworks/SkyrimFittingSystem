# Skyrim Fitting System v1.5.5

- Fixed registered-appearance live BodyMorph/OBody synchronization losing its
  tracked nodes during replacement previews and temporary visibility changes.
- Kept initial preview morphing separate from later live updates to avoid a
  duplicate initial vertex application.
- Retained missed update intent for late attachments and reactivation, with
  bounded actor-local completion and invalidation of stale queued tasks.
- Removed nested morph-node/workbench lock acquisition from node resolution.
- Added regression tests using actual production tracking functions, covering
  direct/deferred updates, player/NPC isolation, first/third person roots,
  retained nodes, late attachment, and initial morph separation.

The BodyFamily catalog option, strip/redress resolvers, appearance locks, dye,
actual equipment, backend selection, and RaceMenu ABI profiles are unchanged.
No separate patch is required. This fixes confirmed tracking-lifecycle gaps;
it does not establish that the BodyFamily checkbox itself caused every report.

Automated checks do not replace in-game verification on DAVE, DAV, and native.
See [regression scope and in-game checks](BodyMorph-v1.5.5-Regression-Checks.md).
