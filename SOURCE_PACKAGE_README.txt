Skyrim Fitting System v1.7.1 - Nexus Source Package

This archive intentionally contains human-readable source and project data only.
It contains no compiled DLL, PDB, PEX, ESL, build output, nested archive, or FOMOD package.

For Nexus scanner compatibility, xmake.lua is stored as xmake.lua.txt and
xmake-requires.lock is stored as xmake-requires.lock.txt. Remove the final .txt
suffixes before building. Use the pinned xmake v3.1.0 release. The exact
CommonLibSSE-NG v6.7.0 source used for this
release is included under third_party/CommonLibSSE-NG. Dear ImGui sources
required by SFS are included under lib/imgui. Exact revisions and checksums are
listed in DEPENDENCIES.md.

v1.7.1 reduces repeated Papyrus type inspection and unused caller tracing on
ordinary Form keyword reads. First-call hooks, late native binding, P+ retries,
final-rendered nudity, and strip/redress behavior are retained. The reported
NPC dialogue pause has not been timed in game. See
docs/NPC-DIALOGUE-PERFORMANCE-2026-09-24.md and the bilingual release notes.

v1.7.0 handles E9 inline detours at supported armor skinning call sites.
Passive actors keep the previous inline route with original machine state;
active SFS actors use the engine call and SFS display policy. SFS takes
display priority in this fallback; arbitrary prior inline behavior is not
preserved for active actors. Ordinary E8 provider routes remain unchanged.
See docs/V1.7.0-INLINE-DETOUR-FIX.md and the bilingual release notes.

v1.6.9 fixes Grid base-row replacement scope, condition-action drop rejection,
and stale equipment/condition queue ownership at save/load boundaries. See
docs/V1.6.9-TRANSACTION-REGRESSION-FIXES.md. Additional fixes cover stale SOS
callbacks, UI control ownership, kit paths, condition IDs and locale parity;
see docs/V1.6.9-STATE-BOUNDARY-FIXES.md and the bilingual release notes.

v1.6.8 releases actor-local morph/HH scene references and dye GPU targets on
real unload/delete events. Saved colors and ordinary hidden/preview tracking
are preserved; old queued work cannot finish a replacement actor task. See
docs/V1.6.8-ACTOR-RESOURCE-LIFECYCLE.md for scope, tests and limitations.

v1.6.7 repairs registered HH_OFFSET selection and late attachment recovery in
the legacy NiOverride and public NiTransform paths, and removes redundant worn
equipment and caller-identity work. Existing stripping, morph, dye, backend and
API ownership is retained. See docs/HH-OFFSET-AND-INVENTORY-AUDIT-2026-09-19.md
and the bilingual release notes for source/ABI checks and in-game limitations.

v1.6.6 adds the explicit SKSE AddTask rendered-outfit query entry while
preserving the original export and v1 POD layout. Consumers must enforce
actual AddTask execution; arbitrary worker or rendering-thread queries are
not supported. See docs/V1.6.6-GAMETASK-API-AUDIT.md for the handoff,
regression coverage, and diagnostic-code audit. The public header below
contains both export names and their shared caller-buffer contract.

v1.6.5 adds the read-only rendered-outfit API, verified IED 1.7.4 BipedSlot
condition bridge, gamepad LT + RS rotation, mapped Cancel handling, and
localized title-bar hints. See docs/RELEASE-NOTES-v1.6.5.md and its Korean
counterpart for scope and outstanding in-game verification. The public header
is extras/SkyrimFittingSystemRenderedOutfitAPI.h; its ABI and consumer contract
are described in docs/DEVELOPMENT-v1.6.5-RENDERED-OUTFIT-API.md.

SFS preserves the v1.4.1 actor-local empty-equipment display bootstrap and
the complete in-game Kit Generator built into SFSCore. The generator can scan
outfit plugins, edit candidate combinations, preview them read-only in the
character and workbench, and write finished kits directly to the SFS user-kit
folder. No separate SFSKitGenerator.dll is included or required. Generator-only
preview exceptions do not change normal registration, application, or saving.
Fitting Dye is integrated into registered-appearance cards in the workbench and
their right-click context menu. It saves colors independently by actor FormID,
registered appearance ARMO, and exact rendered-component identity, and applies
them through private geometry-scoped renderer textures. It never edits source
DDS/NIF files, actual equipment, inventory, workbench rows, conditions, kits,
virtual tokens, strip/redress state, or DAVE/DAV/native ownership.
The integrated scanner uses bounded parallelism and keeps unrelated slots out
of exact multi-slot DP. Candidate selection and editing preserve the result-list
SFW/NSFW classification; the manual result-list toggle button has been removed.
Known outfit-pack catalogs take priority when grouping their set pieces, while
unknown packs keep using the general name, slot, and NIF-model rules. Cross-ESP
deduplication requires substantially shared model paths and slot coverage, and
the result-list Delete Item command removes every Ctrl+left-click selection
without touching unselected kits.
The SFS menu also includes an isolated third-person character presentation
controller with optional SmoothCam public-API coordination. It follows the
actor selected in the workbench, including a crosshair-selected NPC at menu
open, without moving that actor's world position. The original camera target,
camera settings, and temporary actor facing are restored on actor change or
menu close.

