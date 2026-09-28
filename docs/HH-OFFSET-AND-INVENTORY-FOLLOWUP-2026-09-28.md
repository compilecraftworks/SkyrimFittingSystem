# 1.7.1 feedback follow-up: heels and SkyUI latency

Historical diagnosis below. The subsequent [heel lifetime fix](HH-PENDING-ATTACHMENT-FIX-2026-09-29.md)
and [query-cost reduction](SKYUI-QUERY-COST-FIX-2026-09-29.md) document current
implementation and verification; this original investigation was read-only.

This is a read-only runtime-code investigation, not a fix or an in-game
reproduction. Native heel, skinning and virtual-token files remain unchanged.
The separate Favorites Only UI request is implemented in the 1.7.2 working tree.

## New report

- Actual H2135 Fantasy Series 8 Boots apply HH_OFFSET 9 correctly.
- The same boots registered as an SFS appearance, without actual boots equipped,
  render but do not raise the actor.
- Wearing exactly the registered item restores height; different actual boots
  do not, even with their own HH_OFFSET.
- SkyUI inventory opening remains about four seconds; roughly 20 extra items
  noticeably increase the delay. The reporter tested released SFS 1.7.1.

These are reporter observations, not local measurements. The earlier report
identified RaceMenu 0.4.20.0; this follow-up does not supply a new DLL hash/log.

## Confirmed code conditions and test gap

`RaceMenuBodyMorph.cpp` only records attachment roots containing HH_OFFSET.
`ResolveRegisteredHighHeelState` removes any root whose parent chain no longer
reaches the actor's 3D root, then resolves the registered armor's visible state.
The native snapshot observes newly attached scene branches, not arbitrary NIF
metadata. Consequently a missing/non-retained source root can leave the selected
offset absent even when geometry is visible. That possibility needs a real
attachment/metadata fixture, not an assumption that every mesh retains its root.

Both public and legacy synchronization require RaceMenu's NPC/internal position
to exist before replacing it with the selected offset. Public failures retry a
bounded number of times; the legacy path cleans its bootstrap and reports failure.
Absence of this position is used as evidence that automatic transforms were not
accepted, but it does not by itself distinguish disabled equippable transforms
from a source-observation failure. Removing the check blindly would be unsafe;
the source and ownership decision must be corrected together.

Actual equipment equal to registered equipment bypasses SFS's additional
`ApplyArmorAddon` pass. It is therefore a materially different attachment path,
consistent with the report, not proof of its exact cause.

The existing `RaceMenuHighHeelTests.cpp` compiles production synchronization,
queue and callbacks, but stubs `ResolveRegisteredHighHeelState` and
`RememberHighHeelAttachmentRoots`. It supplies selectedOffset directly. Existing
passes do not establish real NIF/scene metadata capture, nor reproducing this
report. A repair must cover actual=none, same item, different HH item, ordinary
flat boots, hide/strip/redress, and all supported attachment owners without
altering actual inventory or unrelated BodyMorph/dye ownership.

Reference checked against the primary upstream implementation:
[NiTransformInterface.cpp](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/NiTransformInterface.cpp)
and [SkeletonExtender.cpp](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/SkeletonExtender.cpp).
Upstream scans HH_OFFSET/SDTA and maintains the automatic internal component;
this source does not certify the reporter's distributed binary or override order.

## Inventory finding and remaining uncertainty

No SFS InventoryMenu/ContainerMenu-open or SkyUI-list hook was found in `src`.
The explicit `PlayerInventory::GetInventoryArmorFormIDs` scan serves SFS's own
inventory-only list filter, not a SkyUI menu-open callback.

Indirect work remains: `BuildDisplaySet` collects worn inventory for managed
actors; final body-keyword decisions and registered-root membership can call it.
Each individual snapshot is request-local, not shared across repeated requests.
Papyrus worn queries still retain caller tracing for strip/redress semantics.
The 1.7.1 global-type inspection memo and ordinary keyword tracing reduction
remain present. None of these observations measures which path consumes the
reported four seconds, or establishes a specific growth rate with inventory size.

Next diagnostic step is call-count/timing attribution of the menu-open interval,
with identical actor/appearance/conditions and controlled inventory sizes. Do not
disable nudity, strip-link, DD/P+, or final-render queries as a performance shortcut.
No diagnostic hooks, logging changes, runtime patches or MO2 changes were made
for this investigation.

## Second-pass evidence: confirmed HH tracking defect

The user confirmed that the inventory delay is a reporter observation; the
user's own installation has not been checked for this symptom. Do not describe
the local installation as reproducing the report.

An isolated probe now compiles the **unchanged production**
`IsAttachedToActorRoot`, `FindLastHighHeelOffset`,
`RememberHighHeelAttachmentRoots`, `ResolveRegisteredHighHeelState`, sync and
queue bodies. Unlike the original regression, it does not stub the selected
offset or root resolver. Fake engine nodes supply parent links, children and
typed NiFloatExtraData, while the existing recording RaceMenu provider models
transform writes. This remains a controlled source test, not engine emulation.

Reproduced with the public v3 and legacy v1/v2 routes:

1. An already attached registered branch containing HH_OFFSET=9 resolves and
   raises the actor correctly.
2. Recording that same valid branch before grafting succeeds.
3. If the first sync runs before the parent chain reaches the actor root,
   `ResolveRegisteredHighHeelState` immediately deletes the owned record.
