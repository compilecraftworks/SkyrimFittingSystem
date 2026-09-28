#pragma once

namespace sfs::ui::catalog {
struct FavoriteFilters {
  bool gear{false};
  bool outfits{false};
  bool kits{false};

  template <class Json> void Load(const Json &settings) {
    const auto read = [&](const char *key, bool fallback) {
      const auto it = settings.find(key);
      return it != settings.end() && it->is_boolean()
                 ? it->template get<bool>() : fallback;
    };
    // Preserve the old shared preference once; explicit per-tab values win.
    const bool legacy = read("catalogFavoritesOnly", false);
    gear = read("catalogGearFavoritesOnly", legacy);
    outfits = read("catalogOutfitFavoritesOnly", legacy);
    kits = read("catalogKitFavoritesOnly", legacy);
  }

  template <class Json> void Save(Json &settings) const {
    settings.erase("catalogFavoritesOnly");
    settings["catalogGearFavoritesOnly"] = gear;
    settings["catalogOutfitFavoritesOnly"] = outfits;
    settings["catalogKitFavoritesOnly"] = kits;
  }
};
} // namespace sfs::ui::catalog
