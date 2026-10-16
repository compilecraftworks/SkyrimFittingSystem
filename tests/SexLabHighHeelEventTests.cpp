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
struct TESObjectARMO : TESForm {
  using TESForm::TESForm;
  struct Mask { std::uint32_t bits{}; std::uint32_t underlying() const { return bits; } } slots;
  Mask GetSlotMask() const { return slots; }
};
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
  bool GetPropertyValue(std::shared_ptr<Object>& object, const char* name, Variable& out) {
    auto* value=object->GetProperty(name); if(!value) return false; out=*value; return true;
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
std::unordered_set<RE::FormID> g_registeredAppearanceHighHeelActors;
std::unordered_map<RE::FormID, std::vector<int>> g_registeredAppearanceAttachmentRoots;
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
  Check(queued.contains(player.id) && g_highHeelScenes.Contains(player.id) &&
        !g_highHeelScenes.ShouldSuppress(player.id,true) && g_highHeelScenes.ShouldSuppress(player.id,false),
        "option OFF still tracks scene: retained actual footwear keeps height; stripped feet suppress registered height");
  send("AnimationEnd", &thread); SKSE::tasks.Drain(); queued.clear();
  vm.config->option.value = true;
  for (auto* sender : std::vector<RE::TESForm*>{nullptr, &player, &foreign}) {
    send("AnimationStart", sender); SKSE::tasks.Drain();
    Check(queued.empty(), "foreign/non-quest/no-sender events cannot activate compensation");
  }
  send("UnrelatedEvent", &thread); Check(SKSE::tasks.pending.empty(), "unrelated mod events enqueue no work");
  foreign.file.fileName="OStim.esp";send("AnimationStart",&foreign);SKSE::tasks.Drain();
  Check(queued.empty(),"OStim's independent heel policy cannot be enrolled as a SexLab scene");
  RE::TESDataHandler::installed = false; send("AnimationStart", &thread); SKSE::tasks.Drain();
  Check(queued.empty(), "absent SexLab cannot activate compensation"); RE::TESDataHandler::installed = true;
  send("AnimationStart", &thread); ++g_highHeelQueueEpoch; SKSE::tasks.Drain();
  Check(queued.empty(), "load/revert invalidates an old queued scene event");
  send("AnimationStart", &thread); SKSE::tasks.Drain(); queued.clear();
  vm.config->option.value = false; send("StageStart", &thread); SKSE::tasks.Drain();
  Check(queued.contains(player.id) && g_highHeelScenes.Contains(player.id) && !g_highHeelScenes.ShouldSuppress(player.id,true), "option disabled during scene keeps membership but reconciles heel policy");
  queued.clear(); vm.config->propertyAvailable=false; send("StageStart", &thread); SKSE::tasks.Drain();
  Check(g_highHeelScenes.ShouldSuppress(player.id,false) && !g_highHeelScenes.ShouldSuppress(player.id,true),"unreadable option never blocks barefoot alignment or flips known OFF policy");
  send("AnimationEnd", &thread); SKSE::tasks.Drain();queued.clear();
  vm.bound=false;send("AnimationStart", &thread);SKSE::tasks.Drain();
  Check(g_highHeelScenes.Contains(player.id) && g_highHeelScenes.ShouldSuppress(player.id,false) && !g_highHeelScenes.ShouldSuppress(player.id,true),"temporarily unbound config cannot lose new barefoot scene membership or invent an ON option");
  vm.bound=true;
  vm.config->propertyAvailable=true;vm.config->option.value=true;send("StageStart", &thread);SKSE::tasks.Drain();
  vm.config->propertyAvailable=false;send("StageStart", &thread);SKSE::tasks.Drain();
  Check(g_highHeelScenes.ShouldSuppress(player.id,true),"transient property read failure cannot turn a known ON scene back into OFF");
  RE::TESObjectARMO boots{0x300}, helmet{0x301};boots.slots.bits=0x80;queued.clear();
  sfs::native::racemenu::ObserveHighHeelEquipmentEvent(&player,&boots);
  Check(queued.contains(player.id),"physical feet strip/redress requeues scene height without waiting for another stage");
  queued.clear();sfs::native::racemenu::ObserveHighHeelEquipmentEvent(&player,&helmet);
  Check(queued.empty(),"non-foot equipment cannot broaden heel-event work");
  send("AnimationEnd", &thread); SKSE::tasks.Drain();queued.clear();
  sfs::native::racemenu::ObserveHighHeelEquipmentEvent(&player,&boots);
  Check(queued.empty(),"ended scene cannot retain foot-event work");
  std::printf("%u production SexLab event adapter checks passed; not in-game proof.\n", checks);
}
