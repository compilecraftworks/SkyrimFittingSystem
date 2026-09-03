## Version 1.5.3

- Added built-in SexLab P+ v2.12.0 strip/redress integration for Skyrim 1.5.97 and 1.6.1170. Mod-Configured and Direct+Mod-Configured retain virtual-token matching; Vanilla and Direct+Vanilla retain actual-equipment transactions.
- Added an optional, default-on BodyFamily filter for Equipment, Outfits, and Kits. Uncertain/custom actors fail open, and registered workbench rows remain unfiltered.
- Fixed scrollbar-drag release resetting long lists and fixed paused Left/Right camera placement on subsequent menu openings.
- Fixed nested false custom-condition saves freezing the game by folding negations before bounded CNF materialization.
- Expanded RaceMenu version routing for legacy NiOverride transforms, public NiTransform v3+, and compatible BodyMorph v4/v5 interfaces without changing actor-local DAVE/DAV/native ownership.
- Excluded compact UBE collision helpers such as `ArmColli`, `FeetColli`, and `ButtLegColli` from Fitting Dye targets.

## Version 1.5.2

- Added actor-local registered-appearance locks from the card context menu. Locks survive Gear, Outfits, Conditions, Kits, and Kit Generator previews/applications and are excluded from automatic strip-link, DD Hider, and Helmet Toggle 2 suppression while retaining manual eye and condition behavior.
- Added RaceMenu `HH_OFFSET` synchronization for registered high heels on players, NPCs, and followers across DAVE, DAV, native refreshes, and late DAVE attachments without proxy equipment.
- Fixed live RaceMenu BodyMorph on visible registered appearances across DAVE, DAV, native display, and deferred `UpdateModelWeight` processing.
- Added W/S kit-list navigation outside active text editing and expanded Fitting Dye base-body/helper filtering for all supported female, male, and Vanilla body families.
- Unified paused and unpaused menu framing at FOV 70 with exact restoration on close. Paused right-drag now orbits the camera without moving actor/SMP roots, preventing FSMP garment stretching; unpaused live rotation remains unchanged.
- Added fast production-rule regression coverage for actor-local state, all four strip-link policies, virtual-token and actual-equipment transactions, and DAVE/DAV/native refresh dispatch.
- Removed `SFSCore.pdb` from runtime and MO2 packages; debug symbols remain in local builds only.

## Version 1.5.1

- Added actor-aware body-family filtering to Equipment, Outfits, and Kits. Female families are grouped as CBBE/3BA/3BBB, UNP/BHUNP, UBE, and Vanilla/fallback; male families are HIMBO, SAM, and Vanilla/fallback.
- Explicitly conflicting families are hidden while unlabelled same-sex entries remain visible. Detection uses loaded form and selected-actor metadata only, without a BodySlide/NIF disk scan, periodic polling, or a nearby/global actor scan.
- Actor changes invalidate only the visible catalog cache. Workbench rows, conditions, actual equipment, saved kits, Fitting Dye, virtual tokens, external strip/redress state, and DAVE/DAV/native ownership remain unchanged and actor-local.
- Restored live RaceMenu BodyMorph synchronization for registered appearances on players, NPCs, and followers.
- Limited Skyrim/other-mod input suppression to a real focused SFS text editor with an active caret, with immediate release when text focus ends. Intentional keybind capture keeps its separate lock.

## Version 1.5.0

