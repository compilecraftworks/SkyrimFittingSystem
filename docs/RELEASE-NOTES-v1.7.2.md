# Skyrim Fitting System v1.7.2 SE-AE

Generated results now have explicit checkboxes. Only checked kits are created,
merged or deleted; single-candidate kits are also listed. Ctrl toggles individual
row highlights and Shift highlights a sorted visible range; toggling one highlighted
row's checkbox sets the whole highlighted group. Checkbox clicks do not preview or
open details; row double-click still opens the candidate list. Select All beside
Delete checks visible rows, while Clear All clears every check, including hidden
ones. A counter identifies checked results hidden by filters.

Kit Generator results (before the preview checkbox) and ESP selection now offer
the same body-family choices/classification as the Kits tab. Each screen defaults
independently to All. ESPs match any contained armor; results match the selected
candidate. These are list-only filters: checks and existing previews survive
filter changes; candidate assembly and appearance application remain unrestricted.

## Changes

- Fixed registered HH_OFFSET sources being discarded before scene grafting.
  Temporary detach/reparent now uses the existing bounded completion retries;
  abandoned roots still expire. Old resolution passes cannot erase a renewed
  capture or apply obsolete height after an observed unload. Actual equipment,
  public/legacy NiOverride routing and named third-party transforms are preserved.
- Removed duplicate worn-equipment collection within individual hide/skinning-mask
  queries and duplicate visible-list construction within individual body/final-state
  queries. Executable condition reads share the same owned condition without
  copying UI metadata. Papyrus inspection caching tolerates colliding hot types
  while retaining the 128-type cap, first-call installation, late binds and P+
  retries. Nudity and strip/redress decisions are preserved; internal decisions
  do not import BCNG results.
- Favorites Only is saved independently for Gear, Outfits and Kits. Changing
  one tab no longer toggles the other tabs. Old shared settings migrate once;
  filtering moves list focus without clearing the current workbench preview.
- Related Community Kit Hub colors, lengths and numbered versions now appear
  as candidates inside one kit group. Each original composition stays intact;
  pieces are not recombined across reference versions or different plugins.
  Numbered sets such as Birth Lingerie also keep their matching pieces together:
  the installed 32-record example becomes one group with 16 paired candidates.
  Rescan the selected ESPs to rebuild results; previously saved kit files are
  not rewritten automatically.
- While SFS list navigation is active, consumed keyboard keys no longer reach
  downstream mod hotkey event sinks. Closing SFS restores new key presses;
  only the trailing held/release events of an SFS-owned press are consumed.
- Kit Generator ESP selection supports Ctrl+click for individual toggles and
  Shift+click for ranges in the visible sorted list. Added an optional worn-model
  plugin filter; it changes only the list, preserves hidden selections, and keeps
  accessory-only plugins. Select/Clear All affect visible results only. Red
  grouping warnings now explain their criteria in a compact, width-limited tooltip
  with paragraph breaks and automatic wrapping, and do not block scans.
- Integrated the cumulative Community Kit Grouping Custom Patch 4 into the full
  official source and package; this is not a renamed replacement DLL.
- Added a saved, shared body-family dropdown before Favorites/Preview in the
  Equipment, Outfits and Kits lists: All, CBBE/3BA, UNP/BHUNP, UBE, HIMBO, SAM,
  Vanilla. All is the default; the old actor-based automatic list filter is
  superseded. CBBE, 3BA and otherwise unqualified 3BBB labels are recognized.
- The dropdown changes list visibility only. After changing it, keyboard/gamepad
  focus moves to the first sorted result and scrolls into view. This focus move
  does not apply or preview that item: the existing preview and registered
  appearances remain unchanged. Empty results have no stale action target.
  Applying and previewing outfits are not gated by the actor's body family.
- Kit grouping uses Modex Community Kit Hub references first, followed by ADD
  screenshot references, spreadsheet families, then generic inference. Explicit
  reference compositions remain intact inside candidates. All 542 ADD kit names and reviewed
  design-specific exceptions are retained; photo sequence numbers are not
  guessed to be equipment design numbers.
- Improved grouping of translated/separately named outfit parts using internal
  outfit identifiers, common model folders and complementary slots. Existing
  named accessories stay with their groups. A shared author, shoe or male
  placeholder model alone does not justify merging unrelated outfits.
- Candidate generation coordinates available colors/styles, allows common
  pieces, and prevents overlap across actual slots 30–61. More than four
  alternatives and all choice axes are retained within the 256-profile budget;
  body-style representatives are prioritized. Long scans retain cancellation.

## Update and scope

This is a full SFS package. Disable/remove the separate Community Kit Grouping
Custom Patch 1–4 when updating so an old patch DLL/locales cannot overwrite it.
Keep user settings, kits and saves. The separate Wet Function, Dynamic
Footprints and Modesty compatibility patches are unchanged.

No new prerequisite, game-runtime support or API ABI is introduced. Skinning-query,
condition-access and script-inspection costs are reduced without changing display/
strip policy or IED routing. Heel attachment lifetime is repaired; BodyMorph
vertex application and dye implementations are unchanged.
Compiled reference tables need no workbook, photos,
Modex installation or Python at runtime; corresponding armor mods must of course
be installed for their items to exist.

## Limits

The reported approximately four-second SkyUI delay has not been measured in the
reporter's game and is not confirmed fully resolved. Cross-query display-result
reuse is not implemented. See [query-cost verification](SKYUI-QUERY-COST-FIX-2026-09-29.md).
The heel fix has a failing-before/passing-after production-source regression,
but the reporter's game configuration remains unverified. See [heel verification](HH-PENDING-ATTACHMENT-FIX-2026-09-29.md).

Body-family labels are metadata-based, not mesh compatibility certification.
Mixed sets can appear in multiple categories; Vanilla includes catalog fallback
items without explicit custom-body evidence. Filtering never promises that a
mesh fits a particular actor.

Reference names represent outfit pools, not pixel-exact photo reconstruction.
BodySlide-only differences without separate ARMO records cannot be separate kit
items. The 256-profile cap remains; not every Cartesian combination is emitted.
Offline corpus validation does not resolve an entire in-game override load order.
Game UI, preview, application and saving have not been exercised in game.
Mod hotkeys that poll key state directly or run before SFS's dispatch hook are
outside this event-filter guarantee and still require an in-game check.

Build/test evidence and reproduction are recorded in
[integration notes](V1.7.2-KIT-INTEGRATION.md).
