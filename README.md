# Skyrim Fitting System

`Skyrim Fitting System` is a Skyrim SKSE mod for player-facing fitting outfit management.

It lets the player keep their real equipped gear for gameplay while changing the visible appearance through a native in-game browser, variant workbench, and native armor skinning hooks.

The basic UI structure of Skyrim Fitting System was developed from Skyrim Vanity System.
Skyrim Outfit System Revived was also consulted while implementing parts of the appearance display engine.

## Features
- Snappy ImGui interface for managing armor appearance overrides
- Searchable catalog for:
  - individual armor pieces
  - outfits
  - Modex kits
- Persistent favorites system
- Preview mode for selected gear, outfits, and kits.
- Shared Mod-Configured and Automatic Vanilla-Slot linking, plus per-slot direct exceptions based on either automatic mode
- Actor-local default and conditional appearances with independent visibility state
- Customizable special-effect slot protection (50, 51, 60, and 61 by default) and optional shield appearances
- Ctrl multi-selection and batch appearance registration from the gear catalog
- UTF-8 SFS/Modex kit loading and creation from equipped gear or active overrides
- Built-in fitting-kit generator with plugin scanning, candidate editing, preview, and direct kit creation
- Actor-local Fitting Dye for exact rendered components of registered appearances, opened from the workbench dye action or card context menu
- Actor-local registered-appearance locks that survive catalog previews and applications while remaining independent from manual eye and condition state
- RaceMenu `HH_OFFSET` synchronization for registered high-heel appearances across DAVE, DAV, and native display backends
- Stable generated-kit SFW/NSFW labels that change only through the result-list toggle, not when candidate selection changes
- Display-only workbench sorting and optional selected-actor third-person menu placement with FOV restoration and pause-safe right-drag rotation
- Actor-local live BodyMorph synchronization without polling or a global actor scan
- Built-in SexLab P+ strip/redress integration, including late-linked
  `StripByData` and `StripByDataEx`, with no separate SFS patch
- Optional selected-actor BodyFamily filtering for Equipment, Outfits, and Kits;
  uncertain actors fail open and the registered-appearance workbench is never filtered
- Stable C ABI for external managers to open, close, query, and temporarily disable the native SFS menu shortcut
- SKSE save/load for overrides.
- Modex-style theme loading, fade behavior, and smooth scrolling.
- No external appearance framework dependency.

