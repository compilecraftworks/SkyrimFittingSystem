#include "EquipmentCatalog.h"
#include "Hooks.h"
#include "InputManager.h"
#include "Plugin.h"
#include "Serialization.h"
#include "kit_generator/Generator.h"
#include "native/ArmorSkinning.h"
#include "native/DaveIntegration.h"
#include "native/DisplayedBodyCondition.h"
#include "native/GridInventoryIntegration.h"
#include "native/OpenAnimationReplacerIntegration.h"
#include "native/RaceMenuBodyMorph.h"
#include "native/SOSStorageSync.h"
#include "native/SmoothCamIntegration.h"
#include "papyrus/FittingPapyrus.h"
#include "poc/DeviousDevicesHiderPoC.h"
#include "poc/VirtualWornTokenPoC.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/Menu.h"
#include "workbench/EquipmentRefreshEventSink.h"

static void SKSEMessageHandler(SKSE::MessagingInterface::Message *a_message) {
  switch (a_message->type) {
  case SKSE::MessagingInterface::kPostLoad:
    sfs::native::smoothcam::RegisterInterfaceListener();
    sfs::native::oar::RegisterConditions();
    break;
  case SKSE::MessagingInterface::kDataLoaded:
    sfs::kit_generator::Generator::Get().SnapshotLoadedArmorForms();
    sfs::poc::InitializeVirtualWornTokens();
    sfs::poc::InitializeDeviousDevicesHider();
    sfs::native::racemenu::InitializeBodyMorphInterface();
    sfs::Menu::GetSingleton()->SetGameDataLoaded(true);
    sfs::native::CaptureArmorClassificationKeywordBaseline();
    sfs::workbench::EquipmentRefreshEventSink::Register();
    sfs::native::RequestSOSStorageSync();
    sfs::native::QueuePlayerArmorRefresh();
    break;
  case SKSE::MessagingInterface::kPostPostLoad:
    sfs::native::smoothcam::RequestInterface();
    sfs::hooks::Install();
    sfs::native::InstallArmorSkinningHooks();
    sfs::native::InstallDisplayedBodyConditionHook();
    sfs::native::racemenu::InitializeBodyMorphInterface();
    break;
  case SKSE::MessagingInterface::kPreLoadGame:
    sfs::Menu::GetSingleton()->SetGameDataLoaded(false);
    sfs::native::InvalidateQueuedArmorRefreshes();
    sfs::native::dave::ForgetHiddenRealEquipmentState();
    sfs::native::CancelSOSStorageSync();
    sfs::native::racemenu::ForgetAllRegisteredAppearanceNodes();
    sfs::poc::ResetDeviousDevicesHider();
    sfs::workbench::EquipmentRefreshEventSink::CancelQueuedRefreshes();
    sfs::ui::conditions::ConditionParamOptionCache::Get().Reset();
    break;
  case SKSE::MessagingInterface::kPostLoadGame:
    sfs::native::InvalidateQueuedArmorRefreshes();
    sfs::native::dave::ForgetHiddenRealEquipmentState();
    sfs::workbench::EquipmentRefreshEventSink::CancelQueuedRefreshes();
    sfs::ui::conditions::ConditionParamOptionCache::Get().Reset();
    sfs::Menu::GetSingleton()->SetGameDataLoaded(true);
    static_cast<void>(sfs::poc::RefreshDeviousDevicesHiderSettings());
    sfs::native::RequestSOSStorageSync();
    sfs::workbench::EquipmentRefreshEventSink::GetSingleton()->QueueRefresh();
    sfs::native::QueuePlayerArmorRefresh();
    sfs::native::ScheduleLegacyArmorClassificationKeywordMigration();
    break;
  default:
    break;
  }
}

extern "C" DLLEXPORT bool SKSEAPI
SKSEPlugin_Load(const SKSE::LoadInterface *a_skse) {
  REL::Module::reset();

  auto *messaging = reinterpret_cast<SKSE::MessagingInterface *>(
      a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));

  if (!messaging) {
    logger::critical("Failed to load messaging interface. This is fatal.");
    return false;
  }

  SKSE::Init(a_skse);
  SKSE::AllocTrampoline(1 << 12);
  logger::info("{} build {}", Plugin::NAME, Plugin::VERSION_STRING);

  messaging->RegisterListener("SKSE", SKSEMessageHandler);
  // Grid Inventory's Costume state is a plugin broadcast, not an SKSE
  // lifecycle message. Keep its unfiltered listener separate so normal SKSE
  // lifecycle dispatch remains exactly as before.
  sfs::native::grid_inventory::RegisterMessageListener(messaging);

  if (auto *papyrus = SKSE::GetPapyrusInterface()) {
    papyrus->Register(sfs::papyrus::Register);
    papyrus->Register(sfs::poc::RegisterVirtualWornTokenPapyrus);
  } else {
    logger::warn("Failed to load Papyrus interface");
  }

  if (auto *serialization = SKSE::GetSerializationInterface()) {
    serialization->SetUniqueID(sfs::serialization::kID);
    serialization->SetSaveCallback(&sfs::serialization::SaveCallback);
    serialization->SetLoadCallback(&sfs::serialization::LoadCallback);
    serialization->SetRevertCallback(&sfs::serialization::RevertCallback);
  }

  logger::info("{} loaded", Plugin::NAME);
  return true;
}
