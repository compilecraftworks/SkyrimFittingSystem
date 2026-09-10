#include "Menu.h"

#include "ArmorUtils.h"
#include "StringUtils.h"
#include "native/FittingSlotState.h"
#include "ui/Localization.h"
#include "workbench/InitialFilterSelection.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace sfs {
namespace {
bool AreWorkbenchFilterStatesEqual(const ui::workbench::FilterState &a_left,
                                   const ui::workbench::FilterState &a_right) {
  return a_left.actorFormID == a_right.actorFormID;
}
[[nodiscard]] bool IsPlayerActor(const RE::Actor *a_actor) {
  const auto *player = RE::PlayerCharacter::GetSingleton();
  return a_actor && player && a_actor->GetFormID() == player->GetFormID();
}

[[nodiscard]] std::uint32_t
GetLowestSlotNumber(const std::uint64_t a_slotMask) {
  std::uint32_t lowestSlotNumber = (std::numeric_limits<std::uint32_t>::max)();
  for (const auto slotMask : sfs::armor::GetAllArmorSlotMasks()) {
    if ((a_slotMask & slotMask) == 0) {
      continue;
    }

    const auto slotNumber = sfs::armor::GetArmorSlotNumber(slotMask);
    if (slotNumber != 0) {
      lowestSlotNumber = (std::min)(lowestSlotNumber, slotNumber);
    }
  }

  return lowestSlotNumber == (std::numeric_limits<std::uint32_t>::max)()
             ? 0
             : lowestSlotNumber;
}

[[nodiscard]] std::uint32_t
GetWorkbenchRowDisplaySlotNumber(const workbench::VariantWorkbenchRow &a_row) {
  if (a_row.isEquipped && !a_row.IsSlotRow()) {
    if (const auto slotNumber =
            GetLowestSlotNumber(a_row.GetSelectionDisplaySlotMask());
        slotNumber != 0) {
      return slotNumber;
    }
  }

  std::uint32_t lowestOverrideSlotNumber =
      (std::numeric_limits<std::uint32_t>::max)();
  for (const auto &overrideItem : a_row.overrides) {
    const auto slotNumber =
        GetLowestSlotNumber(a_row.GetOverrideDisplaySlotMask(overrideItem));
    if (slotNumber != 0) {
      lowestOverrideSlotNumber =
          (std::min)(lowestOverrideSlotNumber, slotNumber);
    }
  }
  if (lowestOverrideSlotNumber != (std::numeric_limits<std::uint32_t>::max)()) {
    return lowestOverrideSlotNumber;
  }

  return GetLowestSlotNumber(a_row.equipped.slotMask);
}
} // namespace

