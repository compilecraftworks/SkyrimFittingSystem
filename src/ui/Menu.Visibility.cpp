#include "Menu.h"

#include "Hooks.h"
#include "InputManager.h"
#include "MenuHost.h"
#include "api/SkyrimFittingSystemAPI.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui_internal.h"
#include "kit_generator/UI.h"
#include "native/ArmorSkinning.h"
#include "native/FittingSlotState.h"
#include "native/GridInventoryIntegration.h"
#include "ui/components/PinnableTooltip.h"
#include "workbench/EquipmentRefreshEventSink.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace {
constexpr float kFadeDurationSeconds = 0.30f;
constexpr float kSmoothScrollStepMultiplier = 5.0f;
constexpr float kSmoothScrollLerpFactor = 0.20f;

using UserEventFlag = RE::ControlMap::UEFlag;

constexpr auto kBlockedGameplayControls = static_cast<UserEventFlag>(
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kMovement) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kLooking) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kActivate) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kFighting) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kSneaking) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kJumping) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kPOVSwitch) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kMainFour) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kWheelZoom) |
    static_cast<std::underlying_type_t<UserEventFlag>>(UserEventFlag::kVATS));

bool IsAnyMouseButtonDown(const ImGuiIO &a_io) {
  for (const bool mouseDown : a_io.MouseDown) {
    if (mouseDown) {
      return true;
    }
  }

  return false;
}

void AllowTextInput([[maybe_unused]] RE::ControlMap *a_controlMap,
                    [[maybe_unused]] bool a_allow) {
#ifdef EXCLUSIVE_SKYRIM_VR
  return;
#else
  using Func = decltype(&AllowTextInput);
  static REL::Relocation<Func> func{RELOCATION_ID(67252, 68552)};
  func(a_controlMap, a_allow);
#endif
}
} // namespace

namespace sfs {

void Menu::SetGameDataLoaded(const bool a_loaded) {
  if (!a_loaded) {
    ui::MenuCharacterPresentation::GetSingleton()->Restore();
  }
  gameDataLoaded_ = a_loaded;
  api::SetGameDataLoaded(a_loaded);
  native::grid_inventory::SetGameDataLoaded(a_loaded);
}

void Menu::Open() {
  if (!initialized_ || enabled_ || !gameDataLoaded_) {
    return;
  }

  // Apply any deferred condition-card ordering on the next opening.
  workbenchSortDeferredUntilClose_ = false;
  workbenchBaseSessionOrder_.clear();
  workbenchConditionalSessionOrder_.clear();
  workbenchDerived_ = {};

  if (!CatalogBrowserState().initialized) {
    QueueCatalogRefresh();
  }

  if (auto *messageQueue = RE::UIMessageQueue::GetSingleton();
      messageQueue != nullptr) {
    messageQueue->AddMessage(MenuHost::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow,
                             nullptr);
  }
}

void Menu::Close() {
  if (!initialized_ || !enabled_ ||
      visibilityState_ == VisibilityState::Closing) {
    return;
  }
  visibilityState_ = VisibilityState::Closing;
  wantTextInput_ = false;
  InputManager::GetSingleton()->SetShortcutSuppressionActive(false);
  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr && skyrimTextInputAllowed_) {
    AllowTextInput(controlMap, false);
    skyrimTextInputAllowed_ = false;
  }
}

void Menu::NotifyWindowShutdown() {
  ui::MenuCharacterPresentation::GetSingleton()->Restore();
  hooks::ResetInputFilterState();
  api::SetMenuLifecycleActive(false);
  api::SetMenuInitialized(false);
  InputManager::GetSingleton()->SetShortcutSuppressionActive(false);
  catalogPane_.activeTransientPopup = ui::catalog::TransientPopup::None;
  catalogPane_.closeActiveTransientPopupRequested = false;

  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr && skyrimTextInputAllowed_) {
    AllowTextInput(controlMap, false);
    skyrimTextInputAllowed_ = false;
  }
  wantTextInput_ = false;

  if (ImGui::GetCurrentContext() != nullptr) {
    auto &io = ImGui::GetIO();
    io.MouseDrawCursor = false;
    io.ClearInputKeys();
    io.ClearEventsQueue();
  }

  ui::components::ClearPinnedTooltips();
  CloseToggleKeyCapture();
  hideMessageQueued_ = false;
  visibilityState_ = VisibilityState::Closed;
  windowAlpha_ = 0.0f;
  pendingSmoothWheelDelta_ = 0.0f;
  smoothScrollWindowId_ = 0;
  smoothScrollTargetY_ = 0.0f;
  lastAppliedSmoothScrollY_ = 0.0f;
  FocusedConditionEditorWindowSlot() = 0;
  enabled_ = false;
}

