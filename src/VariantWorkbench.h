#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include "EquipmentCatalog.h"
#include "conditions/Definition.h"
#include "workbench/Items.h"

#include <limits>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sfs::workbench {
[[nodiscard]] std::uint64_t AllocateVariantWorkbenchUiIdentity();
[[nodiscard]] std::uint64_t AllocateVariantWorkbenchRegistrationOrder();
void ObserveVariantWorkbenchRegistrationOrder(std::uint64_t a_order);

struct VariantWorkbenchRow {
  // Runtime-only identity used to keep a condition row at the same visual
  // position throughout one open menu session. It is deliberately not saved.
  std::uint64_t uiIdentity{AllocateVariantWorkbenchUiIdentity()};
  // Stable creation order used only when the condition table is reopened.
  // Unlike uiIdentity this is serialized so fitting rows and visibility-rule
  // rows retain one common chronological order across save/load.
  std::uint64_t registrationOrder{AllocateVariantWorkbenchRegistrationOrder()};
  std::string key;
  std::string sourceKey;
  std::optional<std::string> conditionId;
  RE::FormID ownerActorFormID{0};
  EquipmentWidgetItem equipped;
  std::vector<EquipmentWidgetItem> overrides;
  bool hideEquipped{false};
  bool isEquipped{false};

  [[nodiscard]] bool operator==(const VariantWorkbenchRow &a_other) const {
    return key == a_other.key && sourceKey == a_other.sourceKey &&
           conditionId == a_other.conditionId &&
           ownerActorFormID == a_other.ownerActorFormID &&
           equipped == a_other.equipped && overrides == a_other.overrides &&
           hideEquipped == a_other.hideEquipped &&
           isEquipped == a_other.isEquipped;
  }

  [[nodiscard]] bool HasCondition() const { return conditionId.has_value(); }
  [[nodiscard]] bool HasOwnerActor() const { return ownerActorFormID != 0; }
  [[nodiscard]] bool IsOwnedByActor(const RE::Actor *a_actor) const {
    return ownerActorFormID == 0 ||
           (a_actor != nullptr && a_actor->GetFormID() == ownerActorFormID);
  }
  [[nodiscard]] bool HasOverridesOrHideState() const {
    return hideEquipped || !overrides.empty();
  }

  [[nodiscard]] bool IsSlotRow() const { return equipped.IsSlot(); }
  [[nodiscard]] bool IsEquippedOnlyRow() const {
    return isEquipped && !IsSlotRow() && overrides.empty();
  }

  [[nodiscard]] bool IsActiveVisualSource() const {
    return HasVisibleOverrides();
  }

  [[nodiscard]] bool IsVisualConflictSource() const {
    return IsActiveVisualSource();
  }

  [[nodiscard]] std::uint64_t GetSelectionConflictSlotMask() const;
  // The workbench conflict mask may intentionally normalize special armor
  // combinations while retaining every occupied logical slot.
  // These display masks retain the armor's original occupied slots for card
  // layout and same-layer replacement without changing the saved item format.
  [[nodiscard]] std::uint64_t GetSelectionDisplaySlotMask() const;
  [[nodiscard]] bool IsAlwaysVisibleActualEquipment() const;

  [[nodiscard]] std::uint64_t
  GetOverrideVisualSlotMask(const EquipmentWidgetItem &a_item) const {
    return a_item.slotMask;
  }

  [[nodiscard]] std::uint64_t
  GetOverrideDisplaySlotMask(const EquipmentWidgetItem &a_item) const;
  [[nodiscard]] bool
  IsProtectedAppearance(const EquipmentWidgetItem &a_item) const;

  [[nodiscard]] std::uint64_t GetOverrideVisualSlotMask() const {
    std::uint64_t slotMask = 0;
    for (const auto &item : overrides) {
      slotMask |= GetOverrideVisualSlotMask(item);
    }
    return slotMask;
  }

