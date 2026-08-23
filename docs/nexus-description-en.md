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
- Final displayed-outfit OAR conditions, built-in Grid Inventory Costume support, and optional DFFMA OAR, Helmet Toggle 2, and Dynamic Footprints bridges

## Version 1.4.6 Update Summary

- Grid Inventory v1.4.1+ Costume synchronization now changes SFS only for a non-empty, player-owned armor Costume. Clearing Costume, selecting no Costume, or sending a non-armor layout preserves the player's saved registered appearances.
- The restored Costume state after startup, save load, or revert remains ignored. SFS preserves its saved registered appearances until Grid Inventory sends a later non-empty Costume change.
- The bridge is now inside `SFSCore.dll`: it uses Grid Inventory's public SKSE message only, copies its data during the callback, and applies it at SFS's normal safe processing point. It never replaces, loads, hooks, or depends on `GridInventory.dll`.
- Remove the old v1.4.4 Grid Inventory replacement-DLL patch when updating. Grid Inventory remains the owner of Costume rendering, loadouts, saves, and actual equipment.
- Mod-Configured Slot Linking, Automatic Vanilla-Slot Linking, Direct Editing, external strip/redress handling, and DAVE/DAV/Skyrim-native display paths remain actor-local and unchanged.

## Requirements

Required:

- SKSE64 matching the installed Skyrim SE/AE runtime
- Address Library for SKSE Plugins
- The included `SkyrimFittingSystem-VirtualTokens.esl` enabled

Optional:

- RaceMenu for live BodyMorph synchronization on registered appearances
- DAVE or DAV; SFS automatically uses the installed display environment
- SOS or TNG for genital conceal/reveal compatibility
- Wet Function Redux with the separate SFS compatibility patch
- Open Animation Replacer and Dynamic Feminine Female Modesty Animations OAR for the separate DFFMA configuration patch
- Grid Inventory v1.4.1+ for built-in Costume synchronization; Helmet Toggle 2 or Dynamic Footprints SKSE BASE v3 only when installing their matching separate SFS patch
- SexLab, Soulgem Oven, Private Needs, Bathing in Skyrim, Devious Devices, Pama Prison Alternative, or other supported external-strip mods

## Installation and Updating

1. Back up `Interface/SkyrimFittingSystem/user` first if it contains personal kits.
2. Completely delete the previous SFS mod folder.
3. Completely delete any older **SFS Helmet Toggle 2 Compatibility Patch** and the **v1.4.4 SFS Grid Inventory Costume Compatibility Patch**. Do not delete their original mods.
4. If an old standalone test **SFS Kit Generator** folder remains, completely delete it. The Kit Generator is built into SFS and requires no separate DLL or mod folder.
5. Install the v1.4.6 distribution ZIP as a new mod.
6. Enable `SkyrimFittingSystem-VirtualTokens.esl`.
7. Restore the backed-up personal kits only if needed. Grid Inventory v1.4.1+ needs no SFS patch; install only the matching separate compatibility patches for Wet Function Redux, DFFMA OAR, Helmet Toggle 2, or Dynamic Footprints after their original mod.

File changes in v1.4.0:

- `SkyrimFittingSystem.dll` → `SFSCore.dll`: the same single main plugin with a new filename, not an additional DLL. The new name also loads before legacy `skee64.dll`.
- `SkyrimFittingSystem-VirtualTokens.esl`: new and required for external-mod strip linking.
- Main ESP, SEQ, and retired per-mod bridge PEX/PSC files: removed. The old fixed-slot Helmet Toggle 2 patch is replaced by a separate v1.4.4 actor-local compatibility patch.
- UI resources and `SkyrimFittingSystemNative.pex/psc`: retained.

A clean reinstall prevents the old and new main DLLs from loading the same hooks twice.

v1.2.x and v1.3.x settings migrate to the v1.4.0 default, **Mod-Configured Slot Linking**. After the settings are saved by v1.4.0, the selected policy is preserved. Existing appearances, conditions, and visibility data remain unchanged.

## Quick Start

1. Open SFS with F6.
2. Select the player or NPC at the top of the workbench.
3. Find an appearance in Equipment, Outfit, or Kits.
4. Register it in the base area or a condition row by double-clicking, dragging, or using the context menu.
5. Use the separate eye controls for actual gear and registered appearances.
6. Attach a built-in or custom condition when an appearance should react to gameplay state.
7. Choose a strip-link policy in Options, or use **Direct Slot Editing** for per-slot exceptions.
8. Save reusable multi-slot setups as Fitting Kits.
9. To build a new kit automatically, choose outfit ESPs in **Kit Generator**, scan them, edit and preview a candidate, then create the kit. Changing candidates preserves the result classification.
10. Under Options, set **Character Position While Open** to Disabled, Left, or Right. Right-drag the outer area where the character is shown to rotate the player; closing SFS restores the original facing.

