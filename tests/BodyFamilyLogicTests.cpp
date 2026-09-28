#include "catalog/BodyFamily.h"
#include "ui/catalog/BodyFamilyFilter.h"
#include "kit_generator/BodyFilter.h"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using sfs::body_family::Bit;
using sfs::body_family::DetectText;
using sfs::body_family::Family;
using sfs::body_family::Mask;
using sfs::body_family::Matches;
using sfs::body_family::MergeCatalogMasks;
using sfs::body_family::Sex;

void Require(const bool a_condition, const std::string_view a_message) {
  if (!a_condition) {
    throw std::runtime_error(std::string(a_message));
  }
}

void TestFemaleAliases() {
  for (const auto label : {"CBBE", "3BA", "3BBB", "CBBE 3BA",
                           "CBBE/3BA", "CBBE3BA", "CBBE_3BBB",
                           "CBBE 3BA 3BBB"}) {
    Require(DetectText(label, Sex::Female) == Bit(Family::Cbbe),
            "Standalone and combined CBBE/3BA/3BBB labels must share CBBE");
  }
  Require(DetectText("CBBE 3BA V2 Armor", Sex::Female) ==
              Bit(Family::Cbbe),
          "CBBE/3BA aliases must share the CBBE family");
  Require(DetectText("3BBB", Sex::Female) == Bit(Family::Cbbe),
          "The custom patch maps an unqualified 3BBB label to CBBE");
  Require(DetectText("UBE 3BBB conversion", Sex::Female) == Bit(Family::Ube),
          "An explicit UBE conversion must remain UBE");
  Require(DetectText("BHUNP 3BBB conversion", Sex::Female) ==
              Bit(Family::Unp),
          "BHUNP and its 3BBB variants must share the UNP family");
  Require(DetectText("UNP UNPB UUNP", Sex::Female) == Bit(Family::Unp),
          "UNP aliases must collapse to one family");
  Require(DetectText("UBE 2.0 outfit", Sex::Female) == Bit(Family::Ube),
          "UBE must be detected as its own family");
  Require(DetectText("cube map armor", Sex::Female) == 0,
          "UBE must not match an unrelated word such as cube");
}

void TestMaleAliases() {
  Require(DetectText("HIMBO V5 refit", Sex::Male) == Bit(Family::Himbo),
          "HIMBO aliases must map to HIMBO");
  Require(DetectText("SAM Light armor", Sex::Male) == Bit(Family::Sam),
          "SAM Light must map to SAM");
  Require(DetectText("samurai armor", Sex::Male) == 0,
          "SAM must not match samurai");
}

void TestCatalogMerge() {
  using namespace sfs::kit_generator;
  using Filter = sfs::ui::catalog::BodyFamilyFilter;
  ArmorRecord cbbe, vanilla, ube;
  cbbe.bodyFamilyMask = Bit(Family::Cbbe);
  vanilla.bodyFamilyMask = Bit(Family::FemaleVanilla);
  ube.bodyFamilyMask = Bit(Family::Ube);
  const auto pluginFilters = PluginBodyFilters({cbbe, vanilla, ube});
  for (const auto filter : {Filter::All, Filter::Cbbe, Filter::Ube, Filter::Vanilla})
    Require((pluginFilters & (1U << static_cast<unsigned>(filter))) != 0,
            "ESP body filter must include each contained family, including standalone Vanilla armor");
  Require((pluginFilters & (1U << static_cast<unsigned>(Filter::Unp))) == 0,
          "ESP body filter cannot invent a family");
  GeneratedKit kit;
  kit.candidates = {KitCandidate{.items = {cbbe, vanilla}}, KitCandidate{.items = {ube}}};
  Require(MatchesKitBodyFilter(kit, Filter::Cbbe) &&
          !MatchesKitBodyFilter(kit, Filter::Vanilla) &&
          !MatchesKitBodyFilter(kit, Filter::Ube),
          "Kit filter uses the selected candidate and main catalog merge rules");
  kit.selectedCandidate = 1;
  Require(MatchesKitBodyFilter(kit, Filter::Ube) &&
          !MatchesKitBodyFilter(kit, Filter::Cbbe) && kit.candidates.size() == 2 &&
          kit.candidates[0].items.size() == 2,
          "Candidate changes update body filter without removing/modifying any items");
  kit.candidates.clear();
  Require(MatchesKitBodyFilter(kit, Filter::All), "Unknown results remain available under All");
  const Mask femaleVanilla = Bit(Family::FemaleVanilla);
  const Mask maleVanilla = Bit(Family::MaleVanilla);
  const auto merged = MergeCatalogMasks(
      femaleVanilla | maleVanilla,
      Bit(Family::Cbbe) | Bit(Family::Himbo));
  Require((merged & femaleVanilla) == 0 &&
              (merged & Bit(Family::Cbbe)) != 0,
          "A CBBE body component must dominate female Vanilla fallback pieces");
  Require((merged & maleVanilla) == 0 &&
              (merged & Bit(Family::Himbo)) != 0,
          "A HIMBO body component must dominate male Vanilla fallback pieces");
}

