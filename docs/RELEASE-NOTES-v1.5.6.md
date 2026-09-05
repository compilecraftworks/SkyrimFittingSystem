# Skyrim Fitting System v1.5.6

- Reworked Fitting Dye's render-pass substitution so the selected diffuse is
  synchronized with Skyrim's renderer shadow state as well as the active D3D11
  binding. This keeps the change geometry-scoped and improves compatibility
  with Community Shaders, ENB, ReShade, and upscaler renderer chains.
- Expanded dye component classification for UBE, CBBE/3BA, and UNP/BHUNP
  outfits while continuing to exclude actual body, anatomy, overlay, normal,
  mask, and cubemap resources.
- Restored Helmet Toggle 2 ownership separation: HT2 continues to control real
  headgear, registered headgear follows the actor-local HT2 state, and head or
  hair visibility is no longer reconstructed from raw armor masks.
- Closed the SexLab P+ load-order window by rechecking `sslActorAlias` member
  natives after the type is fully linked. `StripByData` and `StripByDataEx`
  remain idempotent and use the existing Mod-Configured, Vanilla, and Direct
  strip/redress resolvers for each actor.
- Added installed-environment separation for SOS and TNG. Their internal
  genital or cover forms stay out of normal equipment, outfit, kit, and strip
  transactions while ordinary slot 32/49 armor keeps its existing revealing,
  concealing, covering, and underwear classification.
- Improved Modex and SFS kit JSON compatibility. Missing armor dependencies are
  removed per piece instead of invalidating an otherwise usable kit, and kit
  generation now handles invalid UTF-8, long names, and per-file failures.
- Fixed custom-condition negation normalization, scrollbar drag persistence,
  menu camera re-entry, and kit-row keyboard/gamepad/WASD or double-click
  interaction paths without changing focused text-input capture.

The runtime remains a single SE/AE `SFSCore.dll`; no PDB, replacement DLL,
SexLab P+ PEX/ESP, or Helmet Toggle 2 patch is included. Actor-local
DAVE/DAV/native ownership, live RaceMenu BodyMorph/OBody tracking, appearance
locks, high-heel offsets, and save data remain on their existing boundaries.

The focused regression suite covers runtime layouts, conditions, kit parsing
and navigation, dye rules, actor-local display and strip transactions,
RaceMenu ABI and morph tracking, Helmet Toggle 2 decision rules, and SOS/TNG
classification. Rendered behavior still requires an in-game smoke test with
the installed renderer and optional-mod stack.
