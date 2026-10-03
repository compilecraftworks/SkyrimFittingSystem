// Execute the production candidate resolver and machine stub. The native
// equipment decisions below are a model, not a claim of an in-game playtest.
#include "native/ActualEquipmentConflictHook.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

void Check(bool ok, const char *message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
namespace RE {
struct TESForm { std::uint32_t mask{}; bool worn{}, quest{}; };
struct TESObjectARMO : TESForm {};
struct InventoryChanges {
  std::vector<TESObjectARMO *> items;
  unsigned reads{};
  TESObjectARMO *GetArmorInSlot(std::int32_t slot) {
    ++reads;
    Check(slot >= 30 && slot < 62, "physical slot lookup is 30..61");
    for (auto *item : items) {
      if (item && item->worn && (item->mask & (1u << (slot - 30)))) { return item; }
    }
    return nullptr;
  }
};
struct BIPOBJECT { TESForm *item{}; std::array<std::byte, 0x70> rest{}; };
static_assert(sizeof(BIPOBJECT) == 0x78);
struct BipedAnim {
  std::array<std::byte, 0x10> prefix{};
  std::array<BIPOBJECT, 32> objects{};
};
struct Actor {
  std::uint32_t id{0x14}; InventoryChanges *inventory{};
  std::uint32_t GetFormID() const { return id; }
  InventoryChanges *GetInventoryChanges(bool noInit) {
    Check(noInit, "resolver must not initialize inventory"); return inventory;
  }
};
}
struct Row {
  std::uint32_t ownerActorFormID{}; bool state{};
  bool IsOwnedByActor(RE::Actor *actor) const {
    return ownerActorFormID == actor->id || (!ownerActorFormID && actor->id == 0x14);
  }
  bool HasOverridesOrHideState() const { return state; }
};
struct Workbench {
  std::vector<Row> rows, rules; std::uint32_t preview{};
  struct Lock {};
  Lock AcquireStateLock() { return {}; }
  const auto &GetRows() const { return rows; }
  const auto &GetConditionalVisibilityRules() const { return rules; }
  const void *GetNativePreviewRowsForActor(std::uint32_t id) const {
    return preview == id ? this : nullptr;
  }
};
namespace sfs {
struct Menu {
  static inline Menu *instance{};
  bool loaded{true}; Workbench workbench;
  static Menu *GetSingleton() { return instance; }
  bool IsGameDataLoaded() const { return loaded; }
  Workbench &GetWorkbench() { return workbench; }
};
}
bool IsPlayerActor(RE::Actor *actor) { return actor && actor->id == 0x14; }
namespace sfs::native {
#include "ActualEquipConflict.production.inc"
}

struct Context {
  RE::Actor *actor; RE::BipedAnim *biped;
  std::uint64_t slot, index;
  std::array<std::uint64_t, 7> before{}, after{};
  std::uint64_t beforeFlags{}, afterFlags{};
  std::array<std::uint8_t, 96> beforeXmm{}, afterXmm{};
};
class Harness final : public Xbyak::CodeGenerator {
public:
  explicit Harness(std::uintptr_t stub) {
    Xbyak::Label target;
    push(rbx); push(rbp); push(rdi); push(rsi);
    push(r12); push(r13); push(r14); push(r15);
    sub(rsp, 0x28);
    mov(r15, rcx);
    mov(rdi, ptr[r15 + offsetof(Context, actor)]);
    mov(rax, ptr[r15 + offsetof(Context, biped)]);
    mov(ebx, ptr[r15 + offsetof(Context, slot)]);
    mov(r13, ptr[r15 + offsetof(Context, index)]);
    mov(rcx, 0x1111); mov(rdx, 0x2222); mov(r8, 0x3333);
    mov(r9, 0x4444); mov(r10, 0x5555); mov(r11, 0x6666);
    const std::array<Xbyak::Reg64, 7> regs{rax, rcx, rdx, r8, r9, r10, r11};
    for (std::size_t i = 0; i < regs.size(); ++i) {
      mov(ptr[r15 + offsetof(Context, before) + i * 8], regs[i]);
    }
    for (int i = 0; i < 6; ++i) {
      movdqu(Xbyak::Xmm(i), ptr[r15 + offsetof(Context, beforeXmm) + i * 16]);
    }
    cmp(ebx, ebx); stc();
    pushfq(); pop(qword[r15 + offsetof(Context, beforeFlags)]);
    call(ptr[rip + target]);
    pushfq(); pop(qword[r15 + offsetof(Context, afterFlags)]);
    for (std::size_t i = 0; i < regs.size(); ++i) {
      mov(ptr[r15 + offsetof(Context, after) + i * 8], regs[i]);
    }
    for (int i = 0; i < 6; ++i) {
      movdqu(ptr[r15 + offsetof(Context, afterXmm) + i * 16], Xbyak::Xmm(i));
    }
    mov(rax, r12);
    add(rsp, 0x28);
    pop(r15); pop(r14); pop(r13); pop(r12);
    pop(rsi); pop(rdi); pop(rbp); pop(rbx);
    ret();
    L(target); dq(stub);
    ready();
  }
};

int RunTests() {
  using namespace sfs::native::equip_conflict;
  // Independent byte fragments transcribed from both live decoded functions.
  std::array<std::uint8_t, kContractSize> bytes{};
  const auto put = [&](std::size_t offset, std::initializer_list<std::uint8_t> v) {
    std::copy(v.begin(), v.end(), bytes.begin() + offset);
  };
  put(0x35, {0x48,0x8B,0xF9}); put(0x92, {0x8B,0xD3,0x48,0x8B,0xCD});
  put(0xA4, {0x33,0xC9,0x44,0x8B,0xE9}); put(0xB2, {0x49,0x8B,0x07});
  put(0xC9, {0x4E,0x8B,0x64,0x28,0x10,0x49,0x8B,0xCC});
  put(0x23F, {0x49,0x83,0xC5,0x78,0x49,0x81,0xFD,0x00,0x0F,0x00,0x00});
  Check(MatchesInputContract(bytes), "verified SE/AE input contract accepted");
  for (auto opcode : {0xE8, 0xE9, 0xFF}) {
    bytes[0x97] = static_cast<std::uint8_t>(opcode);
    Check(MatchesInputContract(bytes), "existing outer provider predicate is untouched");
  }
  for (auto offset : {0x35, 0x92, 0xA4, 0xB2, 0xC9, 0xD0, 0x23F, 0x245}) {
    bytes[offset] ^= 1;
    Check(!MatchesInputContract(bytes), "changed register/layout/read must not be patched");
    bytes[offset] ^= 1;
  }
  Check(!MatchesInputContract(std::span(bytes).first(kContractSize - 1)), "short code range rejected");

  CandidateReadCode stub{reinterpret_cast<std::uintptr_t>(
      &sfs::native::ResolveActualEquipConflictCandidate)};
  stub.ready();
  Harness harness{reinterpret_cast<std::uintptr_t>(stub.getCode())};
  auto read = harness.getCode<RE::TESForm *(*)(Context *)>();
  RE::InventoryChanges inventory;
  RE::Actor actor{0x14, &inventory};
  RE::BipedAnim biped;
  sfs::Menu menu; sfs::Menu::instance = &menu;
  menu.workbench.rows = {{0x14, true}};
  Context context{&actor, &biped, 3, 0};
  for (std::size_t i = 0; i < context.beforeXmm.size(); ++i) {
    context.beforeXmm[i] = static_cast<std::uint8_t>(i * 7 + 3);
  }
  const auto run = [&](unsigned slot, unsigned index = 0) {
    context.slot = slot; context.index = index * 0x78;
    auto *result = read(&context);
    Check(context.before == context.after, "all volatile integer registers preserved");
    Check(context.beforeFlags == context.afterFlags, "engine flags preserved");
    Check(context.beforeXmm == context.afterXmm, "all volatile XMM registers preserved");
    return result;
  };
  RE::TESObjectARMO actual, appearance;
  actual.mask = 1u << 3; actual.worn = true;
  appearance.mask = 1u << 2;
  inventory.items = {&actual}; biped.objects[0].item = &appearance;
  const auto originalBiped = biped;
  Check(run(3) == &actual, "hidden worn gloves supplement native candidate");
  Check(inventory.reads == 1 && actual.worn, "exactly one read, no actual equip mutation");
  Check(std::memcmp(&biped, &originalBiped, sizeof(biped)) == 0, "render biped untouched");
  for (unsigned i = 1; i < 32; ++i) {
    Check(run(3, i) == biped.objects[i].item, "remaining native candidates pass through");
  }
  Check(inventory.reads == 1, "no repeated inventory lookup inside 32-entry loop");
  biped.objects[3].item = &actual;
  Check(run(3) == &appearance, "already-rendered actual gear leaves native input unchanged");
  biped.objects[3].item = nullptr;
  Check(run(4) == &appearance, "unoccupied physical slot does not become an appearance conflict");
  Check(run(32) == &appearance, "out-of-range physical slot passes through");
  menu.workbench.rows.clear(); auto reads = inventory.reads;
  Check(run(3) == &appearance && inventory.reads == reads, "idle player does no inventory work");
  menu.workbench.rows = {{0, true}};
  Check(run(3) == &actual, "legacy player-owned rows still restore physical input");
  actor.id = 0x1234; reads = inventory.reads;
  Check(run(3) == &appearance && inventory.reads == reads, "other actor's rows cannot affect NPC");
  menu.workbench.rows = {{actor.id, true}};
  Check(run(3) == &actual, "managed NPC receives same physical input");
  menu.workbench.rows.clear(); menu.workbench.preview = actor.id;
  Check(run(3) == &actual, "preview ownership included");
  menu.workbench.preview = 0; menu.workbench.rules = {{actor.id, true}};
  Check(run(3) == &actual, "condition-only ownership included without evaluating conditions");
  menu.loaded = false; reads = inventory.reads;
  Check(run(3) == &appearance && inventory.reads == reads, "startup/revert does no work");
  menu.loaded = true; menu.workbench.rules.clear();
  actor.id = 0x14; menu.workbench.rows = {{0x14, true}};

  RE::TESObjectARMO ring, necklace;
  ring.mask = 1u << 6; ring.worn = true;
  necklace.mask = 1u << 5; necklace.worn = true;
  inventory.items = {&actual, &ring, &necklace};
  Check(run(6) == &ring && run(5) == &necklace && run(3) == &actual,
        "hidden ring/necklace/gloves retain distinct physical slot ownership");
  Check(ring.worn && necklace.worn && actual.worn,
        "candidate lookup cannot unequip other physical slots");
  actor.inventory = nullptr; reads = inventory.reads;
  Check(run(6) == &appearance && inventory.reads == reads,
        "absent inventory neither initializes nor synthesizes equipped items");
  actor.inventory = &inventory; inventory.items = {&actual};
  sfs::Menu::instance = nullptr;
  Check(run(3) == &appearance, "missing menu preserves original input");
  sfs::Menu::instance = &menu;

  // A provider which completes its own conflict processing returns false at
  // the untouched outer predicate and never reaches our native input read.
  // Model both predicate results, without pretending to execute a DAV DLL.
  for (bool providerContinuesNative : {false, true}) {
    reads = inventory.reads;
    if (providerContinuesNative) { Check(run(3) == &actual, "provider continuation receives the normal correction"); }
    Check(inventory.reads == reads + unsigned(providerContinuesNative),
          "provider-handled conflicts do not trigger duplicate SFS lookup");
  }

  // Model the native decision AFTER the intercepted candidate load. SFS has
  // no unequip call; it only supplies a worn ARMO to this unchanged decision.
  for (unsigned cycle = 0; cycle < 128; ++cycle) {
    for (unsigned slot = 0; slot < 32; ++slot) {
      actual.mask = 1u << slot; actual.worn = true; actual.quest = cycle % 2 != 0;
      auto *candidate = run(slot);
      Check(candidate == &actual && (candidate->mask & (1u << slot)), "every physical slot remains occupied while hidden");
      const bool nativeAllowsReplacement = !candidate->quest;
      if (nativeAllowsReplacement) { candidate->worn = false; }
      Check(actual.worn == actual.quest, "native quest denial and ordinary replacement retained");
      Check(std::memcmp(&biped, &originalBiped, sizeof(biped)) == 0, "repeated reads never change rendered appearance");
    }
  }
  Check(inventory.items.size() == 1, "no inventory additions or retained per-actor history");
  std::puts("Actual equipment conflict input: production resolver/JIT ABI and 4096 physical-slot transitions passed");
  return 0;
}

int main(int argc, char **argv) {
  try {
    // Optional local read-only evidence artifacts are NOT shipped with tests.
    for (int i = 1; i < argc; ++i) {
      std::ifstream input(argv[i], std::ios::binary);
      std::array<std::uint8_t, sfs::native::equip_conflict::kContractSize> code{};
      input.seekg(512);
      input.read(reinterpret_cast<char *>(code.data()), code.size());
      Check(input.good() && sfs::native::equip_conflict::MatchesInputContract(code),
            "captured decoded engine function must match production installer contract");
      std::printf("Live-code contract verified: %s\n", argv[i]);
    }
    return RunTests();
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "Test exception: %s\n", error.what()); return 1;
  }
}