- Added actor-local Fitting Dye to registered-appearance cards in base and condition rows, available from the dedicated dye action and right-click context menu.
- Dye targets an exact rendered component, provides a brief world-space identification pulse, and can restore the component's original color.
- Colors are saved by actor FormID, registered appearance ARMO, and exact component identity. Shared meshes or diffuse textures on another actor or appearance are not recolored, and ambiguous matches fail closed.
- Tinting uses a private geometry-scoped renderer texture without changing source DDS/NIF files, actual equipment, inventory, workbench rows, conditions, kits, virtual tokens, external strip/redress state, or DAVE/DAV/native ownership.
- Fixed an SFSCore CTD when switching the workbench from the player to a new NPC with HT2 installed. NPC/follower ownership now relies only on resolved HT2 monitor spells, exact headgear AddSpell/RemoveSpell signals, and the actor-local FormID cache; the redundant active-effect-list fallback was removed.
- Replaced the separate Helmet Toggle 2 PEX patch with built-in SFSCore integration. SFS observes only HT2's exact player global and actor-local NPC/follower spell transitions, without replacing HT2 scripts or polling actors.
- SFS reads every real helmet's full ARMO slot mask instead of relying on the array position retained after HT2 deduplication.
- Fixed bald actors when HT2 hides a still-equipped real 31+42 helmet. SFS releases only that actor's real Hair 31 bit from the skinning worn mask; the actual ARMO, inventory, HT2/DAVE variant, and keywords remain untouched.
- Pure registered slot 31 remains independent, while registered 31+42 still follows through slot 42. Player, follower, and NPC transitions remain actor-local. Mod-Configured headgear virtual tokens, Automatic Vanilla-Slot Linking, Direct Slot Editing, manual eye controls, external strip/redress handling, and DAVE/DAV/native behavior are unchanged.
- Remove every older SFS Helmet Toggle 2 Compatibility Patch when updating; HT2 now requires no separate SFS patch.

## Version 1.4.9

- Added explicit Skyrim SE 1.5.97 and Skyrim AE runtime hook profiles with instruction validation and fail-closed handling for unknown layouts. Papyrus VM/native slots are centralized and covered by SE/AE boundary tests.
- Fixed the IED custom-skin chain at the actual `VisitWornItems` call sites. SFS filtering now uses the original engine visitor path when IED owns the call site, preventing equipment-rebuild CTDs while retaining actor-local visibility state.
- Moved left/right menu character framing slightly farther outward with symmetric offsets; angle and height are unchanged.

## Version 1.4.8

- Fixed the CommonLibSSE-NG SE/AE virtual layout used by the workbench actor eligibility check, restoring `Actor::IsDead()` to its correct engine slot. Diagnostic actor-discovery experiments were reverted, so actor collection remains identical to v1.4.5.
- Updated the bundled build dependency to CommonLibSSE-NG v6.7.0 and documented its narrow local SE/AE vtable correction. This remains an SE/AE build; it does not add Skyrim VR support.

## Version 1.4.6

- Hardened built-in Grid Inventory Costume synchronization. SFS imports a Costume only when it contains at least one SFS-compatible armour piece.
- No Costume, a cleared or empty Costume, and a Costume without compatible armour now retain the player's existing SFS registered appearances. The legacy clear entry point is non-destructive as well.
- The first restored Grid state after startup, save load, or revert remains ignored, so restored or empty Grid state cannot erase saved SFS appearances.
- Grid Inventory continues to own its renderer, loadouts, saves, and actual equipment; SFS neither replaces nor loads `GridInventory.dll`.

## Version 1.4.5

- Added built-in Grid Inventory v1.4.1+ Costume synchronization through Grid Inventory's public Costume-state SKSE message. The active Costume updates only the player's SFS registered appearances, and clearing it clears those player appearances. The first restored state after startup, save load, or revert is ignored until Costume changes again.
- SFSCore copies the message data during the callback and applies it through its normal safe processing point. It does not replace, load, hook, or depend on `GridInventory.dll`; Grid Inventory keeps ownership of its renderer, loadouts, saves, and actual equipment.
- Remove the retired v1.4.4 SFS Grid Inventory Costume Compatibility Patch when updating. Mod-Configured linking, Vanilla linking, Direct Editing, external strip/redress handling, and DAVE/DAV/native actor-local behavior are unchanged.

## Version 1.4.4

- Added optional Open Animation Replacer public-API conditions that read each actor's final rendered outfit: `SFS_IsShownArmorEquipped`, `SFS_ShownArmorHasKeyword`, and `SFS_IsShownBodyNaked`.
- The conditions include visible actual equipment and registered appearances, exclude actual equipment currently hidden by SFS, and do not mutate armor keywords, equipment, inventory, or save data. OAR is never force-loaded.
- Added the separate DFFMA OAR 4.30 FOMOD patch. It replaces only the selected display-equipment JSON conditions and does not replace DFFMA animations, meshes, scripts, DLLs, or plugins.
- Added separate compatibility patches for Grid Inventory Costume, Helmet Toggle 2, and Dynamic Footprints SKSE BASE v3. Grid applies only the active Costume layout to the player's SFS base appearances; HT2 forwards only its final actor-local managed slots (30/31/42/44 and player 55) without owning real equipment; Dynamic Footprints reads only the visible SFS Feet 37 appearance at its own exact verified v3 call site. The patches are integration boundaries, not fixes for their original mod's own UI, equipment, rendering, save, or conflict problems. The Dynamic Footprints patch stays disabled for a different DLL build.

