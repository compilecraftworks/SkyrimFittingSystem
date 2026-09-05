#pragma once

#include "EquipmentCatalog.h"

#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

namespace sfs::catalog {
using KitArmorLookup =
    std::unordered_map<std::string, RE::TESObjectARMO *>;

[[nodiscard]] const std::filesystem::path &GetPrimaryKitPath();
[[nodiscard]] const std::vector<std::filesystem::path> &GetKitSearchPaths();

void IndexKitArmorForm(RE::TESObjectARMO *a_armor,
                       KitArmorLookup &a_lookup);

[[nodiscard]] std::optional<GearEntry> BuildGearEntry(
    RE::TESObjectARMO *a_armor,
    std::unordered_map<RE::FormID, ArmorMetadata> &a_armorMetadataCache);
[[nodiscard]] std::optional<OutfitEntry> BuildOutfitEntry(
    const RE::BGSOutfit *a_outfit,
    std::unordered_map<RE::FormID, ResolvedReferenceCollection>
        &a_leveledListCache,
    std::unordered_map<RE::FormID, ArmorMetadata> &a_armorMetadataCache);
[[nodiscard]] std::optional<KitEntry> BuildKitEntry(
    const std::filesystem::path &a_rootPath,
    const std::filesystem::path &a_path,
    std::unordered_map<RE::FormID, ArmorMetadata> &a_armorMetadataCache,
    const KitArmorLookup &a_armorLookup);
} // namespace sfs::catalog
