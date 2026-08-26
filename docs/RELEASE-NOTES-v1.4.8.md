# Skyrim Fitting System v1.4.8

## Actor-aware menu presentation

- The optional left/right character presentation now follows the actor selected
  in the workbench instead of always framing the player.
- Opening SFS with the crosshair-NPC option enabled frames the targeted NPC from
  the first menu frame; changing the actor dropdown retargets the camera
  immediately.
- SFS does not move the actor in the world. Camera target, temporary facing,
  player-only pitch, and camera settings are restored when the actor changes or
  the menu closes.

## Actor selector reliability

- Fixed the CommonLibSSE-NG SE/AE virtual layout used by the workbench actor
  eligibility check, restoring `Actor::IsDead()` to its correct engine slot.
- Reverted the diagnostic actor-discovery experiments. Actor collection is
  unchanged from v1.4.5, including its eligibility rule, nearby range,
  duplicate suppression, nearest-first order, and 32-actor cap.

## Build dependency

- Updated the bundled build dependency to CommonLibSSE-NG v6.7.0
  (`3d81614617910e7f34b33d8750881811b5e36445`).
- Documented the narrow local SE/AE vtable correction included with that exact
  upstream revision.
- The runtime remains SE/AE-only; this dependency update does not add Skyrim VR
  support.
- Refreshed the reproducible build baseline to XMake v3.1.0, Dear ImGui
  v1.92.9b, and nlohmann/json v3.12.0 with exact revisions and checksums
  documented in `DEPENDENCIES.md`.