Display changes never move or recreate the registered FormID, original slots, base or conditional row, row order, or actor ownership.

## UI and Workbench

![SFS workbench base and condition areas](https://staticdelivery.nexusmods.com/mods/1704/images/187128/187128-1786733591-284613191.png)

### Workbench Base Area

The upper base area represents the selected actor's ordinary state. **Actual Gear** on the left shows equipment truly worn from inventory, while **Registered Appearance** on the right shows what SFS displays instead. Hiding actual gear with its eye button does not unequip it, so armor rating, enchantments, keywords, and equipment effects remain active. The registered-appearance eye changes only the visual result and does not delete registration.

A registered appearance without a condition is that slot's base appearance. It returns whenever no conditional appearance is applicable or every condition is false. Base actual gear, registered appearances, and individual eye state are stored only for the selected actor.

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

## Optional Compatibility Patches

- **Dynamic Feminine Female Modesty Animations OAR 4.30:** the separate FOMOD changes only its selected OAR JSON conditions to read SFS's final displayed outfit. It does not replace DFFMA animations, meshes, scripts, DLLs, or plugins.
- **Wet Function Redux Visual Effect Patch v1.2.0:** supplies only the visual-effect script. It never replaces Wet Function's MCM script; a RaceMenu warning is Wet Function's own legacy self-check, not an SFS equipment or save-data modification.
- **Grid Inventory Costume v1.4.1+:** built directly into SFSCore through Grid Inventory's public Costume-state message. Only a non-empty, player-owned armor Costume replaces the player's registered appearances. Cleared, empty, and non-armor Costume states leave SFS appearances unchanged; the first restored state after startup, save load, or revert is also ignored. No Grid DLL replacement is installed; Grid Inventory remains the owner of its Costume data, renderer, loadouts, saves, and real equipment.
- **Helmet Toggle 2:** install the separate script patch only with Helmet Toggle 2. It forwards each actor's HT2-managed slots (30/31/42/44 and player 55 where applicable) to matching SFS registered appearances. Real equipment remains under HT2; SFS preserves actor-local linking, saved manual eye state, and its DAVE/DAV/native display path.
- **Dynamic Footprints SKSE BASE v3:** the separate patch reads only a visible SFS Feet 37 appearance for Dynamic Footprints' own footprint classification and otherwise uses actual footwear. It disables itself unless the exact verified v3 DLL build is present.

## External Runtime Menu API

The API module name changed from `SkyrimFittingSystem.dll` to `SFSCore.dll` in v1.4.0. External shortcut and menu-management mods must therefore look up `SFSCore.dll` when resolving these functions:

- `SkyrimFittingSystem_Open`
- `SkyrimFittingSystem_Close`
- `SkyrimFittingSystem_IsMenuOpen`
- `SkyrimFittingSystem_SetHotkeyEnabled`

Only the DLL lookup name changed. The four C export names and function signatures remain unchanged. Consumers supporting both versions should try `SFSCore.dll` first, then fall back to `SkyrimFittingSystem.dll` for v1.3.x. Requests are transferred to SFS's safe UI processing point, and hotkey enable state resets on every game launch. An updated consumer header and `GetProcAddress` example are included in the source package.

## Credits / Thanks

The basic UI structure of Skyrim Fitting System was developed from [Skyrim Vanity System](https://www.nexusmods.com/skyrimspecialedition/mods/175182).

The structure of [Skyrim Outfit System SE Revived](https://www.nexusmods.com/skyrimspecialedition/mods/42162) was consulted while implementing parts of the appearance display engine.

Thank you to the authors who published these projects and their source. SFS respects their licenses, identifies its lineage, and publishes its corresponding source.

SFSCore also uses CommonLibSSE-NG, Dear ImGui, nlohmann/json, rapidcsv, spdlog, SimpleIni, toml11, DirectXMath, DirectX Tool Kit, and Xbyak under their respective MIT or BSD-3-Clause licenses. Full notices are included in each download. SKSE64 (the SKSE Team) and Address Library for SKSE Plugins (meh321) are required dependencies; XMake is the Apache-2.0 build tool used for the source release.
