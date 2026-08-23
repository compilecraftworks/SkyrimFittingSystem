#pragma once

#include <SKSE/SKSE.h>

namespace sfs::native::grid_inventory {

// Registers the second, unfiltered SKSE listener required by Grid Inventory's
// public Costume-state broadcast.  It deliberately handles only Grid's 4CC
// message type and leaves SFS's normal "SKSE" lifecycle listener untouched.
void RegisterMessageListener(SKSE::MessagingInterface *a_messaging);

// Called from SFS lifecycle transitions.  Pending broadcasts are applied only
// after game data is available; changing saves discards the old save's state.
void SetGameDataLoaded(bool a_loaded);

// Runs from SFS's existing safe per-frame processing point.  It is idle unless
// a Grid Inventory Costume transition has been received.
void ProcessPendingCostumeState();

} // namespace sfs::native::grid_inventory
