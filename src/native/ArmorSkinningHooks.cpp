#include "native/ArmorSkinning.h"
#include "native/BranchChainRules.h"
#include "native/DaveIntegration.h"
#include "runtime/RuntimeLayouts.h"

#include <xbyak/xbyak.h>

#include <Windows.h>

#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {
SKSE::Trampoline g_localTrampoline{"SFS native armor skinning"};
std::once_flag g_installOnce;
std::once_flag g_finalizeBackendOnce;
bool g_realEquipmentBackendConfigured{false}; // startup main-thread only
bool g_inlineDetourIedLoaded{false}; // resolved once at hook installation

// E9 points to an inline body with the ENGINE stack/register context, not a
// C++ function. Unmanaged actors tail-jump to it byte-for-byte intact. Active
// SFS skinning uses the verified engine CALL and SFS's existing display policy.
// Do not CALL an inline body, rewrite another mod's code, or guess its resume.
class SkinningHookCode : public Xbyak::CodeGenerator {
public:
  void GateInlineJump(std::uintptr_t a_target, const Xbyak::Reg64 &a_actor) {
    if (!a_target) { return; } // ordinary E8 route stays unchanged
    Xbyak::Label active, body, predicate, previous;
    pushfq();
    push(rax); push(rcx); push(rdx); push(r8); push(r9); push(r10); push(r11);
    sub(rsp, 0x80); // shadow space + all six volatile XMM registers
    for (int i = 0; i < 6; ++i) {
      movdqu(ptr[rsp + 0x20 + i * 16], Xbyak::Xmm(i));
    }
    mov(rcx, a_actor);
    call(ptr[rip + predicate]);
    test(al, al);
    jnz(active, T_NEAR);
    const auto restore = [&] {
      for (int i = 0; i < 6; ++i) {
        movdqu(Xbyak::Xmm(i), ptr[rsp + 0x20 + i * 16]);
      }
      add(rsp, 0x80);
      pop(r11); pop(r10); pop(r9); pop(r8); pop(rdx); pop(rcx); pop(rax);
      popfq();
    };
    restore();
    jmp(ptr[rip + previous]);
    L(active);
    restore();
    jmp(body, T_NEAR);
    L(predicate);
    dq(reinterpret_cast<std::uintptr_t>(&sfs::native::ShouldOverrideSkinning));
    L(previous); dq(a_target);
    L(body);
  }
};

void VisitWornItemsForInlineDetour(
    RE::InventoryChanges *a_inventory,
    RE::InventoryChanges::IItemChangeVisitor *a_visitor,
    RE::TESObjectREFR *a_target, const std::uintptr_t a_engineVisit) {
  sfs::native::VisitWornItemsWithHiddenRealEquipmentFilter(
      a_inventory, a_visitor, a_target, a_engineVisit);
  // Reuse the coalesced/load-canceled public IED queue, never reenter its
  // concrete-visitor hook with an SFS wrapper. No IED installed => no IED work.
  if (g_inlineDetourIedLoaded && a_target) {
    if (auto *actor = a_target->As<RE::Actor>()) {
      sfs::native::QueueIedEvaluation(actor->GetFormID());
    }
  }
}

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

