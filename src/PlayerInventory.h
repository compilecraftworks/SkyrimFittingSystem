#pragma once

#include <unordered_set>

namespace sfs::player_inventory {
[[nodiscard]] std::unordered_set<RE::FormID> GetInventoryArmorFormIDs();
} // namespace sfs::player_inventory
