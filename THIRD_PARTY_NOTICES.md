# Third-Party Notices

## CommonLibSSE-NG v6.7.0

SFS v1.4.8 builds against the vendored
[CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) v6.7.0 source
revision `3d81614617910e7f34b33d8750881811b5e36445`, located at
`third_party/CommonLibSSE-NG`.

CommonLibSSE-NG is distributed under GPL-3.0-or-later with the project
exceptions documented in its included `COPYING` and `EXCEPTIONS.md` files.
Those files are preserved in the source package.

SFS carries a narrow SE/AE vtable-layout correction on top of that exact
upstream revision. The correction and its runtime boundaries are documented in
`third_party/CommonLibSSE-NG/SFS_LOCAL_PATCHES.md`.

## Dear ImGui

SFS includes Dear ImGui v1.92.9b source revision
`f1cc2ae15e53a861a874c3034aae6798fde194ab` under `lib/imgui`. The source
archive used to refresh the vendored tree has SHA-256
`e1c46d676c2bcb7ced847ba27f50553e33a19db97b3cadaec7f8be64449139f8`
(case-insensitive hexadecimal). Dear ImGui is distributed under the MIT
License; its license text is preserved in `lib/imgui/LICENSE.txt`.

## nlohmann/json

SFS resolves nlohmann/json v3.12.0 as an exact XMake package requirement. The
package version and XMake recipe revision are fixed by `xmake-requires.lock`.
nlohmann/json is distributed under the MIT License.

See `DEPENDENCIES.md` for the reproducible build-tool and transitive dependency
baseline.
