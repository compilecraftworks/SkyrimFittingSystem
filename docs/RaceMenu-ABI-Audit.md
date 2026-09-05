# RaceMenu ABI audit — 2026-09-06

Scope: the v1.5.4 base compatibility fix and v1.5.7 backport-layout correction.

## Independent interface versions

RaceMenu package versions, Skyrim runtime versions, and the versions returned
by individual `IPluginInterface::GetVersion()` calls are different identifiers.
Never use one interface's version to choose another interface's vtable.

| Boundary | Reported interface version | SFS route |
| --- | --- | --- |
| ActorUpdateManager | 0, six-entry legacy layout | AddInterface at x64 slot 3 |
| ActorUpdateManager | 0, fourteen-entry public backport layout | AddInterface at x64 slot 11 |
| ActorUpdateManager | 1, 2 | Public prefix; AddInterface at slot 11 |
| ActorUpdateManager | Other/missing | Skip observer registration; retain native scene capture |
| BodyMorph | 4, 5 | Public prefix; ApplyVertexDiff at 12, ApplyBodyMorphs at 13 |
| BodyMorph | 0–3, unknown/missing | No direct SFS public-ABI calls or BodyMorph hooks |
| NiTransform | 1, 2 | Name-based NiOverride Papyrus calls; never cast to public v3 |
| NiTransform | 3 | Public C++ transform interface |
| NiTransform | Other/missing | No SFS height synchronization calls |

A non-null ActorUpdateManager with version 0 can be either legacy data or an
AE-to-SE public-layout backport. SFS validates the exact executable vtable
profile before choosing slot 3 or 11 and fails closed on any other profile.
Missing BodyMorph does not invalidate a received interface
map or disable an independently available NiTransform/attachment interface.
The existing bounded PostPostLoad/DataLoaded initialization and initialization
mutex remain in place. Unknown versions are logged, not presumed compatible.

## Confirmed regression and fix

- v1.5.2 (`b679e2a`) used the legacy ActorUpdateManager prefix.
- v1.5.3 (`ab559ba`) replaced it with the public prefix without separating
  version 0. Its comment incorrectly generalized that prefix to all releases.
- In the audited legacy DLL, slot 11 is outside ActorUpdateManager's primary
  vtable and reaches an unrelated Scaleform function. Correct registration is
  at slot 3. The resulting access violation can strand the SFS initialization
  mutex under `/EHsc` if an outer SKSE SEH handler catches the exception.
- The later UBE AE-to-SE backport keeps reported version 0 but exposes the
  fourteen-entry public layout with AddInterface at slot 11. Treating every v0
  as legacy silently called AddBodyUpdate and left DAVE's late attachments
  unobserved, so live registered-appearance morph synchronization found zero
  nodes.
- Fix: read the stable GetVersion prefix before casting; for v0 verify either
  the exact six-entry legacy or fourteen-entry public-backport executable-slot
  profile; publish `registered=true` only after AddInterface returns. Unknown
  profiles call neither candidate slot.
- The observer stays process-lifetime and is not re-registered on save/load.
  Failed registration remains retryable. No global exception-mode change,
  lock removal, RaceMenu replacement, ESP, or PEX patch was added.

The incorrect call and the exception/stranded-lock/cross-thread wait mechanism
were confirmed independently. The reporting user's complete running process
was not captured, so the reporter-specific cause is not claimed as reproduced.

## Preserved morph, heel, and backend behavior

- Observer ABI has **no virtual destructor**; OnAttach is callback slot 0.
  Reference, ARMO, ARMA, object, first-person flag, skeleton, and root retain
  the original argument order.
- The callback remains backend-independent and requires an active registered
  appearance belonging to the callback actor. It captures late DAVE heel
  attachments and deduplicates native captures. No selected-UI-actor/global
  player substitution was introduced.
- DAV Update3D, native UpdateEquipment, native empty-equipment Update3D, DAVE
  RefreshActor, and DAVE empty-equipment Update3D retain actor-local follow-ups.
  Native before/after attachment capture remains available. No render backend
  or external stripping resolver was modified by this audit.
- ApplyVertexDiff keeps argument 3 `false`. The public header calls this
  parameter `erase`, but the v4/v5 implementation forwards it as
  `isAttaching`; false resets/reapplies existing SHAPEDATA. SHAPEDATA avoids
  initial double application; the thread-local UpdateModelWeight scope avoids duplicate
  public-hook postpasses. Unchanged DAVE attachments remain tracked until they
  actually leave that actor's scene/current appearance set.
- The private NIOVTaskUpdateModelWeight hook is used only with verified
  BodyMorph v4/v5. Both audited upstream task declarations and installed DLLs
  use Run slot 0 and a uint32 Actor FormID at +8. RTTI resolves the current DLL's
  vtable; image/executable checks and verified writes remain in place. Private
  layout compatibility is **not** guaranteed for arbitrary third-party forks.
- NiTransform v3 position/update slots used by SFS are 7, 15, 22, and 24;
  Position/Rotation are three floats (12 bytes). Earlier concrete classes are
  not ABI-compatible with this public declaration.
- Legacy Papyrus calls were checked against both source revisions:
  AddNodeTransformScale (reference, firstPerson, female, node, key, float),
  RemoveNodeTransformScale/Position (same first five arguments),
  UpdateAllReferenceTransforms (reference), and UpdateNodeTransform (reference,
  firstPerson, female, node). Names, parameter order, return types, and NoWait
  registration agree. The installed NiOverride scripts must match RaceMenu.
