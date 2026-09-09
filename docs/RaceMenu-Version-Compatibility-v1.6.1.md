# RaceMenu version families and SFS compatibility policy

Audit date: 2026-09-10. SE/AE only. This supplements the binary evidence in
[RaceMenu-ABI-Audit.md](RaceMenu-ABI-Audit.md); the routing policy below supersedes
that historical document's upper-version rejection rules.

## Evidence, not a package allowlist

The official source history through immutable commit
[`9ebcb733e17be695f994cd2e9cc383043446bc02`](https://github.com/expired6978/SKSE64Plugins/tree/9ebcb733e17be695f994cd2e9cc383043446bc02)
contains 19 changes to the relevant interface/task headers. The read-only
`scripts/audit-racemenu-abi-history.ps1` audits all 19 snapshots, including six
historical concrete-v3 snapshots and 13 public-v4/v5 snapshots. Every public
snapshot has the SFS-used method positions 6/8/12/13. This automated inventory
checks declaration order; source review and independent ABI tests additionally
check argument/return/visitor types. It does not prove arbitrary DLL behavior.

No RaceMenu package version, filename or hash is used as an SFS permission
list. Independently queried interfaces determine each route. A package whose
binary was not individually obtained still connects to its compatible family.

| Source change boundary | BodyMorph family | Distinction relevant to SFS |
| --- | --- | --- |
| Initial alpha `31a0853` through `58f565e` (2017–2019) | Concrete v3 | Save/Load precede the morph methods; strings/visitors use concrete C++ types. ApplyBodyMorphs initially takes one argument, later two. Not public-v4-compatible. These original game targets precede SFS's supported 1.5.97 minimum. |
| `e29537e` (2019-03-13) | Public v4 begins | GetBodyMorphs 6, VisitMorphs 8, ApplyVertexDiff 12, ApplyBodyMorphs 13. |
| `8f58265` (0.4.11 / game 1.5.97 source update) | Public v4 | Same used prefix; legacy ActorUpdateManager v0 and NiTransform v2 remain separate interfaces. |
| `7ffff9a` (0.4.14 source update) | Public v4 | SetCacheLimit changes UInt32 to size_t and cache API is extended without a BodyMorph version increment. SFS never calls this tail; its declaration now ends at used slot 13. |
| `e779c68` (2022-04-11) | Public v4 | NiTransform becomes public v3 independently; morph prefix remains unchanged. |
| `c1b408f` (2022-09-26, game 1.6.640) | Public v4 | ActorUpdateManager becomes public v1; AddInterface moves from legacy slot 3 to public slot 11. |
| `7694eab` / `348607e` | Public v4 | Public interface declarations consolidate; ActorUpdateManager public v2 appends callbacks without moving AddInterface. |
| `9ebcb73` | Public v5 | Adds a morph-shape callback after the v4 methods; used prefix unchanged. Source mentions game 1.7.99, which does NOT add that game runtime to SFS support. |

Between recorded source changes, the preceding source family applies. The
non-release CommonLib port was read for comparison only, not adopted or built.

## Independent runtime routes

| Queried interface | Last compatible route used by SFS |
| --- | --- |
| BodyMorph 4, 5 | Shared public-v4 prefix through ApplyBodyMorphs; public live-update hook plus RTTI-resolved deferred task hook. |
| BodyMorph greater than 5 | Same prefix, with explicit forward-compatibility-assumption log. No upper-version block. |
| NiTransform 1, 2 | ABI-neutral named NiOverride Papyrus calls, not a public-v3 cast. |
| NiTransform 3 or higher | Public-v3 used prefix; higher versions log the compatibility assumption. Invalid callable memory falls back to the independent Papyrus route. |
| ActorUpdateManager 0, original six-entry layout | AddInterface slot 3. |
| ActorUpdateManager 0, public-layout SE backport | AddInterface slot 11. Preserve exact layout discrimination: both implementations really report zero. |
| ActorUpdateManager 1, 2 or higher | Public-v1 prefix, AddInterface slot 11; higher versions are identified as assumed compatible. |
| Missing component / invalid callable memory | Do not call invalid memory. Retain other connected components, native attachment capture and independent dye handling; retry missing components at later startup fences. |

This is an optimistic forward-compatibility policy requested by the maintainer,
not a claim that future ABIs are known. Executable/readable-slot checks catch
invalid addresses, not changed method semantics. A future provider that inserts
or reorders methods can still break compatibility despite passing those checks.
Its exact binary/source must then be reported and investigated. Do not label
synthetic v6/v99 test providers as actual released RaceMenu binaries.

Historical concrete-v3 releases are classified, not mysterious. They are not
silently cast to v4; supporting their old game runtimes would require separate
verified engine layouts. No such game support or VR target is added here.

## Distribution evidence and remaining gaps

The [official RaceMenu files page](https://www.nexusmods.com/skyrimspecialedition/mods/19080?tab=files)
lists SE 0.4.11–0.4.16, multiple AE 0.4.19.x builds, and current 0.4.20.0
Steam/GOG packages. Package, game and queried interface versions are not the
same numbering scheme. The page does not give a complete immutable Git commit
mapping for every historical ZIP; upload dates alone do not prove ABI identity.

Three locally available DLLs have separate read-only PE/RTTI/disassembly
evidence: original SE, UBE AE-to-SE public-v0 backport, and later public-v5.
Their exact hashes and layouts are in the prior audit. Individual AE 0.4.19.x,
historical GOG packages and every re-upload of 0.4.20.0 were not all obtained
and binary-compared. This gap was reported to the maintainer; it does NOT
prevent those packages using their reported compatible interface families.

## Verification

- Reproducible pinned 19-snapshot source-order audit; all 13 public prefixes pass.
- Independent real x64 virtual dispatch tests for BodyMorph 4/5 and synthetic
  higher versions, NiTransform 3 and synthetic higher versions, both v0
  attachment layouts, v1/v2 and synthetic higher attachment versions.
- Invalid callback-memory rejection, actor/argument isolation and registration
  retry/idempotency tests. No unknown package-name gate.
- Production initializer extraction tests cover partial publication, delayed
  components, failed exchanges preserving working connections, and higher
  interface versions connecting without duplicate hooks.
- Public and deferred live-morph production tracking tests cover feature refresh
  triggers with fake engine objects. Actual Skyrim/GPU behavior remains an
  in-game test obligation, including HT2, camera, dye and strip/redress together.
