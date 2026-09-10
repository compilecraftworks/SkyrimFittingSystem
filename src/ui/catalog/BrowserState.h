#pragma once

#include "imgui.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace sfs::ui::catalog {
enum class BrowserTab {
  Gear,
  Outfits,
  Conditions,
  Kits,
  KitGenerator,
  Options
};

enum class RefreshMode : std::uint8_t { Full, KitsOnly };

struct BrowserState {
  bool initialized{false};
  bool refreshQueued{false};
  RefreshMode queuedRefreshMode{RefreshMode::Full};
  BrowserTab activeTab{BrowserTab::Kits};
  int gearPluginIndex{0};
  int outfitPluginIndex{0};
  int kitCollectionIndex{0};
  std::vector<bool> selectedSlotFilters;
  bool previewSelected{true};
  bool favoritesOnly{false};
  bool inventoryOnly{false};
  bool hideUnnamedGear{true};
  std::string selectedKey;
  std::vector<std::string> selectedGearKeys;
  std::string pendingSelectionAfterRefresh;
  std::unordered_set<std::string> favoriteKeys;
  ImGuiTextFilter gearSearch;
  ImGuiTextFilter outfitSearch;
  ImGuiTextFilter kitSearch;
  ImGuiTextFilter gearPluginFilter;
  ImGuiTextFilter outfitPluginFilter;
  ImGuiTextFilter kitCollectionFilter;
};
} // namespace sfs::ui::catalog
