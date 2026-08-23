# Skyrim Fitting System v1.4.0

## External-Mod Strip Linking

- Ordered the shared strip-link policy as `Mod-Configured Slot Linking → Automatic Vanilla-Slot Linking → Direct Slot Editing → No Linking`. Policy and mappings are shared by all actors, while event state, suppression, and restoration remain actor-local.
- **Mod-Configured Slot Linking** uses non-inventory virtual tokens from the included `SkyrimFittingSystem-VirtualTokens.esl` so external mods can apply their own MCM slot, exclusion-keyword, and redress policies.
- **Automatic Vanilla-Slot Linking** classifies names, EditorIDs, keywords, and ARMO/ARMA occupancy to choose a natural actual-equipment anchor from slots 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, and 42.
- **Direct Slot Editing** changes only required exceptions on top of either automatic base. The popup no longer exposes a third fully manual base; untouched slots continue to follow the selected automatic rule.
- Equipped-array, slot, and keyword observations from the first external call are applied retrospectively to the first real strip in the same call stack. Event-added equipment, redress, `RemoveAllItems`, `RemoveItem`, and `SetOutfit` are handled as actor-local wardrobe transactions.
- Mod-configured real-gear strips apply appearance suppression through the engine's existing equipment rebuild, while virtual-token display refresh is coalesced within each transaction to avoid disrupting SGO birth startup.
- DAVE uses its public actor-refresh API; ordinary DAV and Skyrim-native refresh remain separate backend paths.
- Manual inventory equip and unequip are not classified as external events. External events do not lock the individual or global eye controls for actual gear or registered appearances; automatic and manual state remain separate.

## DD, Prison, and Wardrobe Replacement

- The device/Hider layer activates only when Devious Devices is installed. It separates actual devices, ordinary clothing hidden by Hider, and related registered appearances while preserving manual visibility across equip and removal.
- DD-specific handling and generic `RemoveAllItems`, `SetOutfit`, confiscation, prison, and forced-wardrobe transactions no longer clear one another's actor state.
- If a cell or location event arrives before prison confiscation, the coalesced actor refresh observes the context boundary again, preserving the pre-replacement wardrobe snapshot and registered-appearance suppression.
- Only slots that an external mod actually redresses are restored automatically. SFS does not create or re-equip actual gear on the external mod's behalf.

## Additional Improvements

- Removed the dedicated Helmet Toggle 2 Papyrus bridge, transient co-save state, workbench lock/banner, and optional patch source. Head, hair, and circlet appearances use ordinary visibility, condition, and strip-link rules.
- Corrected automatic equipment suppression so native, DAV, and DAVE rendered appearances match the eye-icon state.
- Standardized the default protected special-effect slots as 50, 51, 60, and 61 while preserving customized lists.
- Rebuilds only the affected actor's registered appearance nodes after RaceMenu BodyMorph updates. OBody, FHU, and SGO share the common path without a global actor scan, periodic polling, or per-mod event lists.
- Applies the same SFS slot-32 upper/full-body and slot-49 lower-body decision to actual gear and registered appearances. Only upper-only slot 32 gains Revealing, ordinary full-body slot 32 gains no Concealing/Underwear keywords, and classified lower slot 49 gains Concealing/Underwear. Explicit SOS MCM choices retain priority, ESP/KID source keywords are never deleted, and generic vanilla armor taxonomy is excluded from upper-only classification.

## Display, Workbench, and Safety

- Papyrus and Skyrim engine-condition `WornHasKeyword` use the same actor-local final displayed state. The actor is covered when either actual or registered slot-32 clothing is visible, and uncovered only when both are hidden.
- SFS-owned runtime SOS/TNG keywords are excluded from Vanilla automatic classification to prevent self-reclassification.
- Protected slots and disabled shields are excluded from gear, outfit, kit, and condition registration; hiding; automatic/direct linking; and workbench display. Enabling protection removes registered appearances only and leaves actual gear intact.
- Double-clicking a condition card fills only the highest empty condition card and never moves equipment or an appearance automatically. An empty row with the same condition is reused before creating another row.
- The workbench `+ Create Condition Slot +` row is clipped to the table interior and scrollbar width, fixing offscreen label leakage and right-side truncation.
- Registered appearances, conditions, actual-gear visibility, manual display state, and external-event transactions remain actor-local. An event affecting NPC A does not alter NPC B or the player.

## Package and API

- Removed the main ESP, SEQ, and retired SexLab, DD, Private Needs, Soulgem Oven, Bathing in Skyrim, Body Search, and SOS per-mod bridge PEX/PSC files.
- Renamed the single native plugin to `SFSCore.dll` so it naturally loads before `skee64.dll`, allowing the official RaceMenu interface exchange to work on the legacy Skyrim 1.5.97 / SKSE 2.0.20 / RaceMenu 0.4.16 combination without replacing RaceMenu or shipping a second SFS plugin.
- The main package contains `SFSCore.dll`, UI resources, `SkyrimFittingSystemNative.pex/psc`, and `SkyrimFittingSystem-VirtualTokens.esl`.
- Provides the external menu C ABI `SkyrimFittingSystem_Open`, `SkyrimFittingSystem_Close`, `SkyrimFittingSystem_IsMenuOpen`, and `SkyrimFittingSystem_SetHotkeyEnabled`.

## Installation and Migration

- Back up `Interface/SkyrimFittingSystem/user` if needed, then completely delete the previous SFS mod folder.
- Completely delete the old SFS Helmet Toggle 2 compatibility patch, then install the v1.4.0 ZIP as a new mod.
- Enable `SkyrimFittingSystem-VirtualTokens.esl`. Restore only the backed-up personal kits if needed.
- v1.3.0 used `SkyrimFittingSystem.dll`; v1.4.0 uses `SFSCore.dll`. A clean reinstall prevents both complete plugins from loading the same hooks twice.
- v1.2.x and v1.3.x settings migrate to the v1.4.0 default, `Mod-Configured Slot Linking`. Once saved by v1.4.0, the selected policy is preserved; existing appearances, conditions, and visibility data remain intact.

## Verification Status

- In DAVE, Mod-Configured Slot Linking was checked with SexLab, SGO, PNO, Bathing, DD, and NPC SexLab paths.
- Automatic Vanilla-Slot Linking was checked with SexLab, SGO, PNO, Bathing, and NPC SexLab paths.
- DD device equip/removal, Pama prison and confiscation wardrobe changes, individual/global eye-state restoration, and cross-actor isolation were verified.
- DAV and Skyrim native use the same policy and actor-local state model while keeping their separate display backends. A minimal regression pass in each environment is still recommended before publishing.