  [[nodiscard]] bool
  IsOverrideAutomaticallySuppressed(const EquipmentWidgetItem &a_item) const {
    return !a_item.locked && a_item.automaticEquipmentSuppressed &&
           !a_item.automaticEquipmentUserVisible;
  }

  [[nodiscard]] bool IsOverrideHidden(const EquipmentWidgetItem &a_item) const {
    return IsProtectedAppearance(a_item) ||
           a_item.hidden || IsOverrideAutomaticallySuppressed(a_item);
  }

  [[nodiscard]] std::uint64_t GetOverrideDisplaySlotMask() const {
    std::uint64_t slotMask = 0;
    for (const auto &item : overrides) {
      slotMask |= GetOverrideDisplaySlotMask(item);
    }
    return slotMask;
  }

  [[nodiscard]] bool HasVisibleOverrides() const {
    for (const auto &item : overrides) {
      if (!IsOverrideHidden(item)) {
        return true;
      }
    }
    return false;
  }
};

enum class ConditionalVisibilityTargetKind : std::uint8_t { Actual, Fitting };

struct ConditionalVisibilityRule {
  std::uint64_t uiIdentity{AllocateVariantWorkbenchUiIdentity()};
  std::uint64_t registrationOrder{AllocateVariantWorkbenchRegistrationOrder()};
  std::string conditionId;
  RE::FormID ownerActorFormID{0};
  ConditionalVisibilityTargetKind targetKind{
      ConditionalVisibilityTargetKind::Fitting};
  EquipmentWidgetItem target;
  bool visibleWhenTrue{true};

  [[nodiscard]] bool operator==(
      const ConditionalVisibilityRule &a_other) const {
    return conditionId == a_other.conditionId &&
           ownerActorFormID == a_other.ownerActorFormID &&
           targetKind == a_other.targetKind && target == a_other.target &&
           visibleWhenTrue == a_other.visibleWhenTrue;
  }

  [[nodiscard]] bool IsOwnedByActor(const RE::Actor *a_actor) const {
    return ownerActorFormID == 0 ||
           (a_actor != nullptr && a_actor->GetFormID() == ownerActorFormID);
  }
};

class VariantWorkbench {
public:
  using StateLock = std::unique_lock<std::recursive_mutex>;

  // UI edits run from the Present path while equipment reconciliation and
  // native display rebuilds run from SKSE/game callbacks.  Callers which keep
  // references returned by GetRows()/GetConditionalVisibilityRules() must
  // hold this lock for the complete lifetime of those references.
  [[nodiscard]] StateLock AcquireStateLock() const {
    return StateLock(stateMutex_);
  }

  struct InitialEquippedState {
    std::unordered_set<RE::FormID> wornArmorForms;
    std::uint64_t occupiedSlotMask{0};
  };
  struct ConditionOverrideApplicationPlan {
    int skippedCount{0};
    std::vector<VariantWorkbenchRow> previewRows;

    [[nodiscard]] bool CanApply() const {
      for (const auto &row : previewRows) {
        if (row.HasOverridesOrHideState()) {
          return true;
        }
      }
      return false;
    }
  };

