# Skyrim Fitting System v1.4.2

## Optional Kit Generator Integration

- Added a versioned runtime bridge for the separately distributed SFS Kit Generator option.
- The Kit Generator tab appears after Options only when a compatible option DLL is already loaded.
- Selecting a generated candidate now mirrors its registered appearances into the workbench as a read-only preview. Saved workbench rows, inventory, and actual equipment are not modified.
- The temporary generator preview includes protected-slot and SOS/TNG genital candidate pieces so the workbench does not appear incomplete. Normal registration, application, saving, and visibility filters remain unchanged; pieces that occupy the same Skyrim display slot still obey the existing engine conflict rule.
- Leaving the tab, cancelling generation, or closing SFS clears the generated preview through the existing preview path.
- SFSCore does not link to or forcibly load the option DLL. Without the option installed, the base SFS UI and behavior remain unchanged.

## Updating

Completely remove the previous SFS mod folder, install the v1.4.2 ZIP as a new mod, and enable `SkyrimFittingSystem-VirtualTokens.esl`. Install SFS Kit Generator separately only when wanted; its package does not replace `SFSCore.dll`.
