Skyrim Fitting System v1.4.8 - Corresponding Source Package

This archive contains the preferred source form and build-control files for
the v1.4.8 SFSCore release. It contains no compiled DLL, PDB, PEX, ESL,
build output, nested archive, or FOMOD package.

Build-control files are provided under their real names: xmake.lua and
xmake-requires.lock. The exact CommonLibSSE-NG source used for this release is
included at third_party/CommonLibSSE-NG (commit
3d81614617910e7f34b33d8750881811b5e36445). Dear ImGui source is included
under lib/imgui. See THIRD_PARTY_NOTICES.md for license notices and upstream
source locations.

v1.4.8 includes the v1.4.1 actor-local empty-equipment display bootstrap and
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
The SFS menu also includes an
isolated third-person character presentation controller, with optional
SmoothCam public-API coordination, whose camera offset and player heading are
fully restored on close.

It also includes the read-only Open Animation Replacer final-rendered-outfit
conditions, including the slot-preserving
SFS_IsShownArmorInSlotHasKeyword condition used by the separate DFFMA OAR 4.30
configuration patch. The optional Dynamic Footprints consumer ABI exposes only
the currently displayed SFS footwear FormID; it never alters actual equipment
or armor keywords.

Grid Inventory v1.4.1+ Costume synchronization is built into SFSCore through
Grid Inventory's public SKSE message. Only a non-empty, player-owned armor
Costume replaces the player's SFS registered appearance. Empty, cleared, and
non-armor Costume signals leave saved SFS appearances unchanged. It does not
replace GridInventory.dll; the retired v1.4.4 replacement-DLL source remains
only as historical reference.
The separately distributed DFFMA OAR, Helmet Toggle 2, and Dynamic Footprints
compatibility archives contain the source or editable configuration for their
own integration boundary. They are intentionally not merged into the main
runtime archive.

For Immersive Equipment Displays, v1.4.7 adds a narrow custom-skin rebuild
compatibility boundary: only when IED owns the exact pre-patched worn-item
visitor target used by Skyrim does SFS filter the hidden-real-equipment pass
through the original engine target, then request IED's public actor evaluation.
It is actor-local and does not alter normal calls, appearance data, linking
modes, strip/redress handling, or display backends.

The runtime package is distributed separately and contains the compiled DLL,
PDB, PEX, ESL, localization data, and UI assets.
