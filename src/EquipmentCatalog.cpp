#include "EquipmentCatalog.h"

#include "Utf8Path.h"
#include "catalog/EntryBuilders.h"

#include <cctype>
#include <filesystem>
#include <unordered_set>

namespace {
template <class Entries, class Projection>
std::vector<std::string> BuildSortedUniqueOptions(const Entries &a_entries,
                                                  Projection a_projection) {
  std::vector<std::string> values;
  values.reserve(a_entries.size());

  for (const auto &entry : a_entries) {
    const auto &value = a_projection(entry);
    if (!value.empty()) {
      values.push_back(value);
    }
  }

  std::ranges::sort(values);
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

const std::vector<RE::FormID> kEmptyFormIDs;
const std::vector<sfs::CatalogCollectionItemNode> kEmptyItemTree;
const std::vector<std::string> kEmptyStrings;
const std::string kEmptyText;

struct KitPathEntry {
  std::filesystem::path rootPath;
  std::filesystem::path filePath;
};

std::string NormalizeKitPathKey(const std::filesystem::path &a_path) {
  auto key = sfs::utf8::PathToUtf8GenericString(a_path);
  std::ranges::transform(key, key.begin(), [](const unsigned char a_ch) {
    return static_cast<char>(std::tolower(a_ch));
  });
  return key;
}
} // namespace

namespace sfs {
struct EquipmentCatalog::RefreshState {
  RefreshMode mode{RefreshMode::Full};
  std::vector<RE::TESObjectARMO *> armors;
  std::vector<RE::BGSOutfit *> outfits;
  std::vector<KitPathEntry> kitPaths;
  IncrementalLoader loader;
};

EquipmentCatalog &EquipmentCatalog::Get() {
  static EquipmentCatalog singleton;
  return singleton;
}

EquipmentCatalog::EquipmentCatalog()
    : source_("Catalog not loaded"), revision_("cache-empty") {}

const std::vector<RE::FormID> &OutfitEntry::GetArmorFormIDs() const {
  return resolved ? resolved->armorFormIDs : kEmptyFormIDs;
}

const std::vector<CatalogCollectionItemNode> &OutfitEntry::GetItemTree() const {
  return (resolved && resolved->itemTree) ? *resolved->itemTree
                                          : kEmptyItemTree;
}

const std::vector<std::string> &OutfitEntry::GetPieces() const {
  return resolved ? resolved->pieces : kEmptyStrings;
}

std::uint64_t OutfitEntry::GetSlotMask() const {
  return resolved ? resolved->slotMask : 0;
}

std::string_view OutfitEntry::GetPiecesText() const {
  return resolved ? std::string_view(resolved->piecesText)
                  : std::string_view(kEmptyText);
}

const std::vector<RE::FormID> &KitEntry::GetArmorFormIDs() const {
  return resolved ? resolved->armorFormIDs : kEmptyFormIDs;
}

const std::vector<CatalogCollectionItemNode> &KitEntry::GetItemTree() const {
  return (resolved && resolved->itemTree) ? *resolved->itemTree
                                          : kEmptyItemTree;
}

const std::vector<std::string> &KitEntry::GetPieces() const {
  return resolved ? resolved->pieces : kEmptyStrings;
}

std::uint64_t KitEntry::GetSlotMask() const {
  return resolved ? resolved->slotMask : 0;
}

std::string_view KitEntry::GetPiecesText() const {
  return resolved ? std::string_view(resolved->piecesText)
                  : std::string_view(kEmptyText);
}

const GearEntry *EquipmentCatalog::FindGear(const RE::FormID a_formID) const {
  const auto it = gearIndexByFormID_.find(a_formID);
  if (it == gearIndexByFormID_.end() || it->second >= gear_.size()) {
    return nullptr;
  }

  return std::addressof(gear_[it->second]);
}

const OutfitEntry *
EquipmentCatalog::FindOutfit(const RE::FormID a_formID) const {
  const auto it = outfitIndexByFormID_.find(a_formID);
  if (it == outfitIndexByFormID_.end() || it->second >= outfits_.size()) {
    return nullptr;
  }

  return std::addressof(outfits_[it->second]);
}

std::vector<RE::FormID>
EquipmentCatalog::ResolveArmorFormIDs(const RE::FormID a_formID) const {
  if (const auto *armor =
          RE::TESForm::LookupByID<RE::TESObjectARMO>(a_formID)) {
    return {armor->GetFormID()};
  }

  if (const auto *outfit = FindOutfit(a_formID)) {
    return outfit->GetArmorFormIDs();
  }

  const auto it = leveledListCache_.find(a_formID);
  if (it != leveledListCache_.end()) {
    return it->second.armorFormIDs;
  }

  return {};
}

std::vector<RE::FormID> EquipmentCatalog::ResolveArmorFormIDs(
    const std::vector<RE::FormID> &a_formIDs) const {
  std::vector<RE::FormID> resolved;
  std::unordered_set<RE::FormID> seenForms;

  for (const auto formID : a_formIDs) {
    for (const auto armorFormID : ResolveArmorFormIDs(formID)) {
      if (seenForms.insert(armorFormID).second) {
        resolved.push_back(armorFormID);
      }
    }
  }

  return resolved;
}

void EquipmentCatalog::StartRefreshFromGame(const RefreshMode a_mode) {
  if (a_mode == RefreshMode::Full) {
    gear_.clear();
    outfits_.clear();
    kits_.clear();
    armorMetadataCache_.clear();
    kitArmorByPluginEditorID_.clear();
    gearPlugins_.clear();
    gearSlots_.clear();
    outfitPlugins_.clear();
    kitCollections_.clear();
    leveledListCache_.clear();
    gearIndexByFormID_.clear();
    outfitIndexByFormID_.clear();
  } else {
    kits_.clear();
    kitCollections_.clear();
  }

  refreshState_ = std::make_unique<RefreshState>();
  auto &state = *refreshState_;
  state.mode = a_mode;

  if (a_mode == RefreshMode::Full) {
    auto *dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
      refreshState_.reset();
      source_ = "TESDataHandler unavailable";
      revision_ = "cache-error";
      logger::error(
          "Failed to refresh equipment catalog: TESDataHandler was null");
      return;
    }

    for (auto *armor : dataHandler->GetFormArray<RE::TESObjectARMO>()) {
      state.armors.push_back(armor);
      sfs::catalog::IndexKitArmorForm(armor,
                                      kitArmorByPluginEditorID_);
    }
    for (auto *outfit : dataHandler->GetFormArray<RE::BGSOutfit>()) {
      state.outfits.push_back(outfit);
    }
  }
  std::unordered_set<std::string> discoveredKitKeys;
  for (const auto &kitRoot : sfs::catalog::GetKitSearchPaths()) {
    std::error_code pathError;
    if (!std::filesystem::is_directory(kitRoot, pathError) || pathError) {
      continue;
    }

    std::filesystem::recursive_directory_iterator iterator(
        kitRoot, std::filesystem::directory_options::skip_permission_denied,
        pathError);
    const std::filesystem::recursive_directory_iterator end;
    while (!pathError && iterator != end) {
      const auto entry = *iterator;
      std::error_code entryError;
      if (entry.is_regular_file(entryError) && !entryError) {
        try {
          const auto relativePath =
              entry.path().lexically_relative(kitRoot);
          if (discoveredKitKeys.insert(NormalizeKitPathKey(relativePath))
                  .second) {
            state.kitPaths.push_back({kitRoot, entry.path()});
          }
        } catch (const std::exception &exception) {
          logger::warn("Skipped kit path that could not be converted to UTF-8: "
                       "{}",
                       exception.what());
        }
      }
      iterator.increment(pathError);
    }
    if (pathError) {
      logger::warn("Stopped scanning an unreadable fitting-kit directory: {}",
                   pathError.message());
    }
  }

