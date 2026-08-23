#pragma once

#include "EquipmentCatalog.h"
#include "Keycode.h"
#include "ThemeConfig.h"
#include "VariantWorkbench.h"
#include "components/EquipmentWidget.h"
#include "conditions/Store.h"
#include "imgui.h"
#include "ui/ConditionData.h"
#include "ui/Localization.h"
#include "ui/MenuCharacterPresentation.h"
#include "ui/WorkbenchConflicts.h"
#include "ui/catalog/DerivedState.h"
#include "ui/catalog/PaneState.h"
#include "ui/components/EditableCombo.h"
#include "ui/conditions/EditorState.h"
#include "ui/conditions/PaneState.h"
#include "ui/workbench/FilterState.h"
#include "ui/workbench/Tooltips.h"
#include "workbench/AutomaticEquipmentVisibility.h"

#include <array>
#include <chrono>
#include <limits>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
namespace SKSE {
class SerializationInterface;
}

namespace sfs {
class MenuHost;

class Menu {
public:
  struct FontOption {
    std::string label;
    std::string path;
    bool isBundled{false};
  };

  static Menu *GetSingleton();

  void Init(IDXGISwapChain *a_swapChain, ID3D11Device *a_device,
            ID3D11DeviceContext *a_context);
  void Draw();
  void Open();
  void Close();
  void NotifyWindowShutdown();
  void Toggle();
  void SetGameDataLoaded(bool a_loaded);
  [[nodiscard]] bool IsEnabled() const { return enabled_; }
  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  [[nodiscard]] bool IsGameDataLoaded() const { return gameDataLoaded_; }
  [[nodiscard]] bool PauseGameWhenOpen() const { return pauseGameWhenOpen_; }
  [[nodiscard]] bool HideRealEquipmentWithFitting() const {
    return hideRealEquipmentWithFitting_;
  }
  [[nodiscard]] bool
  HideRealEquipmentWithFittingForActor(RE::Actor *a_actor) const;
  void SetHideRealEquipmentWithFittingForActor(RE::Actor *a_actor, bool a_hide);
  [[nodiscard]] bool HideFittingOverridesForActor(RE::Actor *a_actor) const;
  void SetHideFittingOverridesForActor(RE::Actor *a_actor, bool a_hide);
  [[nodiscard]] bool WantsTextInput() const { return wantTextInput_; }
  [[nodiscard]] bool QueueSmoothScroll(float a_deltaY);
  [[nodiscard]] std::string GetToggleKeyLabel() const;
  [[nodiscard]] std::uint32_t GetToggleKey() const { return toggleKey_; }
  [[nodiscard]] std::uint32_t GetToggleModifier() const {
    return toggleModifier_;
  }
  [[nodiscard]] bool IsCapturingToggleKey() const {
    return awaitingToggleKeyCapture_;
  }
  void OpenToggleKeyCapture();
  void CloseToggleKeyCapture();
  void HandleToggleKeyCapture(std::uint32_t a_scanCode,
                              std::uint32_t a_modifierScanCode);
  [[nodiscard]] bool QueueKitListMove(int a_delta);
  [[nodiscard]] bool QueueKitListApply();
  [[nodiscard]] bool QueueKitListPreview();
  [[nodiscard]] bool QueueKitListBack();
  [[nodiscard]] bool QueueKitListNextPane();
  void ConsumeKitListCommands(int &a_moveDelta, bool &a_applyRequested,
                              bool &a_previewRequested);
  [[nodiscard]] bool ConsumeKitListBack();
  [[nodiscard]] bool ConsumeKitListNextPane();
  void QueueWorkbenchRowsSyncForActor(RE::FormID a_actorFormID);
  void SyncWorkbenchRowsForActor(RE::FormID a_actorFormID);
  void SetFittingOverridesHiddenForActorSlots(RE::Actor *a_actor,
                                              std::uint64_t a_slotMask,
                                              bool a_hidden);
  void ResetTransientWorkbenchUiState(bool a_queueArmorRefresh = true);
  // Every pre-v1.4 ROWS record (v2-v9) predates the final global policy and
  // starts on mod-configured slot linking. In particular, v1.2 uses v7/v8 and
  // v1.3 uses v9. Current v1.4+ records leave the saved setting untouched.
  void MigrateExternalStripLinkModeForWorkbenchVersion(
      std::uint32_t a_workbenchSerializationVersion);
  [[nodiscard]] workbench::VariantWorkbench &GetWorkbench() {
    return workbench_;
  }
  using ConditionStateLock = std::unique_lock<std::recursive_mutex>;
  [[nodiscard]] ConditionStateLock AcquireConditionStateLock() const {
    return ConditionStateLock(conditionStateMutex_);
  }
  [[nodiscard]] std::vector<ui::conditions::Definition> &GetConditions() {
    return ConditionDefinitions();
  }
  [[nodiscard]] const std::vector<ui::conditions::Definition> &
  GetConditions() const {
    return ConditionDefinitions();
  }
  [[nodiscard]] nlohmann::json SerializeConditionState() const;
  [[nodiscard]] bool DeserializeConditionState(const nlohmann::json &a_root,
                                               std::string *a_error = nullptr);
  void SerializeConditions(SKSE::SerializationInterface *a_skse) const;
  void DeserializeConditions(SKSE::SerializationInterface *a_skse);
  void RevertConditions();
  void
  SerializeActorVisibilitySettings(SKSE::SerializationInterface *a_skse) const;
  void DeserializeActorVisibilitySettings(SKSE::SerializationInterface *a_skse);
  void RevertActorVisibilitySettings();
  [[nodiscard]] bool PreviewExternalGeneratedArmorForms(
      std::string_view a_selectionKey,
      const std::vector<RE::FormID> &a_formIDs);
  void ClearExternalGeneratedKitPreview();
  void RefreshExternalGeneratedKits();
  // Called only by the optional Grid Inventory Costume bridge.  This edits the
  // saved player workbench registrations; it is not a temporary renderer
  // overlay and does not modify actual-equipment or condition definitions.
  [[nodiscard]] bool ApplyGridInventoryCostume(const std::uint32_t *a_formIDs,
                                                std::uint32_t a_count);
  [[nodiscard]] bool ClearGridInventoryCostume();

private:
  static constexpr const char *kDefaultFontPath = "C:/Windows/Fonts/malgun.ttf";
  static constexpr const char *kDefaultIconFontPath =
      "Data/Interface/SkyrimFittingSystem/fonts/lucide.ttf";
  static constexpr const char *kBundledFontDirectory =
      "Data/Interface/SkyrimFittingSystem/fonts";
  static constexpr const char *kLocaleDirectory =
      "Data/Interface/SkyrimFittingSystem/locales";
  static constexpr const char *kDefaultLocaleId = "en";
  static constexpr int kDefaultFontSizePixels = 18;
  static constexpr int kMinFontSizePixels = 8;
  static constexpr int kMaxFontSizePixels = 48;

