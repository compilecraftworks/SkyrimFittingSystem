# Skyrim Fitting System v1.4.3

## Built-in Kit Generator

- Integrated the complete SFS Kit Generator into `SFSCore.dll`. A separate `SFSKitGenerator.dll` is no longer required.
- Outfit-plugin selection, scanning, candidate generation, search and sorting, renaming, SFW/NSFW classification, piece replacement and deletion, multi-selection merging, and final kit creation are now available in-game.
- The selected candidate is previewed read-only on the character and in the workbench's Registered Appearances column. Saved rows, inventory, and actual equipment are not modified.
- Protected-slot and SOS/TNG genital candidates are included only in the temporary generator preview. This exception does not affect normal registration, application, saving, Gear, Outfits, Kits, or strip linking.
- Multiple ESPs are scanned in parallel within conservative CPU and memory limits, while a single ESP receives the full available worker budget. Large multi-slot DP state sets inside one profile are also parallelized within that same safe budget, and progress remains monotonic.
- Packs such as ADD 03 Dark Knight no longer let one multi-slot piece drag unrelated slots into one enormous DP. Independent slots are finalized immediately and only genuinely overlapping masks enter exact optimization, preserving candidate rules while substantially reducing heavy-profile scan time.
- Result and candidate lists now share click-to-select and `Ctrl+left-click` multi-selection. Clicking a candidate changes selection and preview only; the separate Edit button is the only way into the editor. Fixed action columns keep candidate counts and Edit buttons visible when the generator pane narrows.
- Repeated color profiles gain distinguishing outfit-piece names and deterministic number suffixes. Ordinary `_X` EditorIDs and `NonStocking` no longer cause false NSFW classification, while explicit `[X]` and `(X)` tags remain supported.
- Common name roots and actual slot coverage are evaluated together, allowing unknown component wording to join one kit when the pieces complement each other like tops, bottoms, and footwear. Collection and series numbers remain distinct kit identities.
- Outfits duplicated across differently localized ESPs are compared by name, NIF model paths, and slot coverage. Only the more complete copy by occupied slots and item count is kept.
- Initial selection favors the non-exposed candidate with the greatest unique slot coverage and the most base-like profile. Internal SFW/NSFW classification and prefix generation remain, while the manual `NSFW/SFW` result-list button has been removed.
- Known outfit packs prioritize their supplied set catalogs when grouping pieces; packs outside those catalogs continue to use the existing name, slot, and model-path rules. Cross-plugin duplicates are collapsed to the more complete copy only when their actual NIF model paths and slot coverage sufficiently overlap.
- **Delete Item** now removes exactly every result selected with Ctrl+left-click. It no longer deletes an unselected result or only one entry from a multi-selection.

## UI and Workbench

- Added **Rename** to the Kit tab's right-click menu. It safely changes only the kit name in a user-kit JSON; its file path, piece composition, and saved layout remain intact. Read-only external kits cannot be changed.
- Reordered the tabs to `Gear → Outfits → Conditions → Kits → Kit Generator → Options`. The UI still opens on Kits.
- Preserved the existing keyboard and gamepad kit-list controls. Preview follows Skyrim's remapped `Jump` event, while Apply follows the remapped `Activate` event.
- Added sorting to the Actual Equipment and Registered Appearances headers in the base workbench area and to the Condition Setup and Action Setup headers in the conditional area.
- Multi-slot cards remain atomic and empty Actual Equipment or Registered Appearance cells participate in whole-row sorting. Condition groups remain intact, while empty action cells no longer distort a group's slot sort key. Sorting changes display order only—not saved data or the underlying row order.
- Added `Disabled / Left / Right` character placement under Options. It now follows Show Player In Menus' player-rotation and offset formula, requests temporary camera ownership through SmoothCam's public API when available, and reapplies the saved side whenever SFS is reopened. Side-specific front-facing correction, right-drag rotation, and full camera/facing restoration remain third-person only.

## Updating

Completely remove the previous SFS mod folder and any test SFS Kit Generator option folder, install the v1.4.3 ZIP as a new mod, and enable `SkyrimFittingSystem-VirtualTokens.esl`.