## Runtime Requirements
- [SKSE64](https://skse.silverlock.org/) matching the installed Skyrim SE/AE runtime
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [powerofthree's Tweaks](https://www.nexusmods.com/skyrimspecialedition/mods/51073) for complete EditorID-based catalog, condition, and armor-classification behavior

SkyUI, DAV/DAVE, RaceMenu, SOS/TNG, Wet Function Redux,
CommonLibSSE-NG, Dear ImGui, XMake, and a C++ compiler are not base runtime
requirements. Optional compatibility features require only their corresponding
mod and that mod's own prerequisites.

## Build Requirements
- [XMake](https://xmake.io) v3.1.0 (exact release used for v1.6.0)
- C++23 compiler on Windows (MSVC or Clang-CL)

## Getting Started
The corresponding source is distributed as a separate download in the Nexus Files tab.
The source archive includes the exact CommonLibSSE-NG and Dear ImGui sources
used by SFS. XMake packages are resolved from the checked-in
`xmake-requires.lock`. See `DEPENDENCIES.md` for exact upstream revisions,
checksums, local patches, and the update policy.

SFS is loaded by **SKSE64**, not by its helper ESL. Install the runtime ZIP as
a normal MO2/Vortex mod, enable `SkyrimFittingSystem-VirtualTokens.esl`, and
launch the game through `skse64_loader.exe`. SFS itself is not a separate tool.
The Skyrim pause menu's **Settings** and **Controls** pages do not contain SFS
controls; after opening SFS with F6, change the binding under
**SFS -> Options -> Toggle UI Button**. See `INSTALL.txt` for exact MO2/Vortex
steps and first-load diagnostics.

## Build
From Windows:

```bat
xmake build
```

This generates `SFSCore.dll` under
`build/v1.6.0/windows/x64/<mode>/` in the project root.

From WSL, to build and deploy directly into the local test mod folder:

```bash
./scripts/build-deploy.sh
```

By default this uses `releasedbg`, but deploys only the runtime DLL. The generated
`.pdb` remains in the local build directory for opt-in crash analysis and is not
copied into MO2.

For a full clean rebuild:

```bash
./scripts/build-deploy.sh --clean
```

By default this deploys to:
`/mnt/f/games/skyrim/modlists/pt_test/mods/Skyrim Fitting System`

## Build And Package
To build the curated runtime and corresponding-source archives from Windows
PowerShell:

```powershell
.\scripts\package-release.ps1
```

For the full local build, deploy, and packaging workflow, see
`docs/Build-Deploy-Release.md`.

This writes the MO2-installable flat runtime ZIP to `Release/`, the minimal
complete corresponding-source ZIP to `Sources/`, matching copies to `dist/`,
and a SHA-256 manifest to `Release/`. Packaging uses `releasedbg`, but the
runtime archive deliberately excludes the generated `.pdb` to keep the user
download small. Debug symbols remain available only in the local build tree.

## Settings And Data
- Runtime settings are stored under:
  - `Data/SKSE/Plugins/SkyrimFittingSystem/settings.json`
  - SFS now creates this file as soon as `SFSCore.dll` loads, before renderer
    and menu-hook initialization. With default MO2 output routing, a new file
    normally appears under
    `Overwrite/SKSE/Plugins/SkyrimFittingSystem/settings.json`.
- Actor-owned appearances, conditions, and visibility are stored in the SKSE co-save. The selected strip-link policy and per-slot mappings are shared user settings applied to all actors, while active transactions and suppression results remain actor-local.
- Fitting Dye colors are stored independently by actor FormID, registered appearance ARMO, and exact rendered-component identity; no source texture, material, actual equipment, or workbench registration data is changed.
- Favorites are stored separately in:
  - `Data/SKSE/Plugins/SkyrimFittingSystem/favorites.json`
- Bundled UI assets are under:
  - `Data/Interface/SkyrimFittingSystem/`

## Runtime Menu API

SFS v1.6.0 exports a stable menu API for external hotkey and menu-management
mods. Managers may temporarily disable the native F6 or user-defined shortcut
without changing its saved binding; this runtime-only state defaults to enabled
on every game launch. Open, Close, and IsMenuOpen remain independent. See
`docs/SkyrimFittingSystem-Menu-API.md` and `extras/SkyrimFittingSystemAPI.h`.
The v1.6.0 module name is `SFSCore.dll`; the four menu C export names are unchanged.

SFS v1.6.0 includes the Kit Generator directly in `SFSCore.dll`. Its tab scans
selected outfit plugins, builds and edits candidate combinations, previews the
selection on the character and in the workbench, and writes finished kits
directly to the SFS user-kit folder. Only this temporary generator preview may
display protected-slot and SOS/TNG candidate pieces; normal registration, kit
application, saving, and visibility filters remain unchanged. No separate
`SFSKitGenerator.dll` is distributed or required.

Generated result rows keep their initial SFW/NSFW classification when the user
changes, edits, or merges candidates. The manual result-list toggle is not
shown. Candidate-name clicks update selection and preview, while the separate
Edit control is the only path into piece editing. Known outfit-pack catalogs
take priority for their set families; other ESPs use the general name, slot,
and NIF-model rules.

The main v1.4.0 package also includes `SkyrimFittingSystem-VirtualTokens.esl`.
Enable this ESL together with the DLL when using Mod-Configured Slot Linking;
it contains fixed non-inventory token forms used only for contextual equipment
and keyword observation.

## Optional Compatibility Patches

The main DLL intentionally does not globally replace `Actor::GetWornArmor()`.
Grid Inventory v1.4.1+ Costume synchronization and Helmet Toggle 2 state
linking are built into SFSCore through narrow public/native signals; neither
requires a replacement DLL or PEX patch. When HT2 hides a still-equipped real
slot-31 headgear, SFS releases only that actor's Hair partition in the skinning
worn mask so the original hair can render; it does not change the ARMO,
inventory, HT2 variant, or Mod-Configured virtual-token behavior. Optional per-consumer patches remain
for Dynamic Feminine Female Modesty Animations OAR 4.30, Wet Function Redux,
and Dynamic Footprints SKSE BASE v3. These use narrow, versioned integration
boundaries;
the core retains actor-local actual equipment, conditions, linking, and display
state. See `docs/RELEASE-NOTES-v1.6.0.md` for the current release scope.

## Fitting Kits

- SFS kits are loaded from and created under:
  - `Data/Interface/SkyrimFittingSystem/user/kits`
- Existing Modex kits are also read from:
  - `Data/Interface/Modex/user/kits`
- UTF-8 Korean, Chinese, and other Unicode names are supported. Invalid JSON is skipped safely.
- SFS can browse, preview, apply, and create kits from equipped gear or active overrides.

## Lint And Format
```bash
./scripts/format.sh
./scripts/format.sh --check
./scripts/lint.sh
./scripts/lint.sh src/ui/Menu.cpp
```

From WSL, `format.sh` prefers Linux `clang-format` when available. `lint.sh` prefers the
Visual Studio LLVM `x64` `clang-tidy.exe` because the generic Visual Studio `Llvm/bin`
binary is a 32-bit build that crashes in this environment.

`lint.sh` also passes `/Y-` to disable MSVC PCH use during linting. xmake's generated
`compile_commands.json` points `clang-tidy` at a PCH path that does not exist, so disabling
PCH is the reliable way to lint these translation units.

## Fast Regression Tests

Run the focused production-rule regression suite from PowerShell:

```powershell
.\tests\run-fast-regressions.ps1
```

It covers runtime layout boundaries, kit generation and navigation, body-family
and dye rules, actor-local BodyMorph/suppression state, all four strip-link
policies, virtual-token and actual-equipment transactions, paused character
rotation isolation, and DAVE/DAV/native refresh dispatch. Skyrim runtime hooks
and rendered 3D behavior still require the corresponding in-game smoke tests.

## Project Generation
For Visual Studio:

```bat
xmake project -k vsxmake
```

For `clangd` or other LSP tooling:

```bat
xmake project -k compile_commands
```