void Menu::BuildWorkbenchFilterOptions(
    std::vector<WorkbenchFilterOption> &a_options) {
  auto *localization = ui::Localization::GetSingleton();
  a_options.clear();

  constexpr int kPlayerPriority = 0;
  constexpr int kDefaultOverridePriority = 1;
  constexpr int kConditionalOverridePriority = 2;
  constexpr int kNearbyPriority = 3;
  const auto *player = RE::PlayerCharacter::GetSingleton();
  const auto playerFormID = player ? player->GetFormID() : 0;

  std::unordered_map<RE::FormID, int> priorities;
  std::unordered_map<RE::FormID, std::size_t> nearbyOrder;
  const auto addCandidate = [&](const RE::FormID actorFormID,
                                const int priority) {
    if (actorFormID == 0) {
      return;
    }
    if (const auto it = priorities.find(actorFormID); it == priorities.end()) {
      priorities.emplace(actorFormID, priority);
    } else {
      it->second = (std::min)(it->second, priority);
    }
  };

  addCandidate(playerFormID, kPlayerPriority);
  for (const auto &row : workbench_.GetRows()) {
    if (row.ownerActorFormID == 0 ||
        (row.overrides.empty() && !row.conditionId.has_value())) {
      continue;
    }
    addCandidate(row.ownerActorFormID, row.conditionId.has_value()
                                           ? kConditionalOverridePriority
                                            : kDefaultOverridePriority);
  }
  // A conditional actual-equipment row is stored separately from fitting
  // rows. Keep its owner discoverable even when the actor is no longer nearby
  // or one side of the condition row is deliberately empty.
  for (const auto &rule : workbench_.GetConditionalVisibilityRules()) {
    if (rule.ownerActorFormID != 0 &&
        (!rule.conditionId.empty() || rule.target.formID != 0)) {
      addCandidate(rule.ownerActorFormID, kConditionalOverridePriority);
    }
  }
  for (std::size_t index = 0; index < workbenchNearbyActorFormIDs_.size();
       ++index) {
    const auto actorFormID = workbenchNearbyActorFormIDs_[index];
    nearbyOrder.try_emplace(actorFormID, index + 1);
    addCandidate(actorFormID, kNearbyPriority);
  }
  if (workbenchFilter_.actorFormID != 0) {
    nearbyOrder.insert_or_assign(workbenchFilter_.actorFormID, 0);
    addCandidate(workbenchFilter_.actorFormID, kNearbyPriority);
  }

  for (const auto &[actorFormID, priority] : priorities) {
    std::string name;
    if (actorFormID == playerFormID) {
      name = localization->Get("workbench.filters.player_prefix");
      if (!name.empty() && name.back() == '[') {
        name.pop_back();
      }
    } else if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
               actor != nullptr) {
      if (const auto *displayName = actor->GetDisplayFullName();
          displayName && displayName[0] != '\0') {
        name = displayName;
      } else if (const auto *actorName = actor->GetName();
                 actorName && actorName[0] != '\0') {
        name = actorName;
      }
    }
    if (name.empty()) {
      name = localization->Get("workbench.filters.actor_prefix");
    }
    if (!name.empty() && name.back() == '[') {
      name.pop_back();
    }
    a_options.push_back({.label = std::string(localization->Get(
                                      "workbench.filters.actor_item_prefix")) +
                                  name + " [" +
                                  armor::FormatFormID(actorFormID) + "]",
                         .actorFormID = actorFormID});
  }

  std::stable_sort(
      a_options.begin(), a_options.end(),
      [&](const auto &left, const auto &right) {
        const auto leftPriority = priorities.at(left.actorFormID);
        const auto rightPriority = priorities.at(right.actorFormID);
        if (leftPriority != rightPriority) {
          return leftPriority < rightPriority;
        }
        if (leftPriority == kNearbyPriority) {
          const auto leftOrder =
              nearbyOrder.contains(left.actorFormID)
                  ? nearbyOrder.at(left.actorFormID)
                  : (std::numeric_limits<std::size_t>::max)();
          const auto rightOrder =
              nearbyOrder.contains(right.actorFormID)
                  ? nearbyOrder.at(right.actorFormID)
                  : (std::numeric_limits<std::size_t>::max)();
          if (leftOrder != rightOrder) {
            return leftOrder < rightOrder;
          }
        }
        return strings::CompareTextInsensitive(left.label, right.label) < 0;
      });
}

void Menu::ValidateWorkbenchFilterSelection() {
  EnsureWorkbenchDerivedState();
  if (!IsWorkbenchFilterSelectionValid() &&
      !workbenchDerived_.filterOptions.empty()) {
    workbenchFilter_.actorFormID =
        workbenchDerived_.filterOptions.front().actorFormID;
  }
}

bool Menu::IsWorkbenchFilterSelectionValid() const {
  return std::ranges::any_of(workbenchDerived_.filterOptions,
                             [&](const WorkbenchFilterOption &option) {
                               return option.actorFormID ==
                                      workbenchFilter_.actorFormID;
                             });
}

void Menu::BumpConditionStoreRevision() { ++conditionStore_.revision; }

bool Menu::HideRealEquipmentWithFittingForActor(RE::Actor *a_actor) const {
  std::lock_guard lock(actorVisibilityMutex_);
  const auto actorFormID = a_actor ? a_actor->GetFormID() : RE::FormID{0};
  if (actorFormID != 0) {
    if (const auto it = hideRealEquipmentByActor_.find(actorFormID);
        it != hideRealEquipmentByActor_.end()) {
      return it->second;
    }
  }

  return IsPlayerActor(a_actor) ? hideRealEquipmentWithFitting_ : false;
}

