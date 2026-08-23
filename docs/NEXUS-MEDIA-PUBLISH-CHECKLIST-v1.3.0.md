# Nexus v1.3.0 SFS screenshot and publishing checklist

Capture every image from the final Skyrim Fitting System v1.3.0 UI after the development session completes its in-game regression pass. Do not reuse v1.2.2 tutorial images for changed workflows.

## Required screenshots

1. Full SFS window with the expanded actor selector showing the complete detected list.
2. Workbench overview with Actual Equipment, Registered Appearance, Condition Settings, Action Settings, and the Strip Link button visible.
3. Equipment tab with Ctrl multi-selection and the right-click batch-registration action.
4. Strip Link ordinary window in No Linking mode, including the selected actor title, slot cards, silhouette, blocked background input, and the wide help area at the top center of the silhouette.
5. Strip Link popup in Vanilla Auto-Match mode with body-region connection lines and computed targets visible.
6. Strip Link popup in Include Mod Slots mode or with one per-slot fixed exception and synchronized multi-slot cards.
7. Options showing Protect Special-Effect Slots with defaults 50, 51, and 61 plus Use Shields as Appearance Slots set to OFF.
8. Conditions/workbench view showing Condition Met and Another Condition Active without covering the base-appearance status.
9. Kits tab or file-layout illustration showing the MO2 path `Interface/SkyrimFittingSystem/user/kits` without an extra `Data` folder.
10. Optional compatibility frame showing RaceMenu BodyMorph, Helmet Toggle 2, or another actor-local integration only if the final image is understandable without debug overlays.

## Capture rules

- Use the final v1.3.0 runtime files and final locale text, not the current development build merely because it compiles.
- Capture at 16:9, preferably 1920×1080 or 2560×1440.
- Use the English UI for the English Nexus description and a separate Korean set if images will be embedded in a Korean post.
- Avoid console windows, logs, debug overlays, private character/save names, and unrelated mod-list details.
- Use one player and one follower/NPC to demonstrate that Strip Link settings are actor-local.
- Show the selected actor label and FormID only with a safe test character.
- Ensure dropdowns, full computed labels such as `Auto-match -> 32 - Body`, connection lines, and card labels remain legible after Nexus scaling.

## Publish rules

- Do not publish or upload from this documentation task.
- Add image placeholders to the description only after the final screenshot set and intended host URLs are known.
- Keep every BBCode source byte-identical to its matching `PASTE-IN-BBCODE-MODE` file.
- Preview headings, lists, line breaks, code text, and external links before saving any live page.
- Confirm that the main description contains usage and compatibility guidance, the v1.3.0 update post contains only v1.3.0 changes, and the full history remains separate.
