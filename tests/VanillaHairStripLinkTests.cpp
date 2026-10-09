// Execute production classification, runtime rebinding and popup mapping.
// Engine lookups/transaction state are fixtures; no physical equip API exists.
#include "workbench/ExternalStripLinkRules.h"
#include "native/HelmetToggle2Rules.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace logger { template<class... T> void info(T&&...) {} }
namespace RE {
using FormID = std::uint32_t;
struct TESForm {
  FormID id{};
  std::string editorID;
  inline static std::unordered_map<FormID, TESForm*> forms;
  template<class T> static T* LookupByID(FormID id) {
    auto found=forms.find(id);
    return found == forms.end() ? nullptr : static_cast<T*>(found->second);
  }
};
struct BGSKeyword : TESForm { bool sfsOwned{}; };
struct TESObjectARMO : TESForm {
  std::string name;
  std::uint64_t mask{}, addonMask{};
  bool internal{};
  std::vector<BGSKeyword*> keywords;
  const auto& GetKeywords() const { return keywords; }
};
struct Actor {
  FormID id;
  FormID GetFormID() const { return id; }
};
}
namespace sfs::armor {
std::uint64_t GetArmorSlotMask(std::uint32_t slot) {
  return sfs::workbench::EquipmentSlotMask(slot);
}
std::uint64_t GetArmorDisplaySlotMask(const RE::TESObjectARMO* a) {
  return a ? a->mask | a->addonMask : 0;
}
std::uint64_t GetArmorAddonSlotMask(const RE::TESObjectARMO* a) { return a->addonMask; }
std::string GetEditorID(const RE::TESForm* form) { return form ? form->editorID : ""; }
std::string GetDisplayName(const RE::TESObjectARMO* a) { return a->name; }
bool IsSosTngInternalArmor(const RE::TESObjectARMO* a) { return a->internal; }
}
namespace sfs::native {
bool IsSFSOwnedRuntimeKeyword(const RE::TESObjectARMO*, const RE::BGSKeyword* k) {
  return k && k->sfsOwned;
}
namespace external_equipment {
std::unordered_map<RE::FormID, std::uint64_t> suppressed;
std::uint64_t GetSuppressedActualSlotMask(RE::FormID id) { return suppressed[id]; }
}
}
namespace sfs::workbench {
ExternalStripLinkPolicyState policy;
ExternalModStripLinkMode GetExternalModStripLinkMode() { return policy.configuredMode; }
ExternalModStripLinkMode GetCustomExternalModStripLinkBaseMode() { return policy.customBaseMode; }
ExternalModStripLinkMode GetEffectiveExternalModStripLinkMode() { return ResolveEffectiveStripLinkMode(policy); }
ExternalModStripLinkMode GetCustomDirectStripLinkAutomaticBaseMode() { return policy.directAutomaticBaseMode; }
std::uint64_t GetEffectiveAppearanceProtectedSlotMask() { return policy.protectedAppearanceSlotMask; }
bool IsExternalModStripLinkAppearanceEnabled(std::uint64_t mask) {
  return IsStripLinkedAppearanceEnabled(policy, mask,
      (mask & policy.protectedAppearanceSlotMask) != 0);
}
std::optional<std::uint64_t> ResolveCustomDirectStripLinkAnchorSlotMask(std::uint64_t mask) {
  return ResolveDirectSlotMask(policy, mask, false);
}
enum class AutomaticEquipmentVisibilityMode : std::uint8_t {
  Disabled=0, VanillaSlots=1, ModdingSlots=2, CustomSlots=3
};
struct EquipmentWidgetItem {
  RE::FormID formID{};
  std::uint64_t slotMask{}, automaticEquipmentAnchorSlotMask{};
  RE::FormID automaticEquipmentAnchorFormID{};
  std::uint8_t automaticEquipmentBindingMode{};
  bool automaticEquipmentSuppressed{}, automaticEquipmentUserVisible{}, hidden{}, locked{};
};
struct VariantWorkbenchRow {
  RE::FormID ownerActorFormID;
  std::vector<EquipmentWidgetItem> overrides;
  std::uint64_t GetOverrideDisplaySlotMask(const EquipmentWidgetItem& item) const { return item.slotMask; }
};
#include "VanillaHairClassification.production.inc"
#include "VanillaHairBindings.production.inc"
}
namespace popup {
using Mode=sfs::workbench::ExternalModStripLinkMode;
using Mappings=sfs::workbench::AutomaticEquipmentSlotMappings;
using Overrides=sfs::workbench::AutomaticEquipmentSlotOverrides;
struct SlotAppearance { RE::FormID formID{}; };
using SlotAppearances=std::array<SlotAppearance, sfs::workbench::kAutomaticEquipmentSlotCount>;
bool IsSlotCardVisible(std::uint32_t slot) {
  return (sfs::workbench::EquipmentSlotMask(slot) &
          sfs::workbench::policy.protectedAppearanceSlotMask) == 0;
}
#include "VanillaHairUI.production.inc"
}

