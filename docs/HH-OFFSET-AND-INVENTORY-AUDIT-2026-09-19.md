# HH_OFFSET / inventory feedback maintenance — 2026-09-19

Development audit on the 1.6.6 baseline; these changes are carried into v1.6.7.
During this audit no version bump, MO2 deployment, game input, release ZIP
replacement or GitHub publication was performed. Numbered release verification
is recorded separately in the v1.6.7 release notes and archive verifier.

## Evidence and scope

The report describes a momentary registered-heel height followed by loss of
height with RaceMenu 0.4.20.0, plus slower SkyUI inventory opening. Read-only
startup inspection of the user's TOFU installation found Skyrim 1.6.1170,
RaceMenu 0.4.20.0 (NiTransform 3 / BodyMorph 5), and DAV/native fallback.
The loaded RaceMenu SHA256 is
`5225E4E3B185E6FC57C8D31B0CEDBE5A030A951D9A744D33071B64C45A38C208`.
This confirms interface connectivity, not post-fix rendering or reporter setup.

Primary source baseline is expired6978/SKSE64Plugins commit
`9ebcb733e17be695f994cd2e9cc383043446bc02`:
[NiTransformInterface.cpp](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/NiTransformInterface.cpp),
[SkeletonExtender.cpp](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/SkeletonExtender.cpp).
The source-level reproduction models the relevant transform behavior; it is
not execution of a RaceMenu DLL and does not prove every packaged binary is
identical to this source.

## Height fixes

1. SFS previously selected the registered HH_OFFSET but only used its presence
   for activation. RaceMenu's full scene scan could leave a later branch's
   zero/different automatic position as the winner. After that scan, SFS now
   selects the registered heel's actual value through the existing automatic
   `NPC/internal` position component. No additional persistent SFS height is
   added, and named mod transforms, rotations and scales are not removed.
2. RaceMenu's incremental attachment path can clear automatic height when its
   recorded node set differs. SFS previously ignored non-fitting attachments
   and did not rearm for fitting attachments lacking HH_OFFSET. The existing
   actor-local callback now queues a repair for those events when registered
   heel tracking already exists. No periodic actor scan or new hook is added.
3. NiTransform 1/2 use the official NiOverride Papyrus functions; version 3+
   uses the public C++ prefix. The version routing is unchanged. A future
   version number alone does not introduce a new rejection. The added lookup
   tests for an automatic position, not a package-version whitelist; it never
   prevents RaceMenu's preceding ordinary full scan. Missing automatic position
   does not fabricate an accepted transform or publish false success.
4. Legacy VM callbacks retain the target captured on the game task. They do not
   reevaluate SFS conditions or traverse the scene to resolve the target on a
   Papyrus callback thread. Edits during dispatch use the existing coalesced
   pending refresh. Save/load generation checks and neutral-bootstrap cleanup
   are retained.

Native, DAV and DAVE already share the refresh follow-ups. Their ownership,
fallback selection, actual-equipment operations and refresh plan are unchanged.
Replacement-preview and first-person heel scope are unchanged. Ordinary gear
events can request a heel repair but are not enrolled as fitting morph/dye roots.

## Inventory-path work reduction

- One final-outfit/body-keyword decision now shares one lazy worn-equipment
  collection, rather than collecting again for the final actual-gear result.
  This is local to a single query, not a cross-frame cache. Equipment/condition
  changes are reevaluated on the next query; runtime scan coverage is unchanged.
- Registered-armor membership reads the same final additional-armor list
  directly, without computing unused visible actual equipment for each root.
- Papyrus WornHasKeyword preparation skips unused caller-identity construction.
  Every strip/redress, token, filter, DD and P+ preparation still builds it.
- Repeated function objects in a caller chain reuse the identity within that
  call. The existing limit remains 24 distinct identities, not 24 frames;
  deeper trusted callers remain discoverable. The function-code hash was
  already cached before this change; the saved work is redundant cache lookup
  and identity formatting, not a previously uncached bytecode scan.

No direct SFS InventoryMenu/ContainerMenu opening scan or SkyUI list hook was
found. These changes remove verified redundant work but do **not** establish
where the reported 3–4 seconds are spent or guarantee that delay is eliminated.
No artificial benchmark is presented as a game menu timing measurement.

## Regression protection and verification

- All 23 fast regression executables and source wiring checks passed.
- New high-heel tests compile the actual public/legacy sync, VM callback chain,
  queue and attachment observer against recording engine/providers. They cover
  late ordinary/fitting attachment, conflicting zero height, no double height,
  other named offsets, last-heel cleanup, real-heel preservation, strip/hide,
  redress-off idle, redress/manual visibility restoration, pending changes,
  callback failures and save generation cancellation.
- Routes 1/2/3 and synthetic future 4/99 are exercised. Synthetic versions
  test dispatch policy, not availability or compatibility of future packages.
- Independent ABI fixtures verify actual MSVC virtual dispatch of the used
  NiTransform slots (3, 7, 15, 22, 24), actor/gender/position arguments.
- History audit passed 19 interface-change source snapshots, including 13
  public BodyMorph prefixes, available public transform prefixes and legacy
  Has/AddNodeTransformPosition signatures. This is not an inventory of all
  historically released RaceMenu binaries.
- Existing tests cover BodyMorph public/deferred/preview refresh, dye, all four
  strip-link combinations, P+, DD, redress-off manual visibility, backend
  refresh rules, IED/Helgen routing, conditions, runtime layouts and API state.
  No strip policy, manual-visibility state machine, IED hook or BodyMorph
  implementation was replaced as part of this fix.

SE/AE-only releasedbg rebuild passed with XMake 3.1.0 and the unchanged pinned
CommonLibSSE-NG 6.7.0 closure/local patches; VR remains disabled. The installed
MSVC toolset directory is 14.51.36231, but its serviced compiler now reports
19.51.36257.0 (product 14.51.36257.0), SHA256
`315A654EA116864516A1674858E587E535E3BC3045FF32ED2F2739A2C1EC5640`.
This differs from the older shared registry observation. An initial stale-PCH
failure was resolved with a full rebuild; no compiler/dependency was downloaded,
installed or silently downgraded. The observed compiler hash is recorded here
for reproducibility; the external shared registry was not modified.

Local DLL: `build/v1.6.6/windows/x64/releasedbg/SFSCore.dll`, version 1.6.6.0,
SHA256 `C7EA2EA00E5FB1A6CDC23707F6D512B283C95F49ECCA9DCC5C076A974569384C`.
Generated logs are under `output/sfs-1.6.6-feedback-audit-20260919/` (excluded
from release/source archives). Tests and source audit scripts are non-runtime;
no temporary diagnostic logger or periodic analysis code was added to the DLL.

At the user's request, no in-game test was performed. Post-fix visual height,
live morphs and menu timing in real mod combinations remain unmeasured. The
running game and installed MO2 DLL were left untouched.
