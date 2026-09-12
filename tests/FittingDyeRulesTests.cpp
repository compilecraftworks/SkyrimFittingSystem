#include "native/FittingDyeRules.h"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using sfs::native::dye::rules::IsCharacterBaseComponent;
using sfs::native::dye::rules::DrawHookInstallAction;
using sfs::native::dye::rules::IsRendererContextMatch;
using sfs::native::dye::rules::ResolveDrawHookInstallAction;
using sfs::native::dye::rules::ShouldInspectRendererTintPass;

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

  for (const auto &[shape, texture] : {
           std::pair{"CBBE Corset", "textures/armor/cbbe/corset_d.dds"},
           std::pair{"3BA Dress", "textures/armor/3ba/dress_d.dds"},
           std::pair{"3BBB Sleeve", "textures/armor/3bbb/sleeve_d.dds"},
           std::pair{"BHUNP Coat", "textures/armor/bhunp/coat_d.dds"},
           std::pair{"UNP Stockings", "textures/armor/unp/stockings_d.dds"},
           std::pair{"UBE 2.0 Corset", "textures/!UBE/outfits/corset_d.dds"},
           std::pair{"UBE_Dress", "textures/!UBE/outfits/dress_d.dds"},
           std::pair{"SAM Armor", "textures/armor/sam/armor_d.dds"},
       }) {
    Require(!IsCharacterBaseComponent(shape, texture),
            "Body-family labels on outfit components must remain dyeable");
  }

  for (const auto shape : {"XF-Femme Flame Top", "UV1_Bra",
                           "CorsetTop"}) {
    Require(!IsCharacterBaseComponent(
                shape, "textures/!UBE/outfits/example_basecolor.dds"),
            "UBE outfit components under the race-specific path must remain dyeable");
  }
}

void TestDrawHookContextReplacementRules() {
  Require(ResolveDrawHookInstallAction(false, false, false) ==
              DrawHookInstallAction::Install,
          "A new D3D11 context vtable must receive its own guarded hook");
  Require(ResolveDrawHookInstallAction(true, true, true) ==
              DrawHookInstallAction::Reuse,
          "An already registered context vtable must be reused idempotently");
  Require(ResolveDrawHookInstallAction(false, true, true) ==
              DrawHookInstallAction::RejectUnknownOwnership &&
              ResolveDrawHookInstallAction(false, true, false) ==
                  DrawHookInstallAction::RejectUnknownOwnership,
          "An untracked SFS-looking or partial hook must fail closed");
  Require(ResolveDrawHookInstallAction(true, false, false) ==
              DrawHookInstallAction::UseExistingChainOrShaderBindingFallback &&
              ResolveDrawHookInstallAction(true, true, false) ==
                  DrawHookInstallAction::UseExistingChainOrShaderBindingFallback,
          "A displaced registered chain must be preserved and use the scoped fallback");
}

void TestDormantRendererFastPath() {
  using sfs::native::dye::rules::ShouldTrackRendererTintPass;
  Require(!ShouldTrackRendererTintPass(false, false),
          "An unrelated top-level pass needs no tint record");
  Require(ShouldTrackRendererTintPass(true, false) &&
              ShouldTrackRendererTintPass(true, true),
          "Matched tint passes must always retain binding and restoration");
  Require(ShouldTrackRendererTintPass(false, true),
          "An unrelated nested pass must mask its parent's tint, even after targets clear");
  Require(!ShouldInspectRendererTintPass(false, false),
          "Dormant Dye hooks must not enter the renderer map/mutex path");
  Require(ShouldInspectRendererTintPass(true, false) &&
              ShouldInspectRendererTintPass(false, true) &&
              ShouldInspectRendererTintPass(true, true),
          "Durable dyes and amber previews must activate renderer inspection");
  Require(IsRendererContextMatch(0x1000, 0x1000) &&
              !IsRendererContextMatch(0x1000, 0x2000) &&
              !IsRendererContextMatch(0, 0),
          "Dye substitution must never cross a D3D context boundary");
}
} // namespace

int main() {
  try {
    TestSupportedBodyFamiliesAreExcluded();
    TestVanillaBodyComponentsAreExcluded();
    TestUbeRaceSpecificComponentsAreExcluded();
    TestOutfitComponentsRemainDyeableCandidates();
    TestDrawHookContextReplacementRules();
    TestDormantRendererFastPath();
    std::cout << "FittingDyeRulesTests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FittingDyeRulesTests failed: " << error.what() << '\n';
    return 1;
  }
}
