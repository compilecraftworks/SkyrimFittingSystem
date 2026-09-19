// Actual SFS public/legacy sync, callback chain, queue and attachment observer.
// Recording providers model only NPC-position/BNDT semantics from 9ebcb733.
// This is NOT the RaceMenu binary, renderer emulation or in-game proof.
#include <algorithm>
#include <any>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "native/RegisteredAppearanceMorphRules.h"
namespace RE {
using FormID = std::uint32_t;
template<class T> using BSTSmartPointer = std::shared_ptr<T>;
namespace BSScript {
struct Object {};
struct Variable {
  std::optional<bool> value;
  bool IsBool() const { return value.has_value(); }
  bool GetBool() const { return value.value(); }
};
struct IStackCallbackFunctor {
  virtual ~IStackCallbackFunctor() = default;
  virtual void operator()(Variable) = 0;
  virtual void SetObject(const BSTSmartPointer<Object>&) = 0;
};
namespace Internal {
struct VirtualMachine {
  std::vector<std::function<void()>> pending;
  std::vector<std::string> calls;
  std::string fail;
  static VirtualMachine* GetSingleton();
  bool DispatchStaticCall(const char*, const char*, std::vector<std::any>,
                          BSTSmartPointer<IStackCallbackFunctor>);
  void Drain() {
    auto batch = std::move(pending);
    pending.clear();
    for (auto& f : batch) f();
  }
} vm;
}
}
template<class... Args> std::vector<std::any> MakeFunctionArguments(Args&&... args) {
  return {std::any(std::forward<Args>(args))...};
}
template<class T> struct NiPointer {
  T* value{};
  NiPointer(T* p) : value(p) {}
  T* get() const { return value; }
};
struct NiAVObject { bool hasHeel{}; };
using NiNode = NiAVObject;
struct TESForm {
  static inline std::unordered_map<FormID, TESForm*> forms;
  FormID id;
  explicit TESForm(FormID v) : id(v) { forms[v] = this; }
  FormID GetFormID() const { return id; }
  template<class T> static T* LookupByID(FormID v) {
    const auto it = forms.find(v);
    return it == forms.end() ? nullptr : static_cast<T*>(it->second);
  }
};
struct TESObjectREFR : TESForm {
  using TESForm::TESForm;
  template<class T> T* As() { return static_cast<T*>(this); }
};
struct TESObjectARMO : TESForm { using TESForm::TESForm; };
struct TESObjectARMA : TESForm { using TESForm::TESForm; };
struct NPC { bool IsFemale() const { return true; } };
struct Actor : TESObjectREFR {
  explicit Actor(FormID id) : TESObjectREFR(id) {}
  NPC base;
  bool loaded{true}, equippableTransforms{true}, stale{}, active{true};
  bool bndtExists{}, bndtContainsNpc{}, temporary{};
  std::optional<float> selectedOffset{12.0f}, internalPosition;
  std::vector<float> sceneOffsets;
  std::unordered_set<FormID> displayed;
  float displayedHeight{}, otherModHeight{};
  unsigned fullUpdates{}, resolverCalls{};
  bool Is3DLoaded() const { return loaded; }
  NPC* GetActorBase() { return &base; }
  Actor* Get3D(bool) { return this; }
};
}
namespace logger {
template<class... T> void warn(T&&...) {}
template<class... T> void debug(T&&...) {}
}
namespace SKSE {
struct Tasks {
  std::vector<std::function<void()>> pending;
  template<class F> void AddTask(F f) { pending.emplace_back(std::move(f)); }
  void Drain() {
    auto batch = std::move(pending);
    pending.clear();
    for (auto& f : batch) f();
  }
} tasks;
Tasks* GetTaskInterface() { return &tasks; }
}
namespace skee {
struct INiTransformInterface {
  struct Position { float x{}, y{}, z{}; };
  bool HasNodeTransformPosition(RE::Actor* a, bool, bool, const char*, const char*) {
    return a->internalPosition.has_value();
  }
  void AddNodeTransformPosition(RE::Actor* a, bool, bool, const char*, const char* key, Position p) {
    if (std::string_view(key) == "internal") a->internalPosition = p.z;
    else a->temporary = true;
  }
  void UpdateNodeAllTransforms(RE::Actor* a) {
    ++a->fullUpdates;
    // SetTransforms scans all objects, writes each HH to the same internal
    // NPC position, updates BNDT only if already present. SDTA and unrelated
    // rotations/scales/first person are outside this deliberately small model.
    if (a->equippableTransforms) {
      for (float offset : a->sceneOffsets) a->internalPosition = offset;
      if (a->bndtExists && !a->sceneOffsets.empty()) a->bndtContainsNpc = true;
    }
    a->displayedHeight = a->internalPosition.value_or(0.0f) + a->otherModHeight;
  }
  void RemoveNodeTransformPosition(RE::Actor* a, bool, bool, const char*, const char* key) {
    if (std::string_view(key) == "internal") a->internalPosition.reset();
    else a->temporary = false;
  }
  void UpdateNodeTransforms(RE::Actor* a, bool, bool, const char*) {
    a->displayedHeight = a->internalPosition.value_or(0.0f) + a->otherModHeight;
  }
} transform;
struct IBodyMorphInterface {} morph;
struct IAddonAttachmentInterface {
  virtual void OnAttach(RE::TESObjectREFR*, RE::TESObjectARMO*, RE::TESObjectARMA*,
                        RE::NiAVObject*, bool, RE::NiNode*, RE::NiNode*) = 0;
};
}
RE::BSScript::Internal::VirtualMachine*
RE::BSScript::Internal::VirtualMachine::GetSingleton() { return &vm; }
bool RE::BSScript::Internal::VirtualMachine::DispatchStaticCall(
    const char* script, const char* method, std::vector<std::any> args,
    BSTSmartPointer<IStackCallbackFunctor> callback) {
  if (std::string_view(script) != "NiOverride") std::abort();
  calls.emplace_back(method);
  if (fail == method) return false;
  pending.emplace_back([args = std::move(args), name = std::string(method), callback] {
    auto* a = std::any_cast<RE::Actor*>(args.at(0));
    Variable result{};
    if (name == "UpdateAllReferenceTransforms") {
      skee::transform.UpdateNodeAllTransforms(a);
    } else {
      if (std::any_cast<bool>(args.at(1)) || !std::any_cast<bool>(args.at(2)) ||
          std::any_cast<std::string>(args.at(3)) != "NPC") std::abort();
      if (name == "AddNodeTransformScale") {
        if (std::any_cast<float>(args.at(5)) != 1.0F) std::abort();
        a->temporary = true;
      } else if (name == "HasNodeTransformPosition") {
        result.value = a->internalPosition.has_value();
      } else if (name == "AddNodeTransformPosition") {
        const auto p = std::any_cast<std::vector<float>>(args.at(5));
        if (p.size() != 3 || p[0] != 0 || p[1] != 0 ||
            std::any_cast<std::string>(args.at(4)) != "internal") std::abort();
        a->internalPosition = p[2];
      } else if (name == "RemoveNodeTransformScale") {
        a->temporary = false;
      } else if (name == "RemoveNodeTransformPosition") {
        a->internalPosition.reset();
      } else if (name == "UpdateNodeTransform") {
        skee::transform.UpdateNodeTransforms(a, false, true, "NPC");
      } else std::abort();
    }
    (*callback)(result);
  });
  return true;
}
#include "callback.production.inc"
struct RegisteredHighHeelState {
  std::optional<float> offset;
  bool staleAttachmentStillPresent{}, previouslyActive{};
};
std::mutex g_nodeMutex, g_highHeelQueueMutex;
std::unordered_set<RE::FormID> g_registeredAppearanceHighHeelActors;
std::unordered_set<RE::FormID> g_highHeelAttachmentActors;
std::unordered_set<RE::FormID> g_queuedHighHeelSyncs, g_pendingHighHeelResyncs;
std::atomic_uint64_t g_highHeelQueueGeneration{1};
std::atomic<skee::INiTransformInterface*> g_transformInterface{&skee::transform};
std::atomic_uint32_t g_transformInterfaceVersion{3};
std::atomic<skee::IBodyMorphInterface*> g_bodyMorphInterface{&skee::morph};
std::uint64_t g_nodeObservation{};
struct RegisteredAppearanceNode {
  RE::NiPointer<RE::NiAVObject> object;
  RE::FormID armorFormID, addonFormID;
  bool firstPerson;
  std::uint64_t observation{};
  unsigned detachedChecks{};
  bool operator==(const RegisteredAppearanceNode& b) const {
    return object.get() == b.object.get() && armorFormID == b.armorFormID &&
           addonFormID == b.addonFormID && firstPerson == b.firstPerson;
  }
};
std::unordered_map<RE::FormID, std::vector<RegisteredAppearanceNode>> g_registeredAppearanceNodes;
std::unordered_map<RE::FormID, std::vector<int>> g_registeredAppearanceAttachmentRoots;
RegisteredHighHeelState ResolveRegisteredHighHeelState(RE::Actor* a) {
  ++a->resolverCalls;
  return {a->selectedOffset, a->stale, g_registeredAppearanceHighHeelActors.contains(a->id)};
}
bool SceneHasNpcPositionSource(RE::Actor* a) { return !a->sceneOffsets.empty(); }
bool IsRegisteredAppearanceDisplayActive(RE::FormID id) { return RE::TESForm::LookupByID<RE::Actor>(id)->active; }
bool RememberHighHeelAttachmentRoots(RE::Actor*,
    const std::vector<RE::NiPointer<RE::NiAVObject>>& nodes, RE::FormID, bool fp) {
  return !fp && std::ranges::any_of(nodes, [](const auto& n) { return n.get()->hasHeel; });
}
void QueuePendingMorphSync(RE::FormID) {}
namespace sfs::native {
bool IsDisplayedFittingArmor(RE::Actor* a, const RE::TESObjectARMO* armor) { return a->displayed.contains(armor->id); }
namespace dye {
unsigned requests{};
void QueueSavedWorldTintRestore(RE::Actor*) { ++requests; }
}
namespace racemenu { void QueueRegisteredAppearanceHighHeelSync(RE::Actor*); }
}
#include "sync.production.inc"
#include "observer.production.inc"
namespace sfs::native::racemenu {
#include "queue.production.inc"
}
unsigned checks{};
void Check(bool result, const char* message) {
  if (!result) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
  ++checks;
  std::printf("PASS: %s\n", message);
}
void Reset(RE::Actor& a) {
  a = RE::Actor{a.id};
  RE::TESForm::forms[a.id] = &a;
  g_registeredAppearanceHighHeelActors.clear();
  g_highHeelAttachmentActors = {a.id};
  g_queuedHighHeelSyncs.clear(); g_pendingHighHeelResyncs.clear();
  SKSE::tasks.pending.clear();
  RE::BSScript::Internal::vm.pending.clear();
  RE::BSScript::Internal::vm.calls.clear();
  RE::BSScript::Internal::vm.fail.clear();
  g_registeredAppearanceAttachmentRoots.clear();
  g_registeredAppearanceNodes.clear();
  sfs::native::dye::requests = 0;
}
void DrainAll() {
  auto& vm = RE::BSScript::Internal::vm;
  for (int i = 0; i != 32 && (!SKSE::tasks.pending.empty() || !vm.pending.empty()); ++i) {
    SKSE::tasks.Drain(); vm.Drain();
  }
  Check(SKSE::tasks.pending.empty() && vm.pending.empty(), "actor-local work terminates without polling");
}
void LaterAttachment(RE::Actor& a, RE::TESObjectARMO& armor, bool heel) {
  // SkeletonExtender::AddTransforms: current/previous symmetric difference
  // removes internal. Only the newly attached object's transforms are reapplied.
  const bool currentNpc = !a.sceneOffsets.empty();
  const bool previousNpc = a.bndtExists && a.bndtContainsNpc;
  if (currentNpc != previousNpc) a.internalPosition.reset();
  if (heel) a.internalPosition = 12.0f;
  a.displayedHeight = a.internalPosition.value_or(0.0f) + a.otherModHeight;
  a.bndtExists = a.bndtExists || currentNpc;
  a.bndtContainsNpc = currentNpc;
  RE::TESObjectARMA addon{0xF000}; RE::NiAVObject object{heel};
  RegisteredAppearanceAttachmentObserver observer;
  observer.OnAttach(&a, &armor, &addon, &object, false, nullptr, nullptr);
}
int main() {
  RE::Actor a{0x14}; RE::TESObjectARMO ordinary{0x800}, registered{0x900};
  const auto queue = [&] { sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&a); };
  // Version routing is independent of backend selection. All three backends
  // share this production queue/observer; no synthetic backend label is proof
  // that the game's own attachment pass has been executed.
  for (auto version : {1U, 2U, 3U, 4U, 99U}) {
    g_transformInterfaceVersion = version;
    g_transformInterface = version < 3 ? nullptr : &skee::transform;
    std::printf("NiTransform interface route %u\n", version);
    Reset(a); a.sceneOffsets = {12.0f}; queue(); DrainAll();
    Check(a.displayedHeight == 12.0f && !a.temporary && !a.bndtExists,
          "HH=12 applies once with no retained bootstrap");
    Check(a.fullUpdates == 1, "success ends immediately without mandatory follow-up frames");
    LaterAttachment(a, ordinary, false);
    Check(a.displayedHeight == 0.0f && SKSE::tasks.pending.size() == 1,
          "ordinary late attachment clearing internal queues repair");
    Check(sfs::native::dye::requests == 0 && g_registeredAppearanceNodes.empty(),
          "ordinary gear does not enter dye or morph consumers");
    DrainAll();
    Check(a.displayedHeight == 12.0f, "late ordinary attachment recovers selected height");
    Reset(a); a.sceneOffsets = {12.0f}; a.displayed.insert(registered.id);
    queue(); DrainAll(); LaterAttachment(a, registered, false); DrainAll();
    Check(a.displayedHeight == 12.0f, "registered non-heel late attachment also recovers");
    Check(sfs::native::dye::requests == 1 && g_registeredAppearanceNodes[a.id].size() == 1,
          "registered attachment keeps dye and live-morph enrollment independent of HH repair");
    Reset(a); a.sceneOffsets = {12.0f}; a.displayed.insert(registered.id);
    LaterAttachment(a, registered, true); DrainAll();
    Check(a.displayedHeight == 12.0f, "registered heel callback starts first synchronization");
    for (const auto& offsets : {std::vector<float>{12, 0}, std::vector<float>{0, 12}}) {
      Reset(a); a.sceneOffsets = offsets; a.otherModHeight = 3; queue(); DrainAll();
      Check(a.displayedHeight == 15 && !a.temporary,
            "registered selection wins over scene order without doubling or erasing named mod offset");
    }
    Reset(a); a.selectedOffset = 0.0F; a.sceneOffsets = {12, 0}; queue(); DrainAll();
    Check(a.displayedHeight == 0 && g_registeredAppearanceHighHeelActors.contains(a.id),
          "zero is a valid selected HH value, not missing data");
    Reset(a); a.sceneOffsets = {12}; a.equippableTransforms = false; queue(); DrainAll();
    Check(a.displayedHeight == 0 && !a.temporary && !g_registeredAppearanceHighHeelActors.contains(a.id),
          "does not manufacture automatic transforms when RaceMenu declines them");
    Reset(a); a.selectedOffset.reset(); a.stale = true;
    g_registeredAppearanceHighHeelActors.insert(a.id); queue(); DrainAll();
    Check(a.resolverCalls == 3 && a.fullUpdates == 0, "stale visible branch retries boundedly without lowering");
    Reset(a); a.sceneOffsets = {12}; queue(); queue(); DrainAll();
    Check(a.fullUpdates == 2, "pending event coalesces into one subsequent pass");
    Reset(a); a.sceneOffsets = {12}; queue(); ++g_highHeelQueueGeneration; DrainAll();
    Check(a.fullUpdates == 0, "save generation invalidates stale queued work");
    Reset(a); a.selectedOffset.reset(); a.internalPosition = 12.0F; a.otherModHeight = 3;
    g_registeredAppearanceHighHeelActors.insert(a.id); queue(); DrainAll();
    Check(a.displayedHeight == 3 && !g_registeredAppearanceHighHeelActors.contains(a.id),
          "last registered heel cleanup preserves named mod height");
    Reset(a); a.selectedOffset.reset(); a.internalPosition = 12.0F; a.sceneOffsets = {7};
    g_registeredAppearanceHighHeelActors.insert(a.id); queue(); DrainAll();
    Check(a.displayedHeight == 7, "real equipped heel survives registered-heel removal");
    Reset(a); a.sceneOffsets = {12}; queue(); DrainAll();
    a.selectedOffset.reset(); a.sceneOffsets.clear(); g_highHeelAttachmentActors.clear();
    queue(); DrainAll();
    Check(a.displayedHeight == 0 && !a.temporary && !g_registeredAppearanceHighHeelActors.contains(a.id),
          "strip/hide transition clears registered heel without retaining a source");
    for (int idle = 0; idle != 8; ++idle) SKSE::tasks.Drain();
    Check(a.displayedHeight == 0 && SKSE::tasks.pending.empty(),
          "redress-off stays lowered with no automatic restoration/polling");
    a.selectedOffset = 12.0F; a.sceneOffsets = {12}; g_highHeelAttachmentActors.insert(a.id);
    queue(); DrainAll();
    Check(a.displayedHeight == 12, "redress or manual visibility restoration reapplies registered heel");
    Reset(a); a.sceneOffsets = {12}; queue(); DrainAll();
    g_highHeelAttachmentActors.clear(); LaterAttachment(a, ordinary, false);
    Check(SKSE::tasks.pending.empty(), "replacement previews do not broaden HH attachment scope");
    Check(!ShouldResyncHighHeelAfterAttachment(&a, true) &&
          !ShouldResyncHighHeelAfterAttachment(nullptr, false), "first-person/null callbacks do not rearm third-person height");
    Reset(a); a.selectedOffset.reset(); queue(); DrainAll();
    Check(a.fullUpdates == 0, "actor without registered heels performs no transform update");
  }
  // Legacy callback chain cancellation/failure must not publish stale success.
  g_transformInterfaceVersion = 2; g_transformInterface = nullptr;
  auto& vm = RE::BSScript::Internal::vm;
  Reset(a); a.sceneOffsets = {12}; queue(); SKSE::tasks.Drain();
  vm.Drain(); // Add neutral scale completed, full scan is queued.
  a.selectedOffset = 7.0F; a.sceneOffsets = {12, 0}; queue(); DrainAll();
  Check(a.displayedHeight == 7, "pending refresh resolves edited appearance on game task after legacy chain");
  for (const auto* failure : {"UpdateAllReferenceTransforms", "HasNodeTransformPosition", "AddNodeTransformPosition", "UpdateNodeTransform"}) {
    Reset(a); a.sceneOffsets = {12}; vm.fail = failure; queue(); DrainAll();
    Check(!a.temporary && !g_registeredAppearanceHighHeelActors.contains(a.id),
          "legacy dispatch failure removes neutral bootstrap and does not report success");
  }
  Reset(a); a.sceneOffsets = {12}; queue(); SKSE::tasks.Drain();
  ++g_highHeelQueueGeneration; DrainAll();
  Check(a.fullUpdates == 0 && !g_registeredAppearanceHighHeelActors.contains(a.id),
        "old legacy callbacks cannot continue into a new load generation");
  std::printf("%u high-heel source regression checks passed; NOT in-game evidence.\n", checks);
}