## Version 1.4.3

- Integrated the complete SFS Kit Generator into `SFSCore.dll`, providing in-game outfit-plugin scanning, candidate generation and editing, preview, and kit creation without a separate generator DLL.
- Candidate previews are read-only on both the character and the workbench's Registered Appearances column. Saved rows, inventory, and actual equipment are untouched, and generator-only protected-slot exceptions do not leak into normal features.
- Reordered tabs to `Gear → Outfits → Conditions → Kits → Kit Generator → Options` while preserving Kits as the initial tab.
- Kit preview follows Skyrim's remapped `Jump` event and Apply follows the remapped `Activate` event.
- Added display-only sorting to all four base and conditional workbench headers. Empty Actual Equipment and Registered Appearance cells participate in whole-row sorting, multi-slot cards and condition groups stay atomic, and empty action cells do not distort group slot keys. Saved row order is unchanged.
- Parallelized multi-ESP Kit Generator scans within conservative CPU and memory limits while giving a single ESP the full worker budget. Large multi-slot DP state sets inside one profile also use that safe budget, and progress remains monotonic.
- Split unrelated slot masks out of exact DP when a pack such as ADD 03 Dark Knight contains only one overlapping multi-slot piece. Independent slots are finalized immediately, preserving candidate rules while reducing heavy-profile scan time.
- Unified result and candidate selection around click and `Ctrl+click` multi-selection, supporting both left- and right-click. Candidate-name clicks change selection and preview only, a separate Edit button opens the editor, fixed action columns remain visible in narrow layouts, and duplicate profile names gain distinguishing piece names and numeric suffixes.
- Removed false NSFW classification from ordinary `_X` EditorIDs and `NonStocking`, while preserving explicit `[X]` and `(X)` tags.
- Changing, editing, or merging candidates—and returning from candidate details—does not reclassify the result row's internal SFW/NSFW state. The manual result-list toggle has been removed.
- Added third-person character placement and right-drag player rotation to the SFS menu. It follows Show Player In Menus' calculation, temporarily requests camera ownership through SmoothCam's public API, reapplies the selected side on every open, and restores the original camera and facing on close.

## Version 1.4.2

- Added a versioned runtime bridge for the separately distributed SFS Kit Generator option. The tab appears only when a compatible option DLL is already loaded; SFSCore neither links to it nor forcibly loads it.
- Generator candidates can now be previewed through the existing appearance engine and mirrored into the workbench's Registered Appearances column as read-only cards.
- This temporary generator preview includes protected-slot and SOS/TNG genital candidates without changing the restrictions used by normal registration, application, saving, or visibility. Existing same-display-slot conflict handling remains active.
- Generated previews never modify saved workbench rows, inventory, or actual equipment, and are cleared through the existing preview cleanup when leaving the tab, cancelling, or closing SFS.
- Without SFS Kit Generator installed, the base SFS UI and runtime behavior remain unchanged.

## Version 1.4.1

- Fixed registered appearances not rendering immediately when applied to a completely unequipped player or NPC. Previously, another equipment change—such as equipping a ring—could be required before the appearance became visible.
- When an actor has no actual armor equipped, SFS now performs one actor-local 3D refresh only when the registered appearance composition changes. No global actor scan or periodic polling is used.
- When both actual equipment and registered appearances are empty, the workbench now shows one empty row and no longer duplicates the condition header.

## Version 1.4.0

### Strip Linking and Wardrobe Transactions

