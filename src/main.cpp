#include "EquipmentCatalog.h"
#include "Hooks.h"
#include "InputManager.h"
#include "Plugin.h"
#include "Serialization.h"
#include "kit_generator/Generator.h"
#include "native/ArmorSkinning.h"
#include "catalog/BodyFamily.h"
#include "native/DaveIntegration.h"
#include "native/DisplayedBodyCondition.h"
#include "native/FittingDye.h"
#include "native/GridInventoryIntegration.h"
#include "native/GenitalCompatibility.h"
#include "native/HelmetToggle2Integration.h"
#include "native/OpenAnimationReplacerIntegration.h"
#include "native/RaceMenuBodyMorph.h"
#include "native/SOSStorageSync.h"
#include "native/SmoothCamIntegration.h"
#include "papyrus/FittingPapyrus.h"
#include "features/devious_devices/DeviousDevicesIntegration.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/Menu.h"
#include "workbench/EquipmentRefreshEventSink.h"

static void SKSEMessageHandler(SKSE::MessagingInterface::Message *a_message) {
  switch (a_message->type) {
  case SKSE::MessagingInterface::kPostLoad:
    // SKSE stores one callback per (sender, listener plugin) pair. Register
    // named integrations first; the later unfiltered Grid registration then
    // skips those senders instead of occupying their callback slot.
    sfs::native::smoothcam::RegisterInterfaceListener();
    sfs::native::grid_inventory::RegisterMessageListener(
        SKSE::GetMessagingInterface());
    sfs::native::oar::RegisterConditions();
    break;
  case SKSE::MessagingInterface::kDataLoaded:
    sfs::native::genital_compatibility::InitializeEnvironment();
    sfs::kit_generator::Generator::Get().SnapshotLoadedArmorForms();
    sfs::virtual_tokens::InitializeVirtualWornTokens();
    sfs::devious_devices::InitializeDeviousDevicesHider();
    sfs::native::racemenu::InitializeBodyMorphInterface();
    sfs::Menu::GetSingleton()->SetGameDataLoaded(true);
    static_cast<void>(sfs::native::helmet_toggle::Initialize());
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
    sfs::serialization::PrepareForLoadTransition();
    break;
  case SKSE::MessagingInterface::kPostLoadGame:
    sfs::native::InvalidateQueuedArmorRefreshes();
    sfs::native::dave::ForgetHiddenRealEquipmentState();
    sfs::workbench::EquipmentRefreshEventSink::CancelQueuedRefreshes();
    sfs::ui::conditions::ConditionParamOptionCache::Get().Reset();
    sfs::Menu::GetSingleton()->SetGameDataLoaded(true);
    sfs::native::helmet_toggle::SynchronizePlayer(false);
    static_cast<void>(
        sfs::devious_devices::RefreshDeviousDevicesHiderSettings());
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
  sfs::Menu::GetSingleton()->PrepareUserSettingsStorage();

  messaging->RegisterListener("SKSE", SKSEMessageHandler);

  if (auto *papyrus = SKSE::GetPapyrusInterface()) {
    papyrus->Register(sfs::papyrus::Register);
    papyrus->Register(
        sfs::virtual_tokens::RegisterVirtualWornTokenPapyrus);
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
