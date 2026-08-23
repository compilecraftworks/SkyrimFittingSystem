# Skyrim Fitting System v1.4.7

- Added a narrow Immersive Equipment Displays (IED) custom-skin compatibility
  boundary. When IED owns the exact pre-patched `VisitWornItems` call used by
  Skyrim's custom-skin rebuild, SFS no longer passes its generic hidden-real-
  equipment filter visitor through IED's concrete visitor hook.
- SFS filters that one hidden-real-equipment call through the original engine
  target, then queues IED's public actor-level evaluation after the rebuild.
  The queue is actor-local, de-duplicated, and cleared with SFS refresh state.
- Normal calls without hidden real equipment, non-IED pre-hook chains, saved
  appearance data, Mod-Configured Slot Linking, Automatic Vanilla-Slot
  Linking, Direct Editing, external strip/redress transactions, and
  DAVE/DAV/native display backends are unchanged.
