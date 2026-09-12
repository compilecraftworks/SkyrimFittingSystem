// Optional binary arguments exercise the actual installed IED helper code in
// an isolated test process. No SKSEPlugin_Load, DllMain, imports, game or MO2
// writes are run. Synthetic forms/keyword vtables satisfy the tested branches.
#include "native/IedBipedConditionAdapter.h"
#include "native/IedConditionBinary.h"
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "api/RenderedOutfitProvider.h"

namespace RE {
inline unsigned lookups = 0;
struct Actor { std::uint32_t GetFormID() const { return 0x14; } };
struct TESObjectARMO {};
struct TESForm {
  template<class T> static T* LookupByID(std::uint32_t) { ++lookups; return nullptr; }
};
}
namespace logger {
template<class... T> void info(T&&...) {}
template<class... T> void warn(T&&...) {}
}
namespace sfs::native {
unsigned evaluations = 0;
void QueueIedEvaluation(std::uint32_t) { ++evaluations; }
}
namespace sfs::api::rendered {
unsigned subscriptions = 0, queries = 0;
void SetDecisionObserver(DecisionObserver) { ++subscriptions; }
std::shared_ptr<const Value> AcquirePublished(std::uint32_t) { ++queries; return {}; }
}
#define SFS_IED_TEST
#include "native/IedConditionIntegration.cpp"

