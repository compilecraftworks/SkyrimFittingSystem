#include "Plugin.h"

#include <Windows.h>
#include <bcrypt.h>
#include <xbyak/xbyak.h>

#include <array>
#include <cstring>
#include <fstream>
#include <mutex>
#include <vector>

namespace {
constexpr auto kDynamicFootprintsModuleName = L"NMN_DynamicFootprints.dll";
constexpr auto kSfsCoreModuleName = L"SFSCore.dll";

// Dynamic Footprints SKSE BASE v3.0, NMN_DynamicFootprints.dll.
constexpr std::array<std::uint8_t, 32> kDynamicFootprintsV30Sha256{
    0x9D, 0xF6, 0x16, 0xD7, 0x7A, 0x1F, 0x90, 0xF3,
    0x8F, 0xBE, 0xF5, 0xB4, 0x46, 0xE4, 0xA9, 0x5B,
    0x38, 0xEC, 0x43, 0xF3, 0x72, 0x49, 0x6E, 0xFD,
    0xD5, 0x57, 0x02, 0x2C, 0x46, 0xE5, 0x7B, 0x71};

constexpr std::ptrdiff_t kFootwearLookupCallOffset = 0x1FCD7;
constexpr std::ptrdiff_t kLocalGetWornArmorFeetOffset = 0x54C70;
constexpr std::array<std::uint8_t, 5> kExpectedCallBytes{
    0xE8, 0x94, 0x4F, 0x03, 0x00};
constexpr std::array<std::uint8_t, 16> kExpectedFunctionPrefix{
    0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x08, 0x49,
    0x89, 0x6B, 0x10, 0x49, 0x89, 0x73, 0x20, 0x57};

using GetHostApiVersion = std::uint32_t(SKSEAPI *)();
using GetDisplayedFootwearFormID = std::uint32_t(SKSEAPI *)(std::uint32_t);
using TryGetDisplayedFootwearFormID = bool(SKSEAPI *)(std::uint32_t,
                                                      std::uint32_t *);
using GetWornArmor = RE::TESObjectARMO *(SKSEAPI *)(
    RE::Actor *, RE::BIPED_MODEL::BipedObjectSlot, bool);

GetDisplayedFootwearFormID g_getDisplayedFootwearFormID = nullptr;
TryGetDisplayedFootwearFormID g_tryGetDisplayedFootwearFormID = nullptr;
GetWornArmor g_originalGetWornArmor = nullptr;
SKSE::Trampoline g_dynamicFootprintsTrampoline{
    "SFS Dynamic Footprints compatibility"};
std::mutex g_installLock;
bool g_installed = false;

[[nodiscard]] bool GetModuleSha256(
    const HMODULE a_module, std::array<std::uint8_t, 32> &a_digest) {
  std::array<wchar_t, 32768> path{};
  const auto length = ::GetModuleFileNameW(a_module, path.data(),
                                            static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size()) {
    logger::warn("SFS Dynamic Footprints patch could not resolve the module path");
    return false;
  }

  std::ifstream file(path.data(), std::ios::binary);
  if (!file) {
    logger::warn("SFS Dynamic Footprints patch could not open the module file");
    return false;
  }

  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD hashObjectLength = 0;
  DWORD resultLength = 0;
  std::vector<std::uint8_t> hashObject;

  const auto close = [&] {
    if (hash != nullptr) {
      BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
      BCryptCloseAlgorithmProvider(algorithm, 0);
    }
  };

  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                  nullptr, 0) != 0 ||
      BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&hashObjectLength),
                        sizeof(hashObjectLength), &resultLength, 0) != 0) {
    close();
    logger::warn("SFS Dynamic Footprints patch could not initialize SHA-256");
    return false;
  }

  hashObject.resize(hashObjectLength);
  if (BCryptCreateHash(algorithm, &hash, hashObject.data(),
                       static_cast<ULONG>(hashObject.size()), nullptr, 0,
                       0) != 0) {
    close();
    logger::warn("SFS Dynamic Footprints patch could not create SHA-256 state");
    return false;
  }

  std::array<std::uint8_t, 64 * 1024> buffer{};
  while (file) {
    file.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
    const auto read = file.gcount();
    if (read > 0 &&
        BCryptHashData(hash, buffer.data(), static_cast<ULONG>(read), 0) !=
            0) {
      close();
      logger::warn("SFS Dynamic Footprints patch could not hash the module file");
      return false;
    }
  }
  if (file.bad()) {
    close();
    logger::warn("SFS Dynamic Footprints patch could not finish reading the module file");
    return false;
  }

  const auto success =
      BCryptFinishHash(hash, a_digest.data(), static_cast<ULONG>(a_digest.size()), 0) == 0;
  close();
  if (!success) {
    logger::warn("SFS Dynamic Footprints patch could not finish SHA-256");
  }
  return success;
}