- Standardized the shared `Mod-Configured Slot Linking`, `Automatic Vanilla-Slot Linking`, `Direct Slot Editing`, and `No Linking` policies while keeping event state, suppression, and restoration actor-local.
- Added Mod-Configured linking through virtual tokens that follow external MCM slot, exclusion-keyword, and redress rules, plus Vanilla linking that classifies names, EditorIDs, keywords, and ARMO/ARMA occupancy into natural actual-equipment anchors.
- Direct Slot Editing changes only required exceptions on top of either automatic base. The separate fully manual base was removed from the workbench popup.
- Handles the first external call, event-added equipment, redress, `RemoveAllItems`, `RemoveItem`, and `SetOutfit` as actor-local transactions while retaining separate DAVE, DAV, and native display backends.
- The device/Hider layer runs only when DD is installed, and DD state no longer overwrites generic prison, confiscation, or forced-wardrobe state.
- When cell or location events arrive before prison confiscation, the coalesced actor refresh observes the context boundary again. This preserves the pre-replacement wardrobe snapshot so Pama suppresses registered appearances under Vanilla linking as well.
- External events do not lock the individual or global eye controls for actual gear or registered appearances; automatic and manual states remain separate.

### Additional Improvements

- Removed the dedicated Helmet Toggle 2 bridge, transient co-save state, workbench lock/banner, and optional patch source; head, hair, and circlet appearances now use the ordinary slot rules.
- Made automatic equipment suppression reach both the eye state and native, DAV, and DAVE rendered appearances, and standardized the protected defaults as slots 50, 51, 60, and 61.
- Added the common OBody, FHU, and SGO RaceMenu BodyMorph path that rebuilds only the affected actor's registered appearances, without a global actor scan, periodic polling, or per-mod event list.
- Applies the same final SOS/TNG slot-32 and slot-49 decision to actual gear and registered appearances while preserving explicit SOS MCM choices and ESP/KID source keywords. Only upper-only slot 32 gains Revealing, ordinary full-body slot 32 gains no Concealing/Underwear keywords, and classified lower slot 49 gains Concealing/Underwear. Generic vanilla armor taxonomy is excluded from upper-only classification input.

### Workbench, Display, and Distribution

- Papyrus and engine-condition `WornHasKeyword` now use the actor-local final body display composed from actual gear and registered appearances.
- Protected special-effect slots and disabled shields are excluded consistently from registration, hiding, automatic/direct linking, and workbench display.
- Condition double-click fills only an empty condition card, and an existing empty row with the same condition is reused before a new row is created.
- Clamped the workbench `+ Create Condition Slot +` row to the table's inner clip and scrollbar width, preventing its label from drawing outside the window when offscreen or losing its right half near the scrollbar.
- Removed the main ESP, SEQ, and retired per-mod bridges and consolidated the package around `SkyrimFittingSystem-VirtualTokens.esl`, the native script, and the external menu C API.
- Renamed the single native plugin to `SFSCore.dll` so the official RaceMenu interface exchange also works with Skyrim 1.5.97 / SKSE 2.0.20 / RaceMenu 0.4.16, while preserving the four existing menu C export names.
- v1.2.x and v1.3.x settings migrate to the v1.4.0 Mod-Configured Slot Linking default while existing appearances, conditions, and visibility data remain intact.

## Version 1.3.0

## Actual-Equipment Strip Linking

- Replaced the old SexLab, Devious Devices, Private Needs, Soulgem Oven, Bathing in Skyrim, and Body Search-specific strip bridges with one system that follows each actor's final worn state.
- Added the actor-specific **Strip Link** popup with **No Linking**, **Auto-match Vanilla Slots**, and **Include Mod Slots** modes. Every actor starts independently with No Linking.
- Added per-appearance-slot choices for Auto-match, Do Not Use, or a fixed actual-equipment slot. Multi-slot cards belonging to one item stay synchronized.
- Automatic hiding is temporary display state. It does not move, delete, or recreate registered FormIDs, original slot masks, base or conditional rows, row order, actor ownership, or kit JSON.
- Manual visibility, conditions, and Helmet Toggle 2 state remain independent. Showing an auto-hidden appearance ignores only that suppression until the linked equipment returns; the next normal strip event can hide it again.

## Automatic Matching

