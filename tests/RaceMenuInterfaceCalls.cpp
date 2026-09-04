#include "native/RaceMenuInterfaces.h"

// Separate TU: exercise actual MSVC virtual dispatch, not calls the optimizer
// can devirtualize against a mock built from the same consumer declaration.
namespace race_menu_test {
namespace abi = sfs::native::racemenu::abi;

void ExerciseBodyMorph(abi::IBodyMorphInterface *a_interface,
                       RE::TESObjectREFR *a_actor, RE::NiAVObject *a_node) {
  a_interface->ApplyVertexDiff(a_actor, a_node, false);
  a_interface->ApplyBodyMorphs(a_actor, true);
}

void ExerciseTransform(abi::INiTransformInterface *a_interface,
                       RE::TESObjectREFR *a_actor, bool a_female) {
  abi::INiTransformInterface::Position neutral{};
  a_interface->AddNodeTransformPosition(a_actor, false, a_female, "NPC",
                                        "SFS_HH_SYNC", neutral);
  a_interface->UpdateNodeAllTransforms(a_actor);
  a_interface->RemoveNodeTransformPosition(a_actor, false, a_female, "NPC",
                                           "SFS_HH_SYNC");
  a_interface->UpdateNodeTransforms(a_actor, false, a_female, "NPC");
}
} // namespace race_menu_test