[[nodiscard]] bool ResolveSfsFootwearApi() {
  const auto sfsCore = ::GetModuleHandleW(kSfsCoreModuleName);
  if (sfsCore == nullptr) {
    logger::info("SFS Dynamic Footprints patch: SFSCore.dll is not loaded; patch remains inactive");
    return false;
  }

  const auto getVersion = reinterpret_cast<GetHostApiVersion>(::GetProcAddress(
      sfsCore, "SkyrimFittingSystem_GetDynamicFootprintsHostAPIVersion"));
  g_getDisplayedFootwearFormID = reinterpret_cast<GetDisplayedFootwearFormID>(
      ::GetProcAddress(sfsCore,
                       "SkyrimFittingSystem_GetDisplayedFootwearFormID"));
  g_tryGetDisplayedFootwearFormID =
      reinterpret_cast<TryGetDisplayedFootwearFormID>(::GetProcAddress(
          sfsCore, "SkyrimFittingSystem_TryGetDisplayedFootwearFormID"));
  if (getVersion == nullptr || g_getDisplayedFootwearFormID == nullptr ||
      getVersion() != 1) {
    g_getDisplayedFootwearFormID = nullptr;
    logger::warn("SFS Dynamic Footprints patch: SFSCore does not provide host API v1; patch remains inactive");
    return false;
  }

  return true;
}

[[nodiscard]] RE::TESObjectARMO *GetDisplayedOrActualFootwear(
    RE::Actor *a_actor, const RE::BIPED_MODEL::BipedObjectSlot a_slot,
    const bool a_noInit) {
  const auto feetSlot = static_cast<std::uint32_t>(
      RE::BIPED_MODEL::BipedObjectSlot::kFeet);
  if (a_actor != nullptr && g_getDisplayedFootwearFormID != nullptr &&
      static_cast<std::uint32_t>(a_slot) == feetSlot) {
    if (g_tryGetDisplayedFootwearFormID != nullptr) {
      std::uint32_t finalFormID = 0;
      if (g_tryGetDisplayedFootwearFormID(a_actor->GetFormID(),
                                         &finalFormID)) {
        return finalFormID != 0
                   ? RE::TESForm::LookupByID<RE::TESObjectARMO>(finalFormID)
                   : nullptr;
      }
    }
    const auto displayedFormID =
        g_getDisplayedFootwearFormID(a_actor->GetFormID());
    if (displayedFormID != 0) {
      if (auto *armor =
              RE::TESForm::LookupByID<RE::TESObjectARMO>(displayedFormID)) {
        return armor;
      }
    }
  }

  return g_originalGetWornArmor != nullptr
             ? g_originalGetWornArmor(a_actor, a_slot, a_noInit)
             : nullptr;
}

