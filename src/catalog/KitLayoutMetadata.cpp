#include "catalog/KitLayoutMetadata.h"

#include <nlohmann/json.hpp>

namespace sfs::catalog {
namespace {
std::string ToTargetKindText(const KitEntry::LayoutTargetKind a_kind) {
  return a_kind == KitEntry::LayoutTargetKind::Slot ? "slot" : "item";
}

std::optional<KitEntry::LayoutTargetKind>
ParseTargetKind(const nlohmann::json &a_json) {
  if (!a_json.is_string()) {
    return std::nullopt;
  }

  const auto value = a_json.get<std::string>();
  if (value == "item") {
    return KitEntry::LayoutTargetKind::Item;
  }
  if (value == "slot") {
    return KitEntry::LayoutTargetKind::Slot;
  }
  return std::nullopt;
}
} // namespace

nlohmann::json SerializeKitLayout(const KitEntry::Layout &a_layout) {
  nlohmann::json rows = nlohmann::json::array();
  for (const auto &row : a_layout.rows) {
    nlohmann::json rowJson{
        {"targetKind", ToTargetKindText(row.targetKind)},
        {"targetSlotMask", row.targetSlotMask},
        {"overrideIdentifiers", row.overrideIdentifiers},
        {"hideEquipped", row.hideEquipped}};
    if (row.actualVisibilitySlotMask.has_value()) {
      rowJson["actualVisibilitySlotMask"] =
          *row.actualVisibilitySlotMask;
      rowJson["hiddenActualSlotMask"] = row.hiddenActualSlotMask;
    }
    rows.push_back(std::move(rowJson));
  }

  return nlohmann::json{{"layoutRows", std::move(rows)}};
}

std::optional<KitEntry::Layout> ParseKitLayout(const nlohmann::json &a_json) {
  if (!a_json.is_object()) {
    return std::nullopt;
  }

  const auto rowsIt = a_json.find("layoutRows");
  if (rowsIt == a_json.end() || !rowsIt->is_array()) {
    return std::nullopt;
  }

  KitEntry::Layout layout;
  for (const auto &rowJson : *rowsIt) {
    if (!rowJson.is_object()) {
      continue;
    }

    const auto kind = ParseTargetKind(rowJson.value("targetKind", ""));
    const auto targetSlotMask = rowJson.value("targetSlotMask", 0ULL);
    if (!kind.has_value() || targetSlotMask == 0) {
      continue;
    }

    KitEntry::LayoutRow row;
    row.targetKind = *kind;
    row.targetSlotMask = targetSlotMask;
    row.hideEquipped = rowJson.value("hideEquipped", false);
    if (const auto visibilityIt =
            rowJson.find("actualVisibilitySlotMask");
        visibilityIt != rowJson.end() &&
        visibilityIt->is_number_unsigned()) {
      row.actualVisibilitySlotMask =
          visibilityIt->get<std::uint64_t>() & targetSlotMask;
      row.hiddenActualSlotMask =
          rowJson.value("hiddenActualSlotMask", std::uint64_t{0}) &
          *row.actualVisibilitySlotMask;
    } else if (row.hideEquipped) {
      row.actualVisibilitySlotMask = targetSlotMask;
      row.hiddenActualSlotMask = targetSlotMask;
    }

    const auto overrideIt = rowJson.find("overrideIdentifiers");
    if (overrideIt != rowJson.end() && overrideIt->is_array()) {
      for (const auto &identifier : *overrideIt) {
        if (identifier.is_string()) {
          const auto value = identifier.get<std::string>();
          if (!value.empty()) {
            row.overrideIdentifiers.push_back(value);
          }
        }
      }
    }

    if (row.overrideIdentifiers.empty()) {
      continue;
    }

    layout.rows.push_back(std::move(row));
  }

  if (layout.rows.empty()) {
    return std::nullopt;
  }

  return layout;
}
} // namespace sfs::catalog
