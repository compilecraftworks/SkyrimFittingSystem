# SFS local CommonLibSSE-NG compatibility patch

Upstream baseline: `alandtse/CommonLibSSE-NG` tag `v6.7.0`
(`3d81614617910e7f34b33d8750881811b5e36445`).

SFS is an SE/AE-only build (`skyrim_vr=false`). The upstream v6.7.0
`TESObjectREFR` declaration exposes the VR-only `Unk_8C` entry as a virtual
function in flat builds. That shifts every following SE/AE slot, including
`Actor::IsDead`, from its engine slot `0x99` to `0x9A`.

The local patch follows CommonLibSSE-NG's documented runtime-exclusive virtual
pattern:

- SE/AE-only: omit `Unk_8C`, so the flat vtable is not shifted.
- VR-only: retain `Unk_8C` as virtual slot `0x8C`.
- Multi-runtime: expose a non-virtual wrapper that dispatches only on VR.

No SFS actor-discovery behavior is changed. The workbench actor candidate and
initial-selection implementation remains the v1.4.5 implementation.
