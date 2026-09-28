#pragma once

#include "imgui.h"
#include "ui/catalog/BodyFamilyFilter.h"
#include "ui/catalog/FilterFocus.h"
#include "ui/catalog/FavoriteFilters.h"

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
  BodyFamilyFilter bodyFamilyFilter{BodyFamilyFilter::All};
  FilterFocus filterFocus;
  FavoriteFilters favoriteFilters;
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

  bool FavoritesOnlyFor(BrowserTab tab) const {
    if (tab == BrowserTab::Gear) return favoriteFilters.gear;
    if (tab == BrowserTab::Outfits) return favoriteFilters.outfits;
    return tab == BrowserTab::Kits && favoriteFilters.kits;
  }

  bool &ActiveFavoritesOnly() {
    if (activeTab == BrowserTab::Gear) return favoriteFilters.gear;
    if (activeTab == BrowserTab::Outfits) return favoriteFilters.outfits;
    return favoriteFilters.kits; // Used only by the three catalog tabs.
  }
};
} // namespace sfs::ui::catalog
