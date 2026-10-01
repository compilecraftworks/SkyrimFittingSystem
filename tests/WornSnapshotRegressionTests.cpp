// Real per-query snapshot and final-state consumer functions. Display/engine
// decisions are fixtures; their independent visibility tests remain separate.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>
#include "native/ArmorRefreshRules.h"
#include "native/HelmetToggle2Rules.h"
namespace logger { template<class... T> void debug(T&&...) {} }
namespace RE {
struct TESObjectREFR { template<class T> T* As() { return static_cast<T*>(this); } };
struct InventoryChanges {};
struct BGSKeyword { std::string_view name; };
struct TESObjectARMO {
  std::uint32_t mask;
  bool clothing, hidden{false};
  bool HasKeyword(const BGSKeyword*) const { return false; }
  bool IsClothing() const { return clothing; }
  bool IsLightArmor() const { return !clothing; }
  bool IsHeavyArmor() const { return false; }
};
struct Actor : TESObjectREFR {
  bool managed{true}, active{true};
  std::unordered_set<const TESObjectARMO*> worn;
  std::vector<const TESObjectARMO*> additional;
  unsigned reads{};
  std::uint32_t formID{0x14};
  std::uint32_t GetFormID() const { return formID; }
};
struct PlayerCharacter {
  static Actor* GetSingleton() { static Actor player; return &player; }
};
struct BGSBipedObjectForm {
  enum class BipedObjectSlot : std::uint32_t { kHead=1, kHair=2, kCirclet=0x1000 };
};
}
namespace sfs::armor {
std::uint32_t GetArmorDisplaySlotMask(const RE::TESObjectARMO* a) { return a->mask; }
std::uint32_t GetArmorSlotMask(unsigned slot) { return 1U << (slot - 30); }
std::string_view GetEditorID(const RE::BGSKeyword* k) { return k->name; }
}
std::unordered_set<const RE::TESObjectARMO*> CollectEquippedArmors(RE::Actor* actor) {
  if (!actor) return {};
  ++actor->reads;
  return actor->worn;
}
#include "WornSnapshot.production.inc"
struct DisplaySet {
  bool active{};
  std::uint32_t slotMask{};
  std::vector<const RE::TESObjectARMO*> armors;
  bool genitalCompatibilityAvailable{}, genitalCorrectionActive{};
  std::uint32_t hiddenSlotMask{};
  bool Contains(const RE::TESObjectARMO* a) const {
    return std::ranges::find(armors, a) != armors.end();
  }
};
std::vector<const RE::TESObjectARMO*> CollectVisibleRealArmors(
    RE::Actor*, const DisplaySet&, const std::unordered_set<const RE::TESObjectARMO*>&);
