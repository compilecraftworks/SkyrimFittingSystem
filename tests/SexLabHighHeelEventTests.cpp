// Compile the unchanged event adapter. Only engine/VM/task boundaries are fakes;
// tests do not claim to execute a SexLab DLL or real game-version alias layout.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "native/HighHeelSceneRules.h"
namespace RE {
using FormID = std::uint32_t;
using BSFixedString = std::string;
template<class T> using BSTSmartPointer = std::shared_ptr<T>;
struct TESForm {
  static inline std::unordered_map<FormID, TESForm*> forms;
  FormID id;
  explicit TESForm(FormID v) : id(v) { forms[v] = this; }
  virtual ~TESForm() = default;
  FormID GetFormID() const { return id; }
  int GetFormType() const { return 77; }
  template<class T> static T* LookupByID(FormID id) {
    const auto it = forms.find(id);
    return it == forms.end() ? nullptr : dynamic_cast<T*>(it->second);
  }
};
struct Actor : TESForm { using TESForm::TESForm; };
struct TESFile { const char* fileName = "SexLab.esm"; };
struct BGSBaseAlias { int type = 139; int GetVMTypeID() const { return type; } };
struct BGSRefAlias : BGSBaseAlias {
  static constexpr int VMTYPEID = 140;
  Actor* actor;
  explicit BGSRefAlias(Actor* a) : actor(a) { type = VMTYPEID; }
  Actor* GetActorReference() const { return actor; }
};
struct TESQuest : TESForm {
  using TESForm::TESForm;
  TESFile file;
  std::vector<BGSBaseAlias*> aliases;
  TESFile* GetFile(int) { return &file; }
};
struct TESDataHandler {
  static inline bool installed = true;
  static TESDataHandler* GetSingleton() { static TESDataHandler d; return &d; }
  template<class T> T* LookupForm(FormID id, const char*) { return installed ? TESForm::LookupByID<T>(id) : nullptr; }
};
namespace BSScript {
struct Variable {
  std::optional<bool> value{true};
  bool IsBool() const { return value.has_value(); }
  bool GetBool() const { return value.value(); }
};
struct Object {
  Variable option;
  bool propertyAvailable = true;
  Variable* GetProperty(const BSFixedString& name) {
    return propertyAvailable && name == "RemoveHeelEffect" ? &option : nullptr;
  }
};
namespace Internal {
struct VirtualMachine {
  struct Policy { std::uintptr_t GetHandleForObject(int, void* p) { return reinterpret_cast<std::uintptr_t>(p); } } policy;
  std::shared_ptr<Object> config = std::make_shared<Object>();
  bool bound = true;
  static VirtualMachine* GetSingleton() { static VirtualMachine vm; return &vm; }
  Policy* GetObjectHandlePolicy() { return &policy; }
  bool FindBoundObject(std::uintptr_t, const char* name, std::shared_ptr<Object>& out) {
    if (!bound || std::string_view(name) != "sslSystemConfig") return false;
    out = config; return true;
  }
};
}
}
}
namespace SKSE {
struct ModCallbackEvent { std::string eventName; RE::TESForm* sender{}; };
struct Tasks {
  std::vector<std::function<void()>> pending;
  void AddTask(std::function<void()> f) { pending.push_back(std::move(f)); }
  void Drain() { auto work = std::move(pending); pending.clear(); for (auto& f : work) f(); }
} tasks;
Tasks* GetTaskInterface() { return &tasks; }
}
std::atomic_uint64_t g_highHeelQueueEpoch{};
std::mutex g_nodeMutex;
sfs::native::racemenu::rules::HighHeelScenes g_highHeelScenes;
std::unordered_set<RE::FormID> queued;
namespace sfs::native::racemenu {
void QueueRegisteredAppearanceHighHeelSync(RE::Actor* a) { queued.insert(a->id); }
#include "scene-event.production.inc"
}
unsigned checks{};
void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
  ++checks;
}
int main() {
  RE::Actor player{0x14}, npc{0x20};
  RE::TESQuest config{0xD62}, thread{0x100}, foreign{0x200}; foreign.file.fileName = "Other.esp";
  RE::BGSRefAlias playerAlias{&player}, npcAlias{&npc}, emptyAlias{nullptr}; RE::BGSBaseAlias location;
  thread.aliases = {&playerAlias, &npcAlias, &emptyAlias, &location, nullptr};
  auto send = [&](const char* name, RE::TESForm* owner) {
    sfs::native::racemenu::ObserveHighHeelSceneEvent({name, owner});
  };
  auto& vm = *RE::BSScript::Internal::VirtualMachine::GetSingleton();
  for (const char* name : {"AnimationStart", "StageStart", "AnimationChange", "PositionChange", "ActorChangeEnd"}) {
    send(name, &thread); Check(queued.empty(), "event work is deferred to game task");
    SKSE::tasks.Drain();
    Check(g_highHeelScenes.Contains(player.id) && g_highHeelScenes.Contains(npc.id) && queued.size() == 2,
          "ordinary SexLab/P+ event observes all actor aliases, not locations or nulls");
    queued.clear(); send("AnimationEnd", &thread); SKSE::tasks.Drain(); queued.clear();
  }
  send("AnimationStart", &thread); SKSE::tasks.Drain(); queued.clear();
  thread.aliases.clear(); send("AnimationEnd", &thread); SKSE::tasks.Drain();
  Check(!g_highHeelScenes.Contains(player.id) && queued.size() == 2, "end after alias clearing still releases both cached scene actors");
  queued.clear(); thread.aliases = {&playerAlias}; vm.config->option.value = false;
  send("AnimationStart", &thread); SKSE::tasks.Drain();
  Check(queued.empty() && !g_highHeelScenes.Contains(player.id), "RemoveHeelEffect OFF leaves ordinary heel behavior alone");
  vm.config->option.value = true;
  for (auto* sender : std::vector<RE::TESForm*>{nullptr, &player, &foreign}) {
    send("AnimationStart", sender); SKSE::tasks.Drain();
    Check(queued.empty(), "foreign/non-quest/no-sender events cannot activate compensation");
  }
  send("UnrelatedEvent", &thread); Check(SKSE::tasks.pending.empty(), "unrelated mod events enqueue no work");
  RE::TESDataHandler::installed = false; send("AnimationStart", &thread); SKSE::tasks.Drain();
  Check(queued.empty(), "absent SexLab cannot activate compensation"); RE::TESDataHandler::installed = true;
  send("AnimationStart", &thread); ++g_highHeelQueueEpoch; SKSE::tasks.Drain();
  Check(queued.empty(), "load/revert invalidates an old queued scene event");
  send("AnimationStart", &thread); SKSE::tasks.Drain(); queued.clear();
  vm.config->option.value = false; send("StageStart", &thread); SKSE::tasks.Drain();
  Check(queued.contains(player.id) && !g_highHeelScenes.Contains(player.id), "option disabled during scene reconciles the previously owned actor");
  std::printf("%u production SexLab event adapter checks passed; not in-game proof.\n", checks);
}