  enum class DragSourceKind : std::uint32_t {
    Catalog = 1,
    Row = 3,
    SlotCatalog = 4,
    ConditionalRow = 5,
    ConditionalVisibilityRule = 6
  };

  struct DraggedEquipmentPayload {
    std::uint32_t sourceKind{0};
    std::int32_t rowIndex{-1};
    std::int32_t itemIndex{-1};
    RE::FormID formID{0};
    std::uint64_t slotMask{0};
    std::uint64_t sourceUiIdentity{0};
  };

  enum class ConditionDragSourceKind : std::uint32_t {
    Catalog = 0,
    ConditionalRow = 1,
    ConditionalVisibilityRule = 2
  };

  struct DraggedConditionPayload {
    std::array<char, 64> conditionId{};
    std::uint32_t sourceKind{
        static_cast<std::uint32_t>(ConditionDragSourceKind::Catalog)};
    std::int32_t sourceIndex{-1};
    std::uint64_t sourceUiIdentity{0};
  };

  struct PendingSlotCreationState {
    bool active{false};
    std::optional<std::string> conditionId;
    RE::FormID ownerActorFormID{0};
    RE::FormID overrideFormID{0};
    std::optional<workbench::ConditionalVisibilityTargetKind>
        visibilityTargetKind;
    RE::FormID visibilityTargetFormID{0};
    bool visibleWhenTrue{true};
  };

