#include "native/SmoothCamIntegration.h"

#include <mutex>

namespace {
constexpr auto kSmoothCamPluginName = "SmoothCam";
constexpr auto kSmoothCamModuleName = L"SmoothCam.dll";
constexpr std::uint32_t kSmoothCamCommandHeader = 0x9007CA50;

enum class InterfaceVersion : std::uint8_t {
  V1,
  V2,
  V3,
};

enum class ApiResult : std::uint8_t {
  OK,
  NotOwner,
  MustKeep,
  AlreadyGiven,
  AlreadyTaken,
  BadThread,
};

// Minimal ABI-compatible declaration of SmoothCam's public V2 interface.  The
// complete V1 method order must remain present even though SFS only uses the
// camera-control subset.
class ISmoothCamV1 {
public:
  virtual unsigned long GetSmoothCamThreadId() const noexcept = 0;
  virtual ApiResult
  RequestCameraControl(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
  virtual ApiResult
  RequestCrosshairControl(SKSE::PluginHandle a_pluginHandle,
                          bool a_restoreDefaults = true) noexcept = 0;
  virtual ApiResult
  RequestStealthMeterControl(SKSE::PluginHandle a_pluginHandle,
                             bool a_restoreDefaults = true) noexcept = 0;
  virtual SKSE::PluginHandle GetCameraOwner() const noexcept = 0;
  virtual SKSE::PluginHandle GetCrosshairOwner() const noexcept = 0;
  virtual SKSE::PluginHandle GetStealthMeterOwner() const noexcept = 0;
  virtual ApiResult
  ReleaseCameraControl(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
  virtual ApiResult
  ReleaseCrosshairControl(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
  virtual ApiResult
  ReleaseStealthMeterControl(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
};

class ISmoothCamV2 : public ISmoothCamV1 {
public:
  virtual RE::NiPoint3 GetLastCameraPosition() const noexcept = 0;
  virtual ApiResult RequestInterpolatorUpdates(
      SKSE::PluginHandle a_pluginHandle, bool a_allowUpdates) noexcept = 0;
  virtual ApiResult
  SendToGoalPosition(SKSE::PluginHandle a_pluginHandle,
                     bool a_shouldMoveToGoal, bool a_moveNow = false,
                     const RE::Actor *a_reference = nullptr) noexcept = 0;
  virtual void GetGoalPosition(RE::TESObjectREFR *a_reference,
                               RE::NiPoint3 &a_world,
                               RE::NiPoint3 &a_local) const noexcept = 0;
  virtual bool IsCameraEnabled() const noexcept = 0;
};

struct PluginCommand {
  enum class Type : std::uint8_t { RequestInterface };

  std::uint32_t header{kSmoothCamCommandHeader};
  Type type{Type::RequestInterface};
  void *commandStructure{nullptr};
};

struct InterfaceRequest {
  InterfaceVersion interfaceVersion{InterfaceVersion::V2};
};

struct PluginResponse {
  enum class Type : std::uint8_t { Error, InterfaceProvider };

  Type type{Type::Error};
  void *responseData{nullptr};
};

struct InterfaceContainer {
  void *interfaceInstance{nullptr};
  InterfaceVersion interfaceVersion{InterfaceVersion::V1};
};

std::mutex g_interfaceMutex;
ISmoothCamV2 *g_interface{nullptr};
bool g_listenerRegistered{false};
bool g_controlOwned{false};
bool g_denialLogged{false};

void OnSmoothCamMessage(SKSE::MessagingInterface::Message *a_message) {
  if (a_message == nullptr || a_message->sender == nullptr ||
      std::strcmp(a_message->sender, kSmoothCamPluginName) != 0 ||
      a_message->type != 0 ||
      a_message->dataLen != sizeof(PluginResponse) ||
      a_message->data == nullptr) {
    return;
  }

  const auto *response = static_cast<const PluginResponse *>(a_message->data);
  if (response->type != PluginResponse::Type::InterfaceProvider ||
      response->responseData == nullptr) {
    logger::debug("SmoothCam V2 interface was not provided");
    return;
  }

  const auto *container =
      static_cast<const InterfaceContainer *>(response->responseData);
  if (container->interfaceInstance == nullptr ||
      container->interfaceVersion < InterfaceVersion::V2) {
    logger::debug("SmoothCam interface is older than V2");
    return;
  }

  const std::scoped_lock lock(g_interfaceMutex);
  g_interface = static_cast<ISmoothCamV2 *>(container->interfaceInstance);
  logger::info("Connected to SmoothCam V2 camera-control API");
}
} // namespace

namespace sfs::native::smoothcam {

void RegisterInterfaceListener() {
  const std::scoped_lock lock(g_interfaceMutex);
  if (g_listenerRegistered ||
      ::GetModuleHandleW(kSmoothCamModuleName) == nullptr) {
    return;
  }
  auto *messaging = SKSE::GetMessagingInterface();
  if (messaging != nullptr &&
      messaging->RegisterListener(kSmoothCamPluginName,
                                  OnSmoothCamMessage)) {
    g_listenerRegistered = true;
  }
}

void RequestInterface() {
  if (::GetModuleHandleW(kSmoothCamModuleName) == nullptr) {
    return;
  }
  auto *messaging = SKSE::GetMessagingInterface();
  if (messaging == nullptr) {
    return;
  }

  InterfaceRequest request;
  PluginCommand command;
  command.commandStructure = std::addressof(request);
  static_cast<void>(messaging->Dispatch(0, std::addressof(command),
                                        sizeof(command),
                                        kSmoothCamPluginName));
}

bool AcquireCameraControl() {
  const std::scoped_lock lock(g_interfaceMutex);
  if (g_interface == nullptr || !g_interface->IsCameraEnabled()) {
    return true;
  }

  const auto pluginHandle = SKSE::GetPluginHandle();
  const auto result = g_interface->RequestCameraControl(pluginHandle);
  if (result != ApiResult::OK && result != ApiResult::AlreadyGiven) {
    if (!g_denialLogged) {
      logger::warn("SmoothCam refused SFS menu camera control (result={})",
                   static_cast<std::uint8_t>(result));
      g_denialLogged = true;
    }
    return false;
  }

  g_controlOwned = true;
  g_denialLogged = false;
  // RequestCameraControl already disables SmoothCam's interpolator updates.
  // Do not override that API default while SFS owns the menu camera.
  return true;
}

void ReleaseCameraControl() {
  const std::scoped_lock lock(g_interfaceMutex);
  if (!g_controlOwned || g_interface == nullptr) {
    g_controlOwned = false;
    return;
  }

  const auto pluginHandle = SKSE::GetPluginHandle();
  static_cast<void>(g_interface->ReleaseCameraControl(pluginHandle));
  g_controlOwned = false;
}

} // namespace sfs::native::smoothcam
