#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <vector>

namespace sfs::native {

// Immutable, actor-local projection of the equipment that SFS expects the
// renderer to show after every manual, conditional, protected-slot, preview,
// strip-link, and compatibility rule has been applied. Integrations must query
// this boundary instead of rebuilding visibility from workbench state.
struct FinalRenderedOutfitSnapshot {
  bool managedBySfs{false};
  std::uint32_t visibleActualSlotMask{0};
  std::uint32_t additionalSlotMask{0};
  std::vector<const RE::TESObjectARMO *> visibleActualArmors;
  std::vector<const RE::TESObjectARMO *> visibleAdditionalArmors;
};

[[nodiscard]] FinalRenderedOutfitSnapshot
GetFinalRenderedOutfitSnapshot(RE::Actor *a_actor);

// Returns only an additional SFS-rendered armor (normally a registered
// appearance), never the actor's ordinary actual equipment.
[[nodiscard]] const RE::TESObjectARMO *
GetDisplayedFittingArmorForSlot(RE::Actor *a_actor,
                                std::uint32_t a_slotMask);

} // namespace sfs::native
