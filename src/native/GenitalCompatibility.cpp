#include "native/GenitalCompatibility.h"

#include "ArmorUtils.h"

#include <atomic>

namespace {
std::atomic_bool g_sosInstalled{false};
std::atomic_bool g_tngInstalled{false};

[[nodiscard]] RE::TESForm *FindSosApiForm() {
  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  auto *apiForm =
      dataHandler
          ? dataHandler->LookupForm(0x1EDA4, "Schlongs of Skyrim.esp")
          : nullptr;
  if (!apiForm) {
    apiForm = RE::TESForm::LookupByEditorID<RE::TESQuest>("SOS_Misc");
  }
  return apiForm;
}
} // namespace

namespace sfs::native::genital_compatibility {

void InitializeEnvironment() {
  auto *sosApiForm = FindSosApiForm();
  auto *tngCover =
      RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("TNG_GenitalCover");
  const bool sosInstalled = sosApiForm != nullptr;
  const bool tngInstalled =
      tngCover != nullptr && sfs::armor::IsTngGenitalCoverArmor(tngCover);
  g_sosInstalled.store(sosInstalled, std::memory_order_release);
  g_tngInstalled.store(tngInstalled, std::memory_order_release);
  logger::info("Detected genital compatibility environment: SOS={} "
               "apiForm={:08X}, TNG={} coverForm={:08X}",
               sosInstalled, sosApiForm ? sosApiForm->GetFormID() : 0,
               tngInstalled, tngCover ? tngCover->GetFormID() : 0);
}

rules::Environment GetEnvironment() {
  return {.sosInstalled =
              g_sosInstalled.load(std::memory_order_acquire),
          .tngInstalled =
              g_tngInstalled.load(std::memory_order_acquire)};
}

bool IsSosInstalled() { return GetEnvironment().sosInstalled; }

bool IsTngInstalled() { return GetEnvironment().tngInstalled; }

} // namespace sfs::native::genital_compatibility