  std::vector<IncrementalLoader::Phase> phases;
  if (a_mode == RefreshMode::Full) {
    phases.push_back({"Building gear catalog...", state.armors.size(),
                      [this, &state](const std::size_t a_index) {
                        if (auto entry = sfs::catalog::BuildGearEntry(
                                state.armors[a_index], armorMetadataCache_)) {
                          gearIndexByFormID_.emplace(entry->formID,
                                                     gear_.size());
                          gear_.push_back(std::move(*entry));
                        }
                      }});
    phases.push_back(
        {"Building outfit catalog...", state.outfits.size(),
         [this, &state](const std::size_t a_index) {
           auto entry = sfs::catalog::BuildOutfitEntry(
               state.outfits[a_index], leveledListCache_, armorMetadataCache_);
           if (entry) {
             outfitIndexByFormID_.emplace(entry->formID, outfits_.size());
             outfits_.push_back(std::move(*entry));
           }
         }});
  }

    phases.push_back({"Building kit catalog...", state.kitPaths.size(),
                    [this, &state](const std::size_t a_index) {
                      try {
                        if (auto entry = sfs::catalog::BuildKitEntry(
                                state.kitPaths[a_index].rootPath,
                                state.kitPaths[a_index].filePath,
                                armorMetadataCache_,
                                kitArmorByPluginEditorID_)) {
                          kits_.push_back(std::move(*entry));
                        }
                      } catch (const std::exception &exception) {
                        logger::warn("Skipped invalid fitting-kit file: {}",
                                     exception.what());
                      } catch (...) {
                        logger::warn("Skipped invalid fitting-kit file");
                      }
                    }});
  phases.push_back(
      {"Finalizing catalog...", 1, [this, &state](const std::size_t) {
         RebuildDerivedData();
         source_ = state.mode == RefreshMode::Full
                       ? "Runtime cache from TESDataHandler"
                       : "Runtime cache from TESDataHandler with "
                         "refreshed kit cache";
         revision_ = "armor=" + std::to_string(gear_.size()) +
                     ", outfits=" + std::to_string(outfits_.size()) +
                     ", kits=" + std::to_string(kits_.size());
         if (state.mode == RefreshMode::Full) {
           logger::info("Equipment catalog refreshed: {} gear "
                        "entries, {} outfits, {} kits",
                        gear_.size(), outfits_.size(), kits_.size());
         } else {
           logger::info("Equipment catalog kits refreshed: {} kits",
                        kits_.size());
         }
       }});