  enum class VisibilityState : std::uint8_t { Closed, Opening, Open, Closing };
  enum class KitListCommand : std::uint8_t {
    MoveUp,
    MoveDown,
    Apply,
    Preview,
    Back,
    NextPane
  };
  enum class WorkbenchSortColumn : std::uint8_t { None, Left, Right };
  struct WorkbenchSortState {
    WorkbenchSortColumn column{WorkbenchSortColumn::None};
    bool ascending{true};
  };
  using KitCreationSource = ui::catalog::KitCreationSource;
  using ConditionEditorState = ui::conditions::editor::State;
  using WorkbenchFilterState = ui::workbench::FilterState;
  using WorkbenchFilterOption = ui::workbench::FilterOption;
  struct ConditionOverrideApplicationSource {
    std::string name;
    std::vector<RE::FormID> formIDs;
    std::optional<KitEntry::Layout> layout;
    bool replaceConditionSet{false};

    [[nodiscard]] bool HasPayload() const {
      return layout.has_value() || !formIDs.empty();
    }
  };

  Menu() = default;
  friend class MenuHost;

  void ApplyStyle();
  void LoadUserSettings();
  void SaveUserSettings() const;
  void LoadFavorites();
  void SaveFavorites() const;
  void RefreshAvailableFonts();
  void NormalizeSelectedLocaleId();
  void NormalizeSelectedFontPath();
  void RebuildFontAtlas();
  void SyncAllowTextInput();
  void UpdateVisibilityAnimation(float a_deltaTime);
  void QueueHideMessage();
  void ApplySmoothScroll();
  void OnMenuShow();
  void OnMenuHide();
  void HandleCancel();
  [[nodiscard]] bool HandleMenuUserEvent(std::string_view a_eventName);
  [[nodiscard]] bool QueueKitListCommand(KitListCommand a_command);
  void DrawWindow();
  void DrawCatalogWindow();
  void DrawCatalogHostControls(bool a_inPopout);
  void DrawCatalogHostBody(bool a_drawBodyChild);
  void DrawCatalogPaneBody();
  void QueueCatalogRefresh(
      ui::catalog::RefreshMode a_mode = ui::catalog::RefreshMode::Full);
  void UpdateCatalogRefresh();
  void DrawCatalogLoadingPane() const;
  void DrawCatalogFilters();
  [[nodiscard]] sfs::ui::components::EquipmentWidgetResult
  DrawCatalogDragWidget(const workbench::EquipmentWidgetItem &a_item,
                        DragSourceKind a_sourceKind);
  [[nodiscard]] bool DrawGearTab();
  [[nodiscard]] bool DrawGearCatalogTable();
  void DrawVariantWorkbenchPane();
  [[nodiscard]] bool DrawWorkbenchFilterBar();
  void DrawWorkbenchToolbar();
  void OpenStripLinkDialog();
  void DrawStripLinkDialog();
  void EnsureStripLinkSilhouetteTexture();
  void DrawWorkbenchEmptyState(const char *a_tableId, const char *a_targetId,
                               const char *a_message);
  void DrawWorkbenchSortableHeader(const char *a_label, const char *a_id,
                                   WorkbenchSortState &a_state,
                                   WorkbenchSortColumn a_column);
  void DrawWorkbenchTable(const std::vector<int> &a_visibleRowIndices);
  void DrawGeneratedKitPreviewWorkbenchTable(
      const std::vector<workbench::VariantWorkbenchRow> &a_previewRows);
  [[nodiscard]] bool DrawSlotCreationRow(bool a_drawConditionSectionHeader,
                                         float a_stickyHeaderBottomY);
  void OpenSlotCreationRow();
  bool TryCommitSlotCreationRow(std::size_t a_index);
  [[nodiscard]] bool DrawOutfitTab();
  [[nodiscard]] bool DrawKitTab();
  [[nodiscard]] bool DrawSlotTab();
  [[nodiscard]] bool DrawConditionTab();
  [[nodiscard]] bool DrawConditionCatalogTable();
  void DrawOptionsTab();
  void DrawConditionEditorDialog();
  void DrawApplyWithConditionOverridesDialog();
  [[nodiscard]] bool ExportSaveDataJson(std::string_view a_path,
                                        std::string &a_error) const;
  [[nodiscard]] bool ImportSaveDataJson(std::string_view a_path,
                                        std::string &a_error);
  void ResetSaveDataPath();
  [[nodiscard]] bool DrawConditionEditorClauseTable(
      ConditionEditorState &a_editor,
      const std::vector<ui::components::EditableDropdownItem<std::string>>
          &a_conditionFunctionItems,
      float a_editButtonWidth, float a_deleteButtonWidth,
      float a_actionsColumnWidth);
  void DrawCreateKitDialog();
  void DrawDeleteKitDialog();
  void DrawRenameKitDialog();
  bool ApplyWorkbenchEmptyDrop(const DraggedEquipmentPayload &a_dragPayload);
  void ClearCatalogSelection();
  [[nodiscard]] std::string BuildFavoriteKey(ui::catalog::BrowserTab a_tab,
                                             std::string_view a_id) const;
  [[nodiscard]] bool IsFavorite(ui::catalog::BrowserTab a_tab,
                                std::string_view a_id) const;
  void SetFavorite(ui::catalog::BrowserTab a_tab, std::string_view a_id,
                   bool a_favorite);
  [[nodiscard]] std::string BuildFavoriteLabel(std::string_view a_name,
                                               bool a_favorite) const;
  void SyncSelectedSlotFilters();
  [[nodiscard]] bool HasAnySelectedSlotFilter() const;
  [[nodiscard]] bool
  MatchesSelectedSlotsOr(const std::vector<std::string> &a_slots) const;
  [[nodiscard]] bool MatchesSelectedSlotsAnd(std::uint64_t a_slotMask) const;
  [[nodiscard]] std::string BuildSelectedSlotPreview() const;