- Vanilla Auto-Match uses name, EditorID, keyword, and original-slot rules expanded from 19,132 real outfit-mod records. Applying the current representative priorities to that sample yields Body 11,227 (58.682%), Feet 2,987 (15.613%), and Hands 2,006 (10.485%) as the largest groups, while Hair, Forearms, Calves, and other vanilla slots remain distinct. This is a classification distribution, not an accuracy percentage; actual gameplay can dynamically fall through to lower-priority links according to currently worn equipment.
- The best currently worn anchor is recalculated after every actual-equipment change. Vanilla Auto-Match candidates are slots 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, and 42, with unresolved ordinary accessories falling back to slot 32.
- Actual occupancy is calculated from the union of ARMO BOD slots and every attached ARMA slot. This detects the auxiliary partitions of 32+34+38 body armor, 33+34 gloves, and 37+38 boots, then applies the existing normalization that removes 49 from non-genital 32+49 equipment and removes 31 from real 31+42 headgear. The popup preview and runtime automatic suppression use the same control-slot mask.
- Lower-body slot-49 appearances can follow actual slot 32, while genuine slot-49 decorations, pelvis items, and belts follow their ordinary priority rules.
- Pure slot-31 wig/hair, slot-34 forearm, and slot-38 calf appearances independently match `31 - Hair`, `34 - Forearms`, and `38 - Calves`. Multi-slot appearances use a representative main-body slot: 32+34+38 body→32, 33+34 gloves→33, and 37+38 boots→37. Real 31+42 headgear is normalized to control slot 42, so a Vanilla Auto-Match slot-31 wig remains visible; explicit user slot-31 connections and Include Mod Slots one-to-one links still follow the real slot-31 state.
- Diagnostic logs record the original ARMO display mask as `slots=` and the normalized ARMO+ARMA result as `controlSlots=`.
- Include Mod Slots links the appearance's first eligible original slot one-to-one with the same actual slot number without locking to a particular armor FormID.

## Actor and Save Isolation

- Strip-link mode, fixed slot exceptions, and temporary visibility are actor-local and stored in the SKSE co-save.
- Nearby actor detection supplies candidates only; it does not create or modify appearances, conditions, visibility, or strip-link settings.
- Preserved the five official v1.2.2 co-save record prefixes and appended the new `AEVS` record as the sixth and final record.
- Official `AEVS` v2 stores settings per save and actor. Existing saves without an `AEVS` record, including official v1.2.2 saves, and the discarded development-only `AEVS` v1 do not import global `settings.json`; every actor starts with linking disabled, while existing appearance, condition, and visibility data remain unchanged.
- Actor runtime maps and temporary node references are cleared at load boundaries to prevent state leaking between save games that reuse a FormID.

## Special-Effect Slots and Shields

- Renamed Hide-Protected Slots to **Protect Special-Effect Slots** and changed the default selection to 50, 51, and 61. Users can customize slots 30–61.
- Actual gear containing a protected slot always remains visible. New appearance registration is blocked, while existing base and conditional appearance data is preserved and becomes active again if protection is removed.
- Equipment batch registration, Outfits, Kits, and condition registration skip only protected items and continue with valid items.
- Added **Use Shields as Appearance Slots**, OFF by default. When off, actual slot-39 shields remain visible and shield appearance registration and display are blocked without deleting saved data.
- Protected slots and disabled shield slot 39 are excluded from Strip Link cards, connection lines, and dropdown targets.

## RaceMenu BodyMorph and Display Compatibility

- Added universal support through RaceMenu's common `ApplyBodyMorphs` path and its internal deferred `UpdateModelWeight` task, resolved from RTTI instead of a version-specific address, rather than maintaining FHU, SGO, OBody, or other mod-specific event lists.
- The verified public `ApplyBodyMorphs` behavior is preserved for OBody. RaceMenu's deferred `UpdateModelWeight` task triggers a separate actor-local SFS rebuild for NiOverride Papyrus callers through the active DAVE, DAV, or native display backend, followed by one final public morph application after the new nodes attach. No polling or global actor scan is used.
- Added actor-local display refresh after actual equipment changes, including DAVE API refresh and DAV/native fallback handling, to remove stale rendered nodes after external strip events.
- Preserved SOS/TNG covering and revealing evaluation and moved SOS StorageUtil list synchronization into the DLL.
- Helmet Toggle 2 and Wet Function Redux remain separate optional patches. Helmet-managed appearances can still be replaced or deleted even while their manual eye control is locked.

