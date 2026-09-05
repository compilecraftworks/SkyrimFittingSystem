#include "native/RaceMenuActorUpdateManager.h"

#include <Windows.h>

#include <cstddef>
#include <memory>

namespace sfs::native::racemenu::abi {
namespace {

enum class SlotKind : std::uint8_t {
  Invalid,
  NonExecutable,
  ExecutableInProvider,
  ExecutableElsewhere,
};

[[nodiscard]] bool IsReadableAddress(const void *a_address,
                                     const std::size_t a_size) {
  if (a_address == nullptr || a_size == 0) {
    return false;
  }

  MEMORY_BASIC_INFORMATION memory{};
  if (VirtualQuery(a_address, std::addressof(memory), sizeof(memory)) == 0 ||
      memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 ||
      (memory.Protect & 0xFFU) == PAGE_NOACCESS) {
    return false;
  }

  const auto start = reinterpret_cast<std::uintptr_t>(a_address);
  const auto regionStart = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
  const auto regionEnd = regionStart + memory.RegionSize;
  return start >= regionStart && start <= regionEnd &&
         a_size <= regionEnd - start;
}

[[nodiscard]] bool IsExecutableAddress(const std::uintptr_t a_address) {
  if (a_address == 0) {
    return false;
  }

  MEMORY_BASIC_INFORMATION memory{};
  if (VirtualQuery(reinterpret_cast<const void *>(a_address),
                   std::addressof(memory), sizeof(memory)) == 0 ||
      memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0) {
    return false;
  }

  switch (memory.Protect & 0xFFU) {
  case PAGE_EXECUTE:
  case PAGE_EXECUTE_READ:
  case PAGE_EXECUTE_READWRITE:
  case PAGE_EXECUTE_WRITECOPY:
    return true;
  default:
    return false;
  }
}

[[nodiscard]] HMODULE ModuleForAddress(const std::uintptr_t a_address) {
  HMODULE module = nullptr;
  if (a_address == 0 ||
      !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(a_address), &module)) {
    return nullptr;
  }
  return module;
}

[[nodiscard]] SlotKind ClassifySlot(const std::uintptr_t *a_vtable,
                                    const std::size_t a_index,
                                    const HMODULE a_providerModule) {
  const auto *slot = a_vtable + a_index;
  if (!IsReadableAddress(slot, sizeof(*slot))) {
    return SlotKind::Invalid;
  }

  const auto target = *slot;
  if (!IsExecutableAddress(target)) {
    return SlotKind::NonExecutable;
  }
  return ModuleForAddress(target) == a_providerModule
             ? SlotKind::ExecutableInProvider
             : SlotKind::ExecutableElsewhere;
}

[[nodiscard]] bool HasExecutablePrefix(const std::uintptr_t *a_vtable,
                                       const std::size_t a_count,
                                       const HMODULE a_providerModule) {
  for (std::size_t index = 0; index < a_count; ++index) {
    if (ClassifySlot(a_vtable, index, a_providerModule) !=
        SlotKind::ExecutableInProvider) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] AttachmentInterfaceLayout
DetectVersionZeroLayout(IPluginInterface *a_plugin) {
  if (!IsReadableAddress(a_plugin, sizeof(void *))) {
    return AttachmentInterfaceLayout::Unknown;
  }

  const auto *vtable = *reinterpret_cast<std::uintptr_t *const *>(a_plugin);
  if (!IsReadableAddress(vtable, sizeof(std::uintptr_t) * 15)) {
    return AttachmentInterfaceLayout::Unknown;
  }

  const auto providerModule = ModuleForAddress(vtable[1]);
  if (providerModule == nullptr) {
    return AttachmentInterfaceLayout::Unknown;
  }

  // Original SE RaceMenu has six virtual entries (0..5). Its AddInterface is
  // slot 3; the first entry after the vtable is non-executable RTTI/data.
  if (HasExecutablePrefix(vtable, 6, providerModule) &&
      ClassifySlot(vtable, 6, providerModule) == SlotKind::NonExecutable) {
    return AttachmentInterfaceLayout::LegacyV0;
  }

  // UBE's AE-to-SE RaceMenu backport reports version 0 but has the later
  // fourteen-entry public prefix (0..13). Its AddInterface is slot 11.
  if (HasExecutablePrefix(vtable, 14, providerModule) &&
      ClassifySlot(vtable, 14, providerModule) == SlotKind::NonExecutable) {
    return AttachmentInterfaceLayout::PublicV0Backport;
  }

  return AttachmentInterfaceLayout::Unknown;
}

} // namespace

const char *
AttachmentInterfaceLayoutName(const AttachmentInterfaceLayout a_layout) {
  switch (a_layout) {
  case AttachmentInterfaceLayout::LegacyV0:
    return "legacy-v0";
  case AttachmentInterfaceLayout::PublicV0Backport:
    return "public-v0-backport";
  case AttachmentInterfaceLayout::PublicV1V2:
    return "public-v1-v2";
  default:
    return "unknown";
  }
}

AttachmentRegistrationResult RegisterAttachmentObserver(
    IPluginInterface *a_plugin, IAddonAttachmentInterface *a_observer,
    std::atomic_bool &a_registered) {
  if (a_registered.load()) {
    return {AttachmentRegistrationStatus::AlreadyRegistered};
  }
  if (a_plugin == nullptr || a_observer == nullptr) {
    return {AttachmentRegistrationStatus::Unavailable};
  }

  const auto version = a_plugin->GetVersion();
  switch (version) {
  case 0: {
    const auto layout = DetectVersionZeroLayout(a_plugin);
    if (layout == AttachmentInterfaceLayout::LegacyV0) {
      static_cast<IActorUpdateManagerV0 *>(a_plugin)->AddInterface(a_observer);
    } else if (layout == AttachmentInterfaceLayout::PublicV0Backport) {
      static_cast<IActorUpdateManagerV1 *>(a_plugin)->AddInterface(a_observer);
    } else {
      return {AttachmentRegistrationStatus::UnsupportedLayout, version,
              AttachmentInterfaceLayout::Unknown};
    }
    a_registered.store(true);
    return {AttachmentRegistrationStatus::Registered, version, layout};
  }
  case 1:
  case 2:
    static_cast<IActorUpdateManagerV1 *>(a_plugin)->AddInterface(a_observer);
    a_registered.store(true);
    return {AttachmentRegistrationStatus::Registered, version,
            AttachmentInterfaceLayout::PublicV1V2};
  default:
    return {AttachmentRegistrationStatus::UnsupportedVersion, version};
  }
}

} // namespace sfs::native::racemenu::abi