It also includes the read-only Open Animation Replacer final-rendered-outfit
conditions, including the slot-preserving
SFS_IsShownArmorInSlotHasKeyword condition used by the separate DFFMA OAR 4.30
configuration patch. The optional Dynamic Footprints consumer ABI exposes only
the currently displayed SFS footwear FormID; it never alters actual equipment
or armor keywords.

Grid Inventory v1.4.1+ Costume synchronization is built into SFSCore through
Grid Inventory's public SKSE message. It does not replace GridInventory.dll;
the retired v1.4.4 replacement-DLL patch and its binary are intentionally
excluded from this source package and the public release tree.
Helmet Toggle 2 integration is also built into SFSCore and observes only its
exact player/NPC/follower state signals; no HT2 PEX replacement is distributed.
For a hidden, still-equipped real headgear which occupies Hair 31, the renderer
releases only that actor's Hair bit from Skyrim's skinning worn mask. The actual
ARMO, inventory, HT2/DAVE variant, registered appearance records, and contextual
Mod-Configured virtual-token catalog remain unchanged.
The separately distributed DFFMA OAR and Dynamic Footprints compatibility
archives contain the source or editable configuration for their own integration
boundary. They are intentionally not merged into the main runtime archive.

The runtime package is distributed separately and contains the compiled DLL,
PEX, ESL, localization data, and UI assets. PDB files remain local debugging
artifacts and are not included in runtime or source ZIPs.

v1.5.1 adds an actor-aware, display-only body-family filter to the Equipment,
Outfits, and Kits catalogs. Female groups are CBBE/3BA/3BBB, UNP/BHUNP, UBE,
and Vanilla/fallback; male groups are HIMBO, SAM, and Vanilla/fallback. It uses
loaded form and selected-actor metadata only, keeps unknown same-sex entries
visible, and does not scan BodySlide or NIF files on disk. The same release
restores actor-local live RaceMenu BodyMorph synchronization and limits global
input suppression to a real focused text editor or intentional keybind capture.

This release uses the vendored CommonLibSSE-NG v6.7.0 source revision
3d81614617910e7f34b33d8750881811b5e36445 with the narrow local SE/AE vtable
layout correction documented in third_party/CommonLibSSE-NG/SFS_LOCAL_PATCHES.md.
The workbench actor selector itself remains the v1.4.5 implementation: the
existing 4096-unit TES range pass, selectable-actor rule, duplicate filtering,
nearest-first ordering, and 32-actor limit are unchanged. Saved workbench data
and appearance, strip/redress, DAVE/DAV, and native display paths are not
changed.

SFS also centralizes the verified Skyrim SE 1.5.97 and Skyrim AE runtime
hook layouts, validates core hook instruction forms before patching, and adds
boundary tests for supported and rejected runtime versions. The IED custom-skin
compatibility boundary is attached to the actual VisitWornItems call sites so
SFS filtering never passes an incompatible visitor object into IED's concrete
visitor hook.

v1.5.4 fixes legacy RaceMenu ActorUpdateManager initialization, separates
verified interface versions, and adds independent ABI regression tests.
v1.5.5 repairs registered-appearance live morph tracking through previews,
temporary hiding, and late attachment completion. Initial preview morphing
remains separate from subsequent updates, and stale queued actor tasks are
invalidated. Tests compile the production tracking functions against a fake
engine; rendered DAVE/DAV/native behavior still needs in-game verification.
v1.5.6 adds renderer-state-scoped Fitting Dye substitution for renderer stacks,
restores Helmet Toggle 2 ownership separation between real equipment and
registered appearances, closes the late-linked SexLab P+ member-native observer
window, and separates SOS/TNG internal genital or cover forms from ordinary
catalog armor. Modex/SFS kit imports retain installed armor pieces while
dropping unavailable ones, and Unicode kit generation is hardened.
v1.5.7 corrects RaceMenu ActorUpdateManager registration for original SE,
public-layout AE-to-SE backports, and current public interfaces. This restores
late DAVE attachment observation and live BodyMorph/OBody updates without a
forced 3D refresh or any change to Fitting Dye.
See docs/RELEASE-NOTES-v1.6.1.md, its Korean counterpart, and
docs/RaceMenu-ABI-Audit.md for changes and verification limits.
The existing actor-local DAVE/DAV/native, BodyMorph, registered high heels,
strip/redress, appearance locks, and dye behavior are preserved.