  [[nodiscard]] static InitialEquippedState
  BuildInitialEquippedState(RE::Actor *a_actor);
  void SyncRowsFromActor(RE::Actor *a_actor);
  void SyncRowsFromPlayer();
  void ClearAutomaticEquipmentVisibilityBindings();
  void
  ClearAutomaticEquipmentVisibilityBindingsForActor(RE::FormID a_actorFormID);
  // Deletes registered appearances whose occupied slots are currently
  // protected. Actual-equipment rows remain intact; conditional rows retain
  // their condition card as an empty action target.
  bool RemoveProtectedAppearanceRegistrations();
  void RebuildAutomaticEquipmentVisibilityBindingsForActor(
      RE::FormID a_actorFormID, bool a_clearExistingBindings = true);
  [[nodiscard]] bool
  IsPreviewingSelection(std::string_view a_selectionKey) const;
  [[nodiscard]] bool CanAcceptOverride(int a_targetRowIndex,
                                       const EquipmentWidgetItem &a_item,
                                       int a_sourceRowIndex = -1,
                                       int a_sourceItemIndex = -1) const;
  bool AddCatalogOverride(int a_targetRowIndex, RE::FormID a_formID);
  // Replaces the action side of an existing conditional fitting row without
  // creating a second condition row. A condition-only row is type-neutral, so
  // its slot is updated to the newly dropped armor.
  bool ReplaceConditionalFittingTarget(int a_targetRowIndex,
                                       RE::FormID a_formID);
  bool AddCatalogSelectionToWorkbench(
      const std::vector<RE::FormID> &a_formIDs,
      const std::vector<int> *a_candidateRowIndices = nullptr);
  bool ReplaceCatalogSelectionInWorkbench(
      const std::vector<RE::FormID> &a_formIDs,
      const std::vector<int> *a_candidateRowIndices = nullptr);
  bool RemoveOverridesOverlappingCatalogSelection(
      const std::vector<RE::FormID> &a_formIDs,
      const std::vector<int> *a_candidateRowIndices = nullptr);
  bool AddCatalogSelectionAsRows(
      const std::vector<RE::FormID> &a_formIDs,
      std::optional<std::string> a_conditionId,
      RE::FormID a_ownerActorFormID = 0,
      const InitialEquippedState *a_initialEquippedState = nullptr);
  bool AddSlotRow(std::uint64_t a_slotMask,
                  std::optional<std::string> a_conditionId,
                  RE::FormID a_ownerActorFormID = 0,
                  const InitialEquippedState *a_initialEquippedState = nullptr);
  [[nodiscard]] ConditionOverrideApplicationPlan
  PlanConditionOverrideApplication(const std::vector<RE::FormID> &a_formIDs,
                                   std::string_view a_conditionId,
                                   RE::Actor *a_sourceActor) const;
  [[nodiscard]] ConditionOverrideApplicationPlan
  PlanConditionOverrideApplication(const KitEntry::Layout &a_layout,
                                   std::string_view a_conditionId,
                                   RE::Actor *a_sourceActor) const;
  bool ApplyConditionOverridePlan(
      const ConditionOverrideApplicationPlan &a_plan,
      std::string_view a_conditionId, bool a_replaceConditionSet,
      const InitialEquippedState *a_initialEquippedState = nullptr);
  bool
  ApplyCatalogPreview(std::string_view a_selectionKey,
                      const std::vector<RE::FormID> &a_formIDs,
                      RE::Actor *a_actor = nullptr,
                      const std::vector<int> *a_candidateRowIndices = nullptr);
  void ClearPreview(bool a_queueArmorRefresh = true);
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool DeleteOverride(int a_rowIndex, int a_itemIndex);
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool SetOverrideHidden(int a_rowIndex, int a_itemIndex, bool a_hidden);
  // Locks one registered appearance against catalog/outfit/kit replacement
  // and automatic external suppression without changing its manual eye or
  // condition state.
  bool SetOverrideLocked(int a_rowIndex, int a_itemIndex, bool a_locked);
  // Applies only the temporary manual-show exception while Helmet Toggle 2
  // owns this card's runtime visibility.  The saved eye state is untouched.
  bool SetOverrideHeadgearToggleManualVisible(int a_rowIndex, int a_itemIndex,
                                              bool a_visible);
  // Converts a settled direct device-slot suppression into the actor's
  // ordinary per-item hidden state without recursively invalidating the
  // external automation layer which is already closing its latch.
  bool PersistOverrideHiddenFromExternalSuppression(int a_rowIndex,
                                                    int a_itemIndex);
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool SetOverrideAutomaticEquipmentUserVisible(int a_rowIndex, int a_itemIndex,
                                                bool a_visible);
  bool SetOverrideConditionId(int a_rowIndex, int a_itemIndex,
                              std::optional<std::string> a_conditionId);
  bool SetEquippedConditionId(int a_rowIndex,
                              std::optional<std::string> a_conditionId);
  // Updates the condition card of an existing conditional slot without
  // deleting its registered appearance/action cards.  An empty string means
  // an intentionally blank condition drop target.
  bool SetConditionAssignmentKeepRow(int a_rowIndex,
                                     std::string_view a_conditionId);
  bool SetEquippedHidden(int a_rowIndex, bool a_hidden);
  bool SetEquippedHiddenForActor(RE::FormID a_actorFormID, int a_rowIndex,
                                 bool a_hidden);
  bool DeleteRow(int a_rowIndex);
  // Removes the condition assignment while retaining the condition-layer row
  // and all of its equipment/appearance cards.
  bool ClearConditionAssignmentKeepRow(int a_rowIndex);
  bool
  AddConditionalVisibilityRule(std::string_view a_conditionId,
                               RE::FormID a_ownerActorFormID,
                               ConditionalVisibilityTargetKind a_targetKind,
                               RE::FormID a_formID, bool a_visibleWhenTrue,
                               std::uint64_t a_uiIdentity = 0,
                               std::uint64_t a_registrationOrder = 0);
  // Converts an empty actual/fitting visibility row into a conditional
  // appearance row while preserving its visual identity and creation order.
  bool ConvertConditionalVisibilityRuleToFittingRow(
      std::size_t a_ruleIndex, RE::FormID a_formID);
  bool DeleteConditionalVisibilityRule(std::size_t a_ruleIndex);
  bool SetConditionalVisibilityRuleConditionId(std::size_t a_ruleIndex,
                                               std::string_view a_conditionId);
  std::size_t ClearConditionalVisibilityRuleConditionsByConditionId(
      std::string_view a_conditionId);
  bool SetConditionalVisibilityRuleTarget(
      std::size_t a_ruleIndex, ConditionalVisibilityTargetKind a_targetKind,
      RE::FormID a_formID);
  bool SetConditionalVisibilityRuleVisible(std::size_t a_ruleIndex,
                                           bool a_visibleWhenTrue);
  std::size_t DeleteRowsByConditionId(std::string_view a_conditionId,
                                      bool a_onlyIfEmpty);
  // Removes only fully empty condition rows. A row with either a condition or
  // an action remains valid and must survive save/load.
  std::size_t PruneFullyEmptyConditionalRows();
  bool ResetAllRows(const std::vector<int> *a_candidateRowIndices = nullptr);
  [[nodiscard]] std::vector<RE::FormID> CollectEquippedArmorFormIDs(
      const std::vector<int> *a_candidateRowIndices = nullptr) const;
  [[nodiscard]] std::vector<RE::FormID>
  CollectOverrideArmorFormIDsFromEquippedRows(
      const std::vector<int> *a_candidateRowIndices = nullptr) const;
  [[nodiscard]] std::optional<KitEntry::Layout> CaptureEquippedKitLayout(
      const std::vector<int> *a_candidateRowIndices = nullptr) const;
  [[nodiscard]] std::optional<KitEntry::Layout>
  CaptureKitLayout(const std::vector<int> *a_candidateRowIndices = nullptr,
                   RE::Actor *a_actor = nullptr) const;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool InsertCatalogRow(
      RE::FormID a_formID, int a_targetRowIndex, bool a_insertAfter,
      std::optional<std::string> a_conditionId,
      RE::FormID a_ownerActorFormID = 0,
      const InitialEquippedState *a_initialEquippedState = nullptr);
  bool
  InsertSlotRow(std::uint64_t a_slotMask, int a_targetRowIndex,
                bool a_insertAfter, std::optional<std::string> a_conditionId,
                RE::FormID a_ownerActorFormID = 0,
                const InitialEquippedState *a_initialEquippedState = nullptr);
  bool
  ApplyKitLayout(const KitEntry::Layout &a_layout, bool a_replaceExisting,
                 std::optional<std::string> a_newSlotRowConditionId,
                 RE::FormID a_newSlotRowOwnerActorFormID = 0,
                 const InitialEquippedState *a_initialEquippedState = nullptr,
                 const std::vector<int> *a_candidateRowIndices = nullptr,
                 bool a_allowSlotFallback = false);
  bool
  PreviewKitLayout(std::string_view a_selectionKey,
                   const KitEntry::Layout &a_layout,
                   RE::Actor *a_actor = nullptr,
                   const std::vector<int> *a_candidateRowIndices = nullptr,
                   bool a_allowRestrictedPreview = false);
  void
  RefreshNativeArmorOverrides(std::vector<conditions::Definition> &a_conditions,
                              std::uint64_t a_conditionRevision = 0,
                              bool a_force = false);
  void
  RefreshNativeArmorOverridesForActor(RE::FormID a_actorFormID,
                                      std::uint64_t a_conditionRevision = 0);
  // Condition-table edits do not change worn inventory. Queue the native
  // redraw without normalizing rows from the actor, which would reorder the
  // table during the current UI session.
  void RefreshNativeArmorOverridesForActorWithoutEquipmentSync(
      RE::FormID a_actorFormID, std::uint64_t a_conditionRevision = 0);
  [[nodiscard]] nlohmann::json SerializeState() const;
  [[nodiscard]] bool
  DeserializeState(const nlohmann::json &a_root,
                   const std::optional<std::string> &a_missingConditionId,
                   std::string *a_error = nullptr,
                   bool a_queueArmorRefresh = true);
  bool MigrateLegacyConditionAssignments(
      std::vector<conditions::Definition> &a_conditions,
      RE::FormID a_defaultOwnerActorFormID);
  void ReplaceState(VariantWorkbench &&a_source);
  void Serialize(SKSE::SerializationInterface *a_skse) const;
  // Returns the ROWS record schema version which was actually consumed.
  // The caller uses this only for one-time release migration; zero means that
  // no supported workbench record was loaded.
  [[nodiscard]] std::uint32_t
  Deserialize(SKSE::SerializationInterface *a_skse,
              std::optional<std::string> a_missingConditionId);
  void Revert(bool a_queueArmorRefresh = true);