void Menu::SetHideRealEquipmentWithFittingForActor(RE::Actor *a_actor,
                                                   const bool a_hide) {
  std::lock_guard lock(actorVisibilityMutex_);
  const auto actorFormID = a_actor ? a_actor->GetFormID() : RE::FormID{0};
  if (actorFormID == 0) {
    return;
  }

  hideRealEquipmentByActor_.insert_or_assign(actorFormID, a_hide);
  if (IsPlayerActor(a_actor)) {
    hideRealEquipmentWithFitting_ = a_hide;
    SaveUserSettings();
  }
}

bool Menu::HideFittingOverridesForActor(RE::Actor *a_actor) const {
  std::lock_guard lock(actorVisibilityMutex_);
  const auto actorFormID = a_actor ? a_actor->GetFormID() : RE::FormID{0};
  if (actorFormID == 0) {
    return false;
  }

  if (const auto it = hideFittingOverridesByActor_.find(actorFormID);
      it != hideFittingOverridesByActor_.end()) {
    return it->second;
  }
  return false;
}

void Menu::SetHideFittingOverridesForActor(RE::Actor *a_actor,
                                           const bool a_hide) {
  std::lock_guard lock(actorVisibilityMutex_);
  const auto actorFormID = a_actor ? a_actor->GetFormID() : RE::FormID{0};
  if (actorFormID == 0) {
    return;
  }

  hideFittingOverridesByActor_.insert_or_assign(actorFormID, a_hide);
}
void Menu::EnsureWorkbenchDerivedState() {
  if (workbenchDerived_.revisionsInitialized &&
      workbenchDerived_.workbenchRevision == workbench_.GetRevision() &&
      workbenchDerived_.conditionRevision == conditionStore_.revision &&
      workbenchDerived_.filterStateInitialized &&
      AreWorkbenchFilterStatesEqual(workbenchDerived_.filterState,
                                    workbenchFilter_)) {
    return;
  }

  RebuildWorkbenchDerivedState();
}

void Menu::RebuildWorkbenchDerivedState() {
  auto &derived = workbenchDerived_;
  derived.filterOptions.clear();
  BuildWorkbenchFilterOptions(derived.filterOptions);

  const auto selectedActorExists = std::ranges::any_of(
      derived.filterOptions, [&](const WorkbenchFilterOption &option) {
        return option.actorFormID == workbenchFilter_.actorFormID;
      });
  if (!selectedActorExists) {
    workbenchFilter_.actorFormID =
        derived.filterOptions.empty()
            ? RE::FormID{0}
            : derived.filterOptions.front().actorFormID;
  }

  const auto &rows = workbench_.GetRows();
  derived.rowConditionStates.clear();
  derived.rowConditionStates.reserve(rows.size());
  for (const auto &row : rows) {
    derived.rowConditionStates.push_back(
        ui::workbench::ResolveRowConditionVisualState(row,
                                                      ConditionDefinitions()));
  }

  auto rowsForConflicts = rows;
  for (std::size_t index = 0; index < rowsForConflicts.size(); ++index) {
    const auto &conditionState = derived.rowConditionStates[index];
    if (conditionState.missing || conditionState.brokenCondition) {
      rowsForConflicts[index].conditionId = std::nullopt;
    }
  }
  derived.conflictState =
      ui::workbench_conflicts::BuildConflictState(rowsForConflicts);

  std::vector<int> currentVisibleRowIndices;
  currentVisibleRowIndices.reserve(rows.size());
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    const bool hasActualDisplay = row.isEquipped && !row.IsSlotRow();
    // A conditional fitting row remains a first-class workbench row even
    // after its last registered appearance is deleted.  The condition card
    // must stay visible beside an empty action drop target, just like an
    // actual-equipment visibility rule whose target card was removed.
    if (row.ownerActorFormID == workbenchFilter_.actorFormID &&
        (hasActualDisplay || row.HasOverridesOrHideState() ||
         row.conditionId.has_value())) {
      currentVisibleRowIndices.push_back(rowIndex);
    }
  }

  // Once the user edits the condition table, preserve its current row order
  // for the rest of this menu session. Sorting here used to defeat the later
  // condition-section guard and made rows jump immediately after a drop or
  // delete. The next menu opening invalidates this cache and sorts normally.
  derived.visibleRowIndices = std::move(currentVisibleRowIndices);
  if (!workbenchSortDeferredUntilClose_) {
    std::stable_sort(
        derived.visibleRowIndices.begin(), derived.visibleRowIndices.end(),
        [&](const int a_left, const int a_right) {
          const auto &leftRow = rows[static_cast<std::size_t>(a_left)];
          const auto &rightRow = rows[static_cast<std::size_t>(a_right)];
          const auto leftSlot = GetWorkbenchRowDisplaySlotNumber(leftRow);
          const auto rightSlot = GetWorkbenchRowDisplaySlotNumber(rightRow);
          if (leftSlot != rightSlot) {
            if (leftSlot == 0)
              return false;
            if (rightSlot == 0)
              return true;
            return leftSlot < rightSlot;
          }
          return a_left < a_right;
        });
  }

  derived.workbenchRevision = workbench_.GetRevision();
  derived.conditionRevision = conditionStore_.revision;
  derived.revisionsInitialized = true;
  derived.filterState = workbenchFilter_;
  derived.filterStateInitialized = true;
}
const std::vector<int> &Menu::BuildVisibleWorkbenchRowIndices() {
  EnsureWorkbenchDerivedState();
  return workbenchDerived_.visibleRowIndices;
}

