#include "workbench/InitialFilterSelection.h"

namespace {
sfs::ui::workbench::FilterState
BuildActorFilterState(const RE::FormID a_actorFormID) {
  return {.actorFormID = a_actorFormID};
}

RE::Actor *ResolveCrosshairActor() {
  const auto *crosshairPickData = RE::CrosshairPickData::GetSingleton();
  if (!crosshairPickData) {
    return nullptr;
  }

  RE::ObjectRefHandle handle;
#if defined(EXCLUSIVE_SKYRIM_FLAT)
  handle = crosshairPickData->targetActor;
#else
  handle = crosshairPickData->targetActor[RE::VR_DEVICE::kHeadset];
#endif

  auto ref = handle.get();
  return ref ? ref->As<RE::Actor>() : nullptr;
}

RE::Actor *
ResolveInitialWorkbenchFilterActor(const bool a_includeCrosshairActor) {
  if (a_includeCrosshairActor) {
    auto *player = RE::PlayerCharacter::GetSingleton();
    if (auto *actor = ResolveCrosshairActor();
        sfs::workbench::IsSelectableWorkbenchActor(actor, player)) {
      return actor;
    }
  }
  return RE::PlayerCharacter::GetSingleton();
}
} // namespace

namespace sfs::workbench {
bool IsSelectableWorkbenchActor(RE::Actor *a_actor, RE::Actor *a_player) {
  return a_actor && a_player && !a_actor->IsDisabled() &&
         a_actor->Is3DLoaded() && !a_actor->IsDead() &&
         !a_actor->IsHostileToActor(a_player) &&
         (a_actor->IsPlayerTeammate() ||
          a_actor->HasKeywordString("ActorTypeNPC"));
}

InitialFilterSelection
BuildInitialFilterSelection(const bool a_includeCrosshairActor) {
  if (auto *actor =
          ResolveInitialWorkbenchFilterActor(a_includeCrosshairActor)) {
    return {.filter = BuildActorFilterState(actor->GetFormID())};
  }
  return {};
}
} // namespace sfs::workbench