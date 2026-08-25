# Skyrim Fitting System

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/M1P225QD23)

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
- Stable generated-kit SFW/NSFW labels that change only through the result-list toggle, not when candidate selection changes
- Display-only workbench sorting and optional third-person menu character placement with right-drag rotation
- Actor-local live BodyMorph synchronization without polling or a global actor scan
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
- [XMake](https://xmake.io) 3.0.0+
- C++23 compiler on Windows (MSVC or Clang-CL)

## Getting Started
The corresponding source is distributed as a separate download in the Nexus Files tab.
The source archive includes the Dear ImGui source and the exact
CommonLibSSE-NG source used for the release. See `THIRD_PARTY_NOTICES.md` for
license notices and upstream provenance.

## Build
From Windows:

```bat
xmake build
```

This generates `SFSCore.dll` under
`build/v1.4.7/windows/x64/<mode>/` in the project root.

From WSL, to build and deploy directly into the local test mod folder:

```bash
./scripts/build-deploy.sh
```

By default this uses `releasedbg`, so the deploy includes a `.pdb` alongside the DLL for better crash logs.

For a full clean rebuild:

```bash
./scripts/build-deploy.sh --clean
```

By default this deploys to:
`/mnt/f/games/skyrim/modlists/pt_test/mods/Skyrim Fitting System`

## Build And Package
To build a release mod archive:

```bash
./scripts/build-package.sh
```

For the full local build, deploy, and packaging workflow, see
`docs/Build-Deploy-Release.md`.

This writes a zip under `dist/` named like:
`Skyrim Fitting System v1.4.7.zip`

Tagged clean builds produce a normal `X.Y.Z` archive version.
Dirty or untagged builds produce a `X.Y.Z-dev+<sha>[.dirty]` archive version.

The archive root contains a single `Data/` directory. Install the archive as-is
with MO2, or copy the contents of that `Data/` directory into Skyrim's `Data/`.
Packaging uses `releasedbg`, so the archive also includes a `.pdb` next to the DLL.

## Settings And Data
- Runtime settings are stored under:
  - `Data/SKSE/Plugins/SkyrimFittingSystem/settings.json`
- Actor-owned appearances, conditions, and visibility are stored in the SKSE co-save. The selected strip-link policy and per-slot mappings are shared user settings applied to all actors, while active transactions and suppression results remain actor-local.
- Favorites are stored separately in:
  - `Data/SKSE/Plugins/SkyrimFittingSystem/favorites.json`
- Bundled UI assets are under:
  - `Data/Interface/SkyrimFittingSystem/`

## Runtime Menu API

SFS v1.4.7 exports a stable menu API for external hotkey and menu-management
mods. Managers may temporarily disable the native F6 or user-defined shortcut
without changing its saved binding; this runtime-only state defaults to enabled
on every game launch. Open, Close, and IsMenuOpen remain independent. See
`docs/SkyrimFittingSystem-Menu-API.md` and `extras/SkyrimFittingSystemAPI.h`.
The v1.4.7 module name is `SFSCore.dll`; the four C export names are unchanged.

SFS v1.4.7 includes the Kit Generator directly in `SFSCore.dll`. Its tab scans
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
Optional per-consumer patches are provided for Dynamic Feminine Female Modesty
Animations OAR 4.30, Helmet Toggle 2, and Dynamic Footprints SKSE BASE v3.
Grid Inventory v1.4.1+ Costume synchronization is built into SFSCore through
its public SKSE message rather than a replacement DLL. These use narrow,
versioned integration boundaries;
the core retains actor-local actual equipment, conditions, linking, and display
state. See `docs/RELEASE-NOTES-v1.4.7.md` for their exact scope.

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

## Project Generation
For Visual Studio:

```bat
xmake project -k vsxmake
```

For `clangd` or other LSP tooling:

```bat
xmake project -k compile_commands
```
