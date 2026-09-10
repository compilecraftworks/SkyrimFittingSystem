// Actual production negotiation/selection functions; providers and engine
// installation are recording fakes. No third-party DLL is loaded by this test.
#include "native/RegisteredAppearanceMorphRules.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

void Expect(bool value, const char* message) {
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
namespace logger {
template<class... T> void info(T&&...) {}
template<class... T> void warn(T&&...) {}
template<class... T> void debug(T&&...) {}
}
namespace dave_fixture { extern bool moduleLoaded; }
void* GetModuleHandleW(const wchar_t*) {
  return dave_fixture::moduleLoaded ? reinterpret_cast<void*>(1) : nullptr;
}
namespace dave_fixture {
struct IDynamicArmorVariantsExtendedInterface001 {} api;
struct DynamicArmorVariantsExtendedMessage {
  enum : unsigned { kMessage_QueryInterface = 'DAVX' };
  void* (*GetApiFunction)(unsigned){};
};
bool moduleLoaded{}, listener{}, publishesApi{}, correctRevision{true};
unsigned queries{}, fallbackLocks{}, nativeInstalls{};
void* GetApi(unsigned revision) { return revision == 1 && correctRevision ? &api : nullptr; }
namespace SKSE {
struct Messaging {
  bool Dispatch(unsigned, void* request, std::size_t, const char*) {
    ++queries;
    if (!listener) return false;
    if (publishesApi) static_cast<DynamicArmorVariantsExtendedMessage*>(request)->GetApiFunction = GetApi;
    return true;
  }
} messaging;
Messaging* GetMessagingInterface() { return &messaging; }
}
constexpr auto kDavePluginName = "DynamicArmorVariants";
constexpr auto kDaveModuleName = L"DynamicArmorVariants.dll";
constexpr auto kInterfaceQueryRetryDelay = std::chrono::seconds(5);
std::mutex g_interfaceMutex;
IDynamicArmorVariantsExtendedInterface001* g_daveInterface{};
bool g_nativeApiUnavailable{}, g_interfaceWarningLogged{}, g_realEquipmentBackendConfigured{};
std::chrono::steady_clock::time_point g_nextInterfaceQueryTime{};
#include "IntegrationDaveQuery.production.inc"
namespace sfs::runtime { struct HookLayout {}; }
namespace sfs::native::dave {
bool IsDynamicArmorVariantsLoaded() { return moduleLoaded; }
bool HasNativeApi(bool force) { return TryGetDaveInterface(force) != nullptr; }
void LockToNativeFallback() { ++fallbackLocks; g_nativeApiUnavailable = true; }
}
bool InstallDontVanillaSkinHook(const sfs::runtime::HookLayout&) { ++nativeInstalls; return true; }
#include "IntegrationBackend.production.inc"
void Reset() {
  moduleLoaded = listener = publishesApi = false; correctRevision = true;
  queries = fallbackLocks = nativeInstalls = 0;
  g_daveInterface = nullptr;
  g_nativeApiUnavailable = g_interfaceWarningLogged = g_realEquipmentBackendConfigured = false;
  g_nextInterfaceQueryTime = {};
}
void Test() {
  sfs::runtime::HookLayout layout;
  Reset(); moduleLoaded = true;
  Expect(!ConfigureRealEquipmentSkinningBackend(layout, false), "absent early DAVE listener must defer ownership selection");
  Expect(!g_nativeApiUnavailable && !fallbackLocks && !nativeInstalls, "early miss must not lock fallback or install conflicting hooks");
  listener = publishesApi = true;
  Expect(ConfigureRealEquipmentSkinningBackend(layout, true), "late DAVE listener must recover at final lifecycle rendezvous");
  Expect(g_daveInterface == &api && queries == 2 && !nativeInstalls && !fallbackLocks, "final retry bypasses throttle without native/DAVE double ownership");
  ConfigureRealEquipmentSkinningBackend(layout, true);
  Expect(queries == 2, "configured backend must not reinstall on repeated fences");
  Reset(); moduleLoaded = listener = true;
  Expect(!ConfigureRealEquipmentSkinningBackend(layout, false), "listener without API must also remain retryable");
  publishesApi = true;
  ConfigureRealEquipmentSkinningBackend(layout, true);
  Expect(g_daveInterface == &api && !nativeInstalls, "late API publication must recover independently of listener timing");
  Reset(); moduleLoaded = true;
  ConfigureRealEquipmentSkinningBackend(layout, false);
  ConfigureRealEquipmentSkinningBackend(layout, true);
  Expect(nativeInstalls == 1 && fallbackLocks == 1, "ordinary DAV must receive exactly one final native fallback");
  listener = publishesApi = true;
  ConfigureRealEquipmentSkinningBackend(layout, true);
  Expect(nativeInstalls == 1 && !g_daveInterface, "do not switch an already-installed native hook owner during gameplay");
  Reset();
  ConfigureRealEquipmentSkinningBackend(layout, false);
  ConfigureRealEquipmentSkinningBackend(layout, true);
  Expect(nativeInstalls == 1 && queries == 0 && fallbackLocks == 0, "plain native must not depend on DAVE availability");
}
}

namespace race_fixture {
namespace rules = ::sfs::native::racemenu::rules;
namespace skee {
struct IPluginInterface { virtual ~IPluginInterface() = default; virtual unsigned GetVersion() = 0; };
struct IBodyMorphInterface : IPluginInterface { unsigned version{4}; unsigned GetVersion() override { return version; } } morph;
struct INiTransformInterface : IPluginInterface { unsigned version{3}; unsigned GetVersion() override { return version; } } transform;
}
bool exchangeReady{}, publishMorph{}, publishTransform{}, publishObserver{}, publicHookReady{true}, taskHookReady{true};
unsigned exchanges{}, registrations{}, publicInstalls{}, taskInstalls{};
std::atomic_bool g_bodyMorphInitializationComplete{}, g_applyBodyMorphsHookInstalled{}, g_updateModelWeightTaskHookInstalled{}, g_attachmentObserverRegistered{};
std::atomic_uint32_t g_bodyMorphInitializationAttempts{}, g_transformInterfaceVersion{};
std::atomic<skee::IBodyMorphInterface*> g_bodyMorphInterface{};
std::atomic<skee::INiTransformInterface*> g_transformInterface{};
std::mutex g_initializeMutex;
int g_attachmentObserver{};
struct Map {
  skee::IPluginInterface* QueryInterface(const char* name) {
    if (std::strcmp(name,"NiTransform") == 0) return publishTransform ? &skee::transform : nullptr;
    return publishObserver ? &skee::morph : nullptr;
  }
} map;
namespace SKSE { int* GetMessagingInterface() { static int value; return &value; } }
struct ExchangeResult { Map* interfaceMap; skee::IPluginInterface* bodyMorphPlugin; unsigned bodyMorphVersion; const char* route; };
std::optional<ExchangeResult> TryInterfaceExchange(int*, unsigned, const char*, const char*) {
  ++exchanges;
  if (!exchangeReady) return {};
  return ExchangeResult{&map, publishMorph ? &skee::morph : nullptr, publishMorph ? skee::morph.version : 0U, "fixture"};
}
bool InstallApplyBodyMorphsHook(skee::IBodyMorphInterface*) {
  if (g_applyBodyMorphsHookInstalled) return true;
  if (publicHookReady) { ++publicInstalls; g_applyBodyMorphsHookInstalled = true; }
  return publicHookReady;
}
bool InstallUpdateModelWeightTaskHook() {
  if (g_updateModelWeightTaskHookInstalled) return true;
  if (taskHookReady) { ++taskInstalls; g_updateModelWeightTaskHookInstalled = true; }
  return taskHookReady;
}
namespace abi {
bool HasCallableInterfacePrefix(skee::IPluginInterface* provider, std::size_t) { return provider != nullptr; }
enum class AttachmentRegistrationStatus { Registered, AlreadyRegistered, UnsupportedLayout, Unavailable };
struct Result { AttachmentRegistrationStatus status; unsigned version{0}; int layout{0}; };
const char* AttachmentInterfaceLayoutName(int) { return "fixture"; }
Result RegisterAttachmentObserver(skee::IPluginInterface* provider, int*, std::atomic_bool& registered) {
  if (registered) return {AttachmentRegistrationStatus::AlreadyRegistered};
  if (!provider) return {AttachmentRegistrationStatus::Unavailable};
  ++registrations; registered = true; return {AttachmentRegistrationStatus::Registered};
}
}
#include "IntegrationRaceMenu.production.inc"
void Test() {
  InitializeBodyMorphInterface();
  Expect(!IsBodyMorphInterfaceReady(), "missing map must remain retryable");
  exchangeReady = publishTransform = publishObserver = true;
  InitializeBodyMorphInterface();
  Expect(g_transformInterface == &skee::transform && registrations == 1 && !IsBodyMorphInterfaceReady(), "missing BodyMorph must not disable independent transform/observer registration");
  publishMorph = true; publicHookReady = taskHookReady = false;
  InitializeBodyMorphInterface();
  Expect(IsBodyMorphInterfaceReady(), "late BodyMorph after successful partial map must become available");
  Expect(!publicInstalls && !taskInstalls && registrations == 1, "partial hook failure must not double-register observer");
  exchangeReady = false;
  InitializeBodyMorphInterface();
  Expect(IsBodyMorphInterfaceReady() && g_transformInterface == &skee::transform, "a later failed exchange must not erase working interfaces");
  exchangeReady = publicHookReady = taskHookReady = true;
  publishMorph = publishTransform = publishObserver = false;
  InitializeBodyMorphInterface();
  Expect(publicInstalls == 1 && taskInstalls == 1 && registrations == 1, "retry only missing hooks while retaining the original connected objects");
  const auto before = exchanges;
  InitializeBodyMorphInterface();
  Expect(exchanges == before && publicInstalls == 1, "complete initialization must be idempotent");
  g_bodyMorphInterface = nullptr; g_transformInterface = nullptr;
  g_transformInterfaceVersion = 0; publishMorph = publishTransform = true;
  skee::morph.version = 6; skee::transform.version = 4;
  InitializeBodyMorphInterface();
  Expect(IsBodyMorphInterfaceReady() && g_transformInterface == &skee::transform &&
      g_transformInterfaceVersion == 4 && registrations == 1,
      "newer versions must retain the last public prefix without a package allowlist or duplicate hooks");
}
}
namespace oar_fixture {
bool apiReady = false, conditionReady = false;
int attempts = 0;
namespace OAR_API::Conditions { void* GetAPI() { return apiReady ? &attempts : nullptr; } }
struct IsShownArmorEquipped {};
struct ShownArmorHasKeyword {};
struct ShownArmorInSlotHasKeyword {};
struct IsShownBodyNaked {};
template<class T> bool RegisterCondition() { ++attempts; return conditionReady; }
#include "IntegrationOar.production.inc"
void Test() {
  RegisterConditions();
  Expect(attempts == 0, "missing OAR API must not consume the success latch");
  apiReady = true; RegisterConditions();
  Expect(attempts == 4, "attempt all OAR conditions even when one fails");
  conditionReady = true; RegisterConditions();
  Expect(attempts == 8, "retry OAR registration after early API/condition failure");
  RegisterConditions();
  Expect(attempts == 8, "successful OAR initialization must remain idempotent");
}
}
int main() {
  dave_fixture::Test(); race_fixture::Test(); oar_fixture::Test();
  std::puts("Production DAVE/RaceMenu initialization recovery tests passed");
}
