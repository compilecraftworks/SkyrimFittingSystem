#include "native/IedConditionIntegration.h"
#include "native/IedBipedConditionAdapter.h"
#include "native/IedConditionBinary.h"
#include "api/RenderedOutfitProvider.h"
#ifndef SFS_IED_TEST
#include "native/ArmorSkinning.h"
#endif

#include <Windows.h>
#include <atomic>
#include <limits>

namespace sfs::native::ied {
namespace {
using Equipped = bool (*)(void*, const void*);
using Node = bool (*)(void*, const void*, void*);
Equipped originalEquipped{};
Node originalNode{};
std::atomic_bool installed{false};

std::shared_ptr<const api::rendered::Value> GetValue(void* params) {
  auto* actor = detail::Read<RE::Actor*>(params, 0);
  return actor ? api::rendered::AcquirePublished(actor->GetFormID()) : nullptr;
}
auto* ResolveArmor(std::uint32_t id) {
  return RE::TESForm::LookupByID<RE::TESObjectARMO>(id);
}
bool EquippedHook(void* params, const void* match) {
  try {
    if (const auto value = GetValue(params)) {
      if (const auto result = detail::MatchBiped(value->items, match, ResolveArmor,
          [](void* shadow, const void* condition) { return originalEquipped(shadow, condition); })) {
        return *result;
      }
    }
  } catch (const std::exception&) { /* Preserve IED on an allocation failure. */ }
  return originalEquipped(params, match);
}
bool NodeHook(void* params, const void* match, void* nodeParams) {
  try {
    if (const auto value = GetValue(params)) {
      if (const auto result = detail::MatchBiped(value->items, match, ResolveArmor,
          [nodeParams](void* shadow, const void* condition) {
            // Crucially keep the ORIGINAL node params as the third argument:
            // IED performs its own lazy item-data setup and matched-slot update.
            return originalNode(shadow, condition, nodeParams);
          })) { return *result; }
    }
  } catch (const std::exception&) { /* Preserve IED on an allocation failure. */ }
  return originalNode(params, match, nodeParams);
}

// Tiny leaf relays, no stolen prologues/unwind state and no external hook SDK.
// Locate free address space near IED so its existing rel32 calls remain valid.
void* AllocateRelays(std::uintptr_t base, std::size_t imageSize) {
  SYSTEM_INFO info{};
  ::GetSystemInfo(&info);
  const auto granularity = static_cast<std::uintptr_t>(info.dwAllocationGranularity);
  const auto minimum = reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
  const auto maximum = reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);
  const auto low = (std::max)(minimum, base > 0x70000000 ? base - 0x70000000 : minimum);
  const auto high = (std::min)(maximum, base + imageSize + 0x70000000);
  for (auto cursor = low; cursor < high;) {
    MEMORY_BASIC_INFORMATION region{};
    if (!::VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region))) { break; }
    const auto begin = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    const auto end = begin + region.RegionSize;
    if (end <= cursor) { break; }
    if (region.State == MEM_FREE) {
      const auto aligned = ((std::max)(cursor, begin) + granularity - 1) & ~(granularity - 1);
      if (aligned < high && aligned + granularity <= end) {
        if (auto* memory = ::VirtualAlloc(reinterpret_cast<void*>(aligned), granularity,
                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)) { return memory; }
      }
    }
    cursor = end;
  }
  return nullptr;
}
void WriteRelay(std::byte* memory, std::uintptr_t destination) {
  const std::array<std::byte, 6> jump{std::byte{0xFF}, std::byte{0x25}};
  std::memcpy(memory, jump.data(), jump.size());
  std::memcpy(memory + 6, &destination, sizeof(destination));
}
} // namespace