[[nodiscard]] bool VerifyDynamicFootprintsV30(
    const HMODULE a_dynamicFootprints) {
  std::array<std::uint8_t, 32> hash{};
  if (!GetModuleSha256(a_dynamicFootprints, hash) ||
      hash != kDynamicFootprintsV30Sha256) {
    logger::warn("SFS Dynamic Footprints patch supports only the verified Dynamic Footprints SKSE BASE v3.0 DLL; patch remains inactive");
    return false;
  }

  const auto base = reinterpret_cast<std::uintptr_t>(a_dynamicFootprints);
  const auto callSite = base + kFootwearLookupCallOffset;
  const auto function = base + kLocalGetWornArmorFeetOffset;
  if (std::memcmp(reinterpret_cast<const void *>(callSite),
                  kExpectedCallBytes.data(), kExpectedCallBytes.size()) != 0 ||
      std::memcmp(reinterpret_cast<const void *>(function),
                  kExpectedFunctionPrefix.data(),
                  kExpectedFunctionPrefix.size()) != 0) {
    logger::warn("SFS Dynamic Footprints patch found an unexpected v3.0 code signature; patch remains inactive");
    return false;
  }

  std::int32_t displacement = 0;
  std::memcpy(&displacement, reinterpret_cast<const void *>(callSite + 1),
              sizeof(displacement));
  if (callSite + 5 + displacement != function) {
    logger::warn("SFS Dynamic Footprints patch found an unexpected footwear lookup target; patch remains inactive");
    return false;
  }

  g_originalGetWornArmor = reinterpret_cast<GetWornArmor>(function);
  return true;
}

void Install() {
  std::scoped_lock lock(g_installLock);
  if (g_installed) {
    return;
  }
  if (!ResolveSfsFootwearApi()) {
    return;
  }

  const auto dynamicFootprints =
      ::GetModuleHandleW(kDynamicFootprintsModuleName);
  if (dynamicFootprints == nullptr) {
    logger::info("SFS Dynamic Footprints patch: NMN_DynamicFootprints.dll is not loaded; patch remains inactive");
    return;
  }
  if (!VerifyDynamicFootprintsV30(dynamicFootprints)) {
    return;
  }

  struct HookStub : Xbyak::CodeGenerator {
    HookStub() {
      Xbyak::Label hook;
      sub(rsp, 0x28);
      call(ptr[rip + hook]);
      add(rsp, 0x28);
      ret();
      L(hook);
      dq(reinterpret_cast<std::uintptr_t>(GetDisplayedOrActualFootwear));
    }
  };

  const auto base = reinterpret_cast<std::uintptr_t>(dynamicFootprints);
  const auto callSite = base + kFootwearLookupCallOffset;
  try {
    // CommonLib searches outward from the supplied address and can legally
    // choose the very bottom of its +/- 2 GiB range.  Anchoring at the DLL
    // base leaves this call site (0x1FCD7 bytes later) just outside a rel32
    // branch in that edge case.  Anchor at the exact patched instruction so
    // the call-site-to-trampoline displacement is always representable.
    g_dynamicFootprintsTrampoline.create(
        512, reinterpret_cast<void *>(callSite));
    HookStub code;
    const auto stub = g_dynamicFootprintsTrampoline.allocate(code);
    g_dynamicFootprintsTrampoline.write_call<5>(
        callSite, reinterpret_cast<std::uintptr_t>(stub));
  } catch (const std::exception &exception) {
    logger::error("SFS Dynamic Footprints patch could not install: {}", exception.what());
    return;
  }

  logger::info("Installed SFS Dynamic Footprints v3.0 footwear display bridge");
  g_installed = true;
}

void SKSEMessageHandler(SKSE::MessagingInterface::Message *a_message) {
  if (a_message->type == SKSE::MessagingInterface::kPostLoad ||
      a_message->type == SKSE::MessagingInterface::kDataLoaded) {
    Install();
  }
}
} // namespace

extern "C" DLLEXPORT bool SKSEAPI
SKSEPlugin_Load(const SKSE::LoadInterface *a_skse) {
  REL::Module::reset();
  SKSE::Init(a_skse);

  const auto messaging = reinterpret_cast<SKSE::MessagingInterface *>(
      a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
  if (messaging == nullptr) {
    logger::critical("SFS Dynamic Footprints patch could not acquire SKSE messaging");
    return false;
  }

  messaging->RegisterListener("SKSE", SKSEMessageHandler);
  logger::info("{} {} loaded", Plugin::NAME, Plugin::VERSION_STRING);
  return true;
}
