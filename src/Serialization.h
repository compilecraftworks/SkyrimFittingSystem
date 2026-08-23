#pragma once

#include <SKSE/SKSE.h>

namespace sfs::serialization {
constexpr std::uint32_t kID = 'SFTS';

void SaveCallback(SKSE::SerializationInterface *a_skse);
void LoadCallback(SKSE::SerializationInterface *a_skse);
void RevertCallback(SKSE::SerializationInterface *a_skse);
} // namespace sfs::serialization
