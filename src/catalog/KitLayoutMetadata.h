#pragma once

#include "EquipmentCatalog.h"

#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string_view>

namespace sfs::catalog {
inline constexpr std::string_view kSfsKitMetadataKey = "SkyrimFittingSystem";

[[nodiscard]] nlohmann::json
SerializeKitLayout(const KitEntry::Layout &a_layout);
[[nodiscard]] std::optional<KitEntry::Layout>
ParseKitLayout(const nlohmann::json &a_json);
} // namespace sfs::catalog