4. No offset plus no previous activation returns `Complete`, not `Retry`.
   The advertised bounded retry does not cover this initial pending-root case.
5. Grafting the same branch later, even followed by another queued sync, does
   not recover the source: the resolver only knows recorded roots and does not
   rediscover the now-attached branch. Height remains zero.
6. A new accepted capture of the same branch restores height. Its numeric
   metadata is not defective.
7. A temporary detach/reparent of a previously active branch likewise removes
   the record, clears height, and fails to recover on same-node reattachment
   without another capture.

This proves a source lifecycle defect under that ordering. It does **not** prove
which backend/attachment ordering occurred on the reporter's machine. The
existing morph resolver already treats a pre-graft callback as pending and
permits bounded completion checks; the heel resolver does not. Fixing heel
ownership/pending completion should not modify BodyMorph, actual inventory,
or broaden indefinite retained-node lifetimes.

The local H2135 Series 8 UBE Greaves mesh was also parsed read-only using the
NifTools format: `FS8_Greaves_1.nif` has HH_OFFSET=8 on BSTriShape block 13,
not on Scene Root block 0. Its SHA256 is
`077e41943fd7a483316d48344834037001e72d9c0a95e1f1035968576171c431`.
This is **not** the reporter's `boots_0/1.nif`, HH_OFFSET=9 asset. The local
original BSA contains ground Greaves meshes; no identical reporter fixture was
obtained. Therefore loss of a NIF root containing metadata is not established
as the cause and must not be asserted as a finding.

## Second-pass evidence: SkyUI indirect work path

SKSE's primary source shows that extended inventory data is built for list
items and calls `PlayerCharacter::GetArmorValue` for each armor; weapon/ammo
entries call `GetDamage`. The extension's keyword enumeration reads the native
keyword array directly; it is not the Papyrus `Form.HasKeyword` hook optimized
earlier. Sources:

- [SKSE 2.0.20 InventoryData](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/ScaleformExtendedData.cpp#L707)
- [SKSE StandardItemData constructor](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_Scaleform.cpp#L1146)
- [SkyUI ItemMenu initialization](https://github.com/schlangster/skyui/blob/master/src/ItemMenus/ItemMenu.as)

Read-only parsing of the local Skyrim.esm found `WellFitted` and `CustomFit`
`ModArmorRating` entry-point effects containing `WornHasKeyword(ArmorCuirass)`.
Thus armor-value/perk evaluation is a concrete connection to investigate, not
merely a generic suggestion about background scripts. The reporter's active
perks, winning overrides and actual evaluation call counts are unknown.

SFS's engine `WornHasKeywordCondition` evaluates the original first, then calls
`GetDisplayedBodyKeywordState`. For ArmorCuirass/ClothingBody that creates a new
request-local EquippedArmorSnapshot and builds display state; the original
condition result is not a reusable equipment snapshot. BuildDisplaySet collects
worn equipment and reevaluates conditional visibility, registered appearances
and relevant compatibility before preparing final-outfit API data. A fresh
query repeats this work even when the underlying state has not changed.
The player always passes ShouldManageActorDisplay; an inactive display is only
recognized after the local collection in this path.

A second isolated probe compiles the current production query/snapshot bodies
against the existing display/engine fixtures. Matching body queries of
100 / 120 / 1000 produce 100 / 120 / 1000 equipment-collection requests.
100 managed-but-inactive queries also collect 100 times before returning the
original-answer fallback. Unrelated keywords and unmanaged actors collect zero.
This verifies repeated request work and the retained guards, **not** the
engine's scan duration, total entries visited, or a measured four-second menu.

The remaining link to certify is an actual menu-open call trace/timing sample
showing that the reporter reaches this condition frequently enough to dominate
the delay. No local menu timing was performed. An attempted bounded read-only
code inspection supplied no usable data because the earlier game process was
no longer available. No debugger, injection, code patch or gameplay action was
performed. Do not label the performance cause fully confirmed or fixed.

## Diagnostic-helper exception shown by the user

The first isolated ArchiveProbe run tried to resolve localized perk text through
Mutagen's default global load-order/string discovery. Reading the sandboxed
user Plugins.txt failed with UnauthorizedAccessException; the uncaught .NET
exception displayed an ArchiveProbe.exe application-error dialog (0xe0434352).
This was our audit utility, **not** SFSCore.dll or SkyrimSE.exe.

The audit now supplies explicit empty string/archive directories (only IDs and
conditions are needed), and wraps all audit operations in a top-level exception
handler. A deliberately missing archive exits with a console error and code 1,
without an unhandled-exception dialog. Process inspection confirmed no remaining
ArchiveProbe/WerFault/JIT debugger process. No user game/mod files were changed.

Reproducible artifacts, all outside packaged runtime/source inputs:
`output/hh-inventory-audit-20260928/GenerateProbe.ps1`, `RootProbe.cpp`,
`BodyProbe.cpp`, `build-probe.cmd`, `probe-results.txt`, `inspect_assets.py`,
and the read-only ArchiveProbe project. C++ uses the existing MSVC toolchain;
ArchiveProbe reuses InspectArmorPlugin's exact Mutagen 0.51.3 cached closure,
with package feeds cleared. The currently installed dotnet SDK reports
10.0.401; no SDK/library was downloaded or upgraded by this audit.

Production HH/body-query code and installed DLLs remain unchanged. These are
diagnostic findings, not completed fixes or release verification.
