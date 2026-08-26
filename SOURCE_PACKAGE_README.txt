Skyrim Fitting System v1.4.9 - Nexus Source Package

This archive intentionally contains human-readable source and project data only.
It contains no compiled DLL, PDB, PEX, ESL, build output, nested archive, or FOMOD package.

For Nexus scanner compatibility, xmake.lua is stored as xmake.lua.txt and
xmake-requires.lock is stored as xmake-requires.lock.txt. Remove the final .txt
suffixes before building. Use the pinned xmake v3.1.0 release. The exact
CommonLibSSE-NG v6.7.0 source used for this
release is included under third_party/CommonLibSSE-NG. Dear ImGui sources
required by SFS are included under lib/imgui. Exact revisions and checksums are
listed in DEPENDENCIES.md.

v1.4.9 includes the v1.4.1 actor-local empty-equipment display bootstrap and
the complete in-game Kit Generator built into SFSCore. The generator can scan
outfit plugins, edit candidate combinations, preview them read-only in the
character and workbench, and write finished kits directly to the SFS user-kit
folder. No separate SFSKitGenerator.dll is included or required. Generator-only
preview exceptions do not change normal registration, application, or saving.
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
the retired v1.4.4 replacement-DLL source remains only as historical reference.
The separately distributed DFFMA OAR, Helmet Toggle 2, and Dynamic Footprints
compatibility archives contain the source or editable configuration for their
own integration boundary. They are intentionally not merged into the main
runtime archive.

The runtime package is distributed separately and contains the compiled DLL,
PDB, PEX, ESL, localization data, and UI assets.

v1.4.9 uses the vendored CommonLibSSE-NG v6.7.0 source revision
3d81614617910e7f34b33d8750881811b5e36445 with the narrow local SE/AE vtable
layout correction documented in third_party/CommonLibSSE-NG/SFS_LOCAL_PATCHES.md.
The workbench actor selector itself remains the v1.4.5 implementation: the
existing 4096-unit TES range pass, selectable-actor rule, duplicate filtering,
nearest-first ordering, and 32-actor limit are unchanged. Saved workbench data
and appearance, strip/redress, DAVE/DAV, and native display paths are not
changed.

v1.4.9 also centralizes the verified Skyrim SE 1.5.97 and Skyrim AE runtime
hook layouts, validates core hook instruction forms before patching, and adds
boundary tests for supported and rejected runtime versions. The IED custom-skin
compatibility boundary is attached to the actual VisitWornItems call sites so
SFS filtering never passes an incompatible visitor object into IED's concrete
visitor hook.