void TestVanillaFallbackVisibility() {
  Require(Matches(Bit(Family::Ube), 0) &&
              Matches(Bit(Family::Cbbe), 0) &&
              Matches(Bit(Family::Unp), 0),
          "Unknown or ambiguous actors must leave the catalog unfiltered");
  Require(Matches(Bit(Family::FemaleVanilla), Bit(Family::Cbbe)),
          "Unlabelled female/Vanilla entries must remain visible to CBBE actors");
  Require(Matches(Bit(Family::MaleVanilla), Bit(Family::Himbo)),
          "Unlabelled male/Vanilla entries must remain visible to HIMBO actors");
  Require(!Matches(Bit(Family::Unp), Bit(Family::Cbbe)),
          "An explicitly UNP entry must not appear for a CBBE actor");
}
void TestManualCatalogFilter() {
  using sfs::ui::catalog::BodyFamilyFilter;
  using sfs::ui::catalog::BodyFamilyFilterFromIndex;
  using sfs::ui::catalog::MatchesBodyFamilyFilter;
  const auto cbbe = Bit(Family::Cbbe);
  const auto ube = Bit(Family::Ube);
  const auto femaleVanilla = Bit(Family::FemaleVanilla);
  const auto maleVanilla = Bit(Family::MaleVanilla);
  for (Mask mask = 0; mask <= sfs::body_family::kAllFamilies; ++mask) {
    Require(MatchesBodyFamilyFilter(mask, BodyFamilyFilter::All),
            "All must include every category and unknown entries");
  }
  const std::array families{Family::Cbbe, Family::Unp, Family::Ube, Family::Himbo, Family::Sam};
  for (std::size_t index = 0; index < families.size(); ++index) {
    const auto filter = BodyFamilyFilterFromIndex(static_cast<int>(index) + 1);
    for (const auto candidate : families) {
      Require(MatchesBodyFamilyFilter(Bit(candidate), filter) == (candidate == families[index]),
              "Explicit categories must not leak into other selections");
    }
    Require(!MatchesBodyFamilyFilter(0, filter) &&
                !MatchesBodyFamilyFilter(femaleVanilla | maleVanilla, filter),
            "Manual family selection must not add generic Vanilla fallback pieces");
  }
  Require(MatchesBodyFamilyFilter(cbbe | ube, BodyFamilyFilter::Cbbe) &&
              MatchesBodyFamilyFilter(cbbe | ube, BodyFamilyFilter::Ube) &&
              !MatchesBodyFamilyFilter(cbbe | ube, BodyFamilyFilter::Unp),
          "Mixed kits/outfits must be visible under each detected family only");
  Require(MatchesBodyFamilyFilter(femaleVanilla, BodyFamilyFilter::Vanilla) &&
              MatchesBodyFamilyFilter(maleVanilla, BodyFamilyFilter::Vanilla) &&
              !MatchesBodyFamilyFilter(cbbe | maleVanilla, BodyFamilyFilter::Vanilla) &&
              !MatchesBodyFamilyFilter(0, BodyFamilyFilter::Vanilla),
          "Vanilla must include both sexes without misclassifying custom sets");
  Require(MatchesBodyFamilyFilter(DetectText("3BBB", Sex::Female), BodyFamilyFilter::Cbbe),
          "3BBB must be included in the CBBE/3BA dropdown choice");
  for (const auto index : {-100, -1, 7, 256}) {
    Require(BodyFamilyFilterFromIndex(index) == BodyFamilyFilter::All,
            "Invalid persisted indices must safely reset to All");
  }
}
} // namespace

int main() {
  try {
    TestFemaleAliases();
    TestMaleAliases();
    TestCatalogMerge();
    TestVanillaFallbackVisibility();
    TestManualCatalogFilter();
    std::cout << "BodyFamilyLogicTests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "BodyFamilyLogicTests failed: " << error.what() << '\n';
    return 1;
  }
}