std::vector<int> Menu::BuildWorkbenchTargetRowIndices(
    const std::optional<std::string> &a_conditionId) {
  const auto ownerActorFormID = ResolveNewWorkbenchRowOwnerActorFormID();
  std::vector<int> indices;
  const auto &rows = workbench_.GetRows();
  indices.reserve(rows.size());
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size());
       ++rowIndex) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    if (row.ownerActorFormID == ownerActorFormID &&
        row.conditionId == a_conditionId) {
      indices.push_back(rowIndex);
    }
  }
  return indices;
}

void Menu::RefreshWorkbenchActorCandidates() {
  // This is a one-shot discovery pass when the workbench is opened, not a
  // background actor scan.  Keep the nearest actors first while allowing a
  // whole populated exterior/interior area to appear in the selector.
  constexpr float kNearbyActorRadius = 4096.0f;
  constexpr std::size_t kMaximumNearbyActors = 32;
  workbenchNearbyActorFormIDs_.clear();

  auto *player = RE::PlayerCharacter::GetSingleton();
  auto *tes = RE::TES::GetSingleton();
  if (!player || !tes) {
    return;
  }

  struct NearbyActorCandidate {
    RE::FormID formID{0};
    float distanceSquared{0.0f};
  };
  std::vector<NearbyActorCandidate> candidates;
  std::unordered_set<RE::FormID> seenActorFormIDs{player->GetFormID()};
  const auto playerPosition = player->GetPosition();
  tes->ForEachReferenceInRange(
      player, kNearbyActorRadius, [&](RE::TESObjectREFR *a_ref) {
        auto *actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
        if (!workbench::IsSelectableWorkbenchActor(actor, player) ||
            !seenActorFormIDs.insert(actor->GetFormID()).second) {
          return RE::BSContainer::ForEachResult::kContinue;
        }

        const auto position = actor->GetPosition();
        const auto deltaX = position.x - playerPosition.x;
        const auto deltaY = position.y - playerPosition.y;
        const auto deltaZ = position.z - playerPosition.z;
        candidates.push_back(
            {.formID = actor->GetFormID(),
             .distanceSquared =
                 deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ});
        return RE::BSContainer::ForEachResult::kContinue;
      });

  std::ranges::sort(candidates, {}, &NearbyActorCandidate::distanceSquared);
  if (candidates.size() > kMaximumNearbyActors) {
    candidates.resize(kMaximumNearbyActors);
  }
  workbenchNearbyActorFormIDs_.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    workbenchNearbyActorFormIDs_.push_back(candidate.formID);
  }
}

