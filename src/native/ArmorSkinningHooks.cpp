#include "native/ArmorSkinning.h"
#include "native/DaveIntegration.h"
#include "runtime/RuntimeLayouts.h"

#include <xbyak/xbyak.h>

#include <Windows.h>

#include <array>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace {
SKSE::Trampoline g_localTrampoline{"SFS native armor skinning"};
std::once_flag g_installOnce;

struct CallSiteBranch {
  std::uint8_t opcode{0};
  std::uintptr_t target{0};
  bool valid{false};
  bool expected{false};

  [[nodiscard]] bool ChainsAsCall() const { return opcode == 0xE8; }
  [[nodiscard]] bool ChainsAsJump() const { return opcode == 0xE9; }
};

[[nodiscard]] std::optional<std::uintptr_t>
TryReadDavInitWornTarget(const std::uintptr_t a_hookAddress) {
  const auto *bytes = reinterpret_cast<const std::uint8_t *>(a_hookAddress);
  if (bytes[6] != 0x48 || bytes[7] != 0xB8 || bytes[16] != 0xFF ||
      bytes[17] != 0xD0) {
    return std::nullopt;
  }

  std::uintptr_t target = 0;
  std::memcpy(&target, bytes + 8, sizeof(target));
  return target;
}

[[nodiscard]] CallSiteBranch InspectCallSite(
    const std::uintptr_t a_hookAddress, const std::uintptr_t a_expectedTarget,
    std::string_view a_label) {
  const auto opcode = *reinterpret_cast<const std::uint8_t *>(a_hookAddress);
  if (opcode != 0xE8 && opcode != 0xE9) {
    logger::warn(
        "SFS native armor skinning hook '{}' found unexpected opcode {:02X} at {:X}",
        a_label, opcode, a_hookAddress);
    return {.opcode = opcode};
  }

  const auto displacement =
      *reinterpret_cast<const std::int32_t *>(a_hookAddress + 1);
  const auto target = a_hookAddress + 5 + displacement;
  if (opcode == 0xE8 && target == a_expectedTarget) {
    return {.opcode = opcode, .target = target, .valid = true, .expected = true};
  }

  logger::warn(
      "SFS native armor skinning hook '{}' call site appears pre-patched: opcode {:02X}, target {:X}, expected {:X}",
      a_label, opcode, target, a_expectedTarget);
  return {.opcode = opcode, .target = target, .valid = true};
}

[[nodiscard]] bool IsAddressInModule(
    const std::uintptr_t a_address, const std::wstring_view a_moduleName) {
  MEMORY_BASIC_INFORMATION memoryInfo{};
  if (a_address == 0 ||
      ::VirtualQuery(reinterpret_cast<const void *>(a_address), &memoryInfo,
                     sizeof(memoryInfo)) != sizeof(memoryInfo) ||
      !memoryInfo.AllocationBase) {
    return false;
  }

  std::array<wchar_t, MAX_PATH> path{};
  const auto length = ::GetModuleFileNameW(
      static_cast<HMODULE>(memoryInfo.AllocationBase), path.data(),
      static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size()) {
    return false;
  }

  const std::wstring fullPath(path.data(), length);
  const auto separator = fullPath.find_last_of(L"\\\\/");
  const auto fileName = fullPath.substr(
      separator == std::wstring::npos ? 0 : separator + 1);
  return _wcsicmp(fileName.c_str(), a_moduleName.data()) == 0;
}

void ConfigureIedCustomSkinCompatibility(const CallSiteBranch &a_callSite,
                                         const std::string_view a_runtime) {
  const bool iedTarget =
      !a_callSite.expected &&
      IsAddressInModule(a_callSite.target, L"ImmersiveEquipmentDisplays.dll");
  sfs::native::SetIedVisitWornItemsChainTarget(
      iedTarget ? a_callSite.target : 0);
  if (iedTarget) {
    logger::info(
        "SFS IED compatibility enabled for {} custom-skin target {:X}; filtered calls use the original engine visitor path and queue IED.Evaluate afterward",
        a_runtime, a_callSite.target);
  }
}

