// Run the real request-local projection and HT2 read-only query. Engine
// visibility/category decisions are fixtures; no equip/unequip API exists.
#include "native/RegisteredLongHairRules.h"
#include "native/HelmetToggle2Rules.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
#include <vector>

namespace RE {
struct Actor { bool ht2Hidden{}, player{true}; };
struct TESObjectARMO {
  std::uint32_t mask;
  bool visible{true}, managedByHt2{true};
  struct Mask {
    std::uint32_t value;
    std::uint32_t underlying() const { return value; }
  };
  Mask GetSlotMask() const { return {mask}; }
};
}
namespace sfs::armor {
std::uint32_t GetArmorDisplaySlotMask(const RE::TESObjectARMO* armor) {
  return armor->mask;
}
std::uint32_t GetArmorSlotMask(std::uint32_t slot) {
  return std::uint32_t{1} << (slot - 30);
}
}
namespace sfs::native::helmet_toggle {
bool available = true;
bool IsAvailable() { return available; }
bool IsActorHidden(RE::Actor* actor) { return actor && actor->ht2Hidden; }
bool IsPlayer(RE::Actor* actor) { return actor && actor->player; }
bool IsManagedHeadgear(const RE::TESObjectARMO* armor, std::uint32_t) {
  return armor->managedByHt2;
}
#include "HelmetToggleLongHair.production.inc"
}

struct DisplaySet {
  bool active{true};
  std::vector<const RE::TESObjectARMO*> armors;
  std::vector<std::uint32_t> armorSlotMasks;
  std::uint32_t slotMask{}, hiddenSlotMask{};
};
bool IsRealArmorVisibleInDisplaySet(RE::Actor*, const DisplaySet& display,
                                  const RE::TESObjectARMO* armor) {
  return armor->visible && (armor->mask & display.hiddenSlotMask) == 0;
}
#include "RegisteredLongHair.production.inc"

