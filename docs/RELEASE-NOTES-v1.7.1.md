# Skyrim Fitting System v1.7.1 SE-AE

## Changes

- Reduced redundant Papyrus global-function inspection when the same linked script type is queried repeatedly. First-call hook installation and the initial post-link check remain; changed types/tables, late native binding, and load/reset boundaries trigger reinspection.
- Removed unused caller-identity tracing from ordinary, non-token `Form.HasKeyword`, `GetKeywords`, `GetNumKeywords`, and `GetNthKeyword` reads. Actual virtual-token queries retain their contextual appearance handling.
- Kept incomplete tables and failed hook installations eligible for retry. Both SexLab P+ member observers retain their existing delayed/natural-lookup reinspection, including recovery after retry exhaustion.
- Bounded installation-history ownership to 128 entries and release it at load/revert boundaries. This is not a cached actor, appearance, inventory, or nudity result.

## Preserved behavior and installation

Final-rendered nudity checks and strip/redress behavior remain enabled. The worn-query, equipment-mutation, catalog-filter, DD, and P+ routes are retained. This release does not change DAVE/DAV/native selection, RaceMenu interfaces, BodyMorph, high heels, dye, IED integration, the supported game-version table, dependencies, save formats, or the API ABI.

Only `SFSCore.dll` changes in the runtime archive compared with 1.7.0. Keep existing settings, kits, saves, the VirtualTokens ESL, and optional compatibility patches. No new dependency, actual-equipment operation, polling loop, or runtime diagnostic instrumentation is added. Optional patch versions are unchanged.

## Verification and limits

The controlled 256-lookup test with 12 globals drops function-entry patch probes from 3,096 to 24 while preserving first-call installation. Ordinary non-token keyword reads no longer build unused caller identities. These are operation counts in host tests, not measured frame times or dialogue latency.

The 29-executable regression suite covers the new routing plus existing condition/UI, rendered-outfit API, IED, RaceMenu/morph/high-heel, strip-link, and resource-lifecycle cases. See [the technical record](NPC-DIALOGUE-PERFORMANCE-2026-09-24.md) for test boundaries and release verification.

**The reported NPC dialogue pause has not been reproduced or timed in-game.** This release removes confirmed redundant work found during that investigation; it does not claim that every dialogue stall or mod combination is fixed. The ESL-free downgrade report also changed the DLL, so the ESL itself is not established as the cause.