[[nodiscard]] bool IsReadableCommittedRange(const std::uintptr_t a_address,
                                            const std::size_t a_size,
                                            const bool a_requireExecutable) {
  if (a_address == 0 || a_size == 0) {
    return false;
  }

  MEMORY_BASIC_INFORMATION memoryInfo{};
  if (::VirtualQuery(reinterpret_cast<const void *>(a_address), &memoryInfo,
                     sizeof(memoryInfo)) != sizeof(memoryInfo) ||
      memoryInfo.State != MEM_COMMIT ||
      (memoryInfo.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
    return false;
  }

  const auto protection = memoryInfo.Protect & 0xFF;
  const bool readable = protection == PAGE_READONLY ||
                        protection == PAGE_READWRITE ||
                        protection == PAGE_WRITECOPY ||
                        protection == PAGE_EXECUTE ||
                        protection == PAGE_EXECUTE_READ ||
                        protection == PAGE_EXECUTE_READWRITE ||
                        protection == PAGE_EXECUTE_WRITECOPY;
  const bool executable = protection == PAGE_EXECUTE ||
                          protection == PAGE_EXECUTE_READ ||
                          protection == PAGE_EXECUTE_READWRITE ||
                          protection == PAGE_EXECUTE_WRITECOPY;
  if (!readable || (a_requireExecutable && !executable)) {
    return false;
  }

  const auto regionStart =
      reinterpret_cast<std::uintptr_t>(memoryInfo.BaseAddress);
  if (a_address < regionStart) {
    return false;
  }
  const auto offset = a_address - regionStart;
  return offset <= memoryInfo.RegionSize &&
         a_size <= memoryInfo.RegionSize - offset;
}

template <class T>
[[nodiscard]] std::optional<T> TryReadMemory(const std::uintptr_t a_address,
                                             const bool a_requireExecutable) {
  if (!IsReadableCommittedRange(a_address, sizeof(T), a_requireExecutable)) {
    return std::nullopt;
  }
  T value{};
  std::memcpy(std::addressof(value), reinterpret_cast<const void *>(a_address),
              sizeof(value));
  return value;
}

struct ModuleChainMatch {
  std::uintptr_t finalTarget{0};
  std::size_t trampolineDepth{0};
};

[[nodiscard]] std::optional<ModuleChainMatch> ResolveBranchChainOwner(
    const std::uintptr_t a_start, const std::wstring_view a_moduleName,
    const std::uintptr_t a_expectedTarget = 0) {
  constexpr std::size_t kMaximumTrampolineDepth = 8;
  // ENDBR64 + mov-r11/jmp-r11 needs 17 bytes. Read only the accessible
  // prefix so even a short veneer at the end of a committed page is valid.
  constexpr std::size_t kDecodeWindow = 20;

  std::uintptr_t current = a_start;
  std::unordered_set<std::uintptr_t> visited;
  for (std::size_t depth = 0; depth <= kMaximumTrampolineDepth; ++depth) {
    if ((a_expectedTarget != 0 && current == a_expectedTarget) ||
        (!a_moduleName.empty() && IsAddressInModule(current, a_moduleName))) {
      return ModuleChainMatch{current, depth};
    }
    if (depth == kMaximumTrampolineDepth || !visited.insert(current).second) {
      break;
    }

    std::array<std::uint8_t, kDecodeWindow> bytes{};
    std::size_t readableLength = 0;
    for (; readableLength < bytes.size(); ++readableLength) {
      if (current > (std::numeric_limits<std::uintptr_t>::max)() -
                        readableLength) {
        break;
      }
      const auto byte = TryReadMemory<std::uint8_t>(
          current + readableLength, true);
      if (!byte.has_value()) {
        break;
      }
      bytes[readableLength] = *byte;
    }
    const auto transfer = sfs::native::branch_chain::rules::DecodeTransfer(
        current, std::span<const std::uint8_t>(bytes.data(), readableLength));
    if (!transfer.has_value()) {
      break;
    }

    if (transfer->kind == sfs::native::branch_chain::rules::TransferKind::
                              IndirectTargetSlot) {
      const auto indirect =
          TryReadMemory<std::uintptr_t>(transfer->address, false);
      if (!indirect.has_value()) {
        break;
      }
      current = *indirect;
    } else {
      current = transfer->address;
    }
    if (current == 0) {
      break;
    }
  }
  return std::nullopt;
}

[[nodiscard]] bool
ConfigureIedCustomSkinCompatibility(const CallSiteBranch &a_callSite,
                                    const std::string_view a_runtime) {
  sfs::native::SetPassthroughVisitWornItemsChainTarget(0);
  if (a_callSite.expected) {
    sfs::native::SetIedVisitWornItemsChainTarget(0);
    return true;
  }

  // A transparent veneer ending at the exact engine visitor has the same
  // generic visitor ABI. Preserve full filtering without guessing by DLL name.
  static REL::Relocation<std::uintptr_t> engineVisitWornItems{
      RELOCATION_ID(15856, 16096)};
  if (ResolveBranchChainOwner(a_callSite.target, {},
                              engineVisitWornItems.address()).has_value()) {
    sfs::native::SetIedVisitWornItemsChainTarget(0);
    return true;
  }

  const auto iedOwner = ResolveBranchChainOwner(
      a_callSite.target, L"ImmersiveEquipmentDisplays.dll");
  const bool iedTarget = iedOwner.has_value();
  sfs::native::SetIedVisitWornItemsChainTarget(
      iedTarget ? a_callSite.target : 0);
  if (iedTarget) {
    logger::info(
        "SFS IED compatibility enabled for {} custom-skin chain {:X} -> {:X} through {} trampoline(s); filtered calls use the original engine visitor path and queue IED.Evaluate afterward",
        a_runtime, a_callSite.target, iedOwner->finalTarget,
        iedOwner->trampolineDepth);
    return true;
  }

  // An opaque CALL target still accepts the game's original concrete visitor.
  // Preserve that object and the existing chain; only the SFS wrapper visitor
  // is unsafe here. Skipping this whole hook also skips the sole registered-
  // armor attachment pass, making every wig/clothing appearance disappear.
  // Filter the engine concrete visitor's callbacks in an actor/visitor-local
  // scope instead of substituting its object. Keep the foreign chain intact.
  sfs::native::SetPassthroughVisitWornItemsChainTarget(a_callSite.target);
  const bool callbackFilter = sfs::native::InstallOriginalWornVisitorFilter();
  if (!callbackFilter) {
    logger::error(
        "SFS original-visitor callback filter could not be installed; retaining registered attachments and existing vanilla/DAV/DAVE hiding paths");
  }
  logger::warn(
      "SFS {} custom-skin target {:X} has unverified visitor ownership; preserving the original visitor and registered-appearance attachment; scoped original-visitor actual-equipment filter={}",
      a_runtime, a_callSite.target, callbackFilter);
  return true;
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
  struct Code : SkinningHookCode {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_nextTarget,
         std::uintptr_t a_inlineJump) {
      Xbyak::Label out;
      Xbyak::Label fNextTarget;
      Xbyak::Label fShouldBlockVanillaArmor;

      GateInlineJump(a_inlineJump, r13);

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
      call(ptr[rip + fNextTarget]);

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

  Code code{hookAddress + 0x5,
            callSite.ChainsAsJump() ? applyArmorAddon.address() : callSite.target,
            callSite.ChainsAsJump() ? callSite.target : 0};
  auto *stub = g_localTrampoline.allocate(code);
  branchTrampoline.write_branch<5>(hookAddress, stub);
  logger::info(
      "Installed SFS native armor skinning vanilla block hook{}",
      callSite.expected ? "" : " with chained pre-patched target");
  return true;
}

bool ConfigureRealEquipmentSkinningBackend(
    const sfs::runtime::HookLayout &a_layout, const bool a_finalAttempt) {
  if (g_realEquipmentBackendConfigured) { return true; }
  const bool davLoaded = sfs::native::dave::IsDynamicArmorVariantsLoaded();
  const bool daveApiAvailable = davLoaded &&
      sfs::native::dave::HasNativeApi(a_finalAttempt);
  if (davLoaded && !daveApiAvailable && !a_finalAttempt) {
    logger::info("SFS real-equipment backend selection deferred until after DataLoaded; registered attachments and worn-mask hooks remain active");
    return false;
  }
  if (daveApiAvailable) {
    logger::info("SFS real-equipment backend: DAVE API; conflicting native skin-block hook remains uninstalled");
  } else {
    if (davLoaded) {
      sfs::native::dave::LockToNativeFallback();
      logger::info("SFS real-equipment backend: DAV/native fallback after final API rendezvous");
    }
    InstallDontVanillaSkinHook(a_layout);
  }
  // Never switch ownership after native code hooks have been installed.
  g_realEquipmentBackendConfigured = true;
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
  if (!callSite.valid) {
    logger::warn("Skipped SFS native armor skinning worn-mask hook for SE");
    return;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning worn-mask hook for SE will chain the existing patched target {:X}",
        callSite.target);
  }
  struct Code : SkinningHookCode {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_getWornMask,
         std::uintptr_t a_inlineJump) {
      Xbyak::Label suppressVanilla;
      Xbyak::Label out;
      Xbyak::Label fShouldOverrideSkinning;
      Xbyak::Label fGetWornMask;
      Xbyak::Label fGetDisplayWornMask;

      GateInlineJump(a_inlineJump, rsi);

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

  Code code{hookAddress + 0x5,
            callSite.ChainsAsJump() ? getWornMask.address() : callSite.target,
            callSite.ChainsAsJump() ? callSite.target : 0};
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
  if (!callSite.valid) {
    logger::warn("Skipped SFS native armor skinning worn-mask hook for AE");
    return;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning worn-mask hook for AE will chain the existing patched target {:X}",
        callSite.target);
  }

  struct Code : SkinningHookCode {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_getWornMask,
         std::uintptr_t a_inlineJump) {
      Xbyak::Label suppressVanilla;
      Xbyak::Label out;
      Xbyak::Label fShouldOverrideSkinning;
      Xbyak::Label fGetWornMask;
      Xbyak::Label fGetDisplayWornMask;

      GateInlineJump(a_inlineJump, rbx);

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

  Code code{hookAddress + 0x5,
            callSite.ChainsAsJump() ? getWornMask.address() : callSite.target,
            callSite.ChainsAsJump() ? callSite.target : 0};
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
  if (!callSite.valid) {
    logger::warn("Skipped SFS native armor skinning custom skin hook for SE");
    return;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning custom skin hook for SE will chain the existing patched target {:X}",
        callSite.target);
  }
  auto visitorCall = callSite;
  if (callSite.ChainsAsJump()) {
    visitorCall = {.opcode = 0xE8, .target = visitWornItems.address(),
                   .valid = true, .expected = true};
    g_inlineDetourIedLoaded =
        ::GetModuleHandleW(L"ImmersiveEquipmentDisplays.dll") != nullptr;
    logger::warn("SFS {} E9 inline skin detour: unmanaged actors retain the previous jump; active SFS actors use engine filtering and registered attachments",
                 a_layout.name);
  }
  if (!ConfigureIedCustomSkinCompatibility(visitorCall, a_layout.name)) {
    return;
  }

  struct Code : SkinningHookCode {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_visitWornItems,
         std::uintptr_t a_inlineJump) {
      Xbyak::Label skipAdditional;
      Xbyak::Label fApplyAdditionalDisplayArmors;
      Xbyak::Label fVisitWornItems;
      Xbyak::Label fVisitWornItemsWithHiddenRealEquipmentFilter;
      Xbyak::Label fShouldOverrideSkinning;

      GateInlineJump(a_inlineJump, rbx);

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
      dq(reinterpret_cast<std::uintptr_t>(a_inlineJump
          ? &VisitWornItemsForInlineDetour
          : &sfs::native::VisitWornItemsWithHiddenRealEquipmentFilter));

      L(fShouldOverrideSkinning);
      dq(reinterpret_cast<std::uintptr_t>(sfs::native::ShouldOverrideSkinning));
    }
  };

  Code code{hookAddress + 0x5,
            visitorCall.target,
            callSite.ChainsAsJump() ? callSite.target : 0};
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
  if (!callSite.valid) {
    logger::warn("Skipped SFS native armor skinning custom skin hook for AE");
    return;
  }
  if (!callSite.expected) {
    logger::warn(
        "SFS native armor skinning custom skin hook for AE will chain the existing patched target {:X}",
        callSite.target);
  }
  auto visitorCall = callSite;
  if (callSite.ChainsAsJump()) {
    visitorCall = {.opcode = 0xE8, .target = visitWornItems.address(),
                   .valid = true, .expected = true};
    g_inlineDetourIedLoaded =
        ::GetModuleHandleW(L"ImmersiveEquipmentDisplays.dll") != nullptr;
    logger::warn("SFS {} E9 inline skin detour: unmanaged actors retain the previous jump; active SFS actors use engine filtering and registered attachments",
                 a_layout.name);
  }
  if (!ConfigureIedCustomSkinCompatibility(visitorCall, a_layout.name)) {
    return;
  }

  struct Code : SkinningHookCode {
    Code(std::uintptr_t a_resumeAddress, std::uintptr_t a_visitWornItems,
         std::uintptr_t a_inlineJump) {
      Xbyak::Label skipAdditional;
      Xbyak::Label fApplyAdditionalDisplayArmors;
      Xbyak::Label fVisitWornItems;
      Xbyak::Label fVisitWornItemsWithHiddenRealEquipmentFilter;
      Xbyak::Label fShouldOverrideSkinning;

      GateInlineJump(a_inlineJump, rbx);

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
      dq(reinterpret_cast<std::uintptr_t>(a_inlineJump
          ? &VisitWornItemsForInlineDetour
          : &sfs::native::VisitWornItemsWithHiddenRealEquipmentFilter));

      L(fShouldOverrideSkinning);
      dq(reinterpret_cast<std::uintptr_t>(sfs::native::ShouldOverrideSkinning));
    }
  };

  Code code{hookAddress + 0x5,
            visitorCall.target,
            callSite.ChainsAsJump() ? callSite.target : 0};
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

    ConfigureRealEquipmentSkinningBackend(*layout, false);
    if (layout->isAE) {
      InstallShimWornFlagsHookAE(*layout);
      InstallCustomSkinHookAE(*layout);
    } else {
      InstallShimWornFlagsHookSE(*layout);
      InstallCustomSkinHookSE(*layout);
    }
  });
}

void FinalizeRealEquipmentSkinningBackend() {
  std::call_once(g_finalizeBackendOnce, [] {
    const auto layout = sfs::runtime::ResolveHookLayout(REL::Module::get().version());
    if (layout) { ConfigureRealEquipmentSkinningBackend(*layout, true); }
  });
}
} // namespace sfs::native