## Workbench, Kits, and Safety

- Added Ctrl multi-selection and right-click batch registration to the Equipment tab.
- Expanded the actor dropdown to the complete detected list and separated popup observation from global native revision updates to reduce unrelated actor refreshes and UI flicker.
- Presented Strip Link as an independent ordinary window rather than a screen-dimming modal and blocked input to the workbench and catalog behind it while open. Added the body silhouette, slot cards, body-region connection lines, full result labels such as `Auto-match -> 32 - Body`, and a wide help area at the top center of the silhouette.
- Clarified Fitting Kit Generator JSON placement for a real MO2 mod-folder layout.
- Added safe UTF-8 Korean, Chinese, and other Unicode names and paths. Invalid or unreadable JSON is isolated so one file cannot terminate the F6 UI load.
- Clarified overlapping condition states as **Condition Met / Another Condition Active** while retaining the concise base-appearance status.

## Package and External Menu API

- Removed the main ESP, SEQ, and the retired SexLab, DD, Private Needs, Soulgem Oven, Bathing in Skyrim, Body Search, and SOS bridge PEX/PSC files.
- The main package now centers on the DLL, UI resources, and `SkyrimFittingSystemNative.pex/psc` and runs without a main ESP.
- Added the stable C ABI exports `SkyrimFittingSystem_Open`, `SkyrimFittingSystem_Close`, `SkyrimFittingSystem_IsMenuOpen`, and `SkyrimFittingSystem_SetHotkeyEnabled` for external shortcut and menu-management mods. Menu requests are forwarded to SFS's safe UI processing point. Native shortcut control is runtime-only, preserves the saved binding, leaves the other API calls available, and defaults to enabled after every game launch.
- Added the consumer header, example, and API documentation under `extras/SkyrimFittingSystemAPI.h`, `tests/MenuApiConsumer.cpp`, and `docs/SkyrimFittingSystem-Menu-API.md`.

## Update Instructions

- When upgrading from v1.2.2, replace the old main mod folder instead of merging over it so retired ESP, SEQ, and bridge scripts do not remain.
- Preserve or back up `Interface/SkyrimFittingSystem/user`, especially personal Fitting Kits.
- Install the Helmet Toggle 2 and Wet Function Redux compatibility patches only if you use those mods.

## Version 1.2.2

### Workbench and Conditional Actions

- Reorganized the workbench into Actual Equipment / Registered Appearance and Condition Settings / Action Settings sections.
- Added condition-driven show or hide actions for both actual equipment and registered appearances without removing their base-workbench cards.
- Kept conditional actions separate from bulk appearance deletion, global appearance hiding, and kit saving.
- Added consistent card styling, type icons, multiline slot labels, condition status labels, and wider condition tooltips.
- Removed obsolete registered-appearance context actions and protected user-defined conditions from deletion while they remain assigned.

### Slot Replacement and Kits

- Preserved every original occupied slot when displaying and replacing multi-slot equipment.
- Made a registered appearance atomic: an item that overlaps any occupied slot replaces the whole earlier item in the same base or condition layer.
- Kept only the first item when an outfit or kit contains multiple pieces claiming an overlapping slot.
- Cleaned legacy same-layer duplicate appearances while preserving separate base and condition layers.
- Saved and restored per-slot actual-equipment visibility in SFS kit metadata while remaining compatible with older kit JSON files.
- Excluded conditional-action cards and hide-protected slots from kit visibility capture.

### Visibility, Input, and Compatibility

- Added configurable Hide-Protected Slots. The default set is 39, 50, 51, 60, and 61; actual equipment and registered appearances using any selected slot cannot be manually or conditionally hidden.
- Added keyboard/gamepad UI hotkey combinations and blocked Skyrim or other-mod shortcuts while a text field or hotkey capture is active, with clean release-state restoration afterward.
- Made Windows font discovery and loading safe for Korean, Chinese, and other Unicode paths.
- Fixed Helmet Toggle 2 handling for real 31+42 headgear combined with a registered slot-31 wig, including the workbench eye state and multi-slot card display.
- Applied the same final slot and visibility rules in DAVE, ordinary DAV, and native environments.
- Kept the v1.2.1 Papyrus bridges, ESL plugin, SEQ, external-event lifecycle, and backend separation unchanged.

