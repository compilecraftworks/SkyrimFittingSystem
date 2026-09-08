#include "VariantWorkbench.h"

#include "ConditionMaterializer.h"
#include "conditions/Status.h"
#include "native/ArmorSkinning.h"

#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace sfs::workbench {
bool VariantWorkbench::ApplyCatalogPreview(
    std::string_view a_selectionKey, const std::vector<RE::FormID> &a_formIDs,
    RE::Actor *a_actor, const std::vector<int> *a_candidateRowIndices) {
  auto stateLock = AcquireStateLock();
  if (!a_actor) {
    ClearPreview();
    return false;
  }

  if (previewSelectionKey_ == a_selectionKey &&
      previewActorFormID_ == a_actor->GetFormID() &&
      !previewNativeRows_.empty() && !previewReplacesNativeRows_) {
    return true;
  }

  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formIDs, assignments, a_candidateRowIndices)) {
    ClearPreview();
    return false;
  }

  std::vector<VariantWorkbenchRow> desiredRows;
  desiredRows.reserve(assignments.size());
  std::unordered_map<int, std::size_t> previewRowIndexBySourceRow;
  previewRowIndexBySourceRow.reserve(assignments.size());

  for (const auto &assignment : assignments) {
    if (assignment.rowIndex < 0 ||
        static_cast<std::size_t>(assignment.rowIndex) >= rows_.size()) {
      continue;
    }

    const auto *overrideArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(assignment.armorFormID);
    if (!overrideArmor) {
      continue;
    }

    const auto [previewIt, inserted] = previewRowIndexBySourceRow.try_emplace(
        assignment.rowIndex, desiredRows.size());
    if (inserted) {
      auto previewRow = rows_[static_cast<std::size_t>(assignment.rowIndex)];
      std::erase_if(previewRow.overrides,
                    [](const EquipmentWidgetItem &a_item) {
                      return !a_item.locked;
                    });
      previewRow.hideEquipped = false;
      desiredRows.push_back(std::move(previewRow));
    }

    AppendOverrideItem(desiredRows[previewIt->second].overrides, overrideArmor);
  }

  if (desiredRows.empty()) {
    ClearPreview();
    return false;
  }

  ClearPreview();
  previewSelectionKey_ = std::string(a_selectionKey);
  previewActorFormID_ = a_actor->GetFormID();
  previewNativeRows_ = std::move(desiredRows);
  previewReplacesNativeRows_ = false;
  sfs::native::QueueArmorRefreshFor(a_actor);
  return true;
}

bool VariantWorkbench::PreviewKitLayout(
    std::string_view a_selectionKey, const KitEntry::Layout &a_layout,
    RE::Actor *a_actor, const std::vector<int> *a_candidateRowIndices,
    const bool a_allowRestrictedPreview) {
  auto stateLock = AcquireStateLock();
  if (!a_actor) {
    ClearPreview();
    return false;
  }

  if (previewSelectionKey_ == a_selectionKey &&
      previewActorFormID_ == a_actor->GetFormID() &&
      !previewNativeRows_.empty() && previewReplacesNativeRows_) {
    return true;
  }

  const auto projection = ProjectKitLayoutRows(
      a_layout, a_candidateRowIndices, KitLayoutFallbackMode::SlotTargetsOnly,
      std::nullopt, a_actor->GetFormID(), true, a_allowRestrictedPreview);

  std::vector<VariantWorkbenchRow> desiredRows;
  desiredRows.reserve(projection.rows.size());
  for (const auto &projectedRow : projection.rows) {
    if (!projectedRow.row.HasOverridesOrHideState()) {
      continue;
    }

    auto previewRow = projectedRow.row;
    previewRow.hideEquipped = false;
    desiredRows.push_back(std::move(previewRow));
  }


  // A replacement preview is still a catalog/outfit/kit operation. Include
  // every locked base-layer card for this actor even when the preview layout
  // has no row targeting that slot, so merely browsing cannot make a locked
  // hair/accessory disappear.
  const auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());
  for (const auto rowIndex : candidateRowIndices) {
    if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
      continue;
    }
    auto lockedRow = rows_[static_cast<std::size_t>(rowIndex)];
    std::erase_if(lockedRow.overrides,
                  [](const EquipmentWidgetItem &a_item) {
                    return !a_item.locked;
                  });
    if (lockedRow.overrides.empty()) {
      continue;
    }
    lockedRow.hideEquipped = false;
    const auto existing = std::ranges::find(desiredRows, lockedRow.key,
                                             &VariantWorkbenchRow::key);
    if (existing == desiredRows.end()) {
      desiredRows.push_back(std::move(lockedRow));
      continue;
    }
    for (const auto &lockedItem : lockedRow.overrides) {
      const auto duplicate =
          std::ranges::find(existing->overrides, lockedItem.formID,
                            &EquipmentWidgetItem::formID);
      if (duplicate == existing->overrides.end()) {
        existing->overrides.insert(existing->overrides.begin(), lockedItem);
      }
    }
  }

  if (desiredRows.empty()) {
    ClearPreview();
    return false;
  }

  ClearPreview();
  previewSelectionKey_ = std::string(a_selectionKey);
  previewActorFormID_ = a_actor->GetFormID();
  previewNativeRows_ = std::move(desiredRows);
  previewReplacesNativeRows_ = true;
  sfs::native::QueueArmorRefreshFor(a_actor);
  return true;
}

