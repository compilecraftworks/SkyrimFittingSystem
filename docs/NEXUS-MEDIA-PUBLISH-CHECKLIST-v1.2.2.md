# Nexus v1.2.2 SFS screenshot and publishing checklist

Capture every image from the final Skyrim Fitting System v1.2.2 UI. Do not reuse older tutorial images or workflows.

## Required screenshots

1. `NEXUS_SCREENSHOT_01_URL` — full SFS window with the actor selector open; show the player and at least one manageable NPC or follower.
2. `NEXUS_SCREENSHOT_02_URL` — workbench overview with **Actual Equipment / Registered Appearance** and **Condition Settings / Action Settings** clearly visible.
3. `NEXUS_SCREENSHOT_03_URL` — Equipment search with **Preview Selected** enabled and a selected item ready to be registered.
4. `NEXUS_SCREENSHOT_04_URL` — real equipment and registered appearance cards showing their independent eye controls; include one useful lock or control tooltip if possible.
5. `NEXUS_SCREENSHOT_05_URL` — a registered appearance with Condition Settings plus the current custom-condition editor.
6. `NEXUS_SCREENSHOT_06_URL` — v1.2.2 Action Settings showing a condition-driven show or hide action for actual equipment or a registered appearance.
7. `NEXUS_SCREENSHOT_07_URL` — Kits tab with a saved kit selected, its contents or preview visible, and the apply workflow understandable from one frame.
8. `NEXUS_SCREENSHOT_08_URL` — Options showing the hotkey control, Hide-Protected Slots, and detected external-integration options.

## Capture rules

- Use the final v1.2.2 DLL, locale files, and release configuration.
- Capture at 16:9, preferably 1920×1080 or 2560×1440.
- Enlarge the SFS window until card labels and tooltips remain readable after Nexus page scaling.
- Use the English UI for the English BBCode description. Capture a separate Korean set only if the Korean page will embed images.
- Avoid unrelated debug overlays, console windows, personal character names, save names, or private mod-list details.
- Use a simple armor combination with obvious before/after differences.
- For conditional examples, choose an easy-to-understand rule such as Combat or Indoors.
- For multi-slot examples, show the complete multiline slot label instead of cropping it.

## Publish rules

- Upload each final image to a stable HTTPS location or attach it through Nexus/GitHub.
- For each screenshot, export a second image at 75% of the original pixel dimensions. Use the `_75` suffix, for example `01_actor_selection.webp` and `01_actor_selection_75.webp`.
- Replace `NEXUS_SCREENSHOT_01_URL` through `NEXUS_SCREENSHOT_08_URL` with the full-size public image URLs.
- Replace `NEXUS_SCREENSHOT_01_75_URL` through `NEXUS_SCREENSHOT_08_75_URL` with the 75%-size public image URLs. The thumbnail is displayed in the description and links to the full-size original.
- Keep `docs/nexus-description.bbcode` and `docs/nexus-description-en-PASTE-IN-BBCODE-MODE.txt` byte-identical.
- Preview the Nexus page and verify all images, headings, lists, line breaks, and links before saving the live description.
- Do not publish while any `NEXUS_SCREENSHOT_` placeholder remains.