## Version 1.2.1

- Excluded ordinary NPCs with no SFS state from SFS display and head-slot calculations.
- Limited Helmet Toggle 2 integration tracking to actors with registered SFS head appearances or active compatibility state.
- Preserved original slot masks for actors with no relevant SFS head state.
- Reworked the Helmet Toggle 2 NPC/follower patch around each original reference instead of broad actor scanning.
- Restricted Helmet Toggle integration to slots 30 and 42; slot 31 remains UI grouping only.
- Applied the same actor-scope protections in DAVE, ordinary DAV, and no-DAV environments.
- Prevented bald NPCs, unnecessary city-wide head refreshes, and related frame loss.
- Kept the workbench save format and kit JSON format unchanged.

## Version 1.2.0

### Conditions and Actor-Specific Workbenches

- Removed actor conditions and made actor selection the sole owner of an actor-specific workbench.
- Fully separated registered appearances, real-clothing visibility, and condition state for player, NPC, and follower actors.
- Added conservative nearby-NPC detection and actor-list ordering.
- Added the optional, default-off crosshair NPC selection setting.
- Rebuilt built-in condition cards and the advanced custom condition editor.
- Added localized sample conditions: City Life, Dungeon Exploration, and Night Infiltration.
- Added conditional-appearance priority, default-appearance fallback, and existing-row-order tie breaking.
- Restricted conditions to registered appearance cards.
- Added condition tooltips, drag guidance, and protected deletion while a condition remains assigned.

### Workbench and UI Stability

- Split same-slot card roles into real equipment, default fitting, active conditional fitting, inactive conditional fittings, and empty-slot rows.
- Fixed duplicate rows, stale empty rows, mixed multi-slot equipment rows, and incorrect row reuse.
- Normalized 31+42 and 42-only head equipment into a slot 42 UI group without changing the real armor slot mask.
- Improved workbench synchronization after equipment changes and actor selection changes.
- Prevented dropdown, popup, and context-menu input from clicking controls behind them.
- Removed unsupported drag-moving of registered appearances between assigned slots.
- Reworked global and per-item visibility interaction, including partial-selection state.
- Allowed kit creation, deletion, and condition editing while global visibility is hidden.
- Fixed preview synchronization and stale actor data when the UI is reopened.

### External Event Integration

- Applied complete SexLab MCM stripping rules by scene category, sex, and role to registered appearances.
- Mirrored DD NG Hider MCM slot relationships.
- Added Private Needs automatic strip/restore and exclusion handling.
- Added Soulgem Oven birth, milking, and wanking integration using original completion signals.
- Added Bathing in Skyrim strip-slot and redress-policy integration.
- Separated Body Search from ordinary SexLab handling and mirrored its no-redress policy.
- Split temporary suppression state by integration source and actor.
- Ensured an overlapping event cannot clear another source's hidden state.
- Reduced end-of-event input lock and restore races by keeping bridge completion and visual refresh paths independent.
- Hid options and disabled bridges for integrations that are not installed.

### SOS / TNG and Display Engine

- Refined classification of slot 32 upper-only clothing, slot 32 one-piece clothing, and slot 49 lower-body clothing.
- Expanded English, Korean, and Chinese classification terms while preventing torso/corset false positives.
- Added one-piece terms including leotard, bodysuit, unitard, catsuit, and jumpsuit without changing established priority.
- Synchronized SFS automatic classification through SFS-owned runtime SOS/TNG keywords.
- Preserved original or externally supplied keywords and recovered stale keywords from older SFS sessions.
- Added read-only synchronization with SOS StorageUtil user Revealing/Concealing lists.
- Fixed GenderBender/SOS/TNG genital armor equip and removal cache invalidation per actor.
- Reduced broad NPC scanning and refreshed only actors whose relevant state changed.
- Disabled Helmet Toggle state tracking and banners completely when its compatibility patch is absent.

### Compatibility and Distribution

- Verified separate DAVE, ordinary DAV, and no-DAV display and restore paths.
- Preserved existing workbench saves and kit JSON.
- Updated Korean, English, and Chinese interface text.
- Consolidated the DLL, bridge PEX files, ESL-flagged plugin, and SEQ into the release package.
- Removed bundled CommonLibSSE-NG source and other unnecessary bulk from source distribution.

