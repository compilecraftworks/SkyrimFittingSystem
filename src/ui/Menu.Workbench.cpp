#include "Menu.h"

#include "ui/Localization.h"
namespace sfs {

bool Menu::ApplyWorkbenchEmptyDrop(
    const DraggedEquipmentPayload &a_dragPayload) {
  const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
  if (a_dragPayload.sourceKind ==
      static_cast<std::uint32_t>(DragSourceKind::Catalog)) {
    return ApplyCatalogSelectionAsFittingOverrides(
        std::vector<RE::FormID>{a_dragPayload.formID}, false);
  }
  if (a_dragPayload.sourceKind ==
      static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
    return workbench_.AddSlotRow(a_dragPayload.slotMask,
                                 ResolveNewWorkbenchRowConditionId(),
                                 ResolveNewWorkbenchRowOwnerActorFormID(),
                                 &initialEquippedState);
  }
  return false;
}

void Menu::DrawVariantWorkbenchPane() {
  std::unordered_set<RE::FormID> pendingActorSyncs;
  {
    std::lock_guard lock(pendingWorkbenchActorSyncMutex_);
    pendingActorSyncs.swap(pendingWorkbenchActorSyncs_);
  }
  for (const auto actorFormID : pendingActorSyncs) {
    SyncWorkbenchRowsForActor(actorFormID);
  }

  EnsureWorkbenchRowsSyncedForPreviewActor();
  EnsureWorkbenchDerivedState();
  if (!IsWorkbenchFilterSelectionValid()) {
    workbenchFilter_ = {};
    EnsureWorkbenchDerivedState();
  }

  blockWorkbenchRowActionsThisFrame_ = DrawWorkbenchFilterBar();
  ImGui::Separator();
  DrawWorkbenchToolbar();

  auto *previewActor = ResolveWorkbenchPreviewActor();
  const auto actorFormID =
      previewActor != nullptr ? previewActor->GetFormID() : RE::FormID{0};
  const auto *nativePreviewRows =
      actorFormID != 0
          ? workbench_.GetNativePreviewRowsForActor(actorFormID)
          : nullptr;
  const bool showGeneratedKitPreview =
      nativePreviewRows != nullptr &&
      workbench_.IsNativePreviewReplacingRowsForActor(actorFormID) &&
      workbench_.IsNativePreviewSelectionForActor(actorFormID,
                                                  "kit-generator:");
  if (showGeneratedKitPreview) {
    DrawGeneratedKitPreviewWorkbenchTable(*nativePreviewRows);
  } else {
    const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
    DrawWorkbenchTable(visibleRowIndices);
  }
  DrawWorkbenchDyePopup();
  blockWorkbenchRowActionsThisFrame_ = false;
}
void Menu::ResetTransientWorkbenchUiState(const bool a_queueArmorRefresh) {
  workbenchFilter_ = {};
  workbenchActorSyncState_ = {};
  workbenchDerived_ = {};
  workbench_.ClearPreview(a_queueArmorRefresh);
  applyWithConditionOverridesDialog_ = {};
  pendingKitListMoveDelta_ = 0;
  pendingKitListApply_ = false;
  pendingKitListPreview_ = false;
  pendingKitListBack_ = false;
  pendingSlotCreations_.clear();
}
} // namespace sfs