namespace d = sfs::native::ied::detail;
namespace abi = sfs::rendered_outfit_api;
void Check(bool value, const char* message) {
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
using Buffer = std::array<std::byte, 0x240>;
bool __fastcall HasKeyword(void* self, const void* keyword) {
  return d::Read<const void*>(self, 8) == keyword;
}
void Exercise(auto originalEquipped, auto originalNode) {
  Buffer actual{}, registered{}, keyword{}, otherKeyword{}, match{};
  std::array<void*, 5> keywordVtable{};
  keywordVtable[4] = reinterpret_cast<void*>(&HasKeyword);
  for (auto* form : {&actual, &registered}) {
    d::Write(form->data(), 0x1A, std::uint8_t{0x1A}); // ARMO
    d::Write(form->data(), 0x1D8, keywordVtable.data());
  }
  d::Write(actual.data(), 0x14, std::uint32_t{0x100});
  d::Write(registered.data(), 0x14, std::uint32_t{0x200});
  d::Write(keyword.data(), 0x14, std::uint32_t{0x300});
  d::Write(keyword.data(), 0x1A, std::uint8_t{4});
  d::Write(otherKeyword.data(), 0x14, std::uint32_t{0x301});
  d::Write(otherKeyword.data(), 0x1A, std::uint8_t{4});
  d::Write(registered.data(), 0x1E0, keyword.data());
  d::Write(match.data(), d::kBipedSlot, std::uint32_t{2});
  auto resolve = [&](std::uint32_t id) -> void* {
    return id == 0x100 ? actual.data() : id == 0x200 ? registered.data() : nullptr;
  };
  std::vector<abi::Item> visible{{0x200, 0x200, 4, 4, abi::Registered, 0}};
  auto run = [&] { return d::MatchBiped(visible, match.data(), resolve, originalEquipped); };
  Check(run() == true, "registered appearance satisfies body biped condition");
  d::Write(match.data(), 0x10, std::uint32_t{0x100});
  Check(run() == false, "hidden real equipment does not satisfy form match");
  d::Write(match.data(), 0x10, std::uint32_t{0x200});
  Check(run() == true, "visible registered form match");
  d::Write(match.data(), d::kFlags, std::uint32_t{1u << 13});
  Check(run() == false, "form negation retained in original helper");
  d::Write(match.data(), d::kFlags, std::uint32_t{0});
  d::Write(match.data(), 0x10, std::uint32_t{0});
  d::Write(match.data(), 0x30, std::uint32_t{0x300});
  d::Write(match.data(), 0x38, keyword.data());
  Check(run() == true, "visible registered keyword through original keyword vcall");
  d::Write(match.data(), d::kFlags, std::uint32_t{1u << 14});
  Check(run() == false, "keyword negation retained");
  d::Write(match.data(), d::kFlags, std::uint32_t{0});
  d::Write(match.data(), 0x30, std::uint32_t{0x301});
  d::Write(match.data(), 0x38, otherKeyword.data());
  Check(run() == false, "nonmatching keyword");
  d::Write(match.data(), 0x30, std::uint32_t{0});
  visible.push_back({0x100, 0x100, 4, 4, abi::Actual, 0});
  d::Write(match.data(), 0x10, std::uint32_t{0x100});
  Check(run() == true, "overlapping visible actual retained, no registration priority");
  visible.clear();
  Check(run() == false, "managed empty does not leak worn fallback");
  visible = {{0x200, 0x200, 4, 1, abi::Registered, 0}};
  d::Write(match.data(), 0x10, std::uint32_t{0});
  Check(run() == false, "effective slot mask, not declared slots");
  visible[0].visibleSlots = 4;
  d::Write(match.data(), d::kFlags, d::kMatchVisible);
  Check(run() == true, "final visible set satisfies Visible");
  d::Write(match.data(), d::kFlags, d::kMatchVisible | d::kNegateVisible);
  Check(run() == false, "hidden items cannot satisfy negative Visible in final set");
  d::Write(match.data(), d::kFlags, std::uint32_t{1u << 11});
  Check(run() == false, "armor does not become a bolt");
  d::Write(match.data(), d::kFlags, std::uint32_t{(1u << 11) | (1u << 15)});
  Check(run() == true, "negative bolt predicate retained");
  d::Write(match.data(), d::kFlags, d::kMatchSkin);
  Check(!run().has_value(), "skin queries remain original IED");
  d::Write(match.data(), d::kFlags, std::uint32_t{0});
  for (std::uint32_t slot : {32u, 41u, 42u, 45u, 0xFFFFFFFFu}) {
    d::Write(match.data(), d::kBipedSlot, slot);
    Check(!run().has_value(), "weapon/quiver/race sentinel never redirected");
  }
  for (std::uint32_t slot = 0; slot < 32; ++slot) {
    d::Write(match.data(), d::kBipedSlot, slot);
    visible[0].visibleSlots = 1u << slot;
    Check(run() == true, "all 32 ARMO slots including highest bit");
  }
  d::Write(match.data(), d::kBipedSlot, std::uint32_t{2});
  visible[0].visibleSlots = 4;
  Buffer node{};
  std::array<std::byte, 0x20> entry{};
  std::array<void*, 3> container{entry.data(), entry.data() + entry.size(), entry.data() + entry.size()};
  d::Write(entry.data(), 0, std::uint32_t{0x200});
  d::Write(entry.data(), 0x10, std::uint32_t{7});
  d::Write(node.data(), 0xF0, container.data());
  d::Write(node.data(), 0xF8, std::uint8_t{1});
  auto nodeRun = [&] {
    return d::MatchBiped(visible, match.data(), resolve,
        [&](void* params, const void* condition) { return originalNode(params, condition, node.data()); });
  };
  Check(nodeRun() == true && d::Read<std::uint64_t>(node.data(), 0x110) == (1ull << 7),
        "original node helper retains its matched-item side effect");
  d::Write(node.data(), 0x110, std::uint64_t{0});
  d::Write(entry.data(), 0, std::uint32_t{0x100});
  Check(nodeRun() == true && d::Read<std::uint64_t>(node.data(), 0x110) == 0,
        "registered form absent from IED real item data does not invent a matched slot");
}

int wmain(int argc, wchar_t** argv) {
  Check(::GetModuleHandleW(L"ImmersiveEquipmentDisplays.dll") == nullptr, "isolated test begins without IED");
  for (int i = 0; i < 1000; ++i) { sfs::native::ied::InstallConditionBridge(); }
  Check(!sfs::native::ied::installed && !sfs::native::ied::originalEquipped &&
        !sfs::native::ied::originalNode && !sfs::native::evaluations &&
        !sfs::api::rendered::subscriptions && !sfs::api::rendered::queries && !RE::lookups,
        "IED absent: production installer creates no hooks/subscriptions/evaluations/actor lookups");
  if (argc < 2) {
    // The regular suite needs no installed game or redistributed IED binary.
    std::array<std::byte, 0x48> match{};
    d::Write(match.data(), d::kBipedSlot, std::uint32_t{2});
    int calls = 0;
    std::array<abi::Item, 1> items{{{1, 1, 4, 4, abi::Registered, 0}}};
    auto result = d::MatchBiped(items, match.data(), [](auto) { return reinterpret_cast<void*>(1); },
        [&](void* params, const void* condition) {
          ++calls;
          const auto* biped = d::Read<const void*>(params, 0x60);
          Check(d::Read<std::uint8_t>(params, 0x68) == 1, "local cached biped avoids actor vcall");
          Check(d::Read<const void*>(biped, 0x10 + 2 * 0x78) == reinterpret_cast<void*>(1), "local candidate item");
          Check(d::Read<std::uint32_t>(condition, d::kFlags) == 0, "original predicates retained");
          return true;
        });
    Check(result == true && calls == 1, "adapter invokes original predicate exactly once");
    Check(d::MatchBiped({}, match.data(), [](auto) { return reinterpret_cast<void*>(1); },
        [&](auto, auto) { ++calls; return true; }) == false && calls == 1, "empty does not invoke worn helper");
  }
  for (int i = 1; i < argc; ++i) {
    const auto module = ::LoadLibraryExW(argv[i], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    Check(module != nullptr, "map binary without running its entrypoint/imports");
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const d::BinaryLayout* layout = nullptr;
    for (const auto& candidate : d::kLayouts) {
      if (candidate.timestamp == pe->FileHeader.TimeDateStamp &&
          candidate.imageSize == pe->OptionalHeader.SizeOfImage) { layout = &candidate; }
    }
    Check(layout && d::Validate({base, pe->OptionalHeader.SizeOfImage}, *layout), "both helper fingerprints and all five dispatchers");
    using Equipped = bool (*)(void*, const void*);
    using Node = bool (*)(void*, const void*, void*);
    Exercise(reinterpret_cast<Equipped>(const_cast<std::byte*>(base) + layout->equipped),
             reinterpret_cast<Node>(const_cast<std::byte*>(base) + layout->node));
    std::vector<std::byte> changed(base, base + pe->OptionalHeader.SizeOfImage);
    changed[layout->sites[4]] = std::byte{0x90};
    Check(!d::Validate(changed, *layout), "modified dispatch site is not overwritten");
    changed[layout->sites[4]] = base[layout->sites[4]];
    changed[layout->node + 0x50] ^= std::byte{1};
    Check(!d::Validate(changed, *layout), "modified helper ABI fails before any patch write");
    sfs::native::ied::InstallConditionBridge();
    Check(sfs::native::ied::installed && sfs::api::rendered::subscriptions == static_cast<unsigned>(i),
          "real production installer accepts each verified distribution");
    for (std::size_t siteIndex = 0; siteIndex < layout->sites.size(); ++siteIndex) {
      const auto* site = base + layout->sites[siteIndex];
      const auto* relay = site + 5 + d::Read<std::int32_t>(site, 1);
      Check(relay[0] == std::byte{0xFF} && relay[1] == std::byte{0x25}, "call points to leaf absolute relay");
      Check(d::Read<std::uintptr_t>(relay, 6) == (siteIndex == 4 ?
            reinterpret_cast<std::uintptr_t>(&sfs::native::ied::NodeHook) :
            reinterpret_cast<std::uintptr_t>(&sfs::native::ied::EquippedHook)), "all dispatch variants point to correct hook");
      MEMORY_BASIC_INFORMATION region{};
      ::VirtualQuery(site, &region, sizeof(region));
      Check(region.Protect == PAGE_EXECUTE_READ, "original executable page protection restored");
    }
    Exercise(reinterpret_cast<Equipped>(const_cast<std::byte*>(base) + layout->equipped),
             reinterpret_cast<Node>(const_cast<std::byte*>(base) + layout->node));
    ::FreeLibrary(module);
    // Only this isolated harness unloads an inert DLL. Production hooks persist
    // for the game process lifetime and never hot-unload or reinitialize.
    sfs::native::ied::installed.store(false);
    sfs::native::ied::originalEquipped = nullptr; sfs::native::ied::originalNode = nullptr;
    std::puts("Installed IED binary: actual equipped/node helper tests passed.");
  }
  std::puts("IED biped condition adapter tests passed.");
}