## Earlier Version History

### v1.1.1

- Stabilized end-of-event restoration ordering and equipment refresh coalescing.
- Clarified global and per-item hide behavior.
- Prevented one actor's external-control state from leaking into another actor's workbench.
- Fixed several SexLab/DD restoration and input-state races.
- Prepared source-isolated temporary suppression and genital-policy resolution used by v1.2.0.
- Preserved existing kit and workbench JSON while reorganizing DLL and PEX distribution.

### v1.1.0

- Added actor-specific registered appearances, visibility state, and conditions for player, NPC, and follower actors.
- Introduced built-in and user-defined condition interfaces with conditional appearance priority.
- Added SexLab MCM strip synchronization and DD Hider integration for registered appearances.
- Added Helmet Toggle 2 synchronization and the optional Wet Function Redux patch.
- Added SOS/TNG genital visibility correction for registered appearances.
- Improved popup input, drag-and-drop behavior, and workbench synchronization.
- Normalized multi-slot head equipment such as 31+42 for workbench display.
- Separated global and per-item visibility by actor.
- Kept optional integrations inactive when their target mod was not installed.

### v1.0.7

- Added the optional Wet Function Redux native compatibility API.
- Improved SOS/TNG genital conceal/reveal correction.
- Improved external-control banners and manual visibility policy.
- Refreshed genital display after slot 32 upper and slot 49 lower combinations changed.
- Separated external temporary-control state from saved manual hide state.

### v1.0.6

- Added Helmet Toggle 2 compatibility signals and head-equipment control banners.
- Improved state display when several external controls overlap.
- Improved workbench synchronization after real equipment changes.
- Synchronized real head equipment and registered head appearances with Helmet Toggle results.
- Prevented manual visibility controls from fighting active Helmet Toggle state.
- Grouped 31+42 equipment with slot 42 registered appearances in the UI.
- Activated compatibility code only when the separate patch supplied a signal.

### v1.0.5

- Separated DAVE API, ordinary DAV, and native fallback display paths.
- Stabilized real-clothing hiding and registered-appearance refresh.
- Reduced unnecessary native refreshes when DAVE was present.
- Improved fallback behavior for DAV and no-DAV environments.
- Reduced stale workbench state immediately after equipment changes.
- Kept real armor equipped so armor rating and effects remained active while visuals were hidden.

### v1.0.4

- Consolidated v1.0.3 functionality for public distribution.
- Aligned DLL, metadata, package, and displayed version numbers.
- Rechecked release folder and installer layout.

### v1.0.3

- Distinguished SexLab/DD temporary-control state by slot banners and tooltips.
- Improved restoration after equipment, slot, condition, and external-event transitions.
- Stabilized condition-editor input and workbench display.
- Prevented one external event from clearing another event's temporary suppression.
- Improved restoration of registered appearances and real-clothing cards.
- Fixed stale condition-row references during move, delete, and edit operations.

### v1.0.2

- Added Options-tab global real-clothing and registered-appearance visibility settings.
- Added automatic SexLab and Devious Devices strip integration options.
- Added new slot creation and drag registration workflow.
- Improved browser filters, equipment cards, and workbench refresh.
- Separated global visibility controls for real clothing and registered appearances.
- Improved equipment, outfit, and kit search and filtering.
- Clarified empty registered-appearance rows and newly created unassigned rows.
- Reorganized workbench cards for side-by-side comparison of real clothing and registered appearances.

### v1.0.1

- Initial public release of Skyrim Fitting System.
- Introduced a native appearance display engine that keeps real equipment and its effects equipped.
- Added equipment, outfit, and kit browsers and a slot-based appearance workbench.
- Added visual hide/show for real clothing and registered appearances, conditions, and SKSE persistence.
- Added ARMO scanning, OTFT outfit registration, and multi-slot fitting kits.
- Hid real equipment visually without unequipping it from inventory.
- Stored user kits and workbench state in JSON and SKSE save data.
- Integrated the Skyrim Vanity System-derived ImGui UI foundation with the SFS native display engine.