void Menu::OnMenuShow() {
  if (!initialized_ || enabled_) {
    return;
  }

  // Remove only rows for which both condition and action were cleared. Old
  // save data may contain these, while every half-complete row remains intact.
  static_cast<void>(workbench_.PruneFullyEmptyConditionalRows());
  workbenchSortDeferredUntilClose_ = false;
  workbenchBaseSessionOrder_.clear();
  workbenchConditionalSessionOrder_.clear();
  workbenchDerived_ = {};

  if (CatalogBrowserState().inventoryOnly) {
    catalogDerived_.gear = {};
  }

  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr) {
    controlMap->ToggleControls(kBlockedGameplayControls, false, false);
  }

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = false;
  io.ClearInputKeys();
  io.ClearEventsQueue();
  catalogPane_.activeTransientPopup = ui::catalog::TransientPopup::None;
  catalogPane_.closeActiveTransientPopupRequested = false;
  wantTextInput_ = false;
  skyrimTextInputAllowed_ = false;
  InputManager::GetSingleton()->SetShortcutSuppressionActive(false);
  visibilityState_ = VisibilityState::Opening;
  api::SetMenuLifecycleActive(true);
  windowAlpha_ = 0.0f;
  hideMessageQueued_ = false;
  pendingSmoothWheelDelta_ = 0.0f;
  smoothScrollWindowId_ = 0;
  smoothScrollTargetY_ = 0.0f;
  lastAppliedSmoothScrollY_ = 0.0f;
  CatalogBrowserState().activeTab = ui::catalog::BrowserTab::Kits;
  pendingCatalogTabSelection_ = true;
  ApplyInitialWorkbenchFilterSelection();
  workbenchActorSyncState_ = {};
  if (auto *actor = ResolveWorkbenchPreviewActor(); actor != nullptr) {
    native::ReconcileFittingSlotState(actor);
    workbench_.SyncRowsFromActor(actor);
    workbench::EquipmentRefreshEventSink::GetSingleton()->QueueActorRefresh(
        actor->GetFormID());
  }
  enabled_ = true;
  ui::MenuCharacterPresentation::GetSingleton()->Apply(menuCharacterSide_);
}

void Menu::OnMenuHide() {
  ui::MenuCharacterPresentation::GetSingleton()->Restore();
  hooks::ResetInputFilterState();
  InputManager::GetSingleton()->SetShortcutSuppressionActive(false);
  if (!initialized_ || !enabled_) {
    return;
  }

  if (CatalogBrowserState().activeTab ==
      ui::catalog::BrowserTab::KitGenerator) {
    kit_generator::UI::Get().NotifyTabClosed();
  }
  ClearCatalogSelection();
  CloseToggleKeyCapture();

  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr) {
    if (skyrimTextInputAllowed_) {
      AllowTextInput(controlMap, false);
      skyrimTextInputAllowed_ = false;
    }
    controlMap->ToggleControls(kBlockedGameplayControls, true, false);
  }
  wantTextInput_ = false;

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = false;
  io.ClearInputKeys();
  io.ClearEventsQueue();
  catalogPane_.activeTransientPopup = ui::catalog::TransientPopup::None;
  catalogPane_.closeActiveTransientPopupRequested = false;
  pendingSmoothWheelDelta_ = 0.0f;
  smoothScrollWindowId_ = 0;
  smoothScrollTargetY_ = 0.0f;
  lastAppliedSmoothScrollY_ = 0.0f;
  FocusedConditionEditorWindowSlot() = 0;
  pendingSlotCreations_.clear();
  static_cast<void>(workbench_.PruneFullyEmptyConditionalRows());
  workbenchDerived_ = {};
  workbenchSortDeferredUntilClose_ = false;
  workbenchBaseSessionOrder_.clear();
  workbenchConditionalSessionOrder_.clear();
  hideMessageQueued_ = false;
  visibilityState_ = VisibilityState::Closed;
  api::SetMenuLifecycleActive(false);
  windowAlpha_ = 0.0f;
  if (io.IniFilename) {
    ImGui::SaveIniSettingsToDisk(io.IniFilename);
  }
  SaveUserSettings();
  enabled_ = false;
}

