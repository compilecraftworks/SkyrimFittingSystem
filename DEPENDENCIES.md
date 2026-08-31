# Dependency baseline

This file is the reproducible external-dependency baseline for Skyrim Fitting
System v1.5.1. Dependency updates follow three rules: use the latest verified
stable upstream release, pin an immutable tag/revision/checksum, and document
every local change and verification boundary.

## Direct dependencies and tools

| Component | Pinned stable version | Immutable reference | Verification |
|---|---:|---|---|
| CommonLibSSE-NG | v6.7.0 | `3d81614617910e7f34b33d8750881811b5e36445` | Vendored under `third_party/CommonLibSSE-NG`; local SE/AE layout correction documented in `SFS_LOCAL_PATCHES.md` |
| Dear ImGui | v1.92.9b | `f1cc2ae15e53a861a874c3034aae6798fde194ab` | Vendored under `lib/imgui`; source ZIP SHA-256 `e1c46d676c2bcb7ced847ba27f50553e33a19db97b3cadaec7f8be64449139f8` |
| nlohmann/json | v3.12.0 | exact XMake requirement | XMake recipe archive SHA-256 `4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187`; recipe repository revision is recorded in `xmake-requires.lock` |
| XMake | v3.1.0 | `96ad28edb71dc4e9c8193924a491629c656e8e8c` | Windows x64 release ZIP SHA-256 `92c320923a0ba52d21934d107ae7fa4cbf03ea5ce972177e7abc2fb7905bd8bd`; minimum enforced by `set_xmakever("3.1.0")` |

`rapidcsv` is not used directly by SFS and is therefore not declared by the
root project. It must not appear in the SE/AE lock graph merely because an old
build once declared it.

## CommonLibSSE-NG transitive baseline

These versions come from the pinned CommonLibSSE-NG v6.7.0 build definition.
Together they are treated as the verified build closure of that stable upstream
release. SFS does not independently replace a member of this closure merely
because a newer standalone release exists. Their exact resolved recipes are
locked in `xmake-requires.lock`.

A transitive component is changed only when the pinned upstream release changes,
or when a confirmed security, correctness, or platform defect requires the
smallest documented exception. Any exception must record its reason, immutable
reference, checksum, compatibility boundary, clean-build result, and regression
verification before it becomes part of this baseline.

| Component | Version selected by CommonLib | XMake recipe archive SHA-256 |
|---|---:|---|
| DirectXMath | 2024.02 | `214d71420107249dfb4bbc37a573f288b0951cc9ffe323dbf662101f3df4d766` |
| DirectXTK | 24.2.0 | `edb643b2444ff24925339cfb1bc9f76c671d5404a5549d32ecaa0d61bbab28c9` |
| spdlog | v1.16.0 | `3d25808d2fc4db86621a46855800c99ab5734999b61c4cbf9470edf631555397` |
| Xbyak | v7.06 | `2d4b312769d3ff12b26ede3e9b105d336ae2b6c7ad3175921acc1ed001213a63` |

The resolved build-only closure also contains CMake 3.30.5 and Ninja 1.11.0.
Their package versions and immutable XMake recipe repository revision
`af695e2f9c6f2ed5fdc2c27d00ca2923bc33335b` are recorded in
`xmake-requires.lock`. The generated lock file may retain the recipe
repository's descriptive `branch = "master"` field, but resolution is pinned by
the adjacent full commit and package version rather than by the moving branch.

The project enables Xbyak and disables Skyrim VR. Optional CommonLib packages
behind disabled REX/VR/test options are not part of the normal SFS dependency
graph.

## Local patch boundary

SFS carries one narrow source correction on top of CommonLibSSE-NG v6.7.0 for
the SE/AE `TESObjectREFR` virtual layout. Its reason, exact affected files, and
runtime boundaries are documented in
`third_party/CommonLibSSE-NG/SFS_LOCAL_PATCHES.md`. Do not replace the vendored
CommonLib tree with another fork or tag without first reapplying and validating
that boundary.

## Update and verification procedure

1. Check the official upstream release page and changelog; do not use a
   floating branch, nightly build, or unpinned archive.
2. Record the stable tag, full commit, and archive checksum before changing
   source or package declarations.
3. Keep vendored dependency trees internally consistent. Never copy only a few
   headers from another release or mix headers, libraries, and generated files.
4. Preserve each pinned upstream release's verified transitive build closure;
   do not upgrade one transitive dependency solely because a newer version is
   available.
5. Regenerate `xmake-requires.lock` with the pinned XMake release and review the
   complete dependency graph.
6. After a CommonLib header/layout update, do a clean rebuild so no stale PCH,
   object, or static-library output remains.
7. Run `KitGeneratorLogicTests`, build the releasedbg plugin, and inspect the
   relevant generated virtual-call slots when a runtime layout changes.
8. Recheck actor enumeration and actor-local appearance, Mod-Configured,
   Automatic Vanilla-Slot, Direct Editing, strip/redress, DAVE, DAV, and native
   display paths whenever the CommonLib or hook boundary changes.