unsigned visibleCollections{};
bool prepareVisibleList{true};
DisplaySet BuildDisplaySet(RE::Actor* a, bool = true, EquippedArmorSnapshot* snapshot = nullptr,
    std::optional<std::vector<const RE::TESObjectARMO*>>* visibleActual = nullptr) {
  if (visibleActual) visibleActual->reset();
  if (!a || !a->managed) return {};
  EquippedArmorSnapshot local(a);
  (void)(snapshot ? *snapshot : local).Get();
  DisplaySet result{a->active, 0, a->additional};
  for (auto* armor : result.armors) result.slotMask |= armor->mask;
  if (visibleActual && result.active && prepareVisibleList) {
    *visibleActual = CollectVisibleRealArmors(a, result, (snapshot ? *snapshot : local).Get());
  }
  return result;
}
std::vector<const RE::TESObjectARMO*> CollectVisibleRealArmors(
    RE::Actor*, const DisplaySet&, const std::unordered_set<const RE::TESObjectARMO*>& worn) {
  ++visibleCollections;
  std::vector<const RE::TESObjectARMO*> result;
  for (auto* a : worn) if (!a->hidden) result.push_back(a);
  return result;
}
struct FinalRenderedOutfitSnapshot {
  bool managedBySfs{};
  std::uint32_t additionalSlotMask{}, visibleActualSlotMask{};
  std::vector<const RE::TESObjectARMO*> visibleActualArmors, visibleAdditionalArmors;
};
thread_local unsigned g_buildDisplaySetDepth{};
#include "WornQuery.production.inc"
namespace sfs::native::dave {
bool loaded{}, ready{};
bool IsDynamicArmorVariantsLoaded() { return loaded; }
bool IsApiReady() { return ready; }
}
namespace sfs::native::helmet_toggle {
std::uint32_t release{};
std::uint32_t GetActualHairSlotReleaseMask(RE::Actor*, std::uint32_t) { return release; }
}
bool ShouldHideRealArmor(RE::Actor*, const DisplaySet& set, const RE::TESObjectARMO* a) {
  return set.active && a->hidden;
}
std::uint32_t GetSkinningSlotMask(const RE::TESObjectARMO* a, bool) { return a->mask; }
#include "WornMaskQuery.production.inc"
void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void TestVariantResolvedHeadMasks() {
  constexpr unsigned head=1, hair=2, body=4, hands=8, circlet=0x1000;
  constexpr unsigned genitals=1U<<22, faceJewelry=1U<<25;
  constexpr unsigned actualHelmetMask=head|hair|circlet;
  struct VariantResult { unsigned mask; bool ht2Hidden; };
  // These inputs are resolved provider results, not a DAV engine simulation.
  constexpr VariantResult variants[] = {
      {actualHelmetMask, false}, {circlet, true}, {hair|circlet, false},
      {head|circlet, false}, {0, true}};
  unsigned cases=0;
  for (unsigned cycle=0; cycle<128; ++cycle) {
    for (bool player : {true, false}) {
      for (unsigned fitting : {0U, body, hair, hair|circlet, head|circlet, genitals, faceJewelry}) {
        for (bool hiddenActual : {false, true}) {
          for (bool sharedHair : {false, true}) {
            RE::Actor actor; actor.formID=player ? 0x14 : 0x20;
            RE::TESObjectARMO helmet{actualHelmetMask,false,hiddenActual};
            RE::TESObjectARMO ordinary{body|hands,false};
            RE::TESObjectARMO wig{hair,false};
            RE::TESObjectARMO appearance{fitting,false};
            actor.worn={&helmet,&ordinary};
            if (sharedHair) actor.worn.insert(&wig);
            if (fitting) actor.additional={&appearance};
            for (auto variant : variants) {
              const unsigned base=body|hands|variant.mask|(sharedHair ? hair : 0);
              const unsigned exclusiveHidden=hiddenActual
                  ? actualHelmetMask & ~(sharedHair ? hair : 0) : 0;
              const unsigned release=sfs::native::helmet_toggle::rules::ComputeActualHairSlotReleaseMask(
                  variant.ht2Hidden, actualHelmetMask, fitting);
              const unsigned expected=((base & ~exclusiveHidden)|fitting)&~release;
              sfs::native::helmet_toggle::release=release;
              unsigned previous=expected;
              // Missing API -> ready API -> unavailable again must not change
              // display ownership. The actual refresh backends remain separate.
              for (bool ready : {false, true, false}) {
                sfs::native::dave::loaded=true;
                sfs::native::dave::ready=ready;
                actor.reads=0;
                const unsigned result=GetDisplayWornMask(nullptr,&actor,base);
                if (result!=expected) {
                  std::fprintf(stderr,"Head-mask mismatch: player=%d apiReady=%d fitting=%08X hiddenActual=%d sharedHair=%d base=%08X expected=%08X result=%08X\n",
                      player,ready,fitting,hiddenActual,sharedHair,base,expected,result);
                }
                Check(result==expected,
                      "DAV/DAVE must preserve resolved head visibility, remove only SFS-owned hidden slots, and add registered slots");
                Check(result==previous && actor.reads==1,
                      "API readiness transitions retain the same mask and one request-local collection");
                Check(actor.worn.contains(&helmet) && actor.worn.size()==(sharedHair ? 3U : 2U),
                      "display-mask composition never equips or removes actual inventory items");
                previous=result;
                ++cases;
              }
            }
          }
        }
      }
    }
  }
  // Inactive/unmanaged/null queries preserve the provider answer and the
  // pre-existing narrowly scoped HT2 Hair release, without eager collection.
  for (bool ready : {false, true}) {
    sfs::native::dave::ready=ready;
    RE::Actor actor; actor.active=false;
    sfs::native::helmet_toggle::release=hair;
    Check(GetDisplayWornMask(nullptr,&actor,body|hair)==body && actor.reads==1,
          "inactive DAV/DAVE query keeps HT2 release without rebuilding head slots");
    actor.managed=false; actor.reads=0;
    Check(GetDisplayWornMask(nullptr,&actor,body|hair)==body && actor.reads==0,
          "unmanaged DAV/DAVE query does not collect equipment");
    sfs::native::helmet_toggle::release=0;
    Check(GetDisplayWornMask(nullptr,nullptr,body|head)==(body|head),
          "null DAV/DAVE query keeps the original mask");
  }
  // Native-only behavior still uses the production unmanaged-NPC rule.
  sfs::native::dave::loaded=false; sfs::native::dave::ready=false;
  RE::Actor native;
  RE::TESObjectARMO helmet{actualHelmetMask,false}; native.worn={&helmet};
  Check(GetDisplayWornMask(nullptr,&native,0)==actualHelmetMask,
        "native player continues reconstructing actual slots without a variant provider");
  native.formID=0x20;
  Check(GetDisplayWornMask(nullptr,&native,circlet)==circlet,
        "native unmanaged NPC headgear continues preserving the incoming mask");
  std::printf("DAV/DAVE head-mask parity: %u cases through 128 cycles, plus inactive/native controls (production functions, fake engine).\n",cases);
}
int main() {
  TestVariantResolvedHeadMasks();
  RE::TESObjectARMO real{4, false}, appearance{4, true}, accessory{128, true};
  RE::BGSKeyword clothing{"ClothingBody"}, armor{"ArmorCuirass"}, unrelated{"Other"};
  RE::Actor a; a.worn = {&real}; a.additional = {&appearance};
  auto state = GetFinalRenderedOutfitSnapshot(&a);
  Check(a.reads == 1 && visibleCollections == 1 && state.visibleActualArmors.size() == 1 && state.additionalSlotMask == 4,
        "one worn collection and one visible-list construction per final snapshot preserve both sets");
  a.reads = visibleCollections = 0;
  Check(GetDisplayedBodyKeywordState(&a, &clothing) == true && a.reads == 1 && visibleCollections == 1,
        "body keyword consumes the visible list from this request without rebuilding it");
  prepareVisibleList = false; a.reads = visibleCollections = 0;
  Check(GetDisplayedBodyKeywordState(&a, &clothing) == true && a.reads == 1 && visibleCollections == 1,
        "missing same-query handoff falls back to the original fresh calculation, never API NotReady");
  prepareVisibleList = true;
  a.reads = visibleCollections = 0;
  for (unsigned n = 0; n < 1000; ++n) {
    Check(GetDisplayedBodyKeywordState(&a, &clothing) == true, "repeated current body decision remains clothing");
  }
  Check(a.reads == 1000 && visibleCollections == 1000,
        "1000 body queries build 1000 visible lists, not 2000, without a cross-query cache");
  a.reads = 0;
  Check(IsDisplayedFittingArmor(&a, &appearance) && !IsDisplayedFittingArmor(&a, &real) && a.reads == 2,
        "membership only reads display decision, never performs second actual-gear collection");
  a.worn.clear(); a.additional = {&accessory}; a.reads = 0;
  state = GetFinalRenderedOutfitSnapshot(&a);
  Check(a.reads == 1 && state.visibleActualArmors.empty() &&
        GetDisplayedBodyKeywordState(&a, &clothing) == false,
        "next query sees unequip and accessory-only changes without stale cache");
  a.additional = {&appearance}; a.worn = {&real}; real.hidden = true;
  Check(GetFinalRenderedOutfitSnapshot(&a).visibleActualArmors.empty() &&
        GetDisplayedBodyKeywordState(&a, &clothing) == true &&
        GetDisplayedBodyKeywordState(&a, &armor) == false,
        "hidden actual and registered body still determine final body state independently");
  a.managed = false; a.reads = 0;
  Check(!GetDisplayedBodyKeywordState(&a, &clothing).has_value() && a.reads == 0,
        "unmanaged actor retains vanilla body answer without collecting worn gear");
  (void)GetFinalRenderedOutfitSnapshot(&a);
  Check(a.reads == 1, "explicit final snapshot still includes unmanaged actual equipment");
  a.managed = true; a.active = false; a.reads = 0;
  Check(!GetDisplayedBodyKeywordState(&a, &clothing).has_value() && a.reads == 1,
        "inactive display retains vanilla answer after one display decision");
  a.reads = 0;
  Check(!GetDisplayedBodyKeywordState(&a, &unrelated).has_value() && a.reads == 0,
        "unrelated keyword incurs no display work");
  g_buildDisplaySetDepth = 1;
  Check(!GetDisplayedBodyKeywordState(&a, &clothing).has_value() && a.reads == 0,
        "recursive condition retains vanilla result without reentering display");
  Check(!IsDisplayedFittingArmor(nullptr, &appearance) && !IsDisplayedFittingArmor(&a, nullptr) &&
        GetFinalRenderedOutfitSnapshot(nullptr).visibleActualArmors.empty(), "null inputs unchanged");
  g_buildDisplaySetDepth = 0;
  a.active = true; a.worn = {&real, &accessory}; a.additional = {&appearance};
  a.reads = 0;
  for (unsigned n = 0; n < 1000; ++n) {
    Check(GetHiddenRealEquipmentSlotMask(&a) == 4, "hidden actual body mask is preserved");
  }
  Check(a.reads == 1000, "1000 hidden queries collect worn gear 1000 times, not 2000");
  a.reads = 0;
  for (unsigned n = 0; n < 1000; ++n) {
    Check(IsRealEquipmentHiddenForActorSlots(&a, 4) && GetDisplayedFittingSlotMask(&a) == 4,
          "Wet-style paired queries retain hidden-actual and registered-slot answers");
  }
  Check(a.reads == 2000, "1000 Wet-style query pairs collect 2000 times, not 3000");
  for (const auto backend : {0, 1, 2}) {
    sfs::native::dave::loaded = backend != 0;
    sfs::native::dave::ready = backend == 2;
    a.reads = 0;
    for (unsigned n = 0; n < 1000; ++n) {
      Check(GetDisplayWornMask(nullptr, &a, 132) == 132,
            "native/DAV/DAVE preserve visible actual plus registered masks");
    }
    Check(a.reads == 1000, "all backends share one worn collection per mask query");
    a.worn.clear(); a.reads = 0;
    Check(GetHiddenRealEquipmentSlotMask(&a) == 0 && a.reads == 1,
          "unequip/strip is visible on the next query without waiting for a cache");
    a.worn = {&real, &accessory}; a.additional.clear(); a.reads = 0;
    Check(GetDisplayWornMask(nullptr, &a, 132) == 128 && a.reads == 1,
          "hidden real body without registered body retains only visible accessory");
    real.hidden = false; a.reads = 0;
    Check(GetHiddenRealEquipmentSlotMask(&a) == 0 && a.reads == 1,
          "manual show/redress is visible on the very next query");
    real.hidden = true; a.additional = {&appearance};
  }
  // Shared-slot visible real items must continue to prevent DAVE's broad-bit removal.
  RE::TESObjectARMO visibleBody{4, true};
  a.worn.insert(&visibleBody); a.additional.clear(); a.reads = 0;
  Check(GetDisplayWornMask(nullptr, &a, 132) == 132 && a.reads == 1,
        "DAVE shared-slot visible armor survives hidden armor in the same slot");
  sfs::native::helmet_toggle::release = 2;
  a.active = false; a.reads = 0;
  Check(GetDisplayWornMask(nullptr, &a, 134) == 132 && a.reads == 1,
        "inactive display still honors HT2 actual hair release");
  a.managed = false; a.reads = 0;
  Check(GetDisplayWornMask(nullptr, &a, 134) == 132 && a.reads == 0,
        "unmanaged mask query remains lazy");
  Check(!IsRealEquipmentHiddenForActorSlots(nullptr, 4) &&
        !IsRealEquipmentHiddenForActorSlots(&a, 0) &&
        GetHiddenRealEquipmentSlotMask(nullptr) == 0, "empty/null slot guards preserved");
  std::puts("Worn snapshot regression checks passed (no persistent cache or engine coverage change).");
}
