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
#include <vector>
#include "native/ArmorRefreshRules.h"
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
  std::uint32_t GetFormID() const { return 0x14; }
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
std::uint32_t PreserveUnmanagedHeadgearWornMask(RE::Actor*, const DisplaySet&,
                                              std::uint32_t, std::uint32_t result) { return result; }
#include "WornMaskQuery.production.inc"
void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
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