  [[nodiscard]] bool
  ResolveEquippedHiddenForActor(RE::Actor *a_actor,
                                const VariantWorkbenchRow &a_row) const;
  [[nodiscard]] bool ResolveEquippedIndividualHiddenForActor(
      RE::Actor *a_actor, const VariantWorkbenchRow &a_row) const;
  [[nodiscard]] const std::vector<VariantWorkbenchRow> &GetRows() const {
    return rows_;
  }
  [[nodiscard]] const std::vector<ConditionalVisibilityRule> &
  GetConditionalVisibilityRules() const {
    return conditionalVisibilityRules_;
  }
  [[nodiscard]] const std::vector<VariantWorkbenchRow> *
  GetNativePreviewRowsForActor(RE::FormID a_actorFormID) const {
    if (previewSelectionKey_.empty() || previewNativeRows_.empty() ||
        previewActorFormID_ != a_actorFormID) {
      return nullptr;
    }

    return &previewNativeRows_;
  }
  [[nodiscard]] bool
  IsNativePreviewReplacingRowsForActor(RE::FormID a_actorFormID) const {
    return previewReplacesNativeRows_ && !previewSelectionKey_.empty() &&
           !previewNativeRows_.empty() && previewActorFormID_ == a_actorFormID;
  }
  [[nodiscard]] bool IsNativePreviewSelectionForActor(
      RE::FormID a_actorFormID, std::string_view a_selectionKeyPrefix) const {
    return previewActorFormID_ == a_actorFormID &&
           previewSelectionKey_.starts_with(a_selectionKeyPrefix) &&
           !previewNativeRows_.empty();
  }
  [[nodiscard]] std::size_t GetRowCount() const { return rows_.size(); }
  [[nodiscard]] bool
  HasRegisteredAppearancesForActor(RE::FormID a_actorFormID) const;
  [[nodiscard]] bool
  HasDisplayStateForActor(RE::FormID a_actorFormID) const;
  [[nodiscard]] bool
  HasActualEquipmentLinkedAppearancesForActor(RE::FormID a_actorFormID) const;
  [[nodiscard]] std::uint64_t
  GetActualEquipmentLinkedSlotMaskForActor(RE::FormID a_actorFormID) const;
  [[nodiscard]] std::uint64_t
  GetLockedAppearanceSlotMaskForActor(RE::FormID a_actorFormID) const;
  [[nodiscard]] bool IsRegisteredAppearanceLockedForActor(
      RE::FormID a_actorFormID, RE::FormID a_appearanceFormID,
      std::uint64_t a_visualSlotMask = 0) const;
  // Resolves an external headgear controller's real equipment slots to the
  // registered-appearance slots that currently follow them.  This keeps
  // compatibility overlays (Helmet Toggle 2) on the same per-card policy as
  // mod-settings linking, vanilla anchors, and direct slot editing.
  [[nodiscard]] std::uint32_t GetHeadgearToggleFittingSlotMaskForActor(
      RE::FormID a_actorFormID, std::uint32_t a_controllerSlotMask) const;
  [[nodiscard]] std::uint64_t GetRevision() const { return revision_; }

private:
  struct PlannedCatalogAssignment {
    int rowIndex{-1};
    RE::FormID armorFormID{0};
  };
  struct PlannedSlotFallbackAssignment {
    std::uint64_t slotMask{0};
    RE::FormID armorFormID{0};
  };
  enum class KitLayoutFallbackMode : std::uint8_t {
    None,
    SlotTargetsOnly,
    AnyTargetSlot
  };
  struct ProjectedKitLayoutRow {
    VariantWorkbenchRow row;
    std::size_t priorityRowIndex{0};
  };
  struct KitLayoutProjection {
    std::vector<ProjectedKitLayoutRow> rows;
  };