  [[nodiscard]] bool MatchesGearFilters(const GearEntry &a_entry) const;
  [[nodiscard]] bool MatchesOutfitFilters(const OutfitEntry &a_entry) const;
  [[nodiscard]] bool MatchesKitFilters(const KitEntry &a_entry) const;
  [[nodiscard]] std::optional<KitEntry::Layout>
  BuildSlotFallbackLayoutFromArmorForms(
      const std::vector<RE::FormID> &a_formIDs) const;
  [[nodiscard]] bool ApplyCatalogSelectionAsFittingOverrides(
      const std::vector<RE::FormID> &a_formIDs, bool a_replaceExisting);
  [[nodiscard]] bool PreviewCatalogSelectionAsFittingOverrides(
      std::string_view a_selectionKey,
      const std::vector<RE::FormID> &a_formIDs);
  [[nodiscard]] std::vector<RE::FormID>
  BuildOutfitFittingFormIDs(const OutfitEntry &a_entry) const;
  void AddGearEntryToWorkbench(const GearEntry &a_entry);
  void AddOutfitEntryToWorkbench(const OutfitEntry &a_entry);
  void AddKitEntryToWorkbench(const KitEntry &a_entry);
  void DrawApplyWithConditionMenu(
      const ConditionOverrideApplicationSource &a_source);
  void OpenApplyWithConditionOverridesDialog(
      const ConditionOverrideApplicationSource &a_source,
      const ui::conditions::Definition &a_condition);
  [[nodiscard]] RE::Actor *ResolveApplyWithConditionActor(
      std::string_view a_conditionId,
      const ConditionOverrideApplicationSource &a_source,
      workbench::VariantWorkbench::ConditionOverrideApplicationPlan &a_plan);
  void
  OpenCreateKitDialog(KitCreationSource a_source,
                      const std::vector<int> *a_candidateRowIndices = nullptr);
  void OpenDeleteKitDialog(const KitEntry &a_entry);
  void OpenRenameKitDialog(const KitEntry &a_entry);
  [[nodiscard]] bool SavePendingKit();
  [[nodiscard]] bool DeletePendingKit();
  [[nodiscard]] bool RenamePendingKit();
  void PreviewGearEntry(const GearEntry &a_entry);
  void PreviewOutfitEntry(const OutfitEntry &a_entry);
  void PreviewKitEntry(const KitEntry &a_entry);
  void EnsureWorkbenchRowsSyncedForPreviewActor();
  void SyncWorkbenchRowsForCurrentFilter();
  void ValidateWorkbenchFilterSelection();
  void EnsureWorkbenchDerivedState();
  void RebuildWorkbenchDerivedState();
  void BumpConditionStoreRevision();
  [[nodiscard]] bool IsWorkbenchFilterSelectionValid() const;
  [[nodiscard]] const std::vector<int> &BuildVisibleWorkbenchRowIndices();
  [[nodiscard]] std::vector<int> BuildWorkbenchTargetRowIndices(
      const std::optional<std::string> &a_conditionId);
  [[nodiscard]] bool
  MatchesWorkbenchFilter(const workbench::VariantWorkbenchRow &a_row);
  void ApplyInitialWorkbenchFilterSelection();
  void RefreshWorkbenchActorCandidates();
  void
  BuildWorkbenchFilterOptions(std::vector<WorkbenchFilterOption> &a_options);
  [[nodiscard]] std::optional<std::string> ResolveNewWorkbenchRowConditionId();
  [[nodiscard]] RE::FormID ResolveNewWorkbenchRowOwnerActorFormID();
  [[nodiscard]] RE::Actor *ResolveWorkbenchPreviewActor();
  [[nodiscard]] workbench::VariantWorkbench::InitialEquippedState
  BuildWorkbenchInitialEquippedState();
  void EnsureDefaultConditions();
  [[nodiscard]] int AllocateConditionEditorWindowSlot() const;
  void OpenNewConditionDialog();
  void OpenConditionEditorDialog(std::size_t a_index);
  void OpenConditionEditorDialogById(std::string_view a_conditionId);
  [[nodiscard]] bool AssignConditionToTopEmptyWorkbenchCard(
      std::string_view a_conditionId);
  [[nodiscard]] bool SaveConditionEditor(ConditionEditorState &a_editor);
  [[nodiscard]] std::size_t CountCatalogConditions() const;
  [[nodiscard]] bool IsWorkbenchSelectableCondition(
      const ui::conditions::Definition &a_condition) const;
  [[nodiscard]] std::vector<const GearEntry *> BuildFilteredGear() const;
  [[nodiscard]] std::vector<const OutfitEntry *> BuildFilteredOutfits() const;
  [[nodiscard]] std::vector<const KitEntry *> BuildFilteredKits() const;
  void SortGearRows(std::vector<const GearEntry *> &a_rows,
                    ImGuiTableSortSpecs *a_sortSpecs) const;
  void SortOutfitRows(std::vector<const OutfitEntry *> &a_rows,
                      ImGuiTableSortSpecs *a_sortSpecs) const;
  void SortKitRows(std::vector<const KitEntry *> &a_rows,
                   ImGuiTableSortSpecs *a_sortSpecs) const;
  [[nodiscard]] const std::vector<const GearEntry *> &GetFilteredGearRows();
  [[nodiscard]] const std::vector<const GearEntry *> &
  GetSortedGearRows(ImGuiTableSortSpecs *a_sortSpecs);
  [[nodiscard]] const std::vector<const OutfitEntry *> &GetFilteredOutfitRows();
  [[nodiscard]] const std::vector<const OutfitEntry *> &
  GetSortedOutfitRows(ImGuiTableSortSpecs *a_sortSpecs);
  [[nodiscard]] const std::vector<const KitEntry *> &GetFilteredKitRows();
  [[nodiscard]] const std::vector<const KitEntry *> &
  GetSortedKitRows(ImGuiTableSortSpecs *a_sortSpecs);
  void InvalidateCatalogDerivedState();
  [[nodiscard]] ui::catalog::BrowserState &CatalogBrowserState() {
    return catalogPane_.browser;
  }
  [[nodiscard]] const ui::catalog::BrowserState &CatalogBrowserState() const {
    return catalogPane_.browser;
  }
  [[nodiscard]] ui::catalog::CreateKitDialogState &CreateKitDialogState() {
    return catalogPane_.createKitDialog;
  }
  [[nodiscard]] ui::catalog::DeleteKitDialogState &DeleteKitDialogState() {
    return catalogPane_.deleteKitDialog;
  }
  [[nodiscard]] ui::catalog::RenameKitDialogState &RenameKitDialogState() {
    return catalogPane_.renameKitDialog;
  }
  [[nodiscard]] ui::conditions::PaneState &ConditionsPaneState() {
    return conditionsPane_;
  }
  [[nodiscard]] const ui::conditions::PaneState &ConditionsPaneState() const {
    return conditionsPane_;
  }
  [[nodiscard]] std::vector<ui::conditions::Definition> &
  ConditionDefinitions() {
    return conditionStore_.definitions;
  }
  [[nodiscard]] const std::vector<ui::conditions::Definition> &
  ConditionDefinitions() const {
    return conditionStore_.definitions;
  }
  [[nodiscard]] int &NextConditionId() {
    return conditionStore_.nextConditionId;
  }
  [[nodiscard]] const int &NextConditionId() const {
    return conditionStore_.nextConditionId;
  }
  [[nodiscard]] std::vector<ConditionEditorState> &ConditionEditors() {
    return conditionsPane_.editors;
  }
  [[nodiscard]] const std::vector<ConditionEditorState> &
  ConditionEditors() const {
    return conditionsPane_.editors;
  }
  [[nodiscard]] int &FocusedConditionEditorWindowSlot() {
    return conditionsPane_.focusedEditorWindowSlot;
  }
  [[nodiscard]] const int &FocusedConditionEditorWindowSlot() const {
    return conditionsPane_.focusedEditorWindowSlot;
  }