void Menu::ApplyInitialWorkbenchFilterSelection() {
  RefreshWorkbenchActorCandidates();
  workbenchFilter_ =
      workbench::BuildInitialFilterSelection(addCrosshairNpcToActorList_)
          .filter;
  pendingSlotCreations_.clear();
  workbenchDerived_.revisionsInitialized = false;
}

std::optional<std::string> Menu::ResolveNewWorkbenchRowConditionId() {
  ValidateWorkbenchFilterSelection();
  return std::nullopt;
}

RE::FormID Menu::ResolveNewWorkbenchRowOwnerActorFormID() {
  ValidateWorkbenchFilterSelection();
  if (workbenchFilter_.actorFormID != 0) {
    return workbenchFilter_.actorFormID;
  }
  if (const auto *player = RE::PlayerCharacter::GetSingleton()) {
    return player->GetFormID();
  }
  return RE::FormID{0};
}

RE::Actor *Menu::ResolveWorkbenchPreviewActor() {
  ValidateWorkbenchFilterSelection();
  if (workbenchFilter_.actorFormID != 0) {
    if (auto *actor =
            RE::TESForm::LookupByID<RE::Actor>(workbenchFilter_.actorFormID)) {
      return actor;
    }
    return nullptr;
  }
  return RE::PlayerCharacter::GetSingleton();
}

workbench::VariantWorkbench::InitialEquippedState
Menu::BuildWorkbenchInitialEquippedState() {
  return workbench::VariantWorkbench::BuildInitialEquippedState(
      ResolveWorkbenchPreviewActor());
}

void Menu::EnsureWorkbenchRowsSyncedForPreviewActor() {
  if (!gameDataLoaded_)
    return;
  auto *actor = ResolveWorkbenchPreviewActor();
  if (!actor) {
    workbenchActorSyncState_ = {};
    return;
  }

  auto equippedState =
      workbench::VariantWorkbench::BuildInitialEquippedState(actor);
  std::vector<RE::FormID> wornArmorFormIDs(equippedState.wornArmorForms.begin(),
                                           equippedState.wornArmorForms.end());
  std::ranges::sort(wornArmorFormIDs);
  const auto actorFormID = actor->GetFormID();
  const bool syncNeeded =
      !workbenchActorSyncState_.initialized ||
      !AreWorkbenchFilterStatesEqual(workbenchActorSyncState_.filterState,
                                     workbenchFilter_) ||
      workbenchActorSyncState_.actorFormID != actorFormID ||
      workbenchActorSyncState_.occupiedSlotMask !=
          equippedState.occupiedSlotMask ||
      workbenchActorSyncState_.wornArmorFormIDs != wornArmorFormIDs;
  if (!syncNeeded)
    return;

  workbench_.SyncRowsFromActor(actor);
  workbenchActorSyncState_.initialized = true;
  workbenchActorSyncState_.filterState = workbenchFilter_;
  workbenchActorSyncState_.actorFormID = actorFormID;
  workbenchActorSyncState_.occupiedSlotMask = equippedState.occupiedSlotMask;
  workbenchActorSyncState_.wornArmorFormIDs = std::move(wornArmorFormIDs);
}

void Menu::SyncWorkbenchRowsForCurrentFilter() {
  ValidateWorkbenchFilterSelection();
  if (auto *actor = ResolveWorkbenchPreviewActor()) {
    workbench_.SyncRowsFromActor(actor);
  }
}

void Menu::SyncWorkbenchRowsForActor(const RE::FormID a_actorFormID) {
  auto workbenchStateLock = workbench_.AcquireStateLock();
  if (!gameDataLoaded_ || a_actorFormID == 0)
    return;
  if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID)) {
    workbench_.SyncRowsFromActor(actor);
  }
}

} // namespace sfs
