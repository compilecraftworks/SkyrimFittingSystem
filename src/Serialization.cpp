#include "Serialization.h"

#include "native/ArmorSkinning.h"
#include "native/DaveIntegration.h"
#include "native/FittingSlotState.h"
#include "native/HelmetToggle2Integration.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/FittingDye.h"
#include "poc/DeviousDevicesHiderPoC.h"
#include "poc/VirtualWornTokenPoC.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/Menu.h"
#include "workbench/EquipmentRefreshEventSink.h"
#include "workbench/AutomaticEquipmentVisibility.h"

namespace sfs::serialization {
void SaveCallback(SKSE::SerializationInterface *a_skse) {
  Menu::GetSingleton()->GetWorkbench().Serialize(a_skse);
  Menu::GetSingleton()->SerializeConditions(a_skse);
  native::SerializeFittingSlotStates(a_skse);
  Menu::GetSingleton()->SerializeActorVisibilitySettings(a_skse);
  native::SerializeArmorClassificationMigrationState(a_skse);
  workbench::SerializeActorAutomaticEquipmentVisibilitySettings(a_skse);
  native::external_equipment::Serialize(a_skse);
  poc::SerializeVirtualWornTokenState(a_skse);
  native::dye::SerializeSavedWorldTints(a_skse);
}

void LoadCallback(SKSE::SerializationInterface *a_skse) {
  native::dye::ClearWorldTint();
  ui::conditions::ConditionParamOptionCache::Get().Reset();
  poc::ResetDeviousDevicesHider();
  poc::ResetVirtualWornTokenRuntimeState();
  native::external_equipment::ClearRuntimeState();
  native::helmet_toggle::ResetRuntimeState();
  native::ClearAllFittingSlotStates();
  native::dave::ForgetHiddenRealEquipmentState();
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
  poc::DeserializeVirtualWornTokenState(a_skse);
  native::dye::DeserializeSavedWorldTints(a_skse);
  menu->ResetTransientWorkbenchUiState(false);
}

void RevertCallback([[maybe_unused]] SKSE::SerializationInterface *a_skse) {
  native::dye::ClearWorldTint();
  native::dye::RevertSavedWorldTints();
  ui::conditions::ConditionParamOptionCache::Get().Reset();
  poc::ResetDeviousDevicesHider();
  poc::ResetVirtualWornTokenRuntimeState();
  native::helmet_toggle::ResetRuntimeState();
  native::ClearAllFittingSlotStates();
  native::InvalidateQueuedArmorRefreshes();
  native::RevertArmorClassificationMigrationState();
  native::dave::ForgetHiddenRealEquipmentState();
  workbench::EquipmentRefreshEventSink::CancelQueuedRefreshes();
  auto *menu = Menu::GetSingleton();
  menu->SetGameDataLoaded(false);
  menu->GetWorkbench().Revert(false);
  menu->RevertConditions();
  menu->RevertActorVisibilitySettings();
  workbench::RevertActorAutomaticEquipmentVisibilitySettings();
  native::external_equipment::ClearRuntimeState();
  menu->ResetTransientWorkbenchUiState(false);
}
} // namespace sfs::serialization
