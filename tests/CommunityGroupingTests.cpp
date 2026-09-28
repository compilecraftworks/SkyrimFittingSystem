#include "kit_generator/CommunityGrouping.h"
#include <future>
#include <iostream>
#include <stdexcept>

namespace c = sfs::kit_generator::community;
struct Record { std::string pluginName; std::string editorID; };
void Require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
std::vector<Record> RecordsFor(std::string_view name) {
  std::vector<Record> result;
  for (const auto &ref : c::kCommunityReferences) {
    if (ref.name == name) {
      for (const auto &member : std::span(c::kCommunityMembers).subspan(ref.offset, ref.count))
        result.push_back({std::string(ref.plugin), std::string(member.editorID)});
    }
  }
  Require(!result.empty(), "Test reference missing");
  return result;
}
auto Match(const std::vector<Record> &records) {
  return c::MatchGroups(records, [] {});
}

int main() {
  try {
    auto longSet = RecordsFor("[DX] Dark Knight Armor (Long)");
    auto shortSet = RecordsFor("[DX] Dark Knight Armor (Short)");
    auto all = longSet;
    for (const auto &item : shortSet) {
      if (std::ranges::none_of(all, [&](const auto &r) { return r.editorID == item.editorID; })) all.push_back(item);
    }
    const auto matches = Match(all);
    Require(matches.size() == 2, "Long/short membership pools must remain separate inside a family");
    Require(matches[0].items.size() == longSet.size() && matches[1].items.size() == shortSet.size(),
            "Shared pieces must occur in both references");
    for (auto &item : all) {
      for (auto &ch : item.pluginName) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      for (auto &ch : item.editorID) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    std::ranges::reverse(all);
    Require(Match(all).size() == 2, "Case and source-order changes must not alter membership");
    auto wrongPlugin = longSet;
    for (auto &item : wrongPlugin) item.pluginName = "Unrelated.esp";
    Require(Match(wrongPlugin).empty(), "An EditorID must never cross a plugin boundary");
    auto commonOnly = longSet;
    std::erase_if(commonOnly, [&](const auto &item) {
      return std::ranges::none_of(shortSet, [&](const auto &r) { return r.editorID == item.editorID; });
    });
    Require(Match(commonOnly).empty(), "Shared accessories alone must not invent missing variants");
    Require(Match(std::vector<Record>{longSet.front()}).empty(), "One record cannot establish a kit");
    auto ambiguous = longSet;
    ambiguous.insert(ambiguous.end(), longSet.begin(), longSet.end());
    Require(Match(ambiguous).empty(), "Duplicate EditorIDs must not resolve arbitrarily");
    bool cancelled = false;
    try { c::MatchGroups(all, [] { throw std::runtime_error("cancelled"); }); }
    catch (const std::runtime_error &) { cancelled = true; }
    Require(cancelled, "Cancellation must propagate");
    std::vector<std::future<std::size_t>> tasks;
    for (int i = 0; i < 8; ++i)
      tasks.push_back(std::async(std::launch::async, [&] { return Match(all).size(); }));
    for (auto &task : tasks) Require(task.get() == 2, "Concurrent scans must produce identical groups");
    // Structural checks catch stale generated offsets and accidental inclusion
    // of base-game records even when those plugins are absent in the corpus.
    for (const auto &ref : c::kCommunityReferences) {
      Require(ref.count >= 2 && ref.offset + ref.count <= std::size(c::kCommunityMembers), "Invalid reference extent");
      Require(ref.plugin != "skyrim.esm" && ref.plugin != "dragonborn.esm" && ref.plugin != "dawnguard.esm",
              "The patch must not broaden base-game scan scope");
    }
    Require(c::FamilyName("[DX] Dark Knight Armor (Long)") == "[DX] Dark Knight Armor" &&
            c::FamilyName("[DX] Dark Knight Armor (Short)") == "[DX] Dark Knight Armor",
            "Curated length versions belong to one family");
    Require(c::FamilyName("[COCO] Goddess War 01 (SMP)") == "[COCO] Goddess War" &&
            c::FamilyName("[COCO] Goddess War 03") == "[COCO] Goddess War" &&
            c::FamilyName("[COCO] Scarlet Rose V2") == "[COCO] Scarlet Rose",
            "Curated numbered and physics versions must share a semantic title");
    Require(c::FamilyName("[TAWOBA] Iron Bikini IV") == "[TAWOBA] Iron Bikini" &&
            c::FamilyName("[TAWOBA] Steel Bikini IV") == "[TAWOBA] Steel Bikini" &&
            c::FamilyName("[DX] Christmas Bunny") != c::FamilyName("[DX] Love Bunny"),
            "Distinct semantic outfits in one plugin must not be combined");
    Require(c::FamilyName("[Maker] 01") == "[Maker] 01" &&
            c::FamilyName("[Maker] (Black)") == "[Maker] (Black)" &&
            c::FamilyName("[Maker] DK 0172") == "[Maker] DK 0172" &&
            c::FamilyName("[Maker] 2B Wedding Outfit 01") == "[Maker] 2B Wedding Outfit",
            "Author-only, long design numbers and embedded title numbers must survive");
    Require(c::FamilyName("[Immersive Armor] Dragonhide Heavy Robe") == "[Immersive Armor] Dragonhide Robe" &&
            c::FamilyName("[Immersive Armor] Dragonhide Unarmored Robe") == "[Immersive Armor] Dragonhide Robe" &&
            c::FamilyName("[Immersive Armor] Einherjar Brigandine Dark") ==
                c::FamilyName("[Immersive Armor] Einherjar Brigandine Light") &&
            c::FamilyName("[Maker] Black Rose") != c::FamilyName("[Maker] White Rose"),
            "Trailing weight/color annotations must combine without stripping semantic title prefixes");
    std::unordered_map<std::string, std::size_t> families;
    for (const auto& ref : c::kCommunityReferences) ++families[c::Key(ref.plugin, c::FamilyName(ref.name))];
    std::size_t combined = 0, versions = 0;
    for (const auto& [key, size] : families) if (size > 1) { ++combined; versions += size; }
    std::cout << "Community titles: " << std::size(c::kCommunityReferences) << " references -> "
              << families.size() << " families; " << versions << " versions in " << combined << " multi-version families\n";
    std::cout << "Community grouping identity, boundary, shared-piece, partial, ambiguity, cancellation and concurrency tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