void InstallConditionBridge() {
  if (installed.load()) { return; }
  const auto module = ::GetModuleHandleW(L"ImmersiveEquipmentDisplays.dll");
  if (!module) { return; }
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096) { return; }
  const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
      pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) { return; }
  const detail::BinaryLayout* layout = nullptr;
  for (const auto& candidate : detail::kLayouts) {
    if (candidate.timestamp == pe->FileHeader.TimeDateStamp &&
        candidate.imageSize == pe->OptionalHeader.SizeOfImage) { layout = &candidate; break; }
  }
  if (!layout || !detail::Validate({reinterpret_cast<const std::byte*>(base),
                                    pe->OptionalHeader.SizeOfImage}, *layout)) {
    logger::warn("SFS IED condition bridge: unverified/modified binary; original IED conditions and SFS rendering retained (timestamp {:08X})",
                 pe->FileHeader.TimeDateStamp);
    return;
  }
  auto* relay = static_cast<std::byte*>(AllocateRelays(base, layout->imageSize));
  if (!relay) { logger::warn("SFS IED condition bridge: no relay memory; original IED retained"); return; }
  WriteRelay(relay, reinterpret_cast<std::uintptr_t>(&EquippedHook));
  WriteRelay(relay + 16, reinterpret_cast<std::uintptr_t>(&NodeHook));
  DWORD relayProtection{};
  if (!::VirtualProtect(relay, 32, PAGE_EXECUTE_READ, &relayProtection)) {
    ::VirtualFree(relay, 0, MEM_RELEASE); return;
  }
  ::FlushInstructionCache(::GetCurrentProcess(), relay, 32);
  std::array<std::array<std::byte, 5>, 5> patches{};
  std::array<DWORD, 5> protections{};
  std::size_t writable = 0;
  for (std::size_t i = 0; i < patches.size(); ++i) {
    const auto destination = reinterpret_cast<std::uintptr_t>(relay + (i == 4 ? 16 : 0));
    const auto displacement = static_cast<std::int64_t>(destination) -
        static_cast<std::int64_t>(base + layout->sites[i] + 5);
    if (displacement < (std::numeric_limits<std::int32_t>::min)() ||
        displacement > (std::numeric_limits<std::int32_t>::max)()) { break; }
    patches[i][0] = i == 0 ? std::byte{0xE9} : std::byte{0xE8};
    const auto relative = static_cast<std::int32_t>(displacement);
    std::memcpy(patches[i].data() + 1, &relative, 4);
    if (!::VirtualProtect(reinterpret_cast<void*>(base + layout->sites[i]), 5,
                          PAGE_EXECUTE_READWRITE, &protections[i])) { break; }
    ++writable;
  }
  // All-or-nothing: validate and obtain write access for every site first.
  // This runs only at PostPostLoad, before actors/IED worker evaluation start.
  if (writable == patches.size()) {
    originalEquipped = reinterpret_cast<Equipped>(base + layout->equipped);
    originalNode = reinterpret_cast<Node>(base + layout->node);
    for (std::size_t i = 0; i < patches.size(); ++i) {
      std::memcpy(reinterpret_cast<void*>(base + layout->sites[i]), patches[i].data(), 5);
      ::FlushInstructionCache(::GetCurrentProcess(), reinterpret_cast<void*>(base + layout->sites[i]), 5);
    }
    installed.store(true);
  }
  // Restore in reverse order: different sites can share one executable page.
  while (writable) {
    --writable;
    DWORD unused{};
    ::VirtualProtect(reinterpret_cast<void*>(base + layout->sites[writable]), 5,
                     protections[writable], &unused);
  }
  if (!installed.load()) { ::VirtualFree(relay, 0, MEM_RELEASE); return; }
  // The 64 KB relay allocation belongs to these process-lifetime hooks. Never
  // free it while any patched call can run. No allocation per actor/condition.
  api::rendered::SetDecisionObserver(&QueueIedEvaluation);
  logger::info("SFS IED final-display BipedSlot bridge enabled: IED 1.7.4 {}, equipment + node conditions",
               layout == &detail::kLayouts[0] ? "SE/pre-629" : "AE/post-629");
}
} // namespace sfs::native::ied
