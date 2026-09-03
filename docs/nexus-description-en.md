[![Support Skyrim Fitting System on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/M1P225QD23)

# Skyrim Fitting System

An SKSE appearance system that preserves the armor rating, enchantments, and effects of actual equipped gear while letting you build a separate visible outfit.

Default UI hotkey: **F6**. It can be changed in Options.

## Introduction

Skyrim Fitting System (SFS) separates actual inventory equipment from registered appearances. Actual gear can be hidden visually without being unequipped, while registered appearances do not replace armor stats or gameplay effects.

Registered appearances, conditions, real-gear visibility, manual display choices, and external-strip state are isolated for the player, each NPC, and each follower. The selected strip-link policy is a shared user setting applied to all actors, while every runtime result remains actor-local.

SFS automatically separates DAVE, ordinary DAV, and Skyrim-native display environments while using the same workbench and save data.

## Highlights

- Separate visible appearances while retaining the stats and effects of actual equipment
- Actor-local workbench rows, conditions, manual eye controls, and external strip/redress state
- Mod-Configured Slot Linking, Automatic Vanilla-Slot Linking, and per-slot Direct Editing exceptions
- Built-in Kit Generator for ESP scanning, candidate editing, preview, and kit creation
- Actor-local Fitting Dye for individual rendered components of a registered appearance, available from its dye button or right-click context menu
- Actor-local registered-appearance locks that survive catalog previews and applications
- RaceMenu high-heel `HH_OFFSET` and live BodyMorph synchronization across DAVE, DAV, and native display
- Optional actor-aware body-family filtering for Equipment, Outfits, and Kits, with fail-open handling for custom actors and no workbench filtering
- Final displayed-outfit OAR conditions, built-in Grid Inventory Costume and Helmet Toggle 2 support, and optional DFFMA OAR and Dynamic Footprints bridges

![Hiding Actual Equipment](https://i.ibb.co/4bb432f/image.gif)
**Hiding Actual Equipment**

![Appearance Preview](https://i.ibb.co/mrfbJX7S/image.gif)
**Appearance Preview (Character Positioned on the Left or Right)**

![Per-Actor Appearance Registration](https://i.ibb.co/TpxyZnR/image.gif)
**Per-Actor Appearance Registration (Nearby Detection and Actor-Specific View)**

![Condition and Action Setup](https://i.ibb.co/Kj4RNjs3/image.gif)
**Condition Setup (Preset/Custom) – Action Setup (Show/Hide Equipment and Appearances)**

![Kit Generator](https://i.ibb.co/RpgXkxYN/image.gif)
**Kit Generator (Automatically Generate Kits from a Selected ESP)**

![Appearance Dye System](https://i.ibb.co/Rf3djws/image.gif)
**Appearance Dye System (Select and Dye Individual Outfit Components)**

![External Mod Strip and Redress Integration](https://i.ibb.co/DDBX4WJH/image.gif)
**External Mod Strip/Redress Integration (Follows Each Mod’s MCM-Configured Slots, Including SexLab)**

![Helmet Toggle 2 Appearance Hiding Integration](https://i.ibb.co/S4JHn3DT/2.gif)
**Helmet Toggle 2 Appearance Hiding Integration (Does Not Interfere with Helmet Toggle 2’s Actual Equipment Control)**

## Version 1.5.3 Update Summary

- Added built-in SexLab P+ v2.12.0 strip/redress integration for both Skyrim 1.5.97 and 1.6.1170. No P+ script, ESP, or separate SFS patch is replaced or required.
- Preserved all four external-strip policies: Mod-Configured and Direct+Mod-Configured use actor-local virtual tokens, while Automatic Vanilla and Direct+Vanilla use the existing actual-equipment transaction path.
- Added a default-on body-compatibility option for Equipment, Outfits, and Kits. Uncertain/custom actors fail open, and the registered-appearance workbench is never filtered.
- Fixed scrollbar drag release resetting long lists, nested false custom-condition saves freezing the game, and paused Left/Right character placement failing on later menu openings.
- Expanded RaceMenu version routing for legacy NiOverride transforms, public NiTransform v3+, and compatible BodyMorph v4/v5 interfaces across DAVE, DAV, and native refreshes.
- Fitting Dye now excludes compact UBE collision helpers such as `ArmColli`, `FeetColli`, and `ButtLegColli` without hiding similarly named real outfit components.

## Version 1.5.2 Update Summary

- Added actor-local locks to registered-appearance context menus. Locked appearances remain through Gear, Outfits, Conditions, Kits, and Kit Generator preview/application and are excluded from automatic strip-link, DD Hider, and Helmet Toggle 2 suppression while manual eye and condition behavior remains unchanged.
- Added RaceMenu `HH_OFFSET` synchronization for registered high heels on players, NPCs, and followers across DAVE, DAV, native refreshes, and late DAVE attachments without proxy equipment.
- Fixed live RaceMenu BodyMorph on visible registered appearances across DAVE, DAV, native display, and deferred `UpdateModelWeight` processing.
- Added W/S kit-list navigation outside active text editing and expanded Fitting Dye body/helper filtering for every supported female, male, and Vanilla body family.
- Unified paused and unpaused character framing at FOV 70 with original-camera restoration. Paused right-drag now orbits the camera without moving actor/SMP roots, preventing FSMP garment stretching; unpaused live actor rotation remains unchanged.
- Added fast production-rule regression coverage for actor-local state, all four strip-link policies, virtual-token and actual-equipment transactions, and DAVE/DAV/native refresh dispatch.
- Removed `SFSCore.pdb` from runtime and MO2 packages; local builds retain debug symbols for optional diagnostics.

## Version 1.5.1 Update Summary

- Equipment, Outfits, and Kits now filter automatically for the body family of the actor selected in the workbench. Female groups are CBBE/3BA/3BBB, UNP/UNPB/UUNP/BHUNP, UBE, and Vanilla/fallback; male groups are HIMBO, SAM, and Vanilla/fallback.
- Explicitly conflicting body families are hidden while unlabelled same-sex Vanilla/fallback entries remain visible. The filter uses loaded form and selected-actor metadata only, with no BodySlide/NIF disk scan, nearby-actor polling, or global actor scan.
- Changing actors rebuilds only the visible catalog cache. Registered appearances, conditions, actual equipment, kits, Fitting Dye, virtual tokens, external strip/redress state, and DAVE/DAV/native ownership are not changed.
- Restored live RaceMenu BodyMorph synchronization for registered appearances on the player, NPCs, and followers.
- Skyrim and other-mod input is now suppressed only while a real SFS text editor owns focus and shows its caret, and is released immediately when focus leaves that editor. Intentional keybind capture remains separately locked.

## Version 1.5.0 Update Summary

- Added **Fitting Dye** to registered-appearance cards in both base and condition rows. Open it with the card's dye button or right-click context menu, select a rendered appearance component, identify it by its brief world-space pulse, then apply a color or restore the original.
- Dye is saved as actor FormID → registered appearance ARMO → exact rendered component. Another actor or appearance using the same mesh or diffuse texture remains unchanged, and an ambiguous or changed component identity fails closed instead of tinting a guessed match.
- The renderer builds a private RGB-multiplied tint texture for the exact matched geometry. Source DDS files, shader materials, actual equipment, inventory, keywords, workbench rows, conditions, kits, virtual tokens, strip-link policies, and DAVE/DAV/native ownership are never modified.
- Fixed an SFSCore CTD when switching the workbench from the player to a newly selected NPC while HT2 was installed. The redundant active-effect-list fallback was removed; HT2 NPC/follower support continues through resolved monitor spells, exact `HT_HeadGearEquipped` AddSpell/RemoveSpell signals, and the actor-local FormID cache.
- Replaced the separate Helmet Toggle 2 PEX patch with built-in SFSCore signal observation; no HT2 script replacement is required.
- Reads each real helmet's full ARMO slot mask instead of its deduplicated array position.
- Fixes bald actors when HT2 hides a still-equipped real 31+42 helmet: SFS releases only that actor's real Hair 31 skinning bit while leaving the ARMO, inventory, HT2/DAVE variant, and virtual tokens unchanged. Pure registered 31 remains independent; registered 31+42 still follows through 42.
- Player, NPC, and follower transitions stay actor-local with no periodic polling or surrounding-actor scan. Mod-Configured, Automatic Vanilla, Direct Editing, manual eye, external strip/redress, and DAVE/DAV/native behavior is preserved.

## Requirements

Required: (Required base mod)

- SKSE64 matching the installed Skyrim SE/AE runtime
- Address Library for SKSE Plugins

Optional: (Required optional patch file)

- Wet Function Redux with the separate SFS compatibility patch
- Dynamic Footprints SKSE BASE v3 only with its matching separate SFS patch
- Open Animation Replacer and Dynamic Feminine Female Modesty Animations OAR with the separate DFFMA configuration patch

Built-in: (Not required any patch file)

- RaceMenu for live BodyMorph synchronization on registered appearances
- DAVE or DAV; SFS automatically uses the installed display environment
- SOS or TNG for genital conceal/reveal compatibility
- Helmet Toggle 2 for built-in actor-local registered-appearance headgear hiding; SFS does not interfere with Helmet Toggle 2's actual equipment control
- Grid Inventory v1.4.1
- SexLab P+ v2.12.0 for built-in actor-local strip/redress integration

## Installation and Updating

1. Back up `Interface/SkyrimFittingSystem/user` first if it contains personal kits.
2. Completely delete the previous SFS mod folder.
3. Completely delete any older **SFS Helmet Toggle 2 Compatibility Patch** and the **v1.4.4 SFS Grid Inventory Costume Compatibility Patch**. Do not delete their original mods.
4. If an old standalone test **SFS Kit Generator** folder remains, completely delete it. The Kit Generator is built into SFS and requires no separate DLL or mod folder.
5. Install the v1.5.3 distribution ZIP as a new mod.
6. Enable `SkyrimFittingSystem-VirtualTokens.esl`.
7. Restore the backed-up personal kits only if needed. Grid Inventory v1.4.1+ and Helmet Toggle 2 need no SFS patch; install only the matching separate compatibility patches for Wet Function Redux, DFFMA OAR, or Dynamic Footprints after their original mod.

File changes in v1.4.0:

- `SkyrimFittingSystem.dll` → `SFSCore.dll`: the same single main plugin with a new filename, not an additional DLL. The new name also loads before legacy `skee64.dll`.
- `SkyrimFittingSystem-VirtualTokens.esl`: new and required for external-mod strip linking.
- Main ESP, SEQ, and retired per-mod bridge PEX/PSC files: removed. Any older SFS Helmet Toggle 2 PEX patch is also retired in v1.5.0 because the integration now lives in SFSCore.
- UI resources and `SkyrimFittingSystemNative.pex/psc`: retained.

A clean reinstall prevents the old and new main DLLs from loading the same hooks twice.

v1.2.x and v1.3.x settings migrate to the v1.4.0 default, **Mod-Configured Slot Linking**. After the settings are saved by v1.4.0, the selected policy is preserved. Existing appearances, conditions, and visibility data remain unchanged.

## Quick Start

1. Open SFS with F6.
2. Select the player or NPC at the top of the workbench.
3. Find an appearance in Equipment, Outfit, or Kits.
4. Register it in the base area or a condition row by double-clicking, dragging, or using the context menu.
5. To recolor a registered appearance, use its dye button or right-click it and choose **Fitting Dye**. Select a component, confirm its brief pulse on the character, choose a color, and apply it.
6. Use the separate eye controls for actual gear and registered appearances.
7. Attach a built-in or custom condition when an appearance should react to gameplay state.
8. Choose a strip-link policy in Options, or use **Direct Slot Editing** for per-slot exceptions.
9. Save reusable multi-slot setups as Fitting Kits.
10. To build a new kit automatically, choose outfit ESPs in **Kit Generator**, scan them, edit and preview a candidate, then create the kit. Changing candidates preserves the result classification.
11. Under Options, set **Character Position While Open** to Disabled, Left, or Right. Right-drag the outer area where the character is shown to rotate the player; closing SFS restores the original facing.

Display changes never move or recreate the registered FormID, original slots, base or conditional row, row order, or actor ownership.

## UI and Workbench

![SFS workbench base and condition areas](https://staticdelivery.nexusmods.com/mods/1704/images/187128/187128-1786733591-284613191.png)

### Workbench Base Area

The upper base area represents the selected actor's ordinary state. **Actual Gear** on the left shows equipment truly worn from inventory, while **Registered Appearance** on the right shows what SFS displays instead. Hiding actual gear with its eye button does not unequip it, so armor rating, enchantments, keywords, and equipment effects remain active. The registered-appearance eye changes only the visual result and does not delete registration.

A registered appearance without a condition is that slot's base appearance. It returns whenever no conditional appearance is applicable or every condition is false. Base actual gear, registered appearances, and individual eye state are stored only for the selected actor.

### Fitting Dye

Every registered-appearance card in the base or condition area has a separate dye action. Use its dye button or right-click the card and choose **Fitting Dye**. The popup lists only dyeable third-person rendered components that belong to that exact registered ARMO. Selecting a row makes the component pulse briefly on the character so it can be identified before choosing and applying a color. **Restore original color** removes the saved tint for that component. If a verified matching first-person counterpart exists, it follows the selected third-person component.

Fitting Dye multiplies the component's source diffuse RGB into a private GPU texture and substitutes it only for the exact matched geometry during its draw. It does not edit the original DDS, NIF, shader material, ARMO/ARMA, inventory item, keyword, equipment state, or registered-appearance data. Body, face, hands, feet, collision/helper geometry, overlays, and components without a supported direct diffuse binding are not offered as dye targets.

Saved color identity is actor FormID → registered appearance ARMO → shape name, diffuse texture, and exact scene path. The same outfit or DDS on another actor or another registered appearance is therefore not recolored. Zero or multiple exact matches are skipped rather than guessed. Saved colors are restored only when that actor and exact registered component return after save loading or an actor 3D refresh, including the existing DAVE, DAV, and native refresh paths. Fitting Dye remains independent of workbench rows, conditions, kits, manual eye state, actual-equipment visibility, virtual tokens, and every external strip/redress linking policy.

### Workbench Condition Area

The lower condition area builds situation-dependent display rules as rows. Each row has two independently preserved halves: **Condition Setup** on the left and **Action Setup** on the right.

- **Condition Setup:** decides when the action runs. Drop a built-in or custom condition card here, or double-click a Conditions-tab card to fill the first empty condition card.
- **Action Setup:** decides what target changes and how. Drop actual gear, an existing registered appearance, or an appearance to register, then use the target eye to select **show when true** or **hide when true**.

`+ Create Condition Slot +` adds a row with both halves empty. A condition-only or action-only row is safely preserved and remains inactive until the other half is filled. Deleting one half does not delete the other card. Only a row with both halves empty is cleaned up when the UI closes or at the next load boundary.

A base registered appearance can be dragged into a condition row's action area. Reassigning a gear or appearance card through its context menu reuses an existing empty action row with the same condition instead of creating a duplicate. A true conditional appearance takes priority over the same actor's base appearance. If several conditional appearances are true, workbench row order determines the winner; another true row reports **Condition Met / Another Condition Active**.

### Equipment Tab

Search installed ARMO records by name, plugin, and slot. Hold Ctrl to select multiple entries, then right-click to register them as a batch.

### Outfit Tab

Register the armor entries of an OTFT outfit together without equipping inventory gear.

### Kit Tab

Save and apply multi-slot appearance and real-gear visibility setups as reusable JSON kits. Existing kit JSON remains compatible. Right-click a user kit and choose **Rename** to change only its kit name; its file path, piece composition, and saved layout remain intact. Read-only external kits cannot be changed.

### Kit Generator Tab

Select installed outfit plugins and scan them to construct compatible candidate groups using local name, EditorID, slot, color, numbering, SMP, and exposure rules. Multiple ESPs are processed in parallel within a bounded CPU and memory budget, while a single selected ESP receives the available worker budget. Progress accumulates completed work only, so it never moves backward when parallel tasks finish in a different order.

Click a result row to select and preview that kit. Hold `Ctrl` and **left-click** rows to select multiple results; selected result rows can be renamed, deleted, or merged. Clicking a candidate name changes selection and read-only preview only. The separate **Edit** button is the only way to open piece replacement and deletion. Keyboard and gamepad up/down move the current list selection together with its preview, while the active input enters the next stage or applies a kit.

Candidate preview is temporary on the character and in the workbench's **Registered Appearances** column. It never changes saved workbench rows, inventory, or actual equipment. Normal Skyrim display-slot conflicts remain active, while the exception that exposes protected-slot and SOS/TNG genital candidates is limited strictly to this temporary generator preview.

The initial SFW/NSFW decision made when scanning creates a result is frozen on that result row. Selecting another candidate, editing pieces, or merging candidates does not reclassify it. Known outfit packs prioritize their supplied set catalogs when grouping pieces; other ESPs use general name, occupied-slot, and actual NIF-model-path rules. Cross-plugin duplicates are retained only when their real model paths and slot coverage sufficiently overlap, keeping the more complete copy. Finished kits are written as JSON under `Interface/SkyrimFittingSystem/user/kits`.

### Conditions Tab

![SFS conditions tab](https://staticdelivery.nexusmods.com/mods/1704/images/187128/187128-1786734114-890820144.png)

Create and manage built-in and custom conditions. Built-in cards are ready-to-use states such as interior, exterior, city, town, dungeon, home, combat, non-combat, day, night, rain, snow, underwater, sneaking, and weapon state.

A custom condition combines Skyrim condition functions, typed function arguments, comparison operators, and comparison values. Multiple clauses can be connected with AND/OR or reference existing built-in and custom conditions. Examples include `City AND non-combat`, `Dungeon AND combat`, and `Night AND sneaking`. Consecutive OR clauses are evaluated as Skyrim-style OR groups, and clause order can be changed by dragging.

Custom conditions have a name, description, and shared display color. The editor presents Form, integer, floating-point, string, and boolean inputs according to each function's metadata, and validates required arguments, comparison values, invalid references, and circular references before saving. A condition already referenced by another condition or a workbench row is protected from accidental deletion or movement.

Double-clicking a condition card fills only the first empty condition card in the workbench. It never pulls an appearance or actual gear into the row automatically; the action target is selected separately on the right.

### Options Tab

Configure language, font, UI opacity, hotkey, game pause, nearby-NPC detection, external-mod strip linking, special-effect slot protection, and shield appearance use. **Character Position When Opening UI** can be Disabled, Left, or Right and affects third-person only. Right-drag over the visible character area rotates the player, and closing SFS restores the original camera and facing.

## External-Mod Strip Linking

External-mod strip linking is not a set of patches limited to a few named mods. It is designed to use the same generic path for any external mod that performs strip, redress, confiscation, or outfit-replacement behavior. SFS does not rely only on a hardcoded name list; it observes real calls and their actor-local results, including `GetWornForm`, `UnequipItem*`, `EquipItem*`, `RemoveAllItems`, `RemoveItem`, and `SetOutfit`. Mod names in the compatibility table are representative examples of these behavior types.

This system targets equipment control initiated by external mods. Ordinary manual inventory equip and unequip actions are not classified as external events. When the original mod re-equips actual gear, the related automatic suppression can clear. If the original mod does not redress the actor, SFS does not fabricate or equip replacement gear.

The selected policy and direct mapping table are shared by all actors. Transactions, suppression tickets, DD device state, and final display results remain FormID-local, so stripping NPC A cannot alter NPC B or the player.

### Mod-Configured Slot Linking

![Mod-Configured Slot Linking](https://staticdelivery.nexusmods.com/mods/1704/images/187128/187128-1786734732-580573963.png)

**This is the recommended default.** SexLab and other external mods remain governed by the strip slots, excluded keywords, and redress rules selected independently in their own MCM or settings.

SFS exposes each registered appearance as a fixed-FormID **virtual worn token** that an external mod can query like worn armor. When a mod inspects an actor through `GetWornForm`, equipped arrays, or keyword filters, it can see the applicable token alongside actual gear. If that mod's own settings select the token for stripping, SFS hides the linked registered appearance. When redress by the same external flow is confirmed, SFS restores the user's previous display state.

The tokens are never added to inventory or equipped and create no stats, weight, or gameplay effects. SFS does not replace every mod's settings with one shared rule. It also does not split the logic simply by vanilla versus extension slot number; it combines actual worn gear with virtual tokens for empty appearance slots and reflects the result produced by each external mod on the registered appearance.

Use this mode when:

- The strip slots, excluded keywords, and redress settings selected independently in SexLab and other external mods should remain authoritative
- Extension-slot appearances from 43–61 should participate in the external mod's configured rules
- Appearances without matching actual gear must still appear in worn arrays or keyword filters

### Automatic Vanilla-Slot Linking

![Automatic Vanilla-Slot Linking](https://staticdelivery.nexusmods.com/mods/1704/images/187128/187128-1786735103-1188508119.png)

The core of this mode is automatic recognition of appearance types such as helmet, body, gloves, boots, and accessories, followed by a natural vanilla equipment-slot binding. SFS analyzes the appearance name, EditorID, keywords, original ARMO/ARMA occupancy, and English, Korean, and Chinese semantic clues. This is deterministic local rule-based classification, not generative AI or online learning.

This mode does not directly follow the MCM strip-slot or keyword settings selected independently in SexLab and other external mods. It follows SFS's vanilla-equipment binding and the resulting actual gear changes, so it operates independently of per-mod settings and can produce a different result from **Mod-Configured Slot Linking**.

Automatic classification proceeds in this order:

1. SFS unions the slots occupied by the appearance ARMO and every attached ARMA. Unambiguous vanilla partitions such as pure Hair 31, Long Hair 41, Forearms 34, and Calves 38 are preserved first.
2. The EditorID and display name are normalized into lowercase word tokens. CamelCase and letter/number boundaries such as `BattleAngelsTop01` are split before matching.
3. Keyword EditorIDs supplied by the original ESP/KID or other mods are added to the same classification text. SFS-owned temporary SOS/TNG runtime keywords are excluded so they cannot feed back into a second classification pass.
4. Specific categories win before broad ones. For example, `circlet`, `glasses`, and `mask` prefer 42; `helmet`, `hood`, and `hat` prefer 30; and `necklace`, `amulet`, and `choker` prefer 35. Equivalent Korean, Simplified Chinese, and Traditional Chinese terms are included.
5. `glove`, `gauntlet`, `bracer`, and bracelet terms prefer Hands 33. `boot`, `shoe`, `heel`, and `stocking` terms prefer Feet 37. Body-garment terms such as `body`, `dress`, `top`, `pants`, `skirt`, `belt`, and `cloak`, plus generic accessories, follow Body 32.
6. When a category has several candidate slots, SFS prefers one actually worn by the current actor or just stripped by an external mod. An otherwise unknown extension-slot appearance falls back safely to Body 32. User-protected slots are excluded from automatic candidates.

The vocabulary covers English, Korean, Simplified Chinese, and Traditional Chinese. The terms above are representative examples; the full local rule table also covers wigs, hair ornaments, shields, rings, arm and leg accessories, underwear, swimwear, robes, bags, wings, and common spelling variants.

Representative bindings:

- Helmet/hood: 30, wig/hair: 31, circlet/glasses/mask: 42
- Body, upper/lower clothing, underwear, skirt, and belt: 32
- Gloves/arm accessory: 33, forearms: 34
- Necklace: 35, ring: 36
- Boots/leg accessory: 37, calves: 38, shield: 39

SFS unions the occupied slots from the ARMO and every attached ARMA, so 32+34+38 body armor, 33+34 gloves, 37+38 boots, and 30+31+42 headgear are tracked naturally. When an external mod removes the linked actual gear, the registered appearance hides. When the gear is re-equipped, it returns to the user's previous display state.

Use this mode when:

- Clothing should follow ordinary body parts independently of each external mod's MCM setup
- Extension-slot outfits should be classified into natural vanilla body, glove, boot, or accessory anchors
- A simple actual-equipment strip/redress relationship is preferred

### Direct Slot Editing

Selecting **Direct Slot Editing** in Options opens a popup that shows the registered appearances and their slot relationships together. Direct editing is not a separate engine that requires every slot to be rebuilt manually. It retains one of the following automatic bases and adds exceptions only where needed:

- Mod-Configured Slot Linking
- Automatic Vanilla-Slot Linking

There is no separate third fully-direct base. Choose one automatic base and edit only the slots that need exceptions. Editing a card does not change the base selected inside the popup; untouched slots and newly registered appearances continue using that base's automatic rules. After Apply, the Options combo displays **Direct Slot Editing** to indicate that user exceptions exist, while the internal Mod-Configured or Vanilla automatic base remains saved.

- **Direct exception on the Mod-Configured base:** moves the appearance's virtual token to the selected target from slots 30–61, so SexLab or another external mod applies its settings and keyword filters for that target slot.
- **Direct exception on the Vanilla base:** replaces name, EditorID, and keyword classification with a user-selected actual vanilla-equipment anchor. The appearance follows external removal or re-equip of real gear in that anchor slot.

- `Automatic Match -> Slot`: remove the direct exception and use the selected base
- `No Linking`: exclude only that appearance slot from external strip linking
- A concrete slot: pin that appearance to the selected target

`Automatic Match` is not a third mode; it removes only that card's direct exception. The Mod-Configured base permits eligible targets from 30–61. The Vanilla base permits only vanilla anchors 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, and 42. Cards belonging to the same multi-slot registered item stay synchronized.

The selected base and direct mapping table are shared policy for all actors. Actual strip transactions, automatic suppression, manual eye state, and restoration results remain isolated by actor FormID.

### No Linking

External strip and redress actions do not automatically change registered appearance visibility. Saved direct mappings are retained and return when linking is enabled again.

### Manual Eye Controls and Automatic State

Eye controls are not locked merely because an external event is active. Automatic state is represented accurately, but the user may still operate individual and global eye controls for actual gear and registered appearances. Only restrictions owned by a separate feature, such as protected slots, remain enforced.

Automatic suppression never deletes registration. An appearance already hidden by the user remains hidden after redress. An appearance that was visible returns when the external mod actually re-equips its linked gear.

## External-Mod Compatibility Table

The mod names below are **representative examples**, not a restricted support list. SFS does not hardcode those names; its generic observer follows common strip, redress, confiscation, and wardrobe-replacement calls together with each actor's final worn state. A mod that is not listed should therefore work in most cases when it uses ordinary strip/redress behavior, without requiring a dedicated SFS patch or an update to that mod. Mods not named in the verification column still need an individual scene check.

| Action Type | Representative Mods | Compatibility | Verification |
|---|---|:---:|---|
| SexLab strip/redress<br>`StripActor` · `GetWornForm`<br>`UnequipItemEx` · `EquipItemEx` | SexLab Framework SE · SexLab Utility Plus · Soulgem Oven / SGO4 Integration Fork · Fill Her Up Baka Edition · SexLab Defeat Bane · BaboDialogue · Bimbos of Skyrim · Balazar’s Bitch · Sexy Adventures · SexLab Approach Redux · SexLab Aroused Creatures · SexLab Dialogues · SexLab Solutions · SexLab Romance · SexLab Body Search | Generic | SexLab and SGO verified |
| Slot filters and individual equip changes<br>`AddAllEquippedItemsToArray`<br>`GetWornForm`<br>`UnequipItem/Slot/Ex`<br>`EquipItem/ByID` | Private Needs – Orgasm · Bathing in Skyrim – Renewed · Licenses – Player Oppression · SLHH Expansion · Trap Needs to Be Real Trap / TNTR · Simple Player Prostitution · Death Trap Chest · Soul Resurrection | Generic | PNO and Bathing verified |
| DD devices and Hider<br>`GetWornForm`<br>`EquipItem*` · `UnequipItem*`<br>`zadNativeFunctions.SyncSetting` | Devious Devices SE/NG · Devious Helpless Redux · Devious Interests · Devious Cidhna · Devious Curses NG · Unforgiving Devices · Laura’s Bondage Shop | DD-specific | DD equip/removal verified |
| Prison, confiscation, and detention<br>`RemoveAllItems` · `RemoveItem`<br>`SetOutfit`<br>`EquipItem/UnequipItem` | Pama Prison Alternative · Pama Orkish Bounty Hunters · Pama Punishment · Pama Bad Ends · Pama Sovngarde · Pama Deadly Furniture · Captured by the Thalmor · Captive Player · Dark Arena · Bandit Paradise · Follower Slavery Mod | Generic | Pama verified |
| Slavery and forced wardrobe replacement<br>`StripActor` · `UnequipAll`<br>`RemoveAllItems` · `SetOutfit`<br>`EquipItem*` | Public Whore · Sanguine Debauchery / SD+ · Simple Slavery Plus Plus / Rebuild · S.L.U.T.S. Resume · Submissive Lola · Dress Up Lover’s NPC Outfit Changer | Generic | More per-mod testing needed |
| Alternate-start and quest wardrobe replacement<br>`UnequipAll` · `RemoveAllItems`<br>`SetOutfit` · `EquipItem` | Alternate Perspective · Deviant Start · Kidnapped Start · Adventurer’s Start · The Adventurer’s Guild | Generic | More per-mod testing needed |

## Actual Gear, Registered Appearance Visibility, and Conditions

Hiding actual gear is visual only and does not unequip it or remove its armor rating, enchantments, or effects. Hiding a registered appearance removes only its display and preserves registration.

Global hide and per-item eye controls compose normally. If individual items differ from the global state, the global checkbox shows the correct partial state. Automatic external-mod state follows the same display rule.

Conditional appearances take priority over the same actor's base appearance. Condition and action cards are preserved independently, and empty condition/action areas share the same drag-and-drop behavior and visual style.

## Actors and NPCs

Registered appearances, actual-gear hiding, conditions, external transactions, and temporary display state are actor-local. The actor selector lists the player, NPCs with saved data, and currently nearby NPCs. Hostile actors and corpses are excluded from automatic nearby detection.

Although every actor uses the same policy and mapping table, actual results are stored only for the affected FormID. A SexLab scene or wardrobe replacement involving NPC A cannot change NPC B or the player.

## Protected Special-Effect Slots and Shields

Slots 50, 51, 60, and 61 are protected by default and can be customized across slots 30–61.

- Actual gear containing a protected slot always remains visible
- New appearance registration for protected items is blocked
- Existing base and conditional appearances stay inactive while protected and return when protection is removed
- Equipment, Outfit, Kit, condition registration, and the strip-link popup use the same filter

**Use Shields as Appearance Slots** is OFF by default. When off, the actual slot-39 shield remains visible and shield appearance registration is blocked. When enabled, slot 39 works as an ordinary appearance slot.

## Fitting Kits

Virtual game path:

`Data/Interface/SkyrimFittingSystem/user/kits`

Inside the SFS MO2 mod folder, do not create another `Data` directory. Use:

`Interface/SkyrimFittingSystem/user/kits`

The built-in **Kit Generator** writes finished JSON files directly here. Externally created kits can also be copied into this folder. UTF-8 Korean, Chinese, other Unicode names and paths, and UTF-8 BOM files are supported; invalid or unreadable JSON is skipped safely.

## RaceMenu BodyMorph and Wig Color

When RaceMenu is present, SFS follows RaceMenu's common BodyMorph/NiOverride update path instead of a list of mod-specific events. There is no polling or global actor scan; only the actor receiving a real morph update and that actor's active registered appearance nodes are processed across DAVE, DAV, and native paths.

If registered wigs become darker after loading a save, [Hair Colour Sync NG](https://www.nexusmods.com/skyrimspecialedition/mods/156414) is recommended. Use `bEnableTintHairSlot=0` in RaceMenu's `Data/SKSE/Plugins/skee64.ini` and do not combine multiple wig-color synchronization mods.

## SOS / TNG and Naked-State Checks

SFS evaluates the final visible combination of actual gear and registered appearances. Original-mod and user-assigned revealing or concealing policy has priority, while SFS-owned runtime keywords are tracked and cleaned safely.

Automatic slot-32/49 classification minimizes source-ARMO mutation. A confirmed slot-32 upper-only item removes opposite SFS-owned Concealing/Covering state and gains Revealing keywords. Ordinary full-body slot-32 armor already conceals through normal SOS/TNG slot behavior, so SFS clears only conflicting SFS Revealing state and does not add Concealing, Covering, or Underwear keywords. A classified slot-49 lower garment gains Concealing/Covering and Underwear keywords because basic SOS/TNG behavior may not conceal slot 49 automatically. Keywords originally supplied by an ESP, KID, or another mod are never deleted.

The slot-32 upper-only name classifier ignores generic vanilla equipment taxonomy such as `ArmorCuirass`, `ArmorHeavy`, `ArmorLight`, `ArmorMaterial*`, `ClothingBody`, and `VendorItemArmor`. Item names, EditorIDs, and semantic mod/KID keywords remain available, preventing ordinary vanilla armor from being mistaken for a `top`, `bra`, or `shirt`.

Papyrus and Skyrim engine-condition `WornHasKeyword` checks use the same actor-local final body display state. The actor is covered when either actual slot-32 gear or a registered slot-32 appearance is displayed, and uncovered when both are hidden. This does not forcibly replace the private logic of an external DLL that scans inventory directly.

## DAV / DAVE / Native Environments

SFS separates DAVE's public refresh API, ordinary DAV follow-up, and Skyrim-native skinning. Save format, actor ownership, registered FormIDs, and original slots remain identical across environments.

## Wet Function Redux

Wet Function Redux applies wet effects to SFS registered appearances through the separate optional compatibility patch.

## Built-in Helmet Toggle 2 Integration

Helmet Toggle 2 needs no separate SFS patch. SFSCore observes only HT2's exact player state global and exact actor-local NPC/follower spell transitions. It reads the managed real ARMO's full slots, excludes pure registered slot 31, and lets registered 31+42 cards follow through slot 42. When a hidden, still-equipped real headgear occupies Hair 31, SFS releases only that actor's Hair bit from the renderer's worn mask so the original hair is skinned. There is no periodic actor scan, and SFS never edits HT2 scripts, calls, actual equipment, DAVE variants, or Mod-Configured virtual tokens.

## Optional Compatibility Patches

- **Dynamic Feminine Female Modesty Animations OAR 4.30:** the separate FOMOD changes only its selected OAR JSON conditions to read SFS's final displayed outfit. It does not replace DFFMA animations, meshes, scripts, DLLs, or plugins.
- **Wet Function Redux Visual Effect Patch v1.2.0:** supplies only the visual-effect script. It never replaces Wet Function's MCM script; a RaceMenu warning is Wet Function's own legacy self-check, not an SFS equipment or save-data modification.
- **Dynamic Footprints SKSE BASE v3:** the separate patch reads only a visible SFS Feet 37 appearance for Dynamic Footprints' own footprint classification and otherwise uses actual footwear. It disables itself unless the exact verified v3 DLL build is present.

## External Runtime Menu API

The API module name changed from `SkyrimFittingSystem.dll` to `SFSCore.dll` in v1.4.0. External shortcut and menu-management mods must therefore look up `SFSCore.dll` when resolving these functions:

- `SkyrimFittingSystem_Open`
- `SkyrimFittingSystem_Close`
- `SkyrimFittingSystem_IsMenuOpen`
- `SkyrimFittingSystem_SetHotkeyEnabled`

Only the DLL lookup name changed. The four C export names and function signatures remain unchanged. Consumers supporting both versions should try `SFSCore.dll` first, then fall back to `SkyrimFittingSystem.dll` for v1.3.x. Requests are transferred to SFS's safe UI processing point, and hotkey enable state resets on every game launch. An updated consumer header and `GetProcAddress` example are included in the source package.

## Credits & License

Skyrim Fitting System is licensed under [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.html). The source code is available on [GitHub](https://github.com/compilecraftworks/SkyrimFittingSystem). This project contains modified work derived from [Skyrim Vanity System](https://www.nexusmods.com/skyrimspecialedition/mods/175182) and [Skyrim Outfit System SE Revived](https://www.nexusmods.com/skyrimspecialedition/mods/42162).

Credits to the authors of CommonLibSSE-NG, Dear ImGui, SKSE, and Address Library for SKSE Plugins. All respective rights belong to their original authors.

[![Support Skyrim Fitting System on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/M1P225QD23)
