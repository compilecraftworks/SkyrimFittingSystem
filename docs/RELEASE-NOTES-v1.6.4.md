# Skyrim Fitting System v1.6.4

## Changes

- Argument 1 of form-based conditions now prefers an EditorID display alias. Missing/unresolved EditorIDs fall back to the original token; actor/reference labels retain a secondary FormID. Merely viewing, focusing or accepting an unchanged label preserves the stored plugin-local reference. Argument 2, numeric/enum values and variable names are unchanged.
- Fixed manual appearance show controls being treated as a no-op after an external strip without redress (for example SexLab redress OFF). The saved eye flag can already be visible while a runtime ticket still hides the appearance; explicit individual/global show now releases that display suppression.
- Kept no-redress appearances hidden until a user action or an existing valid recovery. Actual redress still uses the existing automatic appearance recovery. No real-equipment equip/unequip calls or provider-setting detection were added.
- Scoped manual/delete ticket invalidation to the selected actor so another actor's same-slot appearances retain their suppression and transaction.
- Reduced repeated work during stripping-link cache refreshes: each actor's active appearances are evaluated once and reused for all 32 biped-slot queries, instead of evaluating the entire list once per slot.
- Preserved slot priority, multi-slot identities, protected/locked appearances, direct mappings, and the existing global refresh scope. No persistent condition-result cache or longer refresh interval was introduced.
- Avoided collecting a final worn-outfit snapshot for vanilla body-keyword queries when SFS has no active display override. Managed actors still use final visible actual equipment and registered appearances for body-coverage decisions.
- Reduced dye render-pass bookkeeping for unrelated top-level geometry. Preview timers are read only while a preview exists. Nested non-tinted passes still mask their parent's dye, and matched passes retain texture binding/restoration and renderer-chain compatibility.
- Added bulk-slot equivalence and nested dye-pass regression coverage, plus checks that the production hot paths use these optimizations.

## Scope

This addresses redundant work found while investigating city-walking stutter reported against versions newer than 1.3.0. It is not a measured FPS improvement or confirmation that every reporter's stutter has been resolved. City traversal and actual D3D rendering still require in-game comparison.

No game/RaceMenu support changes, settings migration, save-format changes, or optional patch updates. BodyMorph ownership and DAVE/DAV/native refresh dispatch are unchanged. Keep existing settings and presets.

## Local verification (2026-09-12)

- SE/AE-only releasedbg build passed; DLL file/product version: 1.6.4.0.
- All 17 fast regression executables and the accompanying production-source checks passed, including BodyMorph tracking, stripping-link policies and custom-skin routing.
- Bulk slot tests compare the new selector with independent per-slot searches across empty, single-slot, multi-slot, overlapping and deterministic generated inputs. This measures result equivalence, not frame times.
- Dye target-map synchronization remains in place; the optimization removes unnecessary timer/scoped-record work, not all renderer lookup cost. No long-lived appearance cache was added.
- The replaced internal single-slot appearance-identity API and its unused output parameters were removed after checking all source/consumer references.
- Manual-visibility tests compile the production setter, ticket invalidation and settled-redress functions against fake engine boundaries. They cover no-redress, actual/partial redress, individual/global-style actions, NPC/player isolation, exact-ticket fallback, stale callbacks, DD latch release and 128 repeated strip/show cycles. They do not certify live SexLab gameplay.
- Production form-token tests and real-ImGui dropdown tests cover EditorID aliases, stable token preservation, focus/Enter/blur, loaded suggestions, FormID typing, first-click selection, long labels and fallback without record scans. Game-side visual verification remains outstanding.
- Current DLL SHA-256: `6FFAB0FA46FF063D5D9F7E4D0BD6D1B963B1D21456CC863CCD44333117509549`.
