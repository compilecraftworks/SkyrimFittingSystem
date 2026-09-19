#include "Menu.h"

#include "native/ArmorSkinning.h"

#include <unordered_set>
#include <vector>

namespace sfs {
namespace {
[[nodiscard]] std::vector<int>
BuildPlayerBaseRegistrationRowIndices(
    const workbench::VariantWorkbench &a_workbench,
    const RE::FormID a_playerFormID) {
  std::vector<int> indices;
  const auto &rows = a_workbench.GetRows();
  indices.reserve(rows.size());
  for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
    const auto &row = rows[static_cast<std::size_t>(index)];
    if (row.ownerActorFormID == a_playerFormID && !row.conditionId.has_value()) {
      indices.push_back(index);
    }
  }
  return indices;
}
} // namespace

bool Menu::ApplyGridInventoryCostume(const std::uint32_t *a_formIDs,
                                     const std::uint32_t a_count) {
  if (!gameDataLoaded_ || (a_formIDs == nullptr && a_count != 0)) {
    return false;
  }

  auto *player = RE::PlayerCharacter::GetSingleton();
  if (!player) {
    return false;
  }

  std::vector<RE::FormID> formIDs;
  formIDs.reserve(a_count);
  std::unordered_set<RE::FormID> seen;
  for (std::uint32_t index = 0; index < a_count; ++index) {
    const auto formID = static_cast<RE::FormID>(a_formIDs[index]);
    if (formID != 0 && seen.insert(formID).second) {
      formIDs.push_back(formID);
    }
  }

  // Grid loadouts can contain weapons and consumables.  A Costume with no
  // SFS-compatible armor does not provide an appearance layout for SFS and
  // must leave the player's current registered appearances untouched.
  const auto layout = BuildSlotFallbackLayoutFromArmorForms(formIDs);
  if (!layout.has_value()) {
    logger::info("[GRID COSTUME] ignored empty/non-armor Costume; player "
                 "registered appearances unchanged");
    return true;
  }

  auto workbenchStateLock = workbench_.AcquireStateLock();
  auto conditionStateLock = AcquireConditionStateLock();
  workbench_.ClearPreview(false);
  workbench_.SyncRowsFromActor(player);

  const auto playerFormID = player->GetFormID();
  const auto basePlayerRows =
      BuildPlayerBaseRegistrationRowIndices(workbench_, playerFormID);
  const auto initialEquippedState =
      workbench::VariantWorkbench::BuildInitialEquippedState(player);
  // Let the normal kit projection validate first, then replace only explicit
  // player base rows. Conditional/NPC/global registrations are not owned by
  // Grid; locked cards and unusable layouts keep the usual kit protections.
  const bool changed = workbench_.ApplyKitLayout(
      *layout, true, std::nullopt, playerFormID, &initialEquippedState,
      &basePlayerRows, true);

  if (!changed) {
    return true;
  }

  // The existing per-actor refresh path reconciles all three renderer
  // backends.  No special Grid visibility flag reaches DAVE, DAV, native
  // skinning, condition evaluation, or external strip synchronization.
  workbench_.RefreshNativeArmorOverridesForActor(playerFormID,
                                                  conditionStore_.revision);
  workbenchDerived_.revisionsInitialized = false;
  logger::info("[GRID COSTUME] player registered appearances replaced");
  return true;
}

bool Menu::ClearGridInventoryCostume() {
  // A cleared Grid Costume means Grid is no longer managing an appearance;
  // it does not erase the player's SFS registrations.
  logger::info("[GRID COSTUME] ignored clear request; player registered "
               "appearances unchanged");
  return gameDataLoaded_;
}
} // namespace sfs