void Menu::HandleCancel() {
  auto &createDialog = CreateKitDialogState();
  if (createDialog.open) {
    createDialog.cancelRequested = true;
    return;
  }

  if (catalogPane_.activeTransientPopup != ui::catalog::TransientPopup::None) {
    catalogPane_.closeActiveTransientPopupRequested = true;
    return;
  }

  if (applyWithConditionOverridesDialog_.open) {
    applyWithConditionOverridesDialog_.cancelRequested = true;
    return;
  }

  if (awaitingToggleKeyCapture_ || openToggleKeyPopup_) {
    CloseToggleKeyCapture();
    return;
  }

  if (!ConditionEditors().empty()) {
    auto closeEditor = [&](ConditionEditorState &a_editor) {
      a_editor.error.clear();
      a_editor.open = false;
    };

    if (FocusedConditionEditorWindowSlot() > 0) {
      const auto focusedIt = std::ranges::find(
          ConditionEditors(), FocusedConditionEditorWindowSlot(),
          &ConditionEditorState::windowSlot);
      if (focusedIt != ConditionEditors().end()) {
        closeEditor(*focusedIt);
        FocusedConditionEditorWindowSlot() = 0;
        return;
      }
    }

    closeEditor(ConditionEditors().back());
    FocusedConditionEditorWindowSlot() = 0;
    return;
  }

  if (ui::components::HasPinnedTooltips()) {
    ui::components::ClearPinnedTooltips();
    return;
  }

  Close();
}

bool Menu::QueueKitListCommand(const KitListCommand a_command) {
  if (!enabled_ || wantTextInput_) {
    return false;
  }

  const auto activeTab = CatalogBrowserState().activeTab;
  const auto listTab = activeTab == ui::catalog::BrowserTab::Gear ||
                       activeTab == ui::catalog::BrowserTab::Outfits ||
                       activeTab == ui::catalog::BrowserTab::Kits ||
                       activeTab == ui::catalog::BrowserTab::KitGenerator;
  if (!listTab ||
      (a_command == KitListCommand::Back &&
       activeTab != ui::catalog::BrowserTab::KitGenerator) ||
      (a_command == KitListCommand::NextPane &&
       activeTab != ui::catalog::BrowserTab::KitGenerator)) {
    return false;
  }

  switch (a_command) {
  case KitListCommand::MoveUp:
    pendingKitListMoveDelta_ = -1;
    break;
  case KitListCommand::MoveDown:
    pendingKitListMoveDelta_ = 1;
    break;
  case KitListCommand::Apply:
    pendingKitListApply_ = true;
    break;
  case KitListCommand::Preview:
    pendingKitListPreview_ = true;
    break;
  case KitListCommand::Back:
    pendingKitListBack_ = true;
    break;
  case KitListCommand::NextPane:
    pendingKitListNextPane_ = true;
    break;
  }
  return true;
}

bool Menu::QueueKitListMove(const int a_delta) {
  if (a_delta < 0) {
    return QueueKitListCommand(KitListCommand::MoveUp);
  } else if (a_delta > 0) {
    return QueueKitListCommand(KitListCommand::MoveDown);
  }
  return false;
}

bool Menu::QueueKitListApply() {
  // Keyboard Enter reaches SFS through InputManager while the same physical
  // press can also arrive as Skyrim's Activate user event on the following
  // frame. Treat that pair as one command, otherwise result-list Enter opens
  // the detail and immediately the editor.
  const auto now = std::chrono::steady_clock::now();
  if (now - lastKitListApplyAt_ < std::chrono::milliseconds(150)) {
    return true;
  }
  if (!QueueKitListCommand(KitListCommand::Apply)) {
    return false;
  }
  lastKitListApplyAt_ = now;
  return true;
}

bool Menu::QueueKitListPreview() {
  return QueueKitListCommand(KitListCommand::Preview);
}

bool Menu::QueueKitListBack() {
  return QueueKitListCommand(KitListCommand::Back);
}

bool Menu::QueueKitListNextPane() {
  return QueueKitListCommand(KitListCommand::NextPane);
}

void Menu::ConsumeKitListCommands(int &a_moveDelta, bool &a_applyRequested,
                                 bool &a_previewRequested) {
  a_moveDelta = pendingKitListMoveDelta_;
  a_applyRequested = pendingKitListApply_;
  a_previewRequested = pendingKitListPreview_;
  pendingKitListMoveDelta_ = 0;
  pendingKitListApply_ = false;
  pendingKitListPreview_ = false;
}

