# Skyrim Fitting System v1.4.9

## Runtime layout safety

- Replaced shared compile-time hook assumptions with explicit, verified Skyrim
  SE 1.5.97 and Skyrim AE runtime profiles.
- Core UI/input and native armor hook locations now resolve from the active
  runtime profile and validate their instruction form before patching. Unknown
  runtime layouts fail closed instead of installing a hook at an unverified
  address.
- Centralized the Papyrus VM/native-function slot contract and added boundary
  tests for the first supported SE/AE runtimes, the AE 1.6.629 transition, the
  current Steam/GOG runtimes, and rejected unknown/VR layouts.

## Immersive Equipment Displays compatibility

- Fixed the IED custom-skin compatibility boundary at the actual SE and AE
  `VisitWornItems` call sites.
- When IED already owns that call site, SFS no longer passes its filtering
  visitor to IED's concrete visitor hook. Filtered work uses the original
  engine visitor path and then requests a safe IED actor refresh.
- This prevents the equipment-rebuild crash reproducible during cell or
  scenario transitions such as entering Helgen Keep through Alternative
  Perspective while preserving actor-local hidden-equipment state.

## Menu character framing

- Moved both left and right character presentation slightly farther outward
  with symmetric offsets. Facing angle and vertical position are unchanged.

Mod-Configured Slot Linking, Automatic Vanilla-Slot Linking, Direct Slot
Editing, external strip/redress transactions, conditions, Kit Generator,
Grid Inventory Costume synchronization, and DAVE/DAV/native actor-local state
retain their existing behavior.
