// Unchanged production memo/traversal/type-lookup/registration/queue bodies.
// VM, native patching and task dispatch are fixtures; this is not a game
// benchmark or validation of Papyrus binary layouts.
#include "native/PapyrusObserverInstallRules.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
namespace logger {
template<class... T> void info(T&&...) {}
template<class... T> void debug(T&&...) {}
}
namespace RE {
struct BSFixedString {
  std::string value;
  BSFixedString(const char* v) : value(v) {}
  BSFixedString(const std::string& v) : value(v) {}
  const char* c_str() const { return value.c_str(); }
};
template<class T> using BSTSmartPointer = std::shared_ptr<T>;
namespace BSScript {
struct IFunction { bool patched{}, failPatch{}; };
struct Function { BSTSmartPointer<IFunction> func{std::make_shared<IFunction>()}; };
struct State {
  Function* GetFuncIter() { return nullptr; }
  unsigned GetNumFuncs() const { return 0; }
};
struct ObjectTypeInfo {
  bool linked{true}, tableReady{true};
  unsigned membersDiscovered{}, memberReads{};
  std::vector<Function> globals{12};
  bool IsLinked() const { return linked; }
  Function* GetGlobalFuncIter() { return tableReady ? globals.data() : nullptr; }
  unsigned GetNumGlobalFuncs() const { return static_cast<unsigned>(globals.size()); }
  Function* GetMemberFuncIter() { ++memberReads; return nullptr; }
  unsigned GetNumMemberFuncs() const { return 0; }
  State* GetNamedStateIter() { ++memberReads; return nullptr; }
  unsigned GetNumNamedStates() const { return 0; }
};
struct IVirtualMachine {
  std::map<std::string, BSTSmartPointer<ObjectTypeInfo>> types;
  bool lookupSuccess{true}, bindSuccess{true};
  bool GetScriptObjectTypeNoLoad(const BSFixedString& name,
                               BSTSmartPointer<ObjectTypeInfo>& out) {
    auto found = types.find(name.value);
    out = found == types.end() ? nullptr : found->second;
    return lookupSuccess && bool(out);
  }
};
}
}
using Type = RE::BSScript::ObjectTypeInfo;
using VM = RE::BSScript::IVirtualMachine;
unsigned probes{}, pplusScans{}, externalPplusScans{};
std::function<void()> beforePatch;
bool PatchSelectedNativeFunction(RE::BSScript::IFunction* fn, bool* settled = nullptr) {
  ++probes;
  if (beforePatch) beforePatch();
  if (settled) *settled = fn && !fn->failPatch;
  if (!fn || fn->failPatch || fn->patched) return false;
  fn->patched = true;
  return true;
}
constexpr auto kScriptTypeInspectionDelay = std::chrono::milliseconds(1);
constexpr std::uint8_t kScriptTypeInspectionMaxAttempts = 4;
std::mutex g_scriptTypeInspectionMutex;
std::unordered_map<std::string, std::uint8_t> g_pendingScriptTypeInspections;
bool g_scriptTypeInspectionWorkerScheduled{};
#include "type-inspection.production.inc"
std::size_t InspectFullyLinkedSexLabPPlusAliasMembers(Type* type) {
  ++pplusScans; return type->membersDiscovered;
}
namespace sfs::native::external_equipment {
void InspectFullyLinkedSexLabPPlusAlias(Type*) { ++externalPplusScans; }
}
namespace SKSE {
struct TaskInterface {
  std::mutex mutex;
  std::condition_variable cv;
  std::vector<std::function<void()>> work;
  unsigned submitted{};
  void AddTask(std::function<void()> fn) {
    { std::lock_guard lock(mutex); work.push_back(std::move(fn)); ++submitted; }
    cv.notify_one();
  }
  void Drain() {
    for (;;) {
      std::vector<std::function<void()>> batch;
      {
        std::lock_guard lock(g_scriptTypeInspectionMutex);
        if (!g_scriptTypeInspectionWorkerScheduled) break;
      }
      {
        std::unique_lock lock(mutex);
        Check(cv.wait_for(lock, std::chrono::seconds(5), [&]{ return !work.empty(); }),
              "deferred task arrives");
        batch.swap(work);
      }
      for (auto& fn : batch) fn();
    }
  }
} tasks;
bool taskAvailable{true};
TaskInterface* GetTaskInterface() { return taskAvailable ? &tasks : nullptr; }
}
#include "type-lookup.production.inc"
#include "native-registration.production.inc"
bool OriginalLookup(VM* vm, const RE::BSFixedString& name,
                    RE::BSTSmartPointer<Type>& out) {
  return vm->GetScriptObjectTypeNoLoad(name, out);
}
bool OriginalBind(VM* vm, RE::BSScript::IFunction*) { return vm->bindSuccess; }
bool Lookup(VM& vm, const char* name = "KnownType") {
  RE::BSTSmartPointer<Type> out;
  return ScriptTypeLoadHook::thunk(&vm, name, out);
}
void Settle(VM& vm, const char* name = "KnownType") {
  Check(Lookup(vm, name), "original lookup result preserved");
  SKSE::tasks.Drain();
}
int main() {
  ScriptTypeLoadHook::func.store(&OriginalLookup);
  NativeRegistrationHook::func.store(&OriginalBind);
  VM vm;
  auto type = std::make_shared<Type>();
  vm.types["KnownType"] = type;
  for (unsigned n=0; n<128; ++n) Check(Lookup(vm), "known lookup succeeds");
  Check(probes == 12 && type->globals.front().func->patched,
        "first lookup patches synchronously; pending repeats do not rescan");
  SKSE::tasks.Drain();
  for (unsigned n=0; n<128; ++n) Check(Lookup(vm), "settled lookup succeeds");
  Check(probes == 24 && SKSE::tasks.submitted == 1 && type->memberReads == 0,
        "256 lookups: one immediate + one post-link scan, no generic member walks");
  std::puts("256 known-type lookups: 24 probes (was 3096), 1 task batch (was 2).");

  const auto beforeFailure = probes;
  vm.lookupSuccess = false;
  Check(!Lookup(vm) && probes == beforeFailure, "failed lookup does not inspect");
  vm.lookupSuccess = true;
  ScriptTypeLoadHook::func.store(nullptr);
  Check(!Lookup(vm), "missing original is not called");
  ScriptTypeLoadHook::func.store(&OriginalLookup);

  // Same name, different live object; retaining cached identity must not hide it.
  auto replacement = std::make_shared<Type>();
  vm.types["KnownType"] = replacement;
  Settle(vm);
  Check(replacement->globals.front().func->patched, "replacement type inspected");
  auto next = std::make_shared<RE::BSScript::IFunction>();
  replacement->globals.push_back({next});
  Settle(vm);
  Check(next->patched, "changed global count inspected");
  auto relocated = replacement->globals;
  auto relocatedNative = std::make_shared<RE::BSScript::IFunction>();
  relocated.front().func = relocatedNative;
  replacement->globals.swap(relocated);
  Settle(vm);
  Check(relocatedNative->patched, "same count at new table address inspected");

  auto late = std::make_shared<RE::BSScript::IFunction>();
  replacement->globals.front().func = late; // same address and count
  Check(NativeRegistrationHook::thunk(&vm, late.get()) && late->patched,
        "late bind patched before first call despite completed memo");
  const auto beforeLate = probes;
  Settle(vm);
  Check(probes == beforeLate + 2 * replacement->globals.size(),
        "successful bind invalidates immediate and post-link completion");
  auto failed = std::make_shared<RE::BSScript::IFunction>();
  vm.bindSuccess = false;
  const auto revision = g_scriptTypeInspectionRevision.load();
  Check(!NativeRegistrationHook::thunk(&vm, failed.get()) && !failed->patched &&
        g_scriptTypeInspectionRevision.load() == revision, "failed bind unchanged");
  vm.bindSuccess = true;

  replacement->linked = false;
  Settle(vm); // exhaust four attempts; do not remember success
  replacement->globals.front().func = failed;
  replacement->linked = true;
  Settle(vm);
  Check(failed->patched, "later linked type is retried, not permanently skipped");
  replacement->tableReady = false;
  Settle(vm);
  auto published = std::make_shared<RE::BSScript::IFunction>();
  replacement->globals.front().func = published;
  replacement->tableReady = true;
  Settle(vm);
  Check(published->patched, "missing table never becomes completed memo");

  auto pplus = std::make_shared<Type>();
  vm.types["sslActorAlias"] = pplus;
  Settle(vm, "sslActorAlias");
  Check(pplusScans == 4 && externalPplusScans == 4,
        "both P+ observers retain all four post-link retries");
  pplus->membersDiscovered = 2;
  Settle(vm, "sslActorAlias");
  Check(pplusScans == 5 && externalPplusScans == 5,
        "later P+ members discovered after exhausted attempts");
  Settle(vm, "sslActorAlias");
  Check(pplusScans == 6 && externalPplusScans == 6,
        "both P+ member observers keep natural-lookup recovery independently");

  auto pending = std::make_shared<Type>();
  auto retryNative = pending->globals.front().func;
  retryNative->failPatch = true;
  vm.types["RetryType"] = pending;
  Settle(vm, "RetryType");
  Check(!retryNative->patched, "failed selected hook is not reported installed");
  retryNative->failPatch = false;
  Settle(vm, "RetryType");
  Check(retryNative->patched, "failed selected hook can recover after retries");
  pending->globals.push_back({nullptr});
  Settle(vm, "RetryType");
  pending->globals.back().func = std::make_shared<RE::BSScript::IFunction>();
  Settle(vm, "RetryType");
  Check(pending->globals.back().func->patched,
        "null table entry never creates completed memo; later fill is inspected");

  auto empty = std::make_shared<Type>(); empty->globals.clear();
  vm.types["EmptyType"] = empty;
  Settle(vm, "EmptyType");
  empty->globals.emplace_back();
  Settle(vm, "EmptyType");
  Check(empty->globals.front().func->patched, "empty type can later gain globals");

  std::size_t count = 0;
  const auto prior = InspectScriptTypeGlobals(&vm, replacement, count, true);
  ResetScriptTypeInspectionMemo();
  CompleteScriptTypeInspection(prior.stamp);
  Check(InspectScriptTypeGlobals(&vm, replacement, count).needsPostLink,
        "old completion cannot repopulate reset memo");
  ResetScriptTypeInspectionMemo();
  VM secondVM;
  const auto firstVMResult = InspectScriptTypeGlobals(&vm, replacement, count);
  CompleteScriptTypeInspection(firstVMResult.stamp);
  Check(InspectScriptTypeGlobals(&secondVM, replacement, count).needsPostLink,
        "memo completion is VM-specific");
  ResetScriptTypeInspectionMemo();
  unsigned callbacks = 0;
  beforePatch = [&] {
    std::unique_lock lock(g_scriptTypeMemoMutex, std::try_to_lock);
    Check(lock.owns_lock(), "native inspection executes outside memo lock");
    lock.unlock();
    if (++callbacks == 1) ResetScriptTypeInspectionMemo();
  };
  const auto interrupted = InspectScriptTypeGlobals(&vm, replacement, count);
  beforePatch = {};
  CompleteScriptTypeInspection(interrupted.stamp);
  Check(InspectScriptTypeGlobals(&vm, replacement, count).needsPostLink,
        "reset during scan does not publish stale completed entry");

  // Bounded ownership and evictions: never grow a script-type retention map.
  ResetScriptTypeInspectionMemo();
  std::vector<std::weak_ptr<Type>> lifetime;
  for (unsigned n=0; n<4096; ++n) {
    auto transient = std::make_shared<Type>();
    lifetime.emplace_back(transient);
    const auto result = InspectScriptTypeGlobals(&vm, transient, count);
    CompleteScriptTypeInspection(result.stamp);
  }
  const auto retained = std::count_if(lifetime.begin(), lifetime.end(),
                                    [](const auto& p){ return !p.expired(); });
  Check(retained > 0 && retained <= kScriptTypeInspectionCacheSize,
        "4096 types retain at most 128 owners; collisions release old entries");
  ResetScriptTypeInspectionMemo();
  Check(std::ranges::all_of(lifetime, [](const auto& p){ return p.expired(); }),
        "load/reset releases every memo-owned type reference");
  std::puts("Papyrus inspection regressions passed: first call, late binds, replacement, retries, P+, bounded ownership/reset.");
}