bool Menu::ConsumeKitListBack() {
  const auto requested = pendingKitListBack_;
  pendingKitListBack_ = false;
  return requested;
}

bool Menu::ConsumeKitListNextPane() {
  const auto requested = pendingKitListNextPane_;
  pendingKitListNextPane_ = false;
  return requested;
}

bool Menu::HandleMenuUserEvent(const std::string_view a_eventName) {
  if (a_eventName == "Cancel") {
    HandleCancel();
    return true;
  }

  if (!enabled_ || wantTextInput_ ||
      (CatalogBrowserState().activeTab != ui::catalog::BrowserTab::Gear &&
       CatalogBrowserState().activeTab != ui::catalog::BrowserTab::Outfits &&
       CatalogBrowserState().activeTab != ui::catalog::BrowserTab::Kits &&
       CatalogBrowserState().activeTab != ui::catalog::BrowserTab::KitGenerator)) {
    return false;
  }

  if (a_eventName == "Activate") {
    return QueueKitListApply();
  }
  if (a_eventName == "Jump" || a_eventName == "Jumping") {
    return QueueKitListCommand(KitListCommand::Preview);
  }

  return false;
}

void Menu::Toggle() {
  if (enabled_) {
    Close();
  } else {
    Open();
  }
}

bool Menu::QueueSmoothScroll(const float a_deltaY) {
  if (!enabled_ || !smoothScroll_ || a_deltaY == 0.0f) {
    return false;
  }

  pendingSmoothWheelDelta_ += a_deltaY;
  return true;
}

void Menu::QueueHideMessage() {
  if (hideMessageQueued_) {
    return;
  }

  if (auto *messageQueue = RE::UIMessageQueue::GetSingleton();
      messageQueue != nullptr) {
    messageQueue->AddMessage(MenuHost::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide,
                             nullptr);
    hideMessageQueued_ = true;
  }
}

void Menu::UpdateVisibilityAnimation(const float a_deltaTime) {
  switch (visibilityState_) {
  case VisibilityState::Opening:
    windowAlpha_ += a_deltaTime / kFadeDurationSeconds;
    if (windowAlpha_ >= 1.0f) {
      windowAlpha_ = 1.0f;
      visibilityState_ = VisibilityState::Open;
    }
    break;
  case VisibilityState::Closing:
    windowAlpha_ -= a_deltaTime / kFadeDurationSeconds;
    if (windowAlpha_ <= 0.0f) {
      windowAlpha_ = 0.0f;
      QueueHideMessage();
    }
    break;
  case VisibilityState::Open:
    windowAlpha_ = 1.0f;
    break;
  case VisibilityState::Closed:
    windowAlpha_ = 0.0f;
    break;
  }
}

