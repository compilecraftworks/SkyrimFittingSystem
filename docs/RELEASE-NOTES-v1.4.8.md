# Skyrim Fitting System v1.4.8

## Actor selector reliability

- Fixed the workbench actor selector by enumerating the player's current
  loaded cell before its existing 4096-unit world-range pass.
- Both enumerations use the same actor-local eligibility test, radius,
  duplicate suppression, nearest-first order, and 32-actor cap. Saved rows,
  conditional rules, registered appearances, and actual equipment are not
  changed.

## Build dependency

- Updated the bundled build dependency to CommonLibSSE-NG v6.7.0
  (`3d81614617910e7f34b33d8750881811b5e36445`).
- The runtime remains SE/AE-only; this dependency update does not add Skyrim VR
  support.
