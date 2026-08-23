# Skyrim Fitting System v1.4.1

## Fixes

- Fixed registered appearances not rendering immediately when applied to a completely unequipped player or NPC. Previously, another equipment change—such as equipping a ring—could be required before the appearance became visible.
- When an actor has no actual armor equipped, SFS now performs one actor-local 3D refresh only when the registered appearance composition changes. No global actor scan or periodic polling is used.
- When both actual equipment and registered appearances are empty, the workbench now shows one full-width empty row below the existing column header without duplicating the condition header.

## Updating

Completely remove the previous SFS mod folder, install the v1.4.1 ZIP as a new mod, and enable `SkyrimFittingSystem-VirtualTokens.esl`. Existing registered appearances, conditions, visibility data, and personal kits remain usable.