void Menu::ApplySmoothScroll() {
  if (!smoothScroll_) {
    pendingSmoothWheelDelta_ = 0.0f;
    smoothScrollWindowId_ = 0;
    lastAppliedSmoothScrollY_ = 0.0f;
    return;
  }

  auto *context = ImGui::GetCurrentContext();
  if (context == nullptr) {
    pendingSmoothWheelDelta_ = 0.0f;
    smoothScrollWindowId_ = 0;
    lastAppliedSmoothScrollY_ = 0.0f;
    return;
  }

  auto findScrollWindow = [](ImGuiWindow *a_window) -> ImGuiWindow * {
    while (a_window != nullptr) {
      if (!(a_window->Flags & ImGuiWindowFlags_NoScrollWithMouse) &&
          a_window->ScrollMax.y > 0.0f) {
        return a_window;
      }
      a_window = a_window->ParentWindow;
    }
    return nullptr;
  };

  if (pendingSmoothWheelDelta_ != 0.0f) {
    if (auto *scrollWindow = findScrollWindow(context->HoveredWindow);
        scrollWindow != nullptr) {
      if (smoothScrollWindowId_ != scrollWindow->ID) {
        smoothScrollWindowId_ = scrollWindow->ID;
        smoothScrollTargetY_ = scrollWindow->Scroll.y;
        lastAppliedSmoothScrollY_ = scrollWindow->Scroll.y;
      }

      const auto scrollStep =
          ImGui::GetTextLineHeightWithSpacing() * kSmoothScrollStepMultiplier;
      smoothScrollTargetY_ = std::clamp(
          smoothScrollTargetY_ - pendingSmoothWheelDelta_ * scrollStep, 0.0f,
          scrollWindow->ScrollMax.y);
    }
    pendingSmoothWheelDelta_ = 0.0f;
  }

  if (smoothScrollWindowId_ == 0) {
    return;
  }

  ImGuiWindow *scrollWindow = nullptr;
  for (auto *window : context->Windows) {
    if (window->ID == smoothScrollWindowId_) {
      scrollWindow = window;
      break;
    }
  }

  if (scrollWindow == nullptr) {
    smoothScrollWindowId_ = 0;
    smoothScrollTargetY_ = 0.0f;
    lastAppliedSmoothScrollY_ = 0.0f;
    return;
  }

  smoothScrollTargetY_ =
      std::clamp(smoothScrollTargetY_, 0.0f, scrollWindow->ScrollMax.y);
  const auto currentScrollY = scrollWindow->Scroll.y;
  if (pendingSmoothWheelDelta_ == 0.0f &&
      std::abs(currentScrollY - lastAppliedSmoothScrollY_) > 0.5f &&
      std::abs(currentScrollY - smoothScrollTargetY_) > 0.5f) {
    smoothScrollTargetY_ = currentScrollY;
  }
  auto nextScrollY = currentScrollY + ((smoothScrollTargetY_ - currentScrollY) *
                                       kSmoothScrollLerpFactor);
  if (std::abs(smoothScrollTargetY_ - nextScrollY) <= 0.5f) {
    nextScrollY = smoothScrollTargetY_;
  }

  scrollWindow->Scroll.y = nextScrollY;
  scrollWindow->ScrollTarget.y = nextScrollY;
  scrollWindow->ScrollTargetCenterRatio.y = 0.0f;
  lastAppliedSmoothScrollY_ = nextScrollY;

  if (std::abs(smoothScrollTargetY_ - nextScrollY) <= 0.5f) {
    smoothScrollWindowId_ = 0;
    lastAppliedSmoothScrollY_ = 0.0f;
  }
}

void Menu::SyncAllowTextInput() {
  const auto &io = ImGui::GetIO();
  const bool currentWantTextInput =
      visibilityState_ != VisibilityState::Closing && io.WantTextInput;

  // Toggling Skyrim text input off on every ImGui text-field blur causes the
  // active Scaleform frame to visibly flash, so defer it across blur clicks.
  if (currentWantTextInput && !skyrimTextInputAllowed_) {
    if (auto *controlMap = RE::ControlMap::GetSingleton();
        controlMap != nullptr) {
      AllowTextInput(controlMap, true);
      skyrimTextInputAllowed_ = true;
    }
  } else if (!currentWantTextInput && skyrimTextInputAllowed_ &&
             !IsAnyMouseButtonDown(io)) {
    if (auto *controlMap = RE::ControlMap::GetSingleton();
        controlMap != nullptr) {
      AllowTextInput(controlMap, false);
      skyrimTextInputAllowed_ = false;
    }
  }

  wantTextInput_ = currentWantTextInput;
  InputManager::GetSingleton()->SetShortcutSuppressionActive(
      currentWantTextInput);
}

void Menu::Draw() {
  if (!initialized_ || !enabled_) {
    return;
  }

  if (pendingFontAtlasRebuild_) {
    RebuildFontAtlas();
    pendingFontAtlasRebuild_ = false;
    return;
  }

  ImGui_ImplWin32_NewFrame();
  ImGui_ImplDX11_NewFrame();
  InputManager::GetSingleton()->UpdateMousePosition();
  ImGui::NewFrame();
  ui::components::BeginPinnableTooltipFrame();
  SyncAllowTextInput();

  {
    // DrawWindow keeps direct references into both stores while building the
    // UI. Serialize only that section against SKSE equipment tasks/native
    // display readers, always taking workbench before condition state.
    auto workbenchStateLock = workbench_.AcquireStateLock();
    auto conditionStateLock = AcquireConditionStateLock();
    DrawWindow();
  }
  ui::MenuCharacterPresentation::GetSingleton()->UpdateRotationInteraction();
  SyncAllowTextInput();
  ui::components::EndPinnableTooltipFrame();
  ApplySmoothScroll();
  UpdateVisibilityAnimation(ImGui::GetIO().DeltaTime);

  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
} // namespace sfs
