#include "catalog/BodyFamily.h"

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
  Require(DetectText("CBBE 3BA V2 Armor", Sex::Female) ==
              Bit(Family::Cbbe),
          "CBBE/3BA aliases must share the CBBE family");
  Require(DetectText("3BBB", Sex::Female) == 0,
          "3BBB alone must remain ambiguous");
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
} // namespace

int main() {
  try {
    TestFemaleAliases();
    TestMaleAliases();
    TestCatalogMerge();
    TestVanillaFallbackVisibility();
    std::cout << "BodyFamilyLogicTests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "BodyFamilyLogicTests failed: " << error.what() << '\n';
    return 1;
  }
}
