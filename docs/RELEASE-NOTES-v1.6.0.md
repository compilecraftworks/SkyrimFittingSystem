# Skyrim Fitting System v1.6.0

v1.6.0 starts the architecture and integrity release on the complete v1.5.7
feature baseline. No tab, visibility mode, strip-link mode, protected-slot or
shield option, pause behavior, dye rule, RaceMenu route, save record, or public
integration is intentionally removed.

## Changes

- Condition-card drag/drop now plans moves and occupied-card swaps by stable
  64-bit UI identity and commits both endpoints atomically. Missing, duplicate,
  stale, conflicting, and same-target payloads fail without a partial clear.
- Condition row and visibility-rule ImGui scopes also use stable identities
  instead of mutable vector indices.
- Added `ConditionDropLogicTests` to the full fast regression suite.
- Added one actor-local final-rendered-outfit query boundary. Nudity, shown
  armor/keyword OAR conditions, the narrow body-keyword compatibility hook, and
  the Dynamic Footprints host export now consume the same resolved visible
  actual-equipment plus registered-appearance state.
- Promoted Virtual Worn Tokens from the old `poc` source/build identity into the
  production `features/virtual_tokens` module and namespace. Existing ESL forms,
  Papyrus names, serialization records, and all four strip/redress modes remain
  compatible.
- Promoted the active Devious Devices hider/strip bridge from its old `poc`
  source identity to the production `features/devious_devices` adapter without
  changing its event, visibility, or transaction behavior.
- Kit Generator UI code no longer receives mutable source/result containers.
  Selection, rename, candidate choice, and piece replacement use validated
  Generator commands; grouping assessment remains independently updateable.
- Added the v1.6.0 feature-parity contract, compatibility-patch decision table,
  and feedback regression ledger.
- Locked live BodyMorph tracking to one owner. The production tracking test now
  interleaves equipment/outfit/condition, kit/generator, protected/shield,
  HT2, dye, paused camera/pose, all four strip modes, P+, DD, and DAVE/DAV/native
  refreshes and verifies both direct and deferred current-morph delivery to
  retained registered-appearance nodes.
- Isolated IED custom-skin routing behind a pure, tested decision boundary. The
  exact IED chain is bypassed before SFS's different concrete visitor is used,
  followed by a deferred public actor `Evaluate`; unknown early-game startup
  state stays on the ordinary engine route and fails closed.
- Final visual nudity now has an explicit regression rule over the union of
  visible actual-equipment and visible registered-appearance slot masks.
- IED ownership detection now follows verified transparent x64/CommonLib
  trampoline veneers before deciding whether the pre-patched target belongs
  to IED. Unresolved chains fail closed without installing the custom-skin
  hook over an unknown concrete visitor ABI.
- Condition action-card movement and custom-condition clause reordering now
  commit on the first ImGui delivery. Action target replacement, conversion,
  source cleanup, visibility polarity, and strip/DD automation invalidation are
  staged and committed as one transition; stale payloads change nothing.
- The Dynamic Footprints v1 bridge gained a backward-compatible optional query
  that distinguishes an unmanaged actor from an explicit final-rendered
  barefoot result, preventing fallback to SFS-hidden actual footwear.
- First-install settings storage is now prepared as soon as `SFSCore.dll`
  loads, before renderer/menu hook initialization. The runtime archive includes
  an explicit MO2/Vortex guide explaining that Skyrim must be launched through
  SKSE64 and that `Toggle UI Button` belongs to the SFS Options tab, not
  Skyrim's Escape-menu Controls page.
- Save loading and revert now share one idempotent lifecycle boundary. Queued
  armor, event-sink, SOS, and BodyMorph work is invalidated before renderer,
  strip/DD/token, HT2, DAVE, slot, cache, or transient UI state is cleared and
  before any incoming co-save record is read.
- Fitting Dye now states in the popup that white is the neutral multiplicative
  value. This avoids presenting the source diffuse's original color as a failed
  white dye while retaining the renderer-safe multiply model.

## Compatibility patches

Patchless integration remains the preferred result. Wet Function naked
auto-apply already follows SFS's final body state without an MCM replacement.
The remaining Wet Function effect-script, DFFMA OAR JSON, and Dynamic Footprints
single-call-site bridges are still consumer-owned boundaries; removing them in
SFSCore would require unsafe global behavior or equally version-specific binary
patching. They remain until the upstream consumers adopt SFS's stable queries.

The DFFMA FOMOD package revision is now 1.4.4.1. Eight optional GS Hovering
choices were missing the mandatory FOMOD 5 `description` element, so Vortex's
strict schema validator rejected the installer before showing its options.
Descriptions and a package-time XML/source-path validation gate were added;
the installed OAR JSON payload is unchanged.

The Wet Function v1.2.0 compatibility archive now has a dedicated content
validator and builder. It contains only `WetFunctionEffect.pex` and a README;
`WetFunctionMCM.pex`, saved settings, plugins, and native DLLs are rejected.

The Dynamic Footprints compatibility DLL's internal version, README, binary
resource, and archive are now all 1.6.0. Its package builder rejects version or
layout drift while retaining the exact-v3.0 hash/signature and near-call-site
trampoline safety boundary introduced by v1.4.5.

## Verification

- `releasedbg` `SFSCore.dll` builds as file/product version 1.6.0.0.
- The complete fast regression suite passes, including Skyrim runtime layout,
  Kit Generator, BodyFamily, conditions, condition drag/drop, Fitting Dye, kit
  navigation, final nudity/footwear, IED routing and trampoline decoding, core behavior, RaceMenu ABI versions,
  cross-feature morph tracking, and source ownership checks.
- Automated verification does not replace the documented SE/AE in-game matrix
  for rendered behavior and optional-mod ownership.