  [[nodiscard]] static std::vector<int>
  BuildCandidateRowIndices(const std::vector<int> *a_candidateRowIndices,
                           std::size_t a_rowCount);
  [[nodiscard]] bool
  ResolveCatalogArmors(const std::vector<RE::FormID> &a_formIDs,
                       std::vector<const RE::TESObjectARMO *> &a_armors) const;
  [[nodiscard]] int FindBestCatalogTargetRowIndex(
      const EquipmentWidgetItem &a_item, bool a_requireAcceptable,
      const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
      const std::vector<int> *a_candidateRowIndices) const;
  [[nodiscard]] bool CanAcceptOverrideWithPendingAssignments(
      int a_targetRowIndex, const EquipmentWidgetItem &a_item,
      const std::vector<PlannedCatalogAssignment> &a_pendingAssignments) const;
  [[nodiscard]] int FindBestItemTargetRowIndexBySlotMask(
      std::uint64_t a_targetSlotMask, bool a_requireAcceptable,
      const EquipmentWidgetItem *a_item,
      const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
      const std::vector<int> *a_candidateRowIndices) const;
  [[nodiscard]] bool
  PlanCatalogAssignments(const std::vector<RE::FormID> &a_formIDs,
                         std::vector<PlannedCatalogAssignment> &a_assignments,
                         const std::vector<int> *a_candidateRowIndices) const;
  [[nodiscard]] bool PlanSlotFallbackAssignments(
      const std::vector<RE::FormID> &a_formIDs,
      std::vector<PlannedSlotFallbackAssignment> &a_assignments,
      int &a_skippedCount) const;
  [[nodiscard]] static std::vector<const RE::TESObjectARMO *>
  ResolveKitLayoutOverrideArmors(const KitEntry::LayoutRow &a_layoutRow,
                                 bool a_allowRestrictedPreview = false);
  [[nodiscard]] int FindKitLayoutTargetRowIndex(
      const KitEntry::LayoutRow &a_layoutRow,
      const std::vector<int> &a_candidateRowIndices) const;
  [[nodiscard]] KitLayoutProjection
  ProjectKitLayoutRows(const KitEntry::Layout &a_layout,
                       const std::vector<int> *a_candidateRowIndices,
                       KitLayoutFallbackMode a_fallbackMode,
                       std::optional<std::string> a_fallbackConditionId,
                       RE::FormID a_fallbackOwnerActorFormID,
                       bool a_replaceExisting,
                       bool a_allowRestrictedPreview = false) const;
  static bool AppendOverrideItem(std::vector<EquipmentWidgetItem> &a_overrides,
                                 const RE::TESObjectARMO *a_overrideArmor,
                                 bool a_allowRestrictedPreview = false);
  [[nodiscard]] ConditionOverrideApplicationPlan
  PlanKitLayoutConditionOverrideApplication(
      const KitEntry::Layout &a_layout, std::string_view a_conditionId,
      const std::vector<int> *a_candidateRowIndices, bool a_allowSlotFallback,
      RE::FormID a_ownerActorFormID) const;
  [[nodiscard]] std::vector<VariantWorkbenchRow>
  BuildCatalogRows(const std::vector<RE::FormID> &a_formIDs,
                   std::optional<std::string> a_conditionId,
                   RE::FormID a_ownerActorFormID,
                   const InitialEquippedState *a_initialEquippedState) const;
  [[nodiscard]] std::optional<VariantWorkbenchRow>
  BuildSlotRow(std::uint64_t a_slotMask,
               std::optional<std::string> a_conditionId,
               RE::FormID a_ownerActorFormID,
               const InitialEquippedState *a_initialEquippedState,
               bool a_allowRestrictedPreview = false) const;
  [[nodiscard]] int
  FindBestCatalogTargetRowIndex(const EquipmentWidgetItem &a_item,
                                bool a_requireAcceptable) const;
  [[nodiscard]] bool
  NormalizeOverrideRowsForActor(RE::FormID a_ownerActorFormID);
  [[nodiscard]] std::uint64_t GetLockedAppearanceSlotMaskForCandidateRows(
      const std::vector<int> *a_candidateRowIndices) const;
  [[nodiscard]] std::optional<bool>
  GetEquippedHiddenForActor(RE::FormID a_actorFormID,
                            std::string_view a_rowKey) const;
  void PersistCurrentEquippedHiddenStateForActor(RE::FormID a_actorFormID);
  void RebuildRowOrder();
  void MarkChanged(bool a_affectsNativeDisplay = true);

  mutable std::recursive_mutex stateMutex_;
  std::vector<VariantWorkbenchRow> rows_;
  std::vector<ConditionalVisibilityRule> conditionalVisibilityRules_;
  std::vector<std::string> rowOrder_;
  std::unordered_map<RE::FormID, std::unordered_map<std::string, bool>>
      equippedHiddenByActor_;
  RE::FormID lastSyncedActorFormID_{0};
  std::uint64_t revision_{0};
  std::uint64_t nativeDisplayRevision_{0};
  std::string previewSelectionKey_;
  RE::FormID previewActorFormID_{0};
  std::vector<VariantWorkbenchRow> previewNativeRows_;
  bool previewReplacesNativeRows_{false};
  bool needsConditionOwnershipMigration_{false};
  std::uint64_t lastNativeRefreshWorkbenchRevision_{
      (std::numeric_limits<std::uint64_t>::max)()};
  std::uint64_t lastNativeRefreshConditionRevision_{
      (std::numeric_limits<std::uint64_t>::max)()};
};
} // namespace sfs::workbench