  source_ = a_mode == RefreshMode::Full
                ? "Refreshing runtime cache from TESDataHandler"
                : "Refreshing kit cache from disk";
  revision_ = "cache-refreshing";
  state.loader.Start(std::move(phases));
}

bool EquipmentCatalog::ContinueRefreshFromGame(
    const double a_maxMillisecondsPerTick) {
  if (!refreshState_) {
    return false;
  }

  auto &state = *refreshState_;
  if (!state.loader.Continue(a_maxMillisecondsPerTick)) {
    refreshState_.reset();
    return false;
  }
  return true;
}

bool EquipmentCatalog::IsRefreshing() const {
  return static_cast<bool>(refreshState_);
}

float EquipmentCatalog::GetRefreshProgress() const {
  if (!refreshState_) {
    return 1.0f;
  }
  return refreshState_->loader.GetProgress();
}

std::string_view EquipmentCatalog::GetRefreshStatus() const {
  if (!refreshState_) {
    return source_;
  }
  return refreshState_->loader.GetStatus();
}

void EquipmentCatalog::RebuildDerivedData() {
  gearPlugins_ = BuildSortedUniqueOptions(
      gear_, [](const GearEntry &a_entry) -> const std::string & {
        return a_entry.plugin;
      });
  gearSlots_ = BuildSortedUniqueOptions(
      gear_, [](const GearEntry &a_entry) -> const std::string & {
        return a_entry.slot;
      });
  outfitPlugins_ = BuildSortedUniqueOptions(
      outfits_, [](const OutfitEntry &a_entry) -> const std::string & {
        return a_entry.plugin;
      });
  kitCollections_ = BuildSortedUniqueOptions(
      kits_, [](const KitEntry &a_entry) -> const std::string & {
        return a_entry.collection;
      });
}
} // namespace sfs
