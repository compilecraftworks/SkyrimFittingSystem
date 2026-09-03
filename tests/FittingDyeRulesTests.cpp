#include "native/FittingDyeRules.h"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using sfs::native::dye::rules::IsCharacterBaseComponent;

void Require(const bool a_condition, const std::string_view a_message) {
  if (!a_condition) {
    throw std::runtime_error(std::string(a_message));
  }
}

void TestSupportedBodyFamiliesAreExcluded() {
  for (const auto shape : {"CBBE", "3BA", "3BBB", "3BBB_Anus",
                           "BHUNP", "BHUNP_3BBB", "UNP", "UNPB Body",
                           "UUNP", "UBE", "UBEBody", "HIMBO",
                           "HIMBOBody", "SAM", "SAMBody", "SAM_HighPoly"}) {
    Require(IsCharacterBaseComponent(shape, "armor_piece.dds"),
            "Every supported body-family base shape must be excluded");
  }
}

void TestVanillaBodyComponentsAreExcluded() {
  for (const auto shape : {"Body", "Hands", "Feet", "Face", "Head",
                           "FemaleBody", "MaleBody", "FemaleHands",
                           "MaleHands", "FemaleFeet", "MaleFeet",
                           "VanillaBody",
                           "FemaleGenitals", "MaleGenitals",
                           "VirtualCBBE", "LowerCollision", "ArmColli",
                           "FeetColli", "ButtLegColli"}) {
    Require(IsCharacterBaseComponent(shape, "armor_piece.dds"),
            "Vanilla body and renderer-helper shapes must be excluded");
  }

  for (const auto texture : {
           "textures/actors/character/female/femalebody_1.dds",
           "textures/actors/character/male/malebody_1.dds",
           "textures/actors/character/female/femalehands_1.dds",
           "textures/actors/character/male/malehands_1.dds",
           "textures/custom/ubebody_1.dds",
           "textures/custom/himbobody_1.dds",
           "textures/custom/sambody_1.dds"}) {
    Require(IsCharacterBaseComponent("OutfitBodyProxy", texture),
            "Base-body diffuse bindings must be excluded");
  }
}

void TestUbeRaceSpecificComponentsAreExcluded() {
  for (const auto shape : {"VaginaB1", "VaginaDeep1", "NPC RB Anus2",
                           "NPC Anus Deep2", "Genitals"}) {
    Require(IsCharacterBaseComponent(shape, "outfit_component.dds"),
            "UBE anatomical body/helper shapes must never enter the dye list");
  }

  Require(IsCharacterBaseComponent(
              "UnlabelledShape", "Textures/!UBE/Body/femalebody_1_d.dds"),
          "UBE base-body diffuse paths must be excluded independently of shape names");
}

void TestOutfitComponentsRemainDyeableCandidates() {
  for (const auto shape : {"upper", "leg", "Lower", "PEW_00_Headwear",
                           "samurai_armor", "uber_coat", "cube_armor",
                           "Collier", "CollimatorArmor"}) {
    Require(!IsCharacterBaseComponent(shape, "armor_piece.dds"),
            "Unrelated outfit components must not be filtered by substrings");
  }
  Require(!IsCharacterBaseComponent(
              "Dress", "textures/armor/example/cbbe_dress.dds"),
          "A family-labelled outfit diffuse must not be mistaken for a base body");

  for (const auto shape : {"XF-Femme Flame Top", "UV1_Bra",
                           "CorsetTop"}) {
    Require(!IsCharacterBaseComponent(
                shape, "textures/!UBE/outfits/example_basecolor.dds"),
            "UBE outfit components under the race-specific path must remain dyeable");
  }
}
} // namespace

int main() {
  try {
    TestSupportedBodyFamiliesAreExcluded();
    TestVanillaBodyComponentsAreExcluded();
    TestUbeRaceSpecificComponentsAreExcluded();
    TestOutfitComponentsRemainDyeableCandidates();
    std::cout << "FittingDyeRulesTests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FittingDyeRulesTests failed: " << error.what() << '\n';
    return 1;
  }
}
