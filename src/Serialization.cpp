#include "Serialization.h"

#include "catalog/BodyFamily.h"
#include "native/ArmorSkinning.h"
#include "native/DaveIntegration.h"
#include "native/FittingSlotState.h"
#include "native/HelmetToggle2Integration.h"
#include "native/RaceMenuBodyMorph.h"
#include "native/SOSStorageSync.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/FittingDye.h"
#include "features/devious_devices/DeviousDevicesIntegration.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/Menu.h"
#include "workbench/EquipmentRefreshEventSink.h"
#include "workbench/AutomaticEquipmentVisibility.h"

namespace sfs::serialization {
void PrepareForLoadTransition() {
  auto *menu = Menu::GetSingleton();
  menu->SetGameDataLoaded(false);

  // Invalidate every asynchronous producer before clearing the state it may
  // otherwise republish. Each operation is deliberately idempotent because
  // this boundary is entered by both kPreLoadGame and LoadCallback.
  native::InvalidateQueuedArmorRefreshes();
  workbench::EquipmentRefreshEventSink::CancelQueuedRefreshes();
  native::CancelSOSStorageSync();
  native::racemenu::ForgetAllRegisteredAppearanceNodes();

  body_family::ResetRuntimeCaches();
  native::dye::ClearWorldTint();
  native::dye::RevertSavedWorldTints();
  ui::conditions::ConditionParamOptionCache::Get().Reset();
  devious_devices::ResetDeviousDevicesHider();
  virtual_tokens::ResetVirtualWornTokenRuntimeState();
  native::external_equipment::ClearRuntimeState();
  native::helmet_toggle::ResetRuntimeState();
  native::ClearAllFittingSlotStates();
  native::dave::ForgetHiddenRealEquipmentState();
  menu->ResetTransientWorkbenchUiState(false);
}

void SaveCallback(SKSE::SerializationInterface *a_skse) {
  Menu::GetSingleton()->GetWorkbench().Serialize(a_skse);
  Menu::GetSingleton()->SerializeConditions(a_skse);
  native::SerializeFittingSlotStates(a_skse);
  Menu::GetSingleton()->SerializeActorVisibilitySettings(a_skse);
  native::SerializeArmorClassificationMigrationState(a_skse);
  workbench::SerializeActorAutomaticEquipmentVisibilitySettings(a_skse);
  native::external_equipment::Serialize(a_skse);
  virtual_tokens::SerializeVirtualWornTokenState(a_skse);
  native::dye::SerializeSavedWorldTints(a_skse);
}

void LoadCallback(SKSE::SerializationInterface *a_skse) {
  PrepareForLoadTransition();
  auto *menu = Menu::GetSingleton();
  const auto workbenchSerializationVersion = menu->GetWorkbench().Deserialize(
      a_skse, std::string(ui::conditions::kDefaultConditionId));
  menu->MigrateExternalStripLinkModeForWorkbenchVersion(
      workbenchSerializationVersion);
  menu->DeserializeConditions(a_skse);
  const auto *player = RE::PlayerCharacter::GetSingleton();
  menu->GetWorkbench().MigrateLegacyConditionAssignments(
      menu->GetConditions(), player != nullptr ? player->GetFormID() : 0);
  native::DeserializeFittingSlotStates(a_skse);
  menu->DeserializeActorVisibilitySettings(a_skse);
  native::DeserializeArmorClassificationMigrationState(a_skse);
  workbench::DeserializeActorAutomaticEquipmentVisibilitySettings(a_skse);
  native::external_equipment::Deserialize(a_skse);
  virtual_tokens::DeserializeVirtualWornTokenState(a_skse);
  native::dye::DeserializeSavedWorldTints(a_skse);
  menu->ResetTransientWorkbenchUiState(false);
}

void RevertCallback([[maybe_unused]] SKSE::SerializationInterface *a_skse) {
  PrepareForLoadTransition();
  native::RevertArmorClassificationMigrationState();
  auto *menu = Menu::GetSingleton();
  menu->GetWorkbench().Revert(false);
  menu->RevertConditions();
  menu->RevertActorVisibilitySettings();
  workbench::RevertActorAutomaticEquipmentVisibilitySettings();
}
} // namespace sfs::serialization
