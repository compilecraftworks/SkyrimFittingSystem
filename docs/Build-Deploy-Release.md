# Build, Deploy, And Release

This document describes the WSL-based workflow for:

- local builds with `scripts/build.sh`
- local deploy runs with `scripts/build-deploy.sh`
- dist package creation with `scripts/build-package.sh`
- tag-based GitHub releases with `gh`

## Prerequisites

- Run from WSL in the repo root.
- `powershell.exe` must be available in `PATH`.
- `wslpath` must be available.
- The pinned xmake v3.1.0 release must be installed on the Windows side, since `build.sh` invokes it through PowerShell.
- `gh` should be authenticated for release work.

The complete dependency baseline, immutable revisions, archive checksums, and
local CommonLib patch boundary are recorded in `DEPENDENCIES.md`.

## Versioning

All build flows use the release base in `VERSION` through `scripts/version.sh`.

- If `HEAD` is clean and points at a semver tag like `v1.3.0`, the display version is `1.3.0`.
- Otherwise the display version is `<base>-dev+<shortsha>` with an additional `.dirty` suffix when the worktree is dirty.

Examples:

- tagged clean release: `1.3.0`
- untagged dev build: `1.3.0-dev+abc1234`
- dirty worktree: `1.3.0-dev+abc1234.dirty`

For real release packages, keep the worktree clean and put exactly one release tag on `HEAD`.

## Build

`build.sh` builds the native plugin through Windows `xmake`.

```bash
./scripts/build.sh
./scripts/build.sh releasedbg
./scripts/build.sh release
./scripts/build.sh debug
./scripts/build.sh --clean releasedbg
```

Modes:

- `release`: optimized release build
- `debug`: debug build
- `releasedbg`: optimized build with debug info

Outputs:

- SE/AE DLL: `build/v1.4.9/windows/x64/<mode>/SFSCore.dll`
- SE/AE PDB when present: `build/v1.4.9/windows/x64/<mode>/SFSCore.pdb`

Notes:

- `build.sh` retries with `xmake -j 1` if a parallel MSVC build hits transient `D8000` / `UNKNOWN COMMAND-LINE ERROR`.
- The current official package target is Skyrim SE/AE.

## Build And Deploy

`build-deploy.sh` builds the plugin and copies the flat runtime payload into a target mod folder.

```bash
./scripts/build-deploy.sh
./scripts/build-deploy.sh release
./scripts/build-deploy.sh debug
./scripts/build-deploy.sh --clean releasedbg
```

Defaults:

- mode: `releasedbg`
- destination:
  `/mnt/f/games/skyrim/modlists/pt_test/mods/Skyrim Fitting System`

Override the destination with `MOD_DIR`:

```bash
MOD_DIR="/mnt/f/games/skyrim/modlists/some_profile/mods/Skyrim Fitting System" \
  ./scripts/build-deploy.sh
```

What gets deployed:

- SE/AE `SFSCore.dll`
- SE/AE `SFSCore.pdb` when present
- the repo `data/` tree

Notes:

- `build-deploy.sh` deploys the SE/AE runtime DLL into the mod folder.
- Existing development deploys have the old `SkyrimFittingSystem.dll`, manual
  `000_SkyrimFittingSystem.dll`, temporary native bridge, exact retired main
  ESP/SEQ, and per-mod bridge PSC/PEX files removed automatically. User kits,
  settings, and optional compatibility patches are not cleanup targets.
- The script will fail if the destination file is locked by Skyrim, MO2, or another process.

## Build Dist Package

`build-package.sh` creates the release zip under `dist/`.

```bash
./scripts/build-package.sh
./scripts/build-package.sh --clean
```

Output:

- `dist/Skyrim Fitting System v<version>.zip`

The package contains:

- a single `Data/` wrapper containing the repo runtime payload
- the freshly built SE/AE plugin under `Data/SKSE/Plugins/`
- matching `.pdb` files when present

For v1.3.0, the staged main payload must not contain the retired
`SkyrimFittingSystem-SexLab.esp`, its SEQ file, or the removed SexLab, DD,
Private Needs, Soulgem Oven, Bathing in Skyrim, Body Search, and SOS bridge
PEX/PSC files. The main mod runs without an ESP. Personal files under
`Interface/SkyrimFittingSystem/user` are user data and must be preserved when
replacing an older installation; they are not cleanup targets.

Important:

- The package script always rebuilds the SE/AE plugin before staging it.
- The archive version string comes from `scripts/version.sh`.
- If `HEAD` is not a clean semver tag, the zip name will be a dev build name instead of a release version.
- The archive deliberately contains no FOMOD metadata; the top-level `Data/` folder is the complete runtime payload.

## Release Workflow

Recommended release flow:

1. Make sure the release commit is on the intended branch.
2. Make sure the worktree is clean.
3. Optionally do a final validation build:

```bash
./scripts/build-deploy.sh
```

4. Create or move the release tag:

```bash
git tag v1.3.0
```

If you intentionally need to retarget an existing tag:

```bash
git tag -f v1.3.0 HEAD
```

5. Push the branch and tag:

```bash
git push origin main
git push origin v1.3.0
```

If you retagged an existing release tag:

```bash
git push origin -f v1.3.0
```

6. Build the release package:

```bash
./scripts/build-package.sh
```

7. Create the GitHub release:

```bash
gh release create v1.3.0 \
  -R PenguinToast/SkyrimFittingSystem \
  --title "v1.3.0" \
  --notes-file /path/to/release-notes.md
```

8. Upload the dist zip:

```bash
gh release upload v1.3.0 \
  "dist/Skyrim Fitting System v1.3.0.zip" \
  -R PenguinToast/SkyrimFittingSystem
```

If you need to replace an existing asset:

```bash
gh release upload v1.3.0 \
  "dist/Skyrim Fitting System v1.3.0.zip" \
  -R PenguinToast/SkyrimFittingSystem \
  --clobber
```

## Release Notes

A simple release notes file works well:

```md
## Changelog

- Brief summary of the main fix or feature.
- Any packaging or compatibility changes.
- Any noteworthy risks or limitations.
```

Then create the release with:

```bash
gh release create v1.3.0 \
  -R PenguinToast/SkyrimFittingSystem \
  --title "v1.3.0" \
  --notes-file release-notes.md
```

## Verification

Useful checks after publishing:

```bash
git tag --points-at HEAD
gh release view v1.3.0 -R PenguinToast/SkyrimFittingSystem --json assets,url
sha256sum "dist/Skyrim Fitting System v1.3.0.zip"
```

Verify:

- `HEAD` has the intended release tag
- the release exists on GitHub
- the zip asset is attached
- the package checksum is recorded if you need one
