#include "native/DaveIntegration.h"

#include "ArmorUtils.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>

namespace {
constexpr auto kDavePluginName = "DynamicArmorVariants";
constexpr auto kDaveModuleName = L"DynamicArmorVariants.dll";
constexpr auto kHiddenVariantPrefix = "SFS_HideRealEquipment_";
constexpr auto kInterfaceQueryRetryDelay = std::chrono::seconds(5);

struct DynamicArmorVariantsExtendedMessage {
  enum : std::uint32_t { kMessage_QueryInterface = 'DAVX' };

  void *(*GetApiFunction)(unsigned int revisionNumber) = nullptr;
};

struct IDynamicArmorVariantsExtendedInterface001 {
  virtual bool IsReady() = 0;
  virtual bool RegisterVariantJson(const char *a_name,
                                   const char *a_variantJson) = 0;
  virtual bool DeleteVariant(const char *a_name) = 0;
  virtual bool SetVariantConditionsJson(const char *a_name,
                                        const char *a_conditionsJson) = 0;
  virtual bool RefreshActor(RE::Actor *a_actor) = 0;
  virtual bool ApplyVariantOverride(RE::Actor *a_actor, const char *a_variant,
                                    bool a_keepExistingOverrides = false) = 0;
  virtual bool RemoveVariantOverride(RE::Actor *a_actor,
                                     const char *a_variant) = 0;
  virtual bool
  SetCondition(const char *a_name,
               const std::shared_ptr<RE::TESCondition> &a_condition) = 0;
};

std::mutex g_stateMutex;
std::mutex g_interfaceMutex;
IDynamicArmorVariantsExtendedInterface001 *g_daveInterface{nullptr};
bool g_interfaceWarningLogged{false};
bool g_nativeApiUnavailable{false};
std::chrono::steady_clock::time_point g_nextInterfaceQueryTime{};
std::unordered_map<RE::FormID, std::string> g_hiddenVariantByActor;
std::unordered_map<RE::FormID, std::string> g_hiddenVariantPayloadByActor;
std::unordered_set<RE::FormID> g_hiddenVariantDirtyActors;

[[nodiscard]] std::string BuildHiddenVariantName(const RE::FormID a_actorID) {
  return std::string(kHiddenVariantPrefix) + sfs::armor::FormatFormID(a_actorID);
}

[[nodiscard]] std::vector<std::string>
NormalizeIdentifiers(const std::vector<std::string> &a_identifiers) {
  std::unordered_set<std::string> unique;
  unique.reserve(a_identifiers.size());
  for (const auto &identifier : a_identifiers) {
    if (!identifier.empty()) {
      unique.insert(identifier);
    }
  }

  std::vector<std::string> normalized(unique.begin(), unique.end());
  std::ranges::sort(normalized);
  return normalized;
}

[[nodiscard]] std::string EscapeJsonString(std::string_view a_value) {
  std::string escaped;
  escaped.reserve(a_value.size() + 8);
  for (const auto ch : a_value) {
    switch (ch) {
    case '"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '\b':
      escaped += "\\b";
      break;
    case '\f':
      escaped += "\\f";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped.push_back(ch);
      break;
    }
  }
  return escaped;
}

[[nodiscard]] std::string
BuildHiddenVariantPayload(const std::string &a_name,
                          const std::vector<std::string> &a_identifiers) {
  std::string payload;
  payload.reserve(160 + (a_identifiers.size() * 48));
  payload += "{\"name\":\"";
  payload += EscapeJsonString(a_name);
  payload += "\",\"displayName\":\"$SFS Hidden Real Equipment\",";
  payload += "\"priority\":1000000,\"replaceByForm\":{";

  bool first = true;
  for (const auto &identifier : a_identifiers) {
    if (!first) {
      payload += ',';
    }
    first = false;
    payload += '"';
    payload += EscapeJsonString(identifier);
    payload += "\":[]";
  }
  payload += "}}";
  return payload;
}

[[nodiscard]] IDynamicArmorVariantsExtendedInterface001 *TryGetDaveInterface(
    const bool a_forceRetry = false) {
  std::lock_guard interfaceLock(g_interfaceMutex);
  if (g_daveInterface) {
    return g_daveInterface;
  }

  if (g_nativeApiUnavailable) {
    return nullptr;
  }

  if (::GetModuleHandleW(kDaveModuleName) == nullptr) {
    return nullptr;
  }

  const auto now = std::chrono::steady_clock::now();
  if (!a_forceRetry && g_interfaceWarningLogged && now < g_nextInterfaceQueryTime) {
    return nullptr;
  }

  auto *messaging = SKSE::GetMessagingInterface();
  if (!messaging) {
    return nullptr;
  }

  DynamicArmorVariantsExtendedMessage message;
  const bool dispatched = messaging->Dispatch(
      DynamicArmorVariantsExtendedMessage::kMessage_QueryInterface,
      std::addressof(message), sizeof(message), kDavePluginName);
  if (!dispatched) {
    g_nextInterfaceQueryTime = now + kInterfaceQueryRetryDelay;
    if (!g_interfaceWarningLogged) {
      logger::warn(
          "DynamicArmorVariants.dll has not provided a DAVE native API listener yet; SFS will retry after DataLoaded before finalizing native fallback.");
      g_interfaceWarningLogged = true;
    }
    return nullptr;
  }

  if (!message.GetApiFunction) {
    g_nextInterfaceQueryTime = now + kInterfaceQueryRetryDelay;
    if (!g_interfaceWarningLogged) {
      logger::warn(
          "DynamicArmorVariants.dll is loaded, but the DAVE native API was not provided yet. SFS will retry before applying DAVE-assisted real equipment hiding.");
      g_interfaceWarningLogged = true;
    }
    return nullptr;
  }

  g_daveInterface =
      static_cast<IDynamicArmorVariantsExtendedInterface001 *>(
          message.GetApiFunction(1));
  if (g_daveInterface) {
    logger::info("Connected to Dynamic Armor Variants Extended native API");
  }
  return g_daveInterface;
}

void ForgetHiddenVariant(const RE::FormID a_actorID) {
  std::lock_guard lock(g_stateMutex);
  g_hiddenVariantByActor.erase(a_actorID);
  g_hiddenVariantPayloadByActor.erase(a_actorID);
  g_hiddenVariantDirtyActors.erase(a_actorID);
}
} // namespace

