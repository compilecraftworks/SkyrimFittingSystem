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
namespace RE {
struct BGSKeyword { std::string_view name; };
struct TESObjectARMO {
  std::uint32_t mask;
  bool clothing, hidden{false};
  bool HasKeyword(const BGSKeyword*) const { return false; }
  bool IsClothing() const { return clothing; }
  bool IsLightArmor() const { return !clothing; }
  bool IsHeavyArmor() const { return false; }
};
struct Actor {
  bool managed{true}, active{true};
  std::unordered_set<const TESObjectARMO*> worn;
  std::vector<const TESObjectARMO*> additional;
  unsigned reads{};
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
  bool Contains(const RE::TESObjectARMO* a) const {
    return std::ranges::find(armors, a) != armors.end();
  }
};
DisplaySet BuildDisplaySet(RE::Actor* a, bool = true, EquippedArmorSnapshot* snapshot = nullptr) {
  if (!a || !a->managed) return {};
  EquippedArmorSnapshot local(a);
  (void)(snapshot ? *snapshot : local).Get();
  DisplaySet result{a->active, 0, a->additional};
  for (auto* armor : result.armors) result.slotMask |= armor->mask;
  return result;
}
std::vector<const RE::TESObjectARMO*> CollectVisibleRealArmors(
    RE::Actor*, const DisplaySet&, const std::unordered_set<const RE::TESObjectARMO*>& worn) {
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
void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
  RE::TESObjectARMO real{4, false}, appearance{4, true}, accessory{128, true};
  RE::BGSKeyword clothing{"ClothingBody"}, armor{"ArmorCuirass"}, unrelated{"Other"};
  RE::Actor a; a.worn = {&real}; a.additional = {&appearance};
  auto state = GetFinalRenderedOutfitSnapshot(&a);
  Check(a.reads == 1 && state.visibleActualArmors.size() == 1 && state.additionalSlotMask == 4,
        "one worn collection per final snapshot preserves actual and additional sets");
  a.reads = 0;
  Check(GetDisplayedBodyKeywordState(&a, &clothing) == true && a.reads == 1,
        "body keyword uses the same worn snapshot as display decision");
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
  std::puts("Worn snapshot regression checks passed (no persistent cache or engine coverage change).");
}
