#pragma once

#include <SKSE/SKSE.h>

namespace sfs::serialization {
constexpr std::uint32_t kID = 'SFTS';

// Idempotent save-boundary reset. SKSE normally sends kPreLoadGame before the
// serialization callback, but the callback calls this again so an interrupted
// or reordered load can never replay queued work from the previous world.
void PrepareForLoadTransition();
void SaveCallback(SKSE::SerializationInterface *a_skse);
void LoadCallback(SKSE::SerializationInterface *a_skse);
void RevertCallback(SKSE::SerializationInterface *a_skse);
} // namespace sfs::serialization