unsigned checks{};
void Check(bool ok, const char* message) {
  ++checks;
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
RE::TESObjectARMO Armor(RE::FormID id, std::uint64_t mask, std::string name="") {
  RE::TESObjectARMO result;
  result.id=id; result.mask=mask; result.name=std::move(name);
  return result;
}
int main() {
  using namespace sfs::workbench;
  constexpr auto hair=EquipmentSlotMask(31), longHair=EquipmentSlotMask(41),
      head=EquipmentSlotMask(30), circlet=EquipmentSlotMask(42),
      body=EquipmentSlotMask(32), hands=EquipmentSlotMask(33),
      forearms=EquipmentSlotMask(34), feet=EquipmentSlotMask(37),
      calves=EquipmentSlotMask(38), genital=EquipmentSlotMask(49);
  auto wig31=Armor(1, hair), wig41=Armor(2, longHair),
      helmet=Armor(3, hair|circlet, "Steel Helmet"),
      fullHelmet=Armor(4, head|hair|circlet), partialWig=Armor(5, hair|longHair);
  for (auto* armor : {&wig31,&wig41,&helmet,&fullHelmet,&partialWig}) RE::TESForm::forms[armor->id]=armor;
  for (auto* wig : {&wig31,&wig41,&partialWig}) {
    const auto priority=GetVanillaAnchorPriority(wig);
    Check(priority.count == 1 && priority.slotMasks[0] == longHair,
          "standalone 31/41 wigs have ONLY actual 41 as automatic anchor");
    Check(ResolveVanillaAnchorSlotMask(priority, 0, hair|circlet, hair|circlet) == longHair,
          "even a currently stripped/worn 31+42 helmet cannot become a wig anchor");
    Check(ResolveVanillaAnchorSlotMask(priority, longHair, hair, hair) == 0,
          "protected 41 has no fallback to a helmet's 31");
  }
  for (auto* armor : {&helmet,&fullHelmet}) {
    const auto priority=GetVanillaAnchorPriority(armor);
    Check(priority.count == 2 && priority.slotMasks[0] == head && priority.slotMasks[1] == circlet,
          "31+42 / 30+31+42 helmets retain existing head/circlet priority");
    Check(GetAutomaticEquipmentControlSlotMask(armor) == (armor->mask & ~hair),
          "actual multi-slot helmet masks retain existing 31 normalization");
  }
  Check(GetAutomaticEquipmentControlSlotMask(&wig31) == hair &&
        GetAutomaticEquipmentControlSlotMask(&wig41) == longHair,
        "physical standalone gear masks must not be remapped by an appearance rule");
  auto addonHelmet=Armor(6, circlet); addonHelmet.addonMask=hair;
  Check(GetAutomaticEquipmentControlSlotMask(&addonHelmet) == circlet &&
        GetVanillaAnchorPriority(&addonHelmet).slotMasks[1] == circlet,
        "ARMA-supplied helmet Hair coverage remains on the original path");
  for (const auto name : {"Fashion Wig", "가발", "假发", "假髮", "Hairpiece", "Ponytail"}) {
    auto extensionWig=Armor(7, EquipmentSlotMask(50), name);
    Check(GetVanillaAnchorPriority(&extensionWig).slotMasks[0] == longHair,
          "existing semantic wig classification selects 41 across languages");
    extensionWig.mask=hair|circlet;
    Check(GetVanillaAnchorPriority(&extensionWig).slotMasks[0] == head,
          "a wig-named composite headgear still retains the existing head route");
  }
  for (const auto mask : {body, hands, forearms, feet, calves}) {
    auto item=Armor(7,mask);
    Check(GetVanillaAnchorPriority(&item).slotMasks[0] == mask,
          "unrelated vanilla body/limb categories retain their anchor");
  }
  auto bodyArmor=Armor(8,body|genital);
  Check(GetAutomaticEquipmentControlSlotMask(&bodyArmor) == body,
        "32+49 genital compatibility normalization remains unchanged");
  Check(IsVanillaAnchorSlot(41) && IsVanillaAnchorSlot(31) &&
        popup::IsVanillaDirectTargetSlot(41) && popup::IsVanillaDirectTargetSlot(31),
        "41 is now a selectable vanilla target while explicit 31 remains available");
  Check(popup::DefaultVanillaMapping(31) == 41 && popup::DefaultVanillaMapping(41) == 41 &&
        popup::DefaultVanillaMapping(30) == 30 && popup::DefaultVanillaMapping(42) == 42,
        "empty popup defaults match automatic wig anchors without changing helmets");

  using Mode=ExternalModStripLinkMode;
  RE::Actor player{0x14}, npc{0x1234};
  for (unsigned cycle=0; cycle<128; ++cycle) {
    for (const auto configured : {Mode::VanillaSlots, Mode::Custom}) {
      policy={}; policy.configuredMode=configured; policy.customBaseMode=Mode::VanillaSlots;
      policy.directAutomaticBaseMode=Mode::VanillaSlots;
      std::vector<VariantWorkbenchRow> rows{
          {player.id, {{.formID=wig31.id, .slotMask=hair,
                        .automaticEquipmentAnchorSlotMask=hair,
                        .automaticEquipmentBindingMode=1, .hidden=true}}},
          {npc.id, {{.formID=wig41.id, .slotMask=longHair,
                     .automaticEquipmentAnchorSlotMask=hair,
                     .automaticEquipmentBindingMode=1}}}};
      auto& item=rows[0].overrides[0];
      sfs::native::external_equipment::suppressed[player.id]=circlet|hair;
      sfs::native::external_equipment::suppressed[npc.id]=0;
      Check(UpdateAutomaticEquipmentVisibility(&player,rows,{{helmet.id,circlet}}),
            "migration reports changed binding so the normal refresh can rebuild display");
      Check(item.automaticEquipmentAnchorSlotMask == longHair &&
            !item.automaticEquipmentSuppressed && item.hidden &&
            rows[1].overrides[0].automaticEquipmentAnchorSlotMask == hair,
            "old automatic 31 binding migrates on rebuild, preserving manual hide and other actors");
      sfs::native::external_equipment::suppressed[player.id]=longHair;
      Check(UpdateAutomaticEquipmentVisibility(&player,rows,{}),
            "actual 41 strip reports the changed suppression state");
      Check(item.automaticEquipmentSuppressed && item.hidden,
            "actual 41 strip suppresses the linked wig without editing manual visibility");
      Check(!UpdateAutomaticEquipmentVisibility(&player,rows,{{helmet.id,circlet}}) &&
            item.automaticEquipmentSuppressed,
            "re-equipping a 31+42 helmet does not release an outstanding 41 strip");
      item.automaticEquipmentUserVisible=true;
      Check(!UpdateAutomaticEquipmentVisibility(&player,rows,{}),
            "unchanged actor-local link does not request another rebuild");
      Check(item.automaticEquipmentUserVisible && item.automaticEquipmentSuppressed,
            "manual show during a 41 strip remains a runtime visibility override");
      sfs::native::external_equipment::suppressed[player.id]=0;
      Check(UpdateAutomaticEquipmentVisibility(&player,rows,{{wig41.id,longHair}}),
            "41 redress publishes its changed suppression state");
      Check(!item.automaticEquipmentSuppressed && !item.automaticEquipmentUserVisible && item.hidden,
            "actual 41 redress releases automation but does not show a manually hidden wig");

      popup::SlotAppearances appearances{};
      appearances[1].formID=wig31.id;
      const auto ui=popup::BuildDisplayedMappings(Mode::VanillaSlots,appearances,hair|circlet,
                                                {},{},Mode::VanillaSlots);
      Check(ui[1] == 41 && item.automaticEquipmentAnchorSlotMask == EquipmentSlotMask(ui[1]),
            "popup and runtime use the same migrated automatic mapping");
      if (configured == Mode::Custom) {
        policy.directOverrides[1]=true; policy.directMappings[1]=31;
        Check(UpdateAutomaticEquipmentVisibility(&player,rows,{}),
              "explicit 31 edit changes the runtime binding");
        const auto directUi=popup::BuildDisplayedMappings(Mode::VanillaSlots,appearances,0,
            policy.directMappings,policy.directOverrides,Mode::VanillaSlots);
        Check(item.automaticEquipmentAnchorSlotMask == hair && directUi[1] == 31,
              "explicitly saved 31 mapping is NOT migrated");
        policy.directMappings[1]=41;
        Check(UpdateAutomaticEquipmentVisibility(&player,rows,{}),
              "explicit 41 edit changes the runtime binding");
        Check(item.automaticEquipmentAnchorSlotMask == longHair,
              "direct+vanilla accepts an explicit 41 target");
        policy.directMappings[1]=0;
        Check(UpdateAutomaticEquipmentVisibility(&player,rows,{}),
              "Do Not Link clears the automatic binding");
        Check(item.automaticEquipmentAnchorSlotMask == 0,
              "explicit Do Not Link remains authoritative");
      }
    }
  }
  policy={}; policy.configuredMode=Mode::Custom; policy.customBaseMode=Mode::DirectSlots;
  policy.directAutomaticBaseMode=Mode::ModSettingsSlots;
  policy.directOverrides[1]=true; policy.directMappings[1]=41;
  std::vector<VariantWorkbenchRow> rows{{player.id,{{.formID=wig31.id,.slotMask=hair}}}};
  static_cast<void>(UpdateAutomaticEquipmentVisibility(&player,rows,{}));
  Check(rows[0].overrides[0].automaticEquipmentAnchorSlotMask == 0 &&
        ResolveDirectSlotMask(policy,hair,true) == longHair,
        "direct+mod-settings remains on the token route without a parallel actual binding");
  policy.customBaseMode=Mode::VanillaSlots; policy.directAutomaticBaseMode=Mode::VanillaSlots;
  policy.directOverrides={}; policy.protectedAppearanceSlotMask=longHair;
  static_cast<void>(UpdateAutomaticEquipmentVisibility(&player,rows,{}));
  Check(rows[0].overrides[0].automaticEquipmentAnchorSlotMask == 0,
        "protected 41 target is inactive, with no fallback to 31");
  policy.protectedAppearanceSlotMask=0; policy.disabledAppearanceSlotMask=hair;
  static_cast<void>(UpdateAutomaticEquipmentVisibility(&player,rows,{}));
  Check(rows[0].overrides[0].automaticEquipmentAnchorSlotMask == 0,
        "custom disabled appearance retains Do Not Link behavior");
  const auto ht2=sfs::native::helmet_toggle::rules::ResolveRegisteredAppearanceSuppressionSlots(
      hair|circlet,circlet,false);
  Check(ht2 == circlet &&
        sfs::native::helmet_toggle::rules::ResolveRegisteredAppearanceSuppressionSlots(longHair,circlet,false) == 0,
        "HT2's existing 31+42 and pure-41 suppression rules remain unchanged");
  std::printf("Vanilla Hair strip-link production checks passed: %u checks, 128 rebuild/strip/redress cycles.\n",checks);
}