bool InstallDavInitWornChainHook(const sfs::runtime::HookLayout &a_layout) {
  auto &branchTrampoline = SKSE::GetTrampoline();

  const auto hookAddress = REL::ID(a_layout.armorUpdateRelocationID).address() +
                           a_layout.davInitWornOffset;
  const auto davTarget = TryReadDavInitWornTarget(hookAddress);
  if (!davTarget.has_value()) {
    logger::warn(
        "Skipped SFS DAV init-worn chain hook because the expected DAV patch was not found");
    return false;
  }

  struct Code : Xbyak::CodeGenerator {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_davInitWornTarget) {
      Xbyak::Label out;
      Xbyak::Label fDavInitWornTarget;
      Xbyak::Label fShouldBlockDavInitWornArmor;

      // DAV's patch sees armor in rbp, actor in r13, and biped in r8.
      push(rcx);
      push(rdx);
      push(r8);
      push(r9);
      mov(rcx, rbp);
      mov(rdx, r13);
      sub(rsp, 0x20);
      call(ptr[rip + fShouldBlockDavInitWornArmor]);
      add(rsp, 0x20);
      pop(r9);
      pop(r8);
      pop(rdx);
      pop(rcx);
      test(al, al);
      jnz(out);

      mov(rcx, rbp);
      mov(rdx, r13);
      call(ptr[rip + fDavInitWornTarget]);

      L(out);
      jmp(ptr[rip]);
      dq(a_resumeAddress);

      L(fDavInitWornTarget);
      dq(a_davInitWornTarget);

      L(fShouldBlockDavInitWornArmor);
      dq(reinterpret_cast<std::uintptr_t>(
          sfs::native::ShouldBlockDavInitWornArmor));
    }
  };

  Code code{hookAddress + 0x17, *davTarget};
  auto *stub = g_localTrampoline.allocate(code);
  branchTrampoline.write_branch<5>(hookAddress, stub);
  logger::info(
      "Installed SFS DAV init-worn chain hook targeting {:X}", *davTarget);
  return true;
}

bool InstallDontVanillaSkinHook(const sfs::runtime::HookLayout &a_layout) {
  auto &branchTrampoline = SKSE::GetTrampoline();

  const auto hookAddress = REL::ID(a_layout.armorUpdateRelocationID).address() +
                           a_layout.vanillaArmorOffset;
  static REL::Relocation<std::uintptr_t> applyArmorAddon{
      RELOCATION_ID(17392, 17792)};
  const auto callSite =
      InspectCallSite(hookAddress, applyArmorAddon.address(), "vanilla block");
  if (!callSite.valid) {
    if (callSite.opcode == 0x90) {
      logger::warn(
          "Skipped SFS native armor skinning vanilla block hook because the call site is NOP-patched; SFS will rely on worn-mask filtering for real-equipment hiding");
      InstallDavInitWornChainHook(a_layout);
    } else {
      logger::warn("Skipped SFS native armor skinning vanilla block hook");
    }
    return false;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning vanilla block hook will chain the existing patched target {:X}",
        callSite.target);
  }
  struct Code : Xbyak::CodeGenerator {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_nextTarget,
         bool a_chainAsJump) {
      Xbyak::Label out;
      Xbyak::Label fNextTarget;
      Xbyak::Label fShouldBlockVanillaArmor;

      // armor is in rcx, target actor/reference is in r13.
      push(rcx);
      push(rdx);
      push(r9);
      push(r8);
      mov(rdx, r13);
      sub(rsp, 0x40);
      call(ptr[rip + fShouldBlockVanillaArmor]);
      add(rsp, 0x40);
      pop(r8);
      pop(r9);
      pop(rdx);
      pop(rcx);
      test(al, al);
      jnz(out);
      if (a_chainAsJump) {
        jmp(ptr[rip + fNextTarget]);
      } else {
        call(ptr[rip + fNextTarget]);
      }

      L(out);
      jmp(ptr[rip]);
      dq(a_resumeAddress);

      L(fNextTarget);
      dq(a_nextTarget);

      L(fShouldBlockVanillaArmor);
      dq(reinterpret_cast<std::uintptr_t>(
          sfs::native::ShouldBlockVanillaArmor));
    }
  };

  Code code{hookAddress + 0x5, callSite.target, callSite.ChainsAsJump()};
  auto *stub = g_localTrampoline.allocate(code);
  branchTrampoline.write_branch<5>(hookAddress, stub);
  logger::info(
      "Installed SFS native armor skinning vanilla block hook{}",
      callSite.expected ? "" : " with chained pre-patched target");
  return true;
}