  bool initialized_{false};
  bool enabled_{false};
  bool gameDataLoaded_{false};
  ID3D11Device *device_{nullptr};
  ID3D11DeviceContext *context_{nullptr};
  ID3D11ShaderResourceView *stripLinkSilhouetteTexture_{nullptr};
  std::uint32_t stripLinkSilhouetteWidth_{0};
  std::uint32_t stripLinkSilhouetteHeight_{0};
  bool stripLinkSilhouetteLoadAttempted_{false};
  ui::catalog::PaneState catalogPane_;
  ui::conditions::PaneState conditionsPane_;
  conditions::Store conditionStore_;
  mutable std::recursive_mutex conditionStateMutex_;
  WorkbenchFilterState workbenchFilter_;
  bool blockWorkbenchRowActionsThisFrame_{false};
  int fontSizePixels_{18};
  int pendingFontSizePixels_{18};
  std::string fontPath_{kDefaultFontPath};
  std::string localeId_{kDefaultLocaleId};
  bool pendingFontAtlasRebuild_{false};
  bool pauseGameWhenOpen_{false};
  bool smoothScroll_{true};
  bool addCrosshairNpcToActorList_{false};
  bool hideRealEquipmentWithFitting_{false};
  std::mutex pendingWorkbenchActorSyncMutex_;
  std::unordered_set<RE::FormID> pendingWorkbenchActorSyncs_;
  mutable std::recursive_mutex actorVisibilityMutex_;
  std::unordered_map<RE::FormID, bool> hideRealEquipmentByActor_;
  std::unordered_map<RE::FormID, bool> hideFittingOverridesByActor_;
  std::uint32_t toggleKey_{0x40};
  std::uint32_t toggleModifier_{0};
  std::string themeName_{"default"};
  std::string toggleKeyCaptureError_;
  bool awaitingToggleKeyCapture_{false};
  bool openToggleKeyPopup_{false};
  bool wantTextInput_{false};
  bool skyrimTextInputAllowed_{false};
  bool hideMessageQueued_{false};
  VisibilityState visibilityState_{VisibilityState::Closed};
  float windowAlpha_{0.0f};
  // User-configurable opacity, kept separate from the open/close fade alpha.
  float uiOpacity_{1.0f};
  ui::MenuCharacterSide menuCharacterSide_{ui::MenuCharacterSide::Right};
  float pendingSmoothWheelDelta_{0.0f};
  ImGuiID smoothScrollWindowId_{0};
  float smoothScrollTargetY_{0.0f};
  float lastAppliedSmoothScrollY_{0.0f};
  // A legacy-save strip-link migration can arrive before the renderer has
  // initialized the JSON settings paths. Keep it until LoadUserSettings has
  // completed so the migrated mode cannot be overwritten or lost.
  std::optional<workbench::ExternalModStripLinkMode>
      pendingLegacyExternalStripLinkMode_;
  std::string settingsDirectory_;
  std::string imguiIniPath_;
  std::string userSettingsPath_;
  std::string favoritesPath_;
  std::array<char, 512> saveDataPath_{};
  std::string saveDataStatus_;
  bool saveDataStatusIsError_{false};
  bool openSaveDataErrorPopup_{false};
  bool pendingCatalogTabSelection_{false};
  int pendingKitListMoveDelta_{0};
  bool pendingKitListApply_{false};
  bool pendingKitListPreview_{false};
  bool pendingKitListBack_{false};
  bool pendingKitListNextPane_{false};
  std::chrono::steady_clock::time_point lastKitListApplyAt_{};
  bool workbenchStickyConditionalHeader_{false};
  std::vector<PendingSlotCreationState> pendingSlotCreations_;
  struct ApplyWithConditionOverridesDialogState {
    bool openRequested{false};
    bool cancelRequested{false};
    bool open{false};
    std::string conditionId;
    std::string conditionName;
    ConditionOverrideApplicationSource source;
    RE::FormID actorFormID{0};
    workbench::VariantWorkbench::ConditionOverrideApplicationPlan plan;
  };
  ApplyWithConditionOverridesDialogState applyWithConditionOverridesDialog_;
  struct StripLinkDialogState {
    bool openRequested{false};
    bool open{false};
    workbench::ExternalModStripLinkMode baseMode{
        workbench::ExternalModStripLinkMode::ModSettingsSlots};
    // Direct editing has no automatic dropdown entry. This remembered policy
    // selects only which external observation pipeline feeds its explicit
    // mappings.
    workbench::ExternalModStripLinkMode directAutomaticBaseMode{
        workbench::ExternalModStripLinkMode::ModSettingsSlots};
    workbench::AutomaticEquipmentSlotMappings mappings{};
    workbench::AutomaticEquipmentSlotMappings directMappings{};
    workbench::AutomaticEquipmentSlotOverrides directOverrides{};
    std::uint64_t disabledAppearanceSlotMask{0};
  };
  StripLinkDialogState stripLinkDialog_;
  std::vector<FontOption> bundledFontOptions_;
  std::vector<FontOption> systemFontOptions_;
  workbench::VariantWorkbench workbench_;
  bool workbenchSortDeferredUntilClose_{false};
  WorkbenchSortState workbenchBaseSort_{};
  WorkbenchSortState workbenchConditionalSort_{};
  std::unordered_map<std::uint64_t, std::size_t> workbenchBaseSessionOrder_;
  std::unordered_map<std::uint64_t, std::size_t>
      workbenchConditionalSessionOrder_;
  struct WorkbenchActorSyncState {
    bool initialized{false};
    WorkbenchFilterState filterState{};
    RE::FormID actorFormID{0};
    std::uint64_t occupiedSlotMask{0};
    std::vector<RE::FormID> wornArmorFormIDs;
  };
  WorkbenchActorSyncState workbenchActorSyncState_;
  std::vector<RE::FormID> workbenchNearbyActorFormIDs_;
  struct WorkbenchDerivedState {
    std::uint64_t workbenchRevision{0};
    std::uint64_t conditionRevision{0};
    bool revisionsInitialized{false};
    WorkbenchFilterState filterState{};
    bool filterStateInitialized{false};
    std::vector<WorkbenchFilterOption> filterOptions;
    std::vector<ui::workbench::RowConditionVisualState> rowConditionStates;
    std::vector<int> visibleRowIndices;
    ui::workbench_conflicts::ConflictState conflictState;
  };
  WorkbenchDerivedState workbenchDerived_;
  ui::catalog::DerivedState catalogDerived_;
};
} // namespace sfs