void VariantWorkbench::ClearPreview(const bool a_queueArmorRefresh) {
  auto stateLock = AcquireStateLock();
  if (previewSelectionKey_.empty() && previewNativeRows_.empty() &&
      !previewReplacesNativeRows_) {
    return;
  }

  auto *actor = previewActorFormID_ != 0
                    ? RE::TESForm::LookupByID<RE::Actor>(previewActorFormID_)
                    : nullptr;
  const bool hadNativePreview = !previewNativeRows_.empty();

  previewSelectionKey_.clear();
  previewActorFormID_ = 0;
  previewNativeRows_.clear();
  previewReplacesNativeRows_ = false;

  if (a_queueArmorRefresh && hadNativePreview && actor) {
    sfs::native::QueueArmorRefreshFor(actor);
  }
}

void VariantWorkbench::RefreshNativeArmorOverrides(
    std::vector<conditions::Definition> &a_conditions,
    const std::uint64_t a_conditionRevision, const bool a_force) {
  auto stateLock = AcquireStateLock();
  if (!a_force &&
      lastNativeRefreshWorkbenchRevision_ == nativeDisplayRevision_ &&
      lastNativeRefreshConditionRevision_ == a_conditionRevision) {
    return;
  }

  // Serialized automatic bindings store their stable anchor, while the
  // suppression bit is intentionally derived from the actor's current worn
  // state. Reconcile every actor represented by saved rows/rules before the
  // first post-load native refresh so no stale visible frame survives until a
  // later equip event or UI open.
  std::unordered_set<RE::FormID> actorFormIDsToSync;
  for (const auto &row : rows_) {
    if (row.ownerActorFormID != 0 && row.HasOverridesOrHideState()) {
      actorFormIDsToSync.insert(row.ownerActorFormID);
    }
  }
  for (const auto &rule : conditionalVisibilityRules_) {
    if (rule.ownerActorFormID != 0) {
      actorFormIDsToSync.insert(rule.ownerActorFormID);
    }
  }
  for (const auto actorFormID : actorFormIDsToSync) {
    if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID)) {
      SyncRowsFromActor(actor);
    }
  }

  lastNativeRefreshWorkbenchRevision_ = nativeDisplayRevision_;
  lastNativeRefreshConditionRevision_ = a_conditionRevision;

  std::unordered_set<RE::FormID> refreshedActorFormIDs;
  for (const auto &row : rows_) {
    if (row.ownerActorFormID == 0 || !row.HasOverridesOrHideState() ||
        !refreshedActorFormIDs.insert(row.ownerActorFormID).second) {
      continue;
    }
    if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(row.ownerActorFormID);
        actor != nullptr) {
      sfs::native::QueueArmorRefreshFor(actor);
    }
  }

  for (const auto &rule : conditionalVisibilityRules_) {
    if (rule.ownerActorFormID == 0 ||
        !refreshedActorFormIDs.insert(rule.ownerActorFormID).second) {
      continue;
    }
    if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(rule.ownerActorFormID);
        actor != nullptr) {
      sfs::native::QueueArmorRefreshFor(actor);
    }
  }

  // Workbench rows and conditional visibility rules are actor-owned. Their
  // owners were refreshed above; never fall back to scanning/refreshing nearby
  // actors merely because a generic condition (time, weather, location, etc.)
  // has no explicit GetIsReference target.
  static_cast<void>(a_conditions);
}

void VariantWorkbench::RefreshNativeArmorOverridesForActor(
    const RE::FormID a_actorFormID, const std::uint64_t a_conditionRevision) {
  auto stateLock = AcquireStateLock();
  if (a_actorFormID == 0) {
    return;
  }

  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
  if (!actor) {
    return;
  }

  // A workbench operation is owned by the actor selected in that workbench.
  // Reconcile and refresh only that actor, then consume the current workbench
  // revision so the end-of-frame global safety pass does not touch unrelated
  // actors.
  SyncRowsFromActor(actor);
  lastNativeRefreshWorkbenchRevision_ = nativeDisplayRevision_;
  lastNativeRefreshConditionRevision_ = a_conditionRevision;
  sfs::native::QueueArmorRefreshFor(actor);
}

void VariantWorkbench::
    RefreshNativeArmorOverridesForActorWithoutEquipmentSync(
        const RE::FormID a_actorFormID,
        const std::uint64_t a_conditionRevision) {
  auto stateLock = AcquireStateLock();
  if (a_actorFormID == 0) {
    return;
  }
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
  if (actor == nullptr) {
    return;
  }

  lastNativeRefreshWorkbenchRevision_ = nativeDisplayRevision_;
  lastNativeRefreshConditionRevision_ = a_conditionRevision;
  sfs::native::QueueArmorRefreshFor(actor);
}

void VariantWorkbench::Revert(const bool a_queueArmorRefresh) {
  auto stateLock = AcquireStateLock();
  ClearPreview(a_queueArmorRefresh);

  const auto previousRows = rows_;
  rows_.clear();
  conditionalVisibilityRules_.clear();
  equippedHiddenByActor_.clear();
  needsConditionOwnershipMigration_ = false;
  RebuildRowOrder();
  if (a_queueArmorRefresh) {
    // Interactive reset is a committed deletion of every appearance. Save
    // load/revert uses false and owns its broader runtime epoch teardown.
    InvalidateRemovedAppearanceAutomation(previousRows);
    sfs::native::QueuePlayerArmorRefresh();
  }
  MarkChanged();
}
} // namespace sfs::workbench