void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
DisplaySet Display(std::initializer_list<const RE::TESObjectARMO*> armors) {
  DisplaySet result;
  for (const auto* armor : armors) {
    result.armors.push_back(armor);
    result.armorSlotMasks.push_back(armor->mask);
    result.slotMask |= armor->mask;
  }
  return result;
}
int main() {
  using namespace sfs::native::long_hair::rules;
  constexpr std::uint32_t head=1, hair=2, body=4, feet=128, circlet=0x1000;
  constexpr auto longHair=kLongHairSlotMask;
  RE::TESObjectARMO wig{longHair}, partialHair{hair}, helmet{hair|circlet},
      fullHelmet{head|hair|circlet}, plainCirclet{circlet}, bodyArmor{body},
      boots{feet}, mixedHairHelmet{longHair|circlet}, unrelatedHairBody{hair|body};
  Check(IsOccludableRegisteredLongHair(longHair, false) &&
        !IsOccludableRegisteredLongHair(longHair, true) &&
        !IsOccludableRegisteredLongHair(longHair, false, true) &&
        !IsOccludableRegisteredLongHair(longHair|circlet, false) &&
        !IsOccludableRegisteredLongHair(hair, false),
        "only unlocked standalone registered 41 follows hair occlusion");
  Check(HeadgearOccludesLongHair(helmet.mask) &&
        HeadgearOccludesLongHair(fullHelmet.mask) &&
        !HeadgearOccludesLongHair(plainCirclet.mask) &&
        !HeadgearOccludesLongHair(unrelatedHairBody.mask),
        "requires per-item headgear AND hair-coverage evidence");

  RE::Actor player, npc{false, false};
  const std::unordered_set<const RE::TESObjectARMO*> noEquipment;
  for (unsigned cycle=0; cycle<128; ++cycle) {
    for (auto* actor : {&player, &npc}) {
      actor->ht2Hidden = false;
      auto display=Display({&wig, &bodyArmor, &boots});
      const auto originalMask=display.slotMask;
      ApplyRegisteredLongHairOcclusion(actor, display, {&helmet}, 0);
      Check(display.armors == std::vector<const RE::TESObjectARMO*>{&bodyArmor, &boots} &&
            display.armorSlotMasks == std::vector<std::uint32_t>{body, feet} &&
            display.slotMask == (originalMask & ~longHair),
            "visible physical helmet removes ONLY registered 41");
      Check(display.active && display.hiddenSlotMask == 0 && helmet.visible &&
            helmet.mask == (hair|circlet),
            "actual equipment and its visibility/mask are never mutated");

      actor->ht2Hidden = true;
      display=Display({&wig, &bodyArmor, &boots});
      ApplyRegisteredLongHairOcclusion(actor, display, {&helmet}, 0);
      Check(display.slotMask == originalMask && display.armors.size() == 3,
            "HT2-hidden physical helmet restores otherwise-visible registered 41");
      actor->ht2Hidden = false;
      helmet.visible = false;
      display=Display({&wig});
      ApplyRegisteredLongHairOcclusion(actor, display, {&helmet}, 0);
      Check(display.slotMask == longHair, "SFS-hidden actual helmet does not occlude wig");
      helmet.visible = true;
      display=Display({&wig});
      ApplyRegisteredLongHairOcclusion(actor, display, noEquipment, 0);
      Check(display.slotMask == longHair, "unequipping helmet restores registered wig");

      display=Display({&wig, &fullHelmet, &bodyArmor});
      ApplyRegisteredLongHairOcclusion(actor, display, noEquipment, 0);
      Check(display.armors == std::vector<const RE::TESObjectARMO*>{&fullHelmet, &bodyArmor} &&
            display.slotMask == (fullHelmet.mask|body),
            "visible registered helmet occludes 41 without editing that helmet");
      display=Display({&wig, &partialHair, &plainCirclet});
      ApplyRegisteredLongHairOcclusion(actor, display, {&plainCirclet, &partialHair}, 0);
      Check(display.armors.size() == 3 && display.slotMask == (longHair|hair|circlet),
            "separate partial hair and circlet must not merge into a helmet");
      display=Display({&mixedHairHelmet, &bodyArmor});
      ApplyRegisteredLongHairOcclusion(actor, display, {&helmet}, 0);
      Check(display.armors.size() == 2 && display.slotMask == (longHair|circlet|body),
            "41+42 composite helmet remains wholly on the existing path");

      // A hidden/stripped wig is absent from the selected display. Showing a
      // helmet cannot fabricate it or clear manual/strip state on rebuild.
      actor->ht2Hidden = true;
      display=Display({&bodyArmor});
      ApplyRegisteredLongHairOcclusion(actor, display, {&helmet}, 1);
      Check(display.slotMask == body && display.armors.size() == 1,
            "helmet hide cannot resurrect a manually hidden or stripped wig");
    }
  }
  // Ignore, category-disable and missing HT2: do not infer that every physical
  // helmet is hidden from a global signal belonging to another category.
  player.ht2Hidden=true;
  helmet.managedByHt2=false;
  auto display=Display({&wig});
  ApplyRegisteredLongHairOcclusion(&player, display, {&helmet}, 0);
  Check(display.armors.empty(), "unmanaged/ignored actual helmet still occludes wig");
  helmet.managedByHt2=true;
  sfs::native::helmet_toggle::available=false;
  display=Display({&wig});
  ApplyRegisteredLongHairOcclusion(&player, display, {&helmet}, 0);
  Check(display.armors.empty(), "without HT2 visible actual helmet still occludes wig");
  sfs::native::helmet_toggle::available=true;
  npc.ht2Hidden=false;
  display=Display({&wig});
  ApplyRegisteredLongHairOcclusion(&npc, display, {&helmet}, 0);
  Check(display.armors.empty(), "player hidden state must not affect another actor");

  constexpr auto managed = sfs::native::helmet_toggle::rules::kPlayerManagedActualSlotMask;
  Check(sfs::native::helmet_toggle::rules::ResolveRegisteredAppearanceSuppressionSlots(
            longHair, managed, false) == 0 &&
        sfs::native::helmet_toggle::rules::ResolveRegisteredAppearanceSuppressionSlots(
            hair|circlet, circlet, false) == circlet,
        "HT2 controller masks and 31+42 suppression remain unchanged");
  Check(LongHairDisplayChanged(longHair|body, body) &&
        LongHairDisplayChanged(body, body|longHair) &&
        !LongHairDisplayChanged(body|hair, body) &&
        !LongHairDisplayChanged(body, feet),
        "new refresh trigger is limited to a changed registered 41 display bit");
  std::puts("Registered LongHair projection checks passed (128 actor-local transition cycles).");
}
