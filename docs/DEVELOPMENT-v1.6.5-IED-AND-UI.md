# v1.6.5 IED / menu-input development checkpoint

Historical implementation checkpoint before packaging/deployment. See
RELEASE-NOTES-v1.6.5.md for the shipped scope and verification limitations.
This integration does not replace IED DLLs, scripts, presets, modlists, or
user settings.

## IED boundary and evidence

Both packs contain **IED 1.7.4**; the distributions target different Skyrim
runtimes. The IED version number itself is not different.

| Installed pack | Skyrim | IED archive variant | DLL SHA256 |
| --- | --- | --- | --- |
| TuLED, selected profile TuLED(SL) | 1.5.97.0 | 1.5.39 - 1.6.353 | `9E1EACDAC3066B057C57BC610074FED837F27DEAE2832210CB3300FEA556E0C5` |
| TAKEALOOK, selected profile TKL | 1.6.1170.0 | 1.6.629 and newer | `E12DA86BE6C3412E1AD91EF7C12D51067F19E023A8D4EF8EC703EE43ACEDC38C` |

Source reference: stable [ied-dev 1.7.4](https://github.com/SlavicPotato/ied-dev/tree/3f014c3e8574ef0e88b2ec0b7cdf58b86c9737b0),
commit `3f014c3e8574ef0e88b2ec0b7cdf58b86c9737b0`. Its public plugin interface has
no external outfit-condition provider, so this is a narrow in-memory condition
call-site bridge, not merely an Evaluate notification. It is built into SFS.

`IedConditionBinary.h` pins PE timestamp/image size, full helper-byte
fingerprints, and all five existing rel32 dispatcher destinations. The
production installer validates them before writing any site at PostPostLoad.

| IED distribution | PE timestamp / image size | Equipped helper RVA | Node helper RVA | Call/jump site RVAs |
| --- | --- | --- | --- | --- |
| pre-629 | `6575B68A` / `582000` | `2A130` | `144540` | `28D8B`, `295B2`, `2C5D3`, `2D187`, `142AF1` |
| post-629 | `6575B58C` / `581000` | `2A1A0` | `1445C0` | `28DFB`, `29622`, `2C643`, `2D1F7`, `142B71` |

- Replaces only the BipedSlot predicate's cached biped input for final-visible
  ARMO slots 30 through 61. Actual and registered entries both participate;
  managed-empty is false, not a fallback to hidden worn armor.
- Four equipment dispatcher variants and the node-override dispatcher use the
  bridge. Form/keyword/bolt predicates stay in IED's original helper. The
  original node-override parameters are preserved, including lazy item-data
  initialization and matched-slot side effects. SFS does not invent a real
  inventory slot for a registered appearance absent from IED's item data.
- Weapon/quiver/race-sentinel slots and explicit skin queries remain original
  IED. This does **not** convert every equipped-form/keyword/type, inventory,
  armor-default, or arbitrary custom condition. Presets must use the covered
  BipedSlot conditions to observe the SFS final-display decision.
- Ready means SFS's final display decision, not occlusion, shader alpha, or
  completion of every parallel scene-attachment operation.
- IED workers read immutable ID-only published views. They do not call the
  public game-thread query, rerun SFS display/strip rules, or keep actor/3D
  pointers in provider caches. Normal IED evaluation resolves the candidate
  ARMO forms for its original predicates.
- No IED installed: a single startup GetModuleHandle check returns before
  hook allocation or observer installation. No IED polling or evaluation is
  scheduled. SFS and its optional external API remain independent.
- Modified/unknown binary: no offset guessing or partial installation; the
  original IED conditions and all core SFS rendering remain operational. Only
  this new condition bridge is unavailable, with a diagnostic log.
- Two small relays share one process-lifetime 64 KB allocation. This is not a
  per-actor allocation. Executable page protections are restored in reverse
  order because different call sites can share a page. No stolen prologues.

## Prior Helgen/cell-transition CTD defense

The old failure involved IED expecting its concrete InitWornVisitor while
receiving an incompatible SFS filtering visitor. The existing routing remains:
an exact known IED skinning chain receives no SFS visitor; SFS filters via the
original engine function, then schedules IED.Evaluate. An opaque chain keeps
the original concrete visitor with scoped callback filtering.

The new bridge operates on condition predicates, not those skinning call sites.
It shares the pre-existing deduplicated IED evaluation queue:

1. Publication changes enqueue only actor ID plus a monotonically increasing
   request token. SceneChanged alone does not enqueue IED again.
2. The queued task rechecks the token and resolves the actor afresh. Missing,
   deleted, disabled, or 3D-unloaded actors are skipped before VM dispatch.
3. PrepareForLoadTransition calls InvalidateQueuedArmorRefreshes, which clears
   pending IED tokens. New-game/post-load also invalidate queued refreshes.
   Reusing an actor ID in a new load cannot validate an older task token.
4. Object unload invalidates the published view immediately. Suspended actors
   stay unavailable even if a trailing skinning event arrives before the old
   root pointer is cleared. Delete and epoch reset retire their cached data.

These are tested code-level defenses, **not a claim that live Helgen/cell
transitions have been retested**. The condition bridge adds IED work and must
still pass that game route, repeated saves/loads, and frame-time comparison.

## UI and defaults

- Right-aligned localized rotation hint immediately left of the title-bar X;
  compact/clipped fallback for narrow windows preserves the X hitbox.
- Hold LT and move RS horizontally to use the existing mouse rotation branch.
  Analog deadzone, proportional frame-time-based motion, bounded rotation,
  and focus/device/menu reset prevent stale input. No controller polling thread.
  Existing paused behavior is preserved: camera orbit only while paused;
  actor/camera counter-rotation while running. No BodyMorph ownership changes.
- Keyboard/gamepad Cancel uses the current MenuMode control mapping, including
  remapped keys. Physical input is consumed once before duplicate game-menu
  routing. Existing one-level cancellation is retained: active popup/editor
  first, otherwise close the main SFS window.
- First-run character position is Left; game pause is unchecked. Existing
  explicitly saved user choices are retained. Invalid position values fall
  back to Left. English/Korean/Simplified Chinese strings are updated.

## Verification and remaining gates

- SE/AE-only plugin builds with the repository's pinned XMake 3.1.0,
  MSVC 14.51.36231 and CommonLibSSE-NG 6.7.0 flat baseline; no VR target.
- All 20 fast regression targets and source/package guards pass, including
  40 SE/AE custom-skin routes, BodyMorph tracking, condition input/DnD,
  strip/manual visibility, and public API tests.
- Production IED queue functions are extracted verbatim into the fake-engine
  provider test. Cases cover coalescing, deferred dispatch, unload, deletion,
  disabling, missing actors, load cancellation, same-ID/new-token reuse,
  fresh actor resolution, and unavailable VM.
- IED observer tests cover unload with an uncleared root and trailing skinning,
  fresh-root resume, deletion, pending epoch invalidation, immutable readers,
  and absence of a SceneChanged reevaluation loop.
- Both installed IED binaries passed actual equipped/node helper execution and
  the real bridge installer in an isolated test executable. LoadLibraryEx uses
  DONT_RESOLVE_DLL_REFERENCES; no IED DllMain, SKSEPlugin_Load, or game runs, and
  all code patches exist only in that test process. Installed DLLs are unchanged.
- No-IED production installer test: 1,000 calls with no hooks, subscriptions,
  API queries, IED evaluations, or actor lookups.
- Real ImGui tests cover title layout, narrow widths, and X clicking. Analog
  math and shared keyboard/gamepad remapping helper tests pass. These are not
  physical-device/in-game input tests.

Outstanding in-game validation: live Helgen/new-game cell transitions, repeated
save/load, IED BipedSlot preset behavior for both packs, IED absent, DAVE/DAV/
native attachment ordering, LT+RS and remapped Cancel on physical inputs, and
matched city-route frame-time/memory comparisons with v1.6.4. No zero-overhead,
all-conditions-covered, or CTD-free claim is justified by these harnesses.