namespace sfs::native::dave {
bool IsDynamicArmorVariantsLoaded() {
  return ::GetModuleHandleW(kDaveModuleName) != nullptr;
}

bool HasNativeApi(const bool a_forceRetry) {
  return TryGetDaveInterface(a_forceRetry) != nullptr;
}

bool IsApiReady() {
  auto *api = TryGetDaveInterface();
  return api && api->IsReady();
}

bool RefreshActor(RE::Actor *a_actor) {
  if (!a_actor) {
    return false;
  }

  auto *api = TryGetDaveInterface();
  if (!api || !api->IsReady()) {
    return false;
  }

  return api->RefreshActor(a_actor);
}

void LockToNativeFallback() {
  std::lock_guard interfaceLock(g_interfaceMutex);
  g_daveInterface = nullptr;
  g_nativeApiUnavailable = true;
  g_nextInterfaceQueryTime = {};
}

void MarkHiddenRealEquipmentDirty(RE::Actor *a_actor) {
  if (!a_actor || a_actor->GetFormID() == 0 ||
      !IsDynamicArmorVariantsLoaded()) {
    return;
  }
  std::lock_guard lock(g_stateMutex);
  g_hiddenVariantDirtyActors.insert(a_actor->GetFormID());
}

void SyncHiddenRealEquipment(
    RE::Actor *a_actor,
    const std::vector<std::string> &a_sourceArmorAddonIdentifiers) {
  if (!a_actor) {
    return;
  }

  auto *api = TryGetDaveInterface();
  if (!api || !api->IsReady()) {
    logger::debug(
        "SFS DAVE native: SyncHiddenRealEquipment skipped actor={:08X} apiReady=false identifiers={}",
        a_actor->GetFormID(), a_sourceArmorAddonIdentifiers.size());
    return;
  }

  const auto actorID = a_actor->GetFormID();
  if (actorID == 0) {
    return;
  }

  auto identifiers = NormalizeIdentifiers(a_sourceArmorAddonIdentifiers);
  logger::debug(
      "SFS DAVE native: SyncHiddenRealEquipment actor={:08X} identifiers={} rawIdentifiers={}",
      actorID, identifiers.size(), a_sourceArmorAddonIdentifiers.size());
  if (identifiers.empty()) {
    ClearHiddenRealEquipment(a_actor);
    return;
  }

  const auto variantName = BuildHiddenVariantName(actorID);
  const auto payload = BuildHiddenVariantPayload(variantName, identifiers);
  bool forceReapply = false;

  {
    std::lock_guard lock(g_stateMutex);
    forceReapply = g_hiddenVariantDirtyActors.contains(actorID);
    if (const auto payloadIt = g_hiddenVariantPayloadByActor.find(actorID);
        payloadIt != g_hiddenVariantPayloadByActor.end() &&
        payloadIt->second == payload && !forceReapply) {
      logger::debug(
          "SFS DAVE native: SyncHiddenRealEquipment actor={:08X} skipped unchanged variant='{}'",
          actorID, variantName);
      return;
    }
  }

  if (!api->RegisterVariantJson(variantName.c_str(), payload.c_str())) {
    logger::warn("Failed to register DAVE hidden real equipment variant '{}'",
                 variantName);
    return;
  }

  if (!api->ApplyVariantOverride(a_actor, variantName.c_str(), true)) {
    logger::warn("Failed to apply DAVE hidden real equipment variant '{}' to {}",
                 variantName, actorID);
    return;
  }
  logger::debug(
      "SFS DAVE native: SyncHiddenRealEquipment actor={:08X} applied variant='{}' identifiers={} forceReapply={}",
      actorID, variantName, identifiers.size(), forceReapply);
#if defined(SFS_VIRTUAL_TOKENS)
  if (forceReapply) {
    logger::info(
        "DAVE hidden real equipment reapplied actor={:08X} identifiers={}",
        actorID, identifiers.size());
  }
#endif

  std::lock_guard lock(g_stateMutex);
  g_hiddenVariantByActor[actorID] = variantName;
  g_hiddenVariantPayloadByActor[actorID] = payload;
  g_hiddenVariantDirtyActors.erase(actorID);
}

void ClearHiddenRealEquipment(RE::Actor *a_actor) {
  if (!a_actor) {
    return;
  }

  auto *api = TryGetDaveInterface();
  if (!api || !api->IsReady()) {
    logger::debug(
        "SFS DAVE native: ClearHiddenRealEquipment skipped actor={:08X} apiReady=false",
        a_actor->GetFormID());
    return;
  }

  const auto actorID = a_actor->GetFormID();
  if (actorID == 0) {
    return;
  }

  std::string variantName;
  {
    std::lock_guard lock(g_stateMutex);
    if (const auto it = g_hiddenVariantByActor.find(actorID);
        it != g_hiddenVariantByActor.end()) {
      variantName = it->second;
    } else {
      variantName = BuildHiddenVariantName(actorID);
    }
  }

  api->RemoveVariantOverride(a_actor, variantName.c_str());
  api->DeleteVariant(variantName.c_str());
  logger::debug(
      "SFS DAVE native: ClearHiddenRealEquipment actor={:08X} variant='{}'",
      actorID, variantName);
  ForgetHiddenVariant(actorID);
}

void ForgetHiddenRealEquipmentState() {
  std::lock_guard lock(g_stateMutex);
  g_hiddenVariantByActor.clear();
  g_hiddenVariantPayloadByActor.clear();
  g_hiddenVariantDirtyActors.clear();
}
} // namespace sfs::native::dave