- A failed legacy full-update dispatch now cleans up its neutral bootstrap
  without reporting success or advancing to removal of the internal position.
  Actor generation guards, bounded queues, and non-SFS transform ownership
  remain unchanged.

## Primary evidence

- Legacy source: [87a5cadd — ActorUpdateManager](https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/ActorUpdateManager.h),
  [public BodyMorph prefix](https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/IPluginInterface.h),
  [task layout](https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/BodyMorphInterface.h),
  [Papyrus functions](https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/PapyrusNiOverride.cpp).
- New source: [9ebcb733 — public interfaces](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/IPluginInterface.h),
  [task layout](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/BodyMorphInterface.h),
  [Papyrus functions](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/PapyrusNiOverride.cpp).

Read-only PE/RTTI/disassembly checks (no LoadLibrary/game execution):

| skee64.dll sample | SHA256 | ActorUpdate / BodyMorph / NiTransform |
| --- | --- | --- |
| TuLED13E Race Menu | `255C0DB0BA5FF14640CC6D75CCDDFB24474B498D35B77F3140CCB05EB80E7273` | 0 / 4 / 2 |
| TuLED13E UBE 2.0 AE-to-SE backport | `283EA6F0DF6234B5636D6B03445A57C90369514E61EC3B02DA07F731FCF3469B` | 0 (public layout) / 4 / 2 |
| TAKEALOOK RaceMenu | `5225E4E3B185E6FC57C8D31B0CEDBE5A030A951D9A744D33071B64C45A38C208` | 2 / 5 / 3 |

Legacy/new task vtable RVAs are `16BFB8` / `1DA2E8`, Run RVAs `A460` /
`1F770`; both load the actor through `mov ecx, [rcx+8]`. These RVAs are audit
evidence only, never runtime constants. ActorUpdateManager version 1 is
covered by the official prefix and synthetic ABI tests, not a third DLL sample.

## Automated verification and remaining limits

- Pinned xmake 3.1.0, MSVC 14.51.36231, x64 releasedbg `/MD /O2 /EHsc`.
  Existing CommonLibSSE-NG 6.7.0 build closure unchanged.
- `RaceMenuInterfaceTests`: independent provider vtables with trap slots;
  legacy-v0/public-backport-v0/public-v1/v2 registration, ambiguous-v0 and
  missing/unknown versions, OnAttach pointer and
  first-person arguments, registration idempotency, C++ exception/retry from
  another thread, BodyMorph 4/5 dispatch (including GetBodyMorphs slot 6,
  VisitMorphs slot 8, float return and visitor callback), NiTransform 3 dispatch, actor/gender
  argument isolation, and legacy update-failure completion policy.
- A diagnostic copy restoring the wrong version-0 slot-11 route was rejected
  with the expected trap exit 2. Production source was not altered for this
  negative test.
- Compiled production object inspected: GetVersion calls +08h before
  dispatch, version 0 calls +18h, versions 1/2 call +58h, registration is
  published only after those calls return.
- All eight fast regression executables passed, including core actor-local
  BodyMorph/suppression, all four strip policy combinations, DAVE/DAV/native,
  virtual tokens, conditions, dye, and kit navigation. These are logic/ABI
  checks, not a substitute for engine/render execution.
- The pre-version-bump v1.5.3 maintenance build passed. Its local DLL SHA256:
  `34DE9355EA17667573FB0C06792BEF76D1B30445D60CE8B01BF30BC45E819DC6`.
- The initial v1.5.4 release build and all eight regression executables passed again.
  DLL FileVersion/ProductVersion: `1.5.4.0`; SHA256:
  `40897BAA622E12D78D3B0FF54566C17D66A3F76A72CCFE0AA519752731375ABD`.

### Follow-up BodyMorph diagnostic check

A temporary diagnostic build logged live morph values and actor-local
registered-appearance submissions in the native / BodyMorph v4 environment.
The user reported that the visible result worked during this run. This does
not identify the cause of the earlier report or establish a new morph fix;
logging can affect timing, and CPU-side observations do not prove GPU output.
It also does not constitute DAVE/DAV or BodyMorph v5 in-game verification.

The production repack removes the temporary probes, scene/buffer scans and
deferred diagnostic tasks. Their sources and diagnostic DLL/PDB are retained
only in a local, excluded workspace archive. Production morph application,
tracking, attachment, and refresh decisions remain unchanged. Independent
read-only ABI tests and the corrected argument-name comment are retained.
The no-probe production build still needs the same in-game comparison.

The repacked production DLL was built with the pinned toolchain and all eight
fast regression executables passed again. Diagnostic markers are absent.
FileVersion/ProductVersion: `1.5.4.0`; SHA256:
`EA2AD917A1772A64C9AF0735460338B2E7857467C6735BEBE86E272E089E14F2`.

Still needed in game: startup to main menu with matching SE/AE RaceMenu,
save/reload, player/NPC live morphs, registered heels on all three backends,
late DAVE attachments, and hide/condition/strip/redress restoration. No claim
of complete compatibility with every historical or future RaceMenu package,
no new VR support, and no automatic deployment/publication is made here.
