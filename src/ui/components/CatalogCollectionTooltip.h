#pragma once

#include "EquipmentCatalog.h"

#include <string>
#include <string_view>
#include <vector>

namespace sfs::ui::components {
struct CatalogTooltipMetaRow {
  const char *icon{""};
  std::string label;
  std::string value;
};

void DrawCatalogCollectionTooltip(
    std::string_view a_id, bool a_hoveredSource, std::string_view a_title,
    const std::vector<CatalogTooltipMetaRow> &a_metaRows,
    const std::vector<sfs::CatalogCollectionItemNode> &a_items);
} // namespace sfs::ui::components
