// Execute the actual production SE/AE stubs and routing function with fake
// engine/provider objects. This verifies hook delivery and ABI preservation,
// not NIF output or a user's unobserved mod configuration.
#include "native/IedVisitorRoutingRules.h"
#include "native/BranchChainRules.h"
#include <xbyak/xbyak.h>
#define NOMINMAX
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <thread>
#include <stdexcept>

void Expect(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
namespace logger {
template<class... T> void info(T&&...) {}
template<class... T> void warn(T&&...) {}
template<class... T> void error(T&&...) {}
}
namespace sfs::api::rendered { void NotifySkinning(std::uint32_t) {} }
namespace RE {
namespace BSContainer { enum class ForEachResult { kStop, kContinue }; }
struct TESBoundObject {};
struct TESObjectARMO { bool hidden{}; };
struct InventoryEntryData { TESObjectARMO* armor{}; };
struct TESObjectREFR {
  template<class T> T* As() { return static_cast<T*>(this); }
};
struct Actor : TESObjectREFR {
  bool active{true}; bool hideActual{true};
  std::uint32_t GetFormID() const { return 0x14; }
};
struct ActorWeightModel {};
struct InventoryChanges {
  struct IItemChangeVisitor {
    virtual ~IItemChangeVisitor() = default;
    virtual BSContainer::ForEachResult Visit(InventoryEntryData*) = 0;
    virtual bool ShouldVisit(InventoryEntryData*, TESBoundObject*) = 0;
    virtual BSContainer::ForEachResult Unk_03(InventoryEntryData*, void*, bool*) = 0;
  };
  TESObjectREFR* owner{};
};
}
struct ConcreteVisitor : RE::InventoryChanges::IItemChangeVisitor {
  std::uint64_t providerState{0xABCD1234};
  unsigned visited{}, checked{}, unk{};
  RE::BSContainer::ForEachResult Visit(RE::InventoryEntryData*) override {
    ++visited; return RE::BSContainer::ForEachResult::kStop;
  }
  bool ShouldVisit(RE::InventoryEntryData*, RE::TESBoundObject*) override {
    ++checked; return true;
  }
  RE::BSContainer::ForEachResult Unk_03(RE::InventoryEntryData*, void* arg, bool* out) override {
    Expect(arg == this, "forward the original callback's extra argument");
    ++unk; if (out) *out = false; return RE::BSContainer::ForEachResult::kStop;
  }
};
struct DisplaySet { bool active; };
DisplaySet BuildDisplaySet(RE::Actor* actor) { return {actor && actor->active}; }
std::uint32_t CollectHiddenWornSlotMask(RE::TESObjectREFR* ref, const DisplaySet&) {
  return ref && ref->As<RE::Actor>()->hideActual ? 4 : 0;
}
struct HiddenRealEquipmentFilterVisitor : RE::InventoryChanges::IItemChangeVisitor {
  HiddenRealEquipmentFilterVisitor(RE::Actor*, const DisplaySet&,
                                  RE::InventoryChanges::IItemChangeVisitor&) {}
  RE::BSContainer::ForEachResult Visit(RE::InventoryEntryData*) override { return RE::BSContainer::ForEachResult::kContinue; }
  bool ShouldVisit(RE::InventoryEntryData*, RE::TESBoundObject*) override { return false; }
  RE::BSContainer::ForEachResult Unk_03(RE::InventoryEntryData*, void*, bool*) override { return RE::BSContainer::ForEachResult::kContinue; }
};
RE::TESObjectARMO* GetEntryArmor(RE::InventoryEntryData* entry) { return entry ? entry->armor : nullptr; }
bool ShouldHideRealArmor(RE::Actor*, const DisplaySet& set, const RE::TESObjectARMO* armor) {
  return set.active && armor && armor->hidden;
}
namespace REL {
bool g_rejectWrite{};
bool safe_write(std::uintptr_t dest, const void* src, std::size_t size,
                const void* expected, std::size_t expectedSize) {
  if (g_rejectWrite || std::memcmp(reinterpret_cast<void*>(dest), expected, expectedSize) != 0) return false;
  DWORD old{};
  Expect(VirtualProtect(reinterpret_cast<void*>(dest), size, PAGE_READWRITE, &old) != 0, "allow test vtable patch");
  std::memcpy(reinterpret_cast<void*>(dest), src, size);
  DWORD ignored{};
  Expect(VirtualProtect(reinterpret_cast<void*>(dest), size, old, &ignored) != 0, "restore test vtable protection");
  return true;
}
}
#include "CustomSkinningCallbackFilter.production.inc"
// Force genuine virtual dispatch, just like the opaque ABI boundary. Test both
// the identity of the original object/vptr and its concrete private fields.
__declspec(noinline) bool DispatchShouldVisit(RE::InventoryChanges::IItemChangeVisitor* visitor,
                                             RE::InventoryEntryData* entry) {
  return visitor->ShouldVisit(entry, nullptr);
}
__declspec(noinline) RE::BSContainer::ForEachResult DispatchVisit(
    RE::InventoryChanges::IItemChangeVisitor* visitor, RE::InventoryEntryData* entry) {
  return visitor->Visit(entry);
}
__declspec(noinline) RE::BSContainer::ForEachResult DispatchUnk(
    RE::InventoryChanges::IItemChangeVisitor* visitor, RE::InventoryEntryData* entry, bool* out) {
  return visitor->Unk_03(entry, visitor, out);
}
std::atomic<std::uintptr_t> g_iedVisitWornItemsChainTarget{0};
std::atomic<std::uintptr_t> g_passthroughVisitWornItemsChainTarget{0};
ConcreteVisitor* g_originalVisitor{};
RE::Actor* g_actor{};
RE::ActorWeightModel* g_weight{};
unsigned g_providerCalls{}, g_engineFiltered{}, g_additionalCalls{}, g_iedEvaluations{};
unsigned g_engineMaskCalls{}, g_engineArmorCalls{}, g_displayMaskCalls{};
void (*g_clobberVolatileState)(){};
RE::TESObjectARMO* g_armor{};
std::uint32_t EngineMask(RE::InventoryChanges* inventory) {
  Expect(inventory->owner == g_actor, "worn mask retains inventory owner");
  ++g_engineMaskCalls;
  return 0x44;
}
std::uint32_t ForeignMask(RE::InventoryChanges* inventory) {
  Expect(inventory->owner == g_actor, "foreign worn mask retains inventory owner");
  ++g_providerCalls;
  return 0x44;
}
void EngineArmor(RE::TESObjectARMO* armor, void* arg2, std::uintptr_t arg3, std::uintptr_t arg4) {
  Expect(armor == g_armor && arg2 == g_originalVisitor && arg3 == 0x8888 && arg4 == 0x9999,
         "vanilla armor call retains all four engine arguments");
  ++g_engineArmorCalls;
}
void ForeignArmor(RE::TESObjectARMO* armor, void* arg2, std::uintptr_t arg3, std::uintptr_t arg4) {
  EngineArmor(armor, arg2, arg3, arg4);
  --g_engineArmorCalls;
  ++g_providerCalls;
}
void EngineVisit(RE::InventoryChanges* inventory, RE::InventoryChanges::IItemChangeVisitor* visitor) {
  Expect(inventory->owner == g_actor, "actor-local inventory must be retained");
  if (visitor != g_originalVisitor) {
    Expect(dynamic_cast<HiddenRealEquipmentFilterVisitor*>(visitor) != nullptr,
           "only the original engine may receive the SFS filtering visitor");
    ++g_engineFiltered;
  }
}
void ForeignVisit(RE::InventoryChanges* inventory, RE::InventoryChanges::IItemChangeVisitor* visitor) {
  Expect(inventory->owner == g_actor, "foreign chain must receive the correct actor");
  Expect(visitor == g_originalVisitor, "foreign hooks must receive the original concrete visitor");
  Expect(g_originalVisitor->providerState == 0xABCD1234, "provider layout must remain intact");
  ++g_providerCalls;
  RE::TESObjectARMO hidden{true}; RE::InventoryEntryData entry{&hidden};
  const bool hide = g_actor->active && g_actor->hideActual && g_passthroughVisitWornItemsChainTarget != 0;
  Expect(DispatchShouldVisit(visitor, &entry) == !hide,
         "opaque provider must filter hidden actual equipment without replacing the concrete visitor");
  Expect(DispatchVisit(visitor, &entry) == (hide ? RE::BSContainer::ForEachResult::kContinue : RE::BSContainer::ForEachResult::kStop),
         "opaque direct Visit callbacks must also suppress hidden equipment");
  bool handled = false;
  Expect(DispatchUnk(visitor, &entry, &handled) == (hide ? RE::BSContainer::ForEachResult::kContinue : RE::BSContainer::ForEachResult::kStop) && handled == hide,
         "opaque Unk03 callbacks must preserve handled/return semantics");
}
void QueueIedEvaluate(RE::Actor* actor) {
  Expect(actor == g_actor, "IED refresh must remain actor-local");
  ++g_iedEvaluations;
}
namespace REL {
struct ID {
  std::uint64_t value;
  explicit ID(std::uint64_t id) : value(id) {}
  std::uintptr_t address() const { return static_cast<std::uintptr_t>(value); }
};
template<class T> struct Relocation {
  int id;
  Relocation(int a, int) : id(a) {}
  std::uintptr_t address() const {
    if (id == 17392) return reinterpret_cast<std::uintptr_t>(&EngineArmor);
    if (id == 15806) return reinterpret_cast<std::uintptr_t>(&EngineMask);
    return reinterpret_cast<std::uintptr_t>(&EngineVisit);
  }
};
}
#define RELOCATION_ID(a, b) a, b
namespace sfs::runtime {
struct HookLayout {
  std::string_view name;
  std::uint64_t customSkinRelocationID;
  std::uintptr_t customSkinCallOffset;
  std::uint64_t armorUpdateRelocationID{};
  std::uintptr_t vanillaArmorOffset{};
  std::uint64_t wornMaskRelocationID{};
  std::uintptr_t wornMaskCallOffset{};
};
}
namespace sfs::native {
bool ShouldBlockVanillaArmor(RE::TESObjectARMO* armor, RE::TESObjectREFR* actor) {
  Expect(armor == g_armor && actor == g_actor, "vanilla predicate retains armor and actor");
  return g_actor->active && g_actor->hideActual;
}
std::uint32_t GetDisplayWornMask(RE::InventoryChanges* inventory, RE::TESObjectREFR* actor,
                               std::uint32_t original) {
  Expect(inventory->owner == g_actor && actor == g_actor && original == 0x44,
         "display worn mask retains engine mask, inventory and actor");
  ++g_displayMaskCalls;
  return 0x80 | (g_actor->hideActual ? 0 : original);
}
void QueueIedEvaluation(std::uint32_t id) {
  Expect(g_actor && id==g_actor->GetFormID(), "inline IED follow-up stays actor-local");
  ++g_iedEvaluations;
}
void SetIedVisitWornItemsChainTarget(std::uintptr_t target) { g_iedVisitWornItemsChainTarget = target; }
void SetPassthroughVisitWornItemsChainTarget(std::uintptr_t target) { g_passthroughVisitWornItemsChainTarget = target; }
bool InstallOriginalWornVisitorFilter() {
  ConcreteVisitor visitor;
  return ScopedHiddenWornVisitorFilter::Install(*reinterpret_cast<std::uintptr_t*>(&visitor));
}
bool ShouldOverrideSkinning(RE::TESObjectREFR* actor) {
  Expect(actor == g_actor, "SE/AE stub must preserve its actor register");
  if (g_clobberVolatileState) g_clobberVolatileState();
  return g_actor->active;
}
void ApplyAdditionalDisplayArmors(RE::Actor* actor, RE::ActorWeightModel* weight) {
  Expect(actor == g_actor && weight == g_weight,
         "registered armor attachment must receive SE rdi / AE r15 weight model");
  ++g_additionalCalls;
}
#include "CustomSkinningVisitor.production.inc"
}
struct CallSiteBranch {
  std::uint8_t opcode{0xE8};
  std::uintptr_t target{};
  bool valid{true};
  bool expected{false};
  bool ChainsAsCall() const { return opcode == 0xE8; }
  bool ChainsAsJump() const { return opcode == 0xE9; }
};
CallSiteBranch g_callSite;
CallSiteBranch InspectCallSite(std::uintptr_t, std::uintptr_t, std::string_view) { return g_callSite; }
struct ModuleChainMatch { std::uintptr_t finalTarget; std::size_t trampolineDepth; };
bool g_knownIed{};
bool IsAddressInModule(std::uintptr_t target, std::wstring_view) {
  return g_knownIed && target == reinterpret_cast<std::uintptr_t>(&ForeignVisit);
}
#include "CustomSkinningChain.production.inc"
// Copy generated machine code before the production stack-local generator dies.
// Its branch labels are RIP-relative within the copied block.
struct LocalTrampoline {
  std::unique_ptr<Xbyak::CodeGenerator> code;
  void* allocate(const Xbyak::CodeGenerator& source) {
    code = std::make_unique<Xbyak::CodeGenerator>(4096);
    code->db(source.getCode(), source.getSize());
    code->ready();
    return const_cast<std::uint8_t*>(code->getCode());
  }
} g_localTrampoline;
namespace SKSE {
struct BranchTrampoline {
  void* stub{};
  std::uintptr_t patchedAddress{};
  template<int> void write_branch(std::uintptr_t address, void* target) {
    patchedAddress = address;
    stub = target;
  }
} g_branch;
BranchTrampoline& GetTrampoline() { return g_branch; }
}
bool g_inlineDetourIedLoaded{};
void InstallDavInitWornChainHook(const sfs::runtime::HookLayout&) {
  Expect(false, "valid E8/E9 must not enter DAV NOP fallback");
}
#include "CustomSkinningHooks.production.inc"

enum class HookKind { Custom, Mask, Vanilla };
struct InlineState {
  std::array<std::uint64_t, 7> gp{};
  std::array<std::uint64_t, 12> xmm{};
  std::uintptr_t stack{}, flags{};
};
InlineState g_expectedInlineState{}, g_observedInlineState{};
std::uint64_t g_stackCanary{};
const std::array<std::uint64_t, 12> kXmmSeed{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
struct TestCode : Xbyak::CodeGenerator {
  // Preserve the captured state, including flags, while recording it.
  void Capture(InlineState& state) {
    push(rax);
    mov(rax, reinterpret_cast<std::uintptr_t>(&state));
    mov(ptr[rax + 8], rcx); mov(ptr[rax + 16], rdx);
    mov(ptr[rax + 24], r8); mov(ptr[rax + 32], r9);
    mov(ptr[rax + 40], r10); mov(ptr[rax + 48], r11);
    for (int i=0; i<6; ++i) movdqu(ptr[rax + offsetof(InlineState, xmm) + i*16], Xbyak::Xmm(i));
    push(r10);
    mov(r10, ptr[rsp + 8]); mov(ptr[rax], r10);
    lea(r10, ptr[rsp + 16]); mov(ptr[rax + offsetof(InlineState, stack)], r10);
    pushfq(); pop(r10); mov(ptr[rax + offsetof(InlineState, flags)], r10);
    pop(r10); pop(rax);
  }
};

// Emulate the verified inline call site with real x64 registers and stack
// alignment, then return through the production stub's continuation jump.
struct Driver : TestCode {
  std::uintptr_t callSite{};
  Driver(bool ae, RE::InventoryChanges* inventory, ConcreteVisitor* visitor,
         HookKind kind = HookKind::Custom) {
    push(rbx); push(rdi); push(r15); push(rsi); push(r13);
    sub(rsp, 0x30);
    mov(qword[rsp + 0x20], 0x12345678);
    mov(rbx, reinterpret_cast<std::uintptr_t>(g_actor));
    mov(rsi, reinterpret_cast<std::uintptr_t>(g_actor));
    mov(r13, reinterpret_cast<std::uintptr_t>(g_actor));
    mov(rdi, ae ? 0xBAD : reinterpret_cast<std::uintptr_t>(g_weight));
    mov(r15, ae ? reinterpret_cast<std::uintptr_t>(g_weight) : 0xBAD);
    mov(rcx, kind == HookKind::Vanilla ? reinterpret_cast<std::uintptr_t>(g_armor)
                                    : reinterpret_cast<std::uintptr_t>(inventory));
    mov(rdx, reinterpret_cast<std::uintptr_t>(visitor));
    mov(r10, reinterpret_cast<std::uintptr_t>(kXmmSeed.data()));
    for (int i=0; i<6; ++i) movdqu(Xbyak::Xmm(i), ptr[r10 + i*16]);
    mov(r8, 0x8888); mov(r9, 0x9999); mov(r10, 0xAAAA); mov(r11, 0xBBBB);
    cmp(r8, r9);
    // Indirect jump through the writable test dispatch slot, followed by an
    // unreachable 5-byte stand-in for the original CALL instruction.
    mov(rax, reinterpret_cast<std::uintptr_t>(&SKSE::g_branch.stub));
    Capture(g_expectedInlineState);
    jmp(ptr[rax]);
    callSite = reinterpret_cast<std::uintptr_t>(getCurr());
    nop(5);
    mov(r10, reinterpret_cast<std::uintptr_t>(&g_stackCanary));
    mov(r11, ptr[rsp + 0x20]); mov(ptr[r10], r11);
    add(rsp, 0x30);
    pop(r13); pop(rsi); pop(r15); pop(rdi); pop(rbx);
    ret();
    ready();
  }
};

// This is an inline JMP detour, NOT a callable function. It enters on the
// engine frame and jumps back to site+5 instead of executing RET.
struct InlineProvider : TestCode {
  InlineProvider(std::uintptr_t resume, std::uintptr_t target) {
    Capture(g_observedInlineState);
    sub(rsp, 0x20);
    call(ptr[rip + visit]);
    add(rsp, 0x20);
    jmp(ptr[rip + continuation]);
    L(visit); dq(target);
    L(continuation); dq(resume);
    ready();
  }
  Xbyak::Label visit, continuation;
};

int main() {
  Xbyak::CodeGenerator clobber;
  for (int i=0; i<6; ++i) clobber.pxor(Xbyak::Xmm(i), Xbyak::Xmm(i));
  clobber.xor_(clobber.rax, clobber.rax); clobber.xor_(clobber.rcx, clobber.rcx);
  clobber.xor_(clobber.rdx, clobber.rdx); clobber.xor_(clobber.r8, clobber.r8);
  clobber.xor_(clobber.r9, clobber.r9); clobber.xor_(clobber.r10, clobber.r10);
  clobber.xor_(clobber.r11, clobber.r11); clobber.ret(); clobber.ready();
  g_clobberVolatileState=clobber.getCode<void(*)()>();
  {
    ConcreteVisitor visitor, other;
    const auto vptr = *reinterpret_cast<std::uintptr_t*>(&visitor);
    const auto* table = reinterpret_cast<const std::uintptr_t*>(vptr);
    const auto destructor = table[0];
    const auto rtti = table[-1];
    const std::array before{table[1], table[2], table[3]};
    REL::g_rejectWrite = true;
    Expect(!sfs::native::InstallOriginalWornVisitorFilter(), "a rejected callback write must fail safely");
    Expect(table[1] == before[0] && table[2] == before[1] && table[3] == before[2], "failed install must not partly patch callbacks");
    REL::g_rejectWrite = false;
    Expect(sfs::native::InstallOriginalWornVisitorFilter(), "install original-visitor callback filter");
    Expect(sfs::native::InstallOriginalWornVisitorFilter(), "repeat install must not chain back into itself");
    Expect(*reinterpret_cast<std::uintptr_t*>(&visitor) == vptr && table[0] == destructor && table[-1] == rtti,
           "preserve original vptr, destructor and RTTI, not just the visitor pointer");
    RE::Actor actor, second;
    DisplaySet active{true}, inactive{false};
    RE::TESObjectARMO hidden{true}, visible{false};
    RE::InventoryEntryData hiddenEntry{&hidden}, visibleEntry{&visible}, nonArmor{};
    Expect(DispatchShouldVisit(&visitor, &hiddenEntry), "ordinary engine visits must remain unchanged");
    {
      ScopedHiddenWornVisitorFilter outer{&visitor, &actor, active, true};
      Expect(!DispatchShouldVisit(&visitor, &hiddenEntry), "hide actual equipment in the current invocation");
      Expect(DispatchShouldVisit(&visitor, &visibleEntry) && DispatchShouldVisit(&visitor, &nonArmor) && DispatchShouldVisit(&visitor, nullptr),
             "visible armor, non-armor and null entries must retain original handling");
      Expect(DispatchShouldVisit(&other, &hiddenEntry), "same vtable must not leak filtering into an unrelated visitor");
      std::thread worker([&] {
        Expect(DispatchShouldVisit(&visitor, &hiddenEntry), "scope must not leak across threads even with the same pointer");
      });
      worker.join();
      {
        ScopedHiddenWornVisitorFilter inner{&visitor, &second, inactive, false};
        Expect(DispatchShouldVisit(&visitor, &hiddenEntry), "nested inactive calls must shadow the outer filter");
      }
      Expect(!DispatchShouldVisit(&visitor, &hiddenEntry), "restore outer scope after nested same-visitor call");
      try {
        ScopedHiddenWornVisitorFilter inner{&other, &second, active, true};
        Expect(!DispatchShouldVisit(&other, &hiddenEntry) && !DispatchShouldVisit(&visitor, &hiddenEntry),
               "nested actor scopes must resolve by exact visitor identity");
        throw std::runtime_error("provider exception fixture");
      } catch (const std::runtime_error&) {}
      Expect(DispatchShouldVisit(&other, &hiddenEntry) && !DispatchShouldVisit(&visitor, &hiddenEntry),
             "exception unwind must restore the enclosing scope without stale actor state");
      Expect(DispatchUnk(&visitor, &hiddenEntry, nullptr) == RE::BSContainer::ForEachResult::kContinue,
             "hidden callback must tolerate null handled output");
    }
    Expect(DispatchShouldVisit(&visitor, &hiddenEntry), "filter state must not survive synchronous provider completion");
    Expect(visitor.providerState == 0xABCD1234 && dynamic_cast<ConcreteVisitor*>(static_cast<RE::InventoryChanges::IItemChangeVisitor*>(&visitor)) == &visitor,
           "concrete state and dynamic type must remain intact");
  }
  // Read a complete branch at a page boundary without probing the adjacent
  // uncommitted page. Exercise the actual VirtualQuery/read/chain resolver.
  SYSTEM_INFO systemInfo{};
  GetSystemInfo(&systemInfo);
  const auto pageSize = static_cast<std::size_t>(systemInfo.dwPageSize);
  auto* region = static_cast<std::uint8_t*>(VirtualAlloc(
      nullptr, pageSize * 2, MEM_RESERVE, PAGE_NOACCESS));
  Expect(region != nullptr, "reserve a guarded branch fixture");
  Expect(VirtualAlloc(region, pageSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE) != nullptr,
         "commit only the first branch fixture page");
  auto* boundary = region + pageSize - 12;
  boundary[0] = 0x48; boundary[1] = 0xB8;
  const auto foreignAddress = reinterpret_cast<std::uintptr_t>(&ForeignVisit);
  std::memcpy(boundary + 2, &foreignAddress, sizeof(foreignAddress));
  boundary[10] = 0xFF; boundary[11] = 0xE0;
  g_knownIed = true;
  const auto boundaryMatch = ResolveBranchChainOwner(
      reinterpret_cast<std::uintptr_t>(boundary), L"ImmersiveEquipmentDisplays.dll");
  Expect(boundaryMatch && boundaryMatch->finalTarget == foreignAddress,
         "a valid page-end veneer must retain verified IED filtering");
  // A self-loop or truncated branch must terminate without an unsafe read.
  boundary[0] = 0xEB; boundary[1] = 0xFE;
  Expect(!ResolveBranchChainOwner(reinterpret_cast<std::uintptr_t>(boundary), L"IED"),
         "cyclic branch chains must terminate");
  region[pageSize - 1] = 0xE9;
  Expect(!ResolveBranchChainOwner(reinterpret_cast<std::uintptr_t>(region + pageSize - 1), L"IED"),
         "truncated branches must not cross an unreadable page");
  Expect(!ResolveBranchChainOwner(0, L"IED"), "null branch chains must fail safely");
  VirtualFree(region, 0, MEM_RELEASE);
  unsigned cases = 0;
  // The hook must attach every kind of registered armor independently of
  // provider ownership. Slot selection and morph logic remain separately tested.
  for (bool ae : {false, true}) {
    for (int provider : {0, 1, 2, 3, 4}) { // engine, IED, unknown, IED veneer, engine veneer
      for (bool active : {false, true}) {
        for (bool hideActual : {false, true}) {
          RE::Actor actor; actor.active = active; actor.hideActual = hideActual;
          RE::ActorWeightModel weight;
          ConcreteVisitor visitor;
          RE::InventoryChanges inventory{&actor};
          g_actor = &actor; g_weight = &weight; g_originalVisitor = &visitor;
          g_providerCalls = g_engineFiltered = g_additionalCalls = g_iedEvaluations = 0;
          g_knownIed = provider == 1 || provider == 3;
          Xbyak::CodeGenerator veneer;
          veneer.db(0xF3); veneer.db(0x0F); veneer.db(0x1E); veneer.db(0xFA);
          veneer.mov(veneer.r11, reinterpret_cast<std::uintptr_t>(
              provider == 4 ? &EngineVisit : &ForeignVisit));
          veneer.jmp(veneer.r11);
          veneer.ready();
          g_callSite = {.target = reinterpret_cast<std::uintptr_t>(
                           provider == 0 ? &EngineVisit : &ForeignVisit),
                        .expected = provider == 0};
          if (provider == 3 || provider == 4) {
            g_callSite.target = reinterpret_cast<std::uintptr_t>(veneer.getCode());
          }
          SKSE::g_branch.stub = nullptr;
          Driver driver(ae, &inventory, &visitor);
          sfs::runtime::HookLayout layout{ae ? "AE" : "SE", driver.callSite, 0};
          if (ae) InstallCustomSkinHookAE(layout); else InstallCustomSkinHookSE(layout);
          Expect(SKSE::g_branch.stub != nullptr,
                 "an unknown pre-patched CALL must not disable registered wig/clothing attachment");
          Expect(SKSE::g_branch.patchedAddress == driver.callSite, "patch only the verified call site");
          driver.getCode<void(*)()>()();
          Expect(g_additionalCalls == unsigned(active), "active registered appearances must attach exactly once");
          const bool filtered = active && hideActual && provider != 2;
          Expect(g_engineFiltered == unsigned(filtered), "filter only through a verified engine-compatible target");
          Expect(g_iedEvaluations == unsigned(filtered && g_knownIed), "preserve IED follow-up without extra calls");
          Expect(g_providerCalls == unsigned(provider != 0 && provider != 4 && !filtered), "preserve opaque foreign chain behavior");
          ++cases;
        }
      }
    }
    for (auto kind : {HookKind::Custom, HookKind::Mask, HookKind::Vanilla}) {
      for (bool jump : {false, true}) {
        // Existing E8 custom-provider routes are covered above.
        if (!jump && kind == HookKind::Custom) continue;
        for (bool ied : {false, true}) {
          RE::Actor actor;
          RE::ActorWeightModel weight; ConcreteVisitor visitor; RE::TESObjectARMO armor;
          RE::InventoryChanges inventory{&actor};
          g_actor=&actor; g_weight=&weight; g_originalVisitor=&visitor; g_armor=&armor;
          g_knownIed=false;
          Driver driver(ae,&inventory,&visitor,kind);
          const auto foreign = kind == HookKind::Custom ? reinterpret_cast<std::uintptr_t>(&ForeignVisit)
              : kind == HookKind::Mask ? reinterpret_cast<std::uintptr_t>(&ForeignMask)
                                      : reinterpret_cast<std::uintptr_t>(&ForeignArmor);
          InlineProvider prior(driver.callSite+5, foreign);
          g_callSite={.opcode=std::uint8_t(jump?0xE9:0xE8),
              .target=jump?reinterpret_cast<std::uintptr_t>(prior.getCode()):foreign,.valid=true};
          SKSE::g_branch.stub=nullptr;
          sfs::runtime::HookLayout layout{ae?"AE":"SE",driver.callSite,0,driver.callSite,0,driver.callSite,0};
          if (kind == HookKind::Custom) {
            if(ae) InstallCustomSkinHookAE(layout); else InstallCustomSkinHookSE(layout);
          } else if (kind == HookKind::Mask) {
            if(ae) InstallShimWornFlagsHookAE(layout); else InstallShimWornFlagsHookSE(layout);
          } else {
            Expect(InstallDontVanillaSkinHook(layout), "install vanilla armor hook");
          }
          // The production installer resolves the module once; exercise both
          // results without loading an actual game DLL into this test process.
          g_inlineDetourIedLoaded=ied;
          Expect(SKSE::g_branch.stub!=nullptr,"E9 must not disable SFS registration/hiding");
          for (unsigned cycle=0; cycle<128; ++cycle) {
            actor.active=(cycle&1)!=0; actor.hideActual=(cycle&2)!=0;
            g_providerCalls=g_engineFiltered=g_additionalCalls=g_iedEvaluations=0;
            g_engineMaskCalls=g_engineArmorCalls=g_displayMaskCalls=0;
            g_observedInlineState={}; g_stackCanary=0;
            const auto mask=driver.getCode<std::uint32_t(*)()>()();
            Expect(g_stackCanary==0x12345678,"all hook routes preserve the engine stack frame");
            if (jump && !actor.active) {
              Expect(std::memcmp(&g_expectedInlineState,&g_observedInlineState,sizeof(InlineState))==0,
                     "passive E9 preserves RSP, all volatile GP/XMM registers and flags, without CALL/RET");
            }
            if (kind == HookKind::Custom) {
              Expect(g_additionalCalls==unsigned(actor.active),"E9 active actor attaches registered armor once");
              Expect(g_engineFiltered==unsigned(actor.active&&actor.hideActual),"E9 active actor filters hidden real equipment only");
              Expect(g_providerCalls==unsigned(!actor.active),"E9 passive actor retains prior inline provider");
              Expect(g_iedEvaluations==unsigned(actor.active&&ied),"E9 IED follow-up only when installed and active");
            } else if (kind == HookKind::Mask) {
              Expect(mask==(actor.active ? (0x80u | (actor.hideActual?0u:0x44u)) : 0x44u),"worn mask retains real and registered slot policy");
              Expect(g_displayMaskCalls==unsigned(actor.active),"no display mask leaks to passive actor");
              Expect(g_engineMaskCalls==unsigned(jump&&actor.active),"active E9 uses original engine worn-mask call");
              Expect(g_providerCalls==unsigned(!jump||!actor.active),"E8 worn provider and passive E9 retained");
            } else {
              const bool blocked=actor.active&&actor.hideActual;
              Expect(g_engineArmorCalls==unsigned(jump&&actor.active&&!blocked),"active E9 applies only visible engine armor");
              Expect(g_providerCalls==unsigned(!blocked&&(!jump||!actor.active)),"vanilla E8/passive E9 behavior retained");
            }
            ++cases;
          }
        }
      }
    }
    for (auto opcode : {std::uint8_t{0x90}}) {
      g_callSite = {.opcode = opcode, .target = 0x1234, .valid = false};
      SKSE::g_branch.stub = nullptr;
      sfs::runtime::HookLayout layout{ae ? "AE" : "SE", 0x10000, 0};
      if (ae) InstallCustomSkinHookAE(layout); else InstallCustomSkinHookSE(layout);
      Expect(SKSE::g_branch.stub == nullptr, "unknown instruction/resume layouts must remain untouched");
    }
  }
  std::printf("Custom skinning production regression tests passed (%u SE/AE routing cases plus invalid opcodes)\n", cases);
}