void InstallShimWornFlagsHookSE(const sfs::runtime::HookLayout &a_layout) {
  auto &branchTrampoline = SKSE::GetTrampoline();

  const auto hookAddress = REL::ID(a_layout.wornMaskRelocationID).address() +
                           a_layout.wornMaskCallOffset;
  static REL::Relocation<std::uintptr_t> getWornMask{RELOCATION_ID(15806,
                                                                   16044)};
  const auto callSite =
      InspectCallSite(hookAddress, getWornMask.address(), "SE worn mask");
  if (!callSite.valid || !callSite.ChainsAsCall()) {
    logger::warn("Skipped SFS native armor skinning worn-mask hook for SE");
    return;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning worn-mask hook for SE will chain the existing patched target {:X}",
        callSite.target);
  }
  struct Code : Xbyak::CodeGenerator {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_getWornMask) {
      Xbyak::Label suppressVanilla;
      Xbyak::Label out;
      Xbyak::Label fShouldOverrideSkinning;
      Xbyak::Label fGetWornMask;
      Xbyak::Label fGetDisplayWornMask;

      // target actor/reference is in rsi on SE.
      push(rcx);
      mov(rcx, rsi);
      sub(rsp, 0x8);
      sub(rsp, 0x20);
      call(ptr[rip + fShouldOverrideSkinning]);
      add(rsp, 0x20);
      add(rsp, 0x8);
      pop(rcx);
      test(al, al);
      jnz(suppressVanilla);
      call(ptr[rip + fGetWornMask]);
      jmp(out);

      L(suppressVanilla);
      sub(rsp, 0x30);
      mov(ptr[rsp + 0x20], rcx);
      mov(ptr[rsp + 0x28], rdx);
      call(ptr[rip + fGetWornMask]);
      mov(r8d, eax);
      mov(rcx, ptr[rsp + 0x20]);
      mov(rdx, rsi);
      call(ptr[rip + fGetDisplayWornMask]);
      mov(rdx, ptr[rsp + 0x28]);
      add(rsp, 0x30);

      L(out);
      jmp(ptr[rip]);
      dq(a_resumeAddress);

      L(fShouldOverrideSkinning);
      dq(reinterpret_cast<std::uintptr_t>(sfs::native::ShouldOverrideSkinning));

      L(fGetWornMask);
      dq(a_getWornMask);

      L(fGetDisplayWornMask);
      dq(reinterpret_cast<std::uintptr_t>(sfs::native::GetDisplayWornMask));
    }
  };

  Code code{hookAddress + 0x5, callSite.target};
  auto *stub = g_localTrampoline.allocate(code);
  branchTrampoline.write_branch<5>(hookAddress, stub);
  logger::info("Installed SFS native armor skinning worn-mask hook for SE");
}

void InstallShimWornFlagsHookAE(const sfs::runtime::HookLayout &a_layout) {
  auto &branchTrampoline = SKSE::GetTrampoline();

  const auto hookAddress = REL::ID(a_layout.wornMaskRelocationID).address() +
                           a_layout.wornMaskCallOffset;
  static REL::Relocation<std::uintptr_t> getWornMask{RELOCATION_ID(15806,
                                                                   16044)};
  const auto callSite =
      InspectCallSite(hookAddress, getWornMask.address(), "AE worn mask");
  if (!callSite.valid || !callSite.ChainsAsCall()) {
    logger::warn("Skipped SFS native armor skinning worn-mask hook for AE");
    return;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning worn-mask hook for AE will chain the existing patched target {:X}",
        callSite.target);
  }

  struct Code : Xbyak::CodeGenerator {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_getWornMask) {
      Xbyak::Label suppressVanilla;
      Xbyak::Label out;
      Xbyak::Label fShouldOverrideSkinning;
      Xbyak::Label fGetWornMask;
      Xbyak::Label fGetDisplayWornMask;

      // target actor/reference is in rbx on AE.
      push(rcx);
      mov(rcx, rbx);
      sub(rsp, 0x8);
      sub(rsp, 0x20);
      call(ptr[rip + fShouldOverrideSkinning]);
      add(rsp, 0x20);
      add(rsp, 0x8);
      pop(rcx);
      test(al, al);
      jnz(suppressVanilla);
      call(ptr[rip + fGetWornMask]);
      jmp(out);

      L(suppressVanilla);
      sub(rsp, 0x30);
      mov(ptr[rsp + 0x20], rcx);
      mov(ptr[rsp + 0x28], rdx);
      call(ptr[rip + fGetWornMask]);
      mov(r8d, eax);
      mov(rcx, ptr[rsp + 0x20]);
      mov(rdx, rbx);
      call(ptr[rip + fGetDisplayWornMask]);
      mov(rdx, ptr[rsp + 0x28]);
      add(rsp, 0x30);

      L(out);
      jmp(ptr[rip]);
      dq(a_resumeAddress);

      L(fShouldOverrideSkinning);
      dq(reinterpret_cast<std::uintptr_t>(sfs::native::ShouldOverrideSkinning));

      L(fGetWornMask);
      dq(a_getWornMask);

      L(fGetDisplayWornMask);
      dq(reinterpret_cast<std::uintptr_t>(sfs::native::GetDisplayWornMask));
    }
  };

  Code code{hookAddress + 0x5, callSite.target};
  auto *stub = g_localTrampoline.allocate(code);
  branchTrampoline.write_branch<5>(hookAddress, stub);
  logger::info("Installed SFS native armor skinning worn-mask hook for AE");
}

