#include "api/SkyrimFittingSystemAPI.h"

#include "ArmorUtils.h"
#include "native/ArmorSkinning.h"
#include "native/GridInventoryIntegration.h"
#include "ui/Menu.h"
#include <atomic>

namespace {
enum class MenuRequest : std::uint8_t { None, Open, Close };

std::atomic<MenuRequest> g_pendingRequest{MenuRequest::None};
std::atomic_bool g_menuInitialized{false};
std::atomic_bool g_gameDataLoaded{false};
std::atomic_bool g_menuLifecycleActive{false};
std::atomic_bool g_openRequestInFlight{false};
std::atomic_bool g_hotkeyEnabled{true};

[[nodiscard]] bool CanAcceptOpenRequest() {
  return g_menuInitialized.load(std::memory_order_acquire) &&
         g_gameDataLoaded.load(std::memory_order_acquire);
}
} // namespace

bool SkyrimFittingSystem_Open() {
  if (!CanAcceptOpenRequest()) {
    return false;
  }

  if (g_menuLifecycleActive.load(std::memory_order_acquire) ||
      g_openRequestInFlight.exchange(true, std::memory_order_acq_rel)) {
    return true;
  }

  g_pendingRequest.store(MenuRequest::Open, std::memory_order_release);
  return true;
}

void SkyrimFittingSystem_Close() {
  if (!g_menuInitialized.load(std::memory_order_acquire)) {
    return;
  }

  // A close request also cancels an accepted open request that has not reached
  // the UI message queue yet. Repeated close calls simply replace Close with
  // Close and are therefore harmless.
  g_openRequestInFlight.store(false, std::memory_order_release);
  g_pendingRequest.store(MenuRequest::Close, std::memory_order_release);
}

bool SkyrimFittingSystem_IsMenuOpen() {
  // This snapshot becomes true in OnMenuShow (Opening) and remains true until
  // OnMenuHide finishes (Closed). An accepted Open request can therefore
  // return true one frame before this function does, by design.
  return g_menuLifecycleActive.load(std::memory_order_acquire);
}

void SkyrimFittingSystem_SetHotkeyEnabled(const bool a_enabled) {
  // Runtime-only by design. This value is never serialized, and the static
  // initializer restores the native shortcut on every DLL load.
  g_hotkeyEnabled.store(a_enabled, std::memory_order_release);
}

std::uint32_t SkyrimFittingSystem_GetGridInventoryCostumeAPIVersion() {
  return 1;
}

bool SkyrimFittingSystem_ApplyGridInventoryCostume(
    const std::uint32_t *a_formIDs, const std::uint32_t a_count) {
  if (!CanAcceptOpenRequest() || (a_formIDs == nullptr && a_count != 0)) {
    return false;
  }

  // Grid's Costume::Tick runs from PlayerCharacter::Update, i.e. Skyrim's
  // game thread.  Apply synchronously here so Grid can retry on a later tick
  // if SFS is still loading, rather than keeping an unacknowledged async copy.
  return sfs::Menu::GetSingleton()->ApplyGridInventoryCostume(
      a_formIDs, a_count);
}

bool SkyrimFittingSystem_ClearGridInventoryCostume() {
  if (!CanAcceptOpenRequest()) {
    return false;
  }

  return sfs::Menu::GetSingleton()->ClearGridInventoryCostume();
}

std::uint32_t SkyrimFittingSystem_GetDynamicFootprintsHostAPIVersion() {
  return 1;
}

std::uint32_t SkyrimFittingSystem_GetDisplayedFootwearFormID(
    const std::uint32_t a_actorFormID) {
  if (!CanAcceptOpenRequest() || a_actorFormID == 0) {
    return 0;
  }

  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
  if (!actor) {
    return 0;
  }

  const auto feetSlotMask = static_cast<std::uint32_t>(
      sfs::armor::GetArmorSlotMask(37));
  const auto *footwear =
      sfs::native::GetActiveFittingArmorForSlot(actor, feetSlotMask);
  return footwear ? footwear->GetFormID() : 0;
}

namespace sfs::api {
bool IsHotkeyEnabled() {
  return g_hotkeyEnabled.load(std::memory_order_acquire);
}

void SetMenuInitialized(const bool a_initialized) {
  g_menuInitialized.store(a_initialized, std::memory_order_release);
  if (!a_initialized) {
    g_pendingRequest.store(MenuRequest::None, std::memory_order_release);
    g_openRequestInFlight.store(false, std::memory_order_release);
    g_menuLifecycleActive.store(false, std::memory_order_release);
  }
}

void SetGameDataLoaded(const bool a_loaded) {
  g_gameDataLoaded.store(a_loaded, std::memory_order_release);
  if (!a_loaded) {
    g_pendingRequest.store(MenuRequest::None, std::memory_order_release);
    g_openRequestInFlight.store(false, std::memory_order_release);
  }
}

void SetMenuLifecycleActive(const bool a_active) {
  g_menuLifecycleActive.store(a_active, std::memory_order_release);
  if (a_active) {
    g_openRequestInFlight.store(false, std::memory_order_release);
  }
}

void ProcessMenuRequests() {
  // The official Grid Inventory Costume callback only copies a transition.
  // Apply it here, alongside SFS's established game/UI-safe request handling.
  native::grid_inventory::ProcessPendingCostumeState();

  const auto request =
      g_pendingRequest.exchange(MenuRequest::None, std::memory_order_acq_rel);
  switch (request) {
  case MenuRequest::Open:
    if (!CanAcceptOpenRequest()) {
      g_openRequestInFlight.store(false, std::memory_order_release);
      return;
    }
    Menu::GetSingleton()->Open();
    return;
  case MenuRequest::Close:
    Menu::GetSingleton()->Close();
    return;
  case MenuRequest::None:
    return;
  }
}
} // namespace sfs::api