void InstallCustomSkinHookSE(const sfs::runtime::HookLayout &a_layout) {
  auto &branchTrampoline = SKSE::GetTrampoline();

  const auto hookAddress = REL::ID(a_layout.customSkinRelocationID).address() +
                           a_layout.customSkinCallOffset;
  static REL::Relocation<std::uintptr_t> visitWornItems{RELOCATION_ID(15856,
                                                                     16096)};
  const auto callSite =
      InspectCallSite(hookAddress, visitWornItems.address(), "SE custom skin");
  if (!callSite.valid || !callSite.ChainsAsCall()) {
    logger::warn("Skipped SFS native armor skinning custom skin hook for SE");
    return;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning custom skin hook for SE will chain the existing patched target {:X}",
        callSite.target);
  }
  ConfigureIedCustomSkinCompatibility(callSite, a_layout.name);

  struct Code : Xbyak::CodeGenerator {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_visitWornItems) {
      Xbyak::Label skipAdditional;
      Xbyak::Label fApplyAdditionalDisplayArmors;
      Xbyak::Label fVisitWornItems;
      Xbyak::Label fVisitWornItemsWithHiddenRealEquipmentFilter;
      Xbyak::Label fShouldOverrideSkinning;

      mov(r8, rbx);
      mov(r9, ptr[rip + fVisitWornItems]);
      sub(rsp, 0x20);
      call(ptr[rip + fVisitWornItemsWithHiddenRealEquipmentFilter]);
      add(rsp, 0x20);

      push(rcx);
      push(rdx);
      mov(rcx, rbx);
      sub(rsp, 0x20);
      call(ptr[rip + fShouldOverrideSkinning]);
      add(rsp, 0x20);
      pop(rdx);
      pop(rcx);
      test(al, al);
      jz(skipAdditional);

      push(rdx);
      push(rcx);
      mov(rcx, rbx);
      mov(rdx, rdi);
      sub(rsp, 0x20);
      call(ptr[rip + fApplyAdditionalDisplayArmors]);
      add(rsp, 0x20);
      pop(rcx);
      pop(rdx);

      L(skipAdditional);
      jmp(ptr[rip]);
      dq(a_resumeAddress);

      L(fApplyAdditionalDisplayArmors);
      dq(reinterpret_cast<std::uintptr_t>(
          sfs::native::ApplyAdditionalDisplayArmors));

      L(fVisitWornItems);
      dq(a_visitWornItems);

      L(fVisitWornItemsWithHiddenRealEquipmentFilter);
      dq(reinterpret_cast<std::uintptr_t>(
          sfs::native::VisitWornItemsWithHiddenRealEquipmentFilter));

      L(fShouldOverrideSkinning);
      dq(reinterpret_cast<std::uintptr_t>(sfs::native::ShouldOverrideSkinning));
    }
  };

  Code code{hookAddress + 0x5, callSite.target};
  auto *stub = g_localTrampoline.allocate(code);
  branchTrampoline.write_branch<5>(hookAddress, stub);
  logger::info("Installed SFS native armor skinning custom skin hook for SE");
}

void InstallCustomSkinHookAE(const sfs::runtime::HookLayout &a_layout) {
  auto &branchTrampoline = SKSE::GetTrampoline();

  const auto hookAddress = REL::ID(a_layout.customSkinRelocationID).address() +
                           a_layout.customSkinCallOffset;
  static REL::Relocation<std::uintptr_t> visitWornItems{RELOCATION_ID(15856,
                                                                     16096)};
  const auto callSite =
      InspectCallSite(hookAddress, visitWornItems.address(), "AE custom skin");
  if (!callSite.valid || !callSite.ChainsAsCall()) {
    logger::warn("Skipped SFS native armor skinning custom skin hook for AE");
    return;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning custom skin hook for AE will chain the existing patched target {:X}",
        callSite.target);
  }
  ConfigureIedCustomSkinCompatibility(callSite, a_layout.name);

  struct Code : Xbyak::CodeGenerator {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_visitWornItems) {
      Xbyak::Label skipAdditional;
      Xbyak::Label fApplyAdditionalDisplayArmors;
      Xbyak::Label fVisitWornItems;
      Xbyak::Label fVisitWornItemsWithHiddenRealEquipmentFilter;
      Xbyak::Label fShouldOverrideSkinning;

      mov(r8, rbx);
      mov(r9, ptr[rip + fVisitWornItems]);
      sub(rsp, 0x20);
      call(ptr[rip + fVisitWornItemsWithHiddenRealEquipmentFilter]);
      add(rsp, 0x20);

      push(rcx);
      push(rdx);
      mov(rcx, rbx);
      sub(rsp, 0x20);
      call(ptr[rip + fShouldOverrideSkinning]);
      add(rsp, 0x20);
      pop(rdx);
      pop(rcx);
      test(al, al);
      jz(skipAdditional);

      push(rdx);
      push(rcx);
      mov(rcx, rbx);
      mov(rdx, r15);
      sub(rsp, 0x20);
      call(ptr[rip + fApplyAdditionalDisplayArmors]);
      add(rsp, 0x20);
      pop(rcx);
      pop(rdx);

      L(skipAdditional);
      jmp(ptr[rip]);
      dq(a_resumeAddress);

      L(fApplyAdditionalDisplayArmors);
      dq(reinterpret_cast<std::uintptr_t>(
          sfs::native::ApplyAdditionalDisplayArmors));

      L(fVisitWornItems);
      dq(a_visitWornItems);

      L(fVisitWornItemsWithHiddenRealEquipmentFilter);
      dq(reinterpret_cast<std::uintptr_t>(
          sfs::native::VisitWornItemsWithHiddenRealEquipmentFilter));

      L(fShouldOverrideSkinning);
      dq(reinterpret_cast<std::uintptr_t>(sfs::native::ShouldOverrideSkinning));
    }
  };

  Code code{hookAddress + 0x5, callSite.target};
  auto *stub = g_localTrampoline.allocate(code);
  branchTrampoline.write_branch<5>(hookAddress, stub);
  logger::info("Installed SFS native armor skinning custom skin hook for AE");
}
} // namespace

namespace sfs::native {
void InstallArmorSkinningHooks() {
  std::call_once(g_installOnce, [] {
    if (REL::Module::IsVR()) {
      logger::warn("SFS native armor skinning hooks are disabled on VR");
      return;
    }

    const auto runtimeVersion = REL::Module::get().version();
    const auto layout = sfs::runtime::ResolveHookLayout(runtimeVersion);
    if (!layout.has_value()) {
      logger::critical(
          "SFS native armor skinning hooks are disabled on unsupported Skyrim runtime {}",
          runtimeVersion.string("."));
      return;
    }
    SetIedVisitWornItemsChainTarget(0);
    logger::info("Selected SFS armor hook layout for {} ({})", layout->name,
                 runtimeVersion.string("."));

    if (g_localTrampoline.empty()) {
      g_localTrampoline.create(64 * 1024);
    }

    const bool dynamicArmorVariantsLoaded =
        sfs::native::dave::IsDynamicArmorVariantsLoaded();
    const bool daveNativeApiAvailable =
        dynamicArmorVariantsLoaded && sfs::native::dave::HasNativeApi();
    if (daveNativeApiAvailable) {
      logger::info(
          "DynamicArmorVariants.dll is loaded. SFS will skip its conflicting real-equipment skin block hook and use DAVE native API for real-equipment hiding when available.");
    } else if (dynamicArmorVariantsLoaded) {
      sfs::native::dave::LockToNativeFallback();
      logger::warn(
          "DynamicArmorVariants.dll is loaded, but its native API is unavailable. SFS will install the native real-equipment skin block hook as a fallback.");
    }

    if (daveNativeApiAvailable) {
      logger::info(
          "Skipped SFS native armor skinning vanilla block hook for DAV/DAVE compatibility");
    } else {
      InstallDontVanillaSkinHook(*layout);
    }
    if (layout->isAE) {
      InstallShimWornFlagsHookAE(*layout);
      InstallCustomSkinHookAE(*layout);
    } else {
      InstallShimWornFlagsHookSE(*layout);
      InstallCustomSkinHookSE(*layout);
    }
  });
}
} // namespace sfs::native
