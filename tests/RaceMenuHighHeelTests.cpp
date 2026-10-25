// Actual SFS public/legacy sync, callback chain, queue and attachment observer.
// Recording providers model only NPC-position/BNDT semantics from 9ebcb733.
// This is NOT the RaceMenu binary, renderer emulation or in-game proof.
#include <algorithm>
#include <array>
#include <cmath>
#include <any>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "native/RegisteredAppearanceMorphRules.h"
#include "native/ActorResourceWork.h"
#include "native/HighHeelSceneRules.h"
namespace RE {
using FormID = std::uint32_t;
template<class T> using BSTSmartPointer = std::shared_ptr<T>;
namespace BSScript {
struct Object {};
struct Variable {
  std::any argument;
  std::optional<bool> value;
  std::optional<std::vector<float>> floats;
  bool IsArray() const { return floats.has_value(); }
  template<class T> T Unpack() const {
    if constexpr (std::is_same_v<T, std::vector<float>>) { return *floats; }
    else { return std::any_cast<T>(argument); }
  }
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
  std::string invalidResult;
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
  NiPointer(T* p = nullptr) : value(p) {}
  T* get() const { return value; }
  T* operator->() const { return value; }
  explicit operator bool() const { return value != nullptr; }
};
struct NiAVObject {
  bool hasHeel{};
  struct { struct { float z{}; } translate; } local;
};
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
struct TESObjectARMO : TESForm {
  using TESForm::TESForm;
  struct Mask { std::uint32_t bits{}; std::uint32_t underlying() const { return bits; } } slots;
  Mask GetSlotMask() const { return slots; }
};
struct TESObjectARMA : TESForm { using TESForm::TESForm; };
struct NPC { bool IsFemale() const { return true; } };
struct Actor : TESObjectREFR {
  explicit Actor(FormID id) : TESObjectREFR(id) {}
  NPC base;
  bool loaded{true}, equippableTransforms{true}, stale{}, active{true};
  bool bndtExists{}, bndtContainsNpc{}, temporary{};
  struct Inventory {
    TESObjectARMO* feet{};
    TESObjectARMO* GetArmorInSlot(int slot) { if(slot != 37) std::abort(); return feet; }
  } inventory;
  bool inventoryAvailable{true};
  Inventory* GetInventoryChanges(bool noInit) { if(!noInit) std::abort(); ++inventoryReads; return inventoryAvailable ? &inventory : nullptr; }
  std::optional<float> selectedOffset{12.0f}, internalPosition, sexLabPosition;
  std::array<float, 2> internalXY{};
  std::vector<float> sceneOffsets;
  std::vector<std::array<float, 3>> scenePositions;
  std::unordered_set<FormID> displayed, actualVisible;
  float displayedHeight{}, otherModHeight{};
  unsigned fullUpdates{}, resolverCalls{}, inventoryReads{};
  unsigned ignoredPositionWrites{}, rescanAfterUpdates{}, positionReads{}, nodeUpdates{};
  std::function<void(std::string_view)> afterPositionRead;
  bool Is3DLoaded() const { return loaded; }
  NPC* GetActorBase() { return &base; }
  Actor* Get3D(bool) { return this; }
  NiAVObject heightNode;
  NiAVObject* GetObjectByName(const char*) { return &heightNode; }
  void SetDisplayedHeight(float value) { displayedHeight = value; heightNode.local.translate.z = value; }
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
  bool HasNodeTransformPosition(RE::Actor* a, bool, bool, const char*, const char* key) {
    if (std::string_view(key) == "SexLab.esm") return a->sexLabPosition.has_value();
    return a->internalPosition.has_value();
  }
  Position GetNodeTransformPosition(RE::Actor* a, bool, bool, const char*, const char* key) {
    ++a->positionReads;
    const Position result = std::string_view(key) == "SexLab.esm"
        ? Position{0, 0, a->sexLabPosition.value_or(0)}
        : Position{a->internalXY[0], a->internalXY[1], a->internalPosition.value_or(0)};
    if (a->afterPositionRead) a->afterPositionRead(key);
    return result;
  }
  void AddNodeTransformPosition(RE::Actor* a, bool, bool, const char*, const char* key, Position p) {
    if (std::string_view(key) == "internal") {
      if (a->ignoredPositionWrites) { --a->ignoredPositionWrites; return; }
      a->internalPosition = p.z; a->internalXY = {p.x, p.y};
    }
    else a->temporary = true;
  }
  void UpdateNodeAllTransforms(RE::Actor* a) {
    ++a->fullUpdates;
    // The test supplies expected scan results independently of SFS's metadata
    // parser. Position cases can model SDTA X/Y as well as HH's Z. Unrelated
    // rotations/scales/first person are outside this deliberately small model.
    if (a->equippableTransforms) {
      for (float offset : a->sceneOffsets) a->internalPosition = offset;
      for (const auto& position : a->scenePositions) {
        a->internalXY = {position[0], position[1]}; a->internalPosition = position[2];
      }
      if (a->bndtExists && !a->sceneOffsets.empty()) a->bndtContainsNpc = true;
    }
    a->SetDisplayedHeight(a->internalPosition.value_or(0.0f) + a->otherModHeight + a->sexLabPosition.value_or(0));
  }
  void RemoveNodeTransformPosition(RE::Actor* a, bool, bool, const char*, const char* key) {
    if (std::string_view(key) == "internal") a->internalPosition.reset();
    else a->temporary = false;
  }
  void UpdateNodeTransforms(RE::Actor* a, bool, bool, const char*) {
    ++a->nodeUpdates;
    a->SetDisplayedHeight(a->internalPosition.value_or(0.0f) + a->otherModHeight + a->sexLabPosition.value_or(0));
    // A full rescan can interleave after the write/update and before SFS records
    // its baseline. Deliberately send no attachment/position-native notification.
    if (a->rescanAfterUpdates) { --a->rescanAfterUpdates; UpdateNodeAllTransforms(a); }
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
  pending.emplace_back([this, args = std::move(args), name = std::string(method), callback] {
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
      } else if (name == "GetNodeTransformPosition") {
        const auto key = std::any_cast<std::string>(args.at(4));
        const auto p = skee::transform.GetNodeTransformPosition(a, false, true, "NPC", key.c_str());
        result.floats = std::vector<float>{p.x, p.y, p.z};
      } else if (name == "AddNodeTransformPosition") {
        const auto p = std::any_cast<std::vector<float>>(args.at(5));
        if (p.size() != 3 ||
            std::any_cast<std::string>(args.at(4)) != "internal") std::abort();
        skee::transform.AddNodeTransformPosition(a, false, true, "NPC", "internal", {p[0], p[1], p[2]});
      } else if (name == "RemoveNodeTransformScale") {
        a->temporary = false;
      } else if (name == "RemoveNodeTransformPosition") {
        a->internalPosition.reset();
      } else if (name == "UpdateNodeTransform") {
        skee::transform.UpdateNodeTransforms(a, false, true, "NPC");
      } else std::abort();
    }
    if (invalidResult == name) result = Variable{};
    (*callback)(result);
  });
  return true;
}
#include "callback.production.inc"
struct RegisteredHighHeelState {
  std::optional<std::array<float, 3>> position;
  bool hiddenAttachmentStillPresent{}, previouslyActive{}, awaitingAttachment{};
  std::optional<std::array<float, 3>> remainingPosition;
  bool suppressForScene{};
};
std::mutex g_nodeMutex, g_highHeelQueueMutex;
sfs::native::resource_work::ActorBuilds g_sceneObservations;
#include "observation.production.inc"
std::unordered_set<RE::FormID> g_registeredAppearanceHighHeelActors;
sfs::native::racemenu::rules::HighHeelScenes g_highHeelScenes;
std::unordered_set<RE::FormID> g_highHeelAttachmentActors;
sfs::native::resource_work::ActorTasks g_queuedHighHeelSyncs;
std::unordered_set<RE::FormID> g_pendingHighHeelResyncs;
#include "height-watch-state.production.inc"
std::atomic<std::uint64_t> g_highHeelQueueEpoch{0};
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
  const auto position = a->selectedOffset
      ? std::optional{std::array<float, 3>{0, 0, *a->selectedOffset}} : std::nullopt;
  return {position, a->stale, g_registeredAppearanceHighHeelActors.contains(a->id), false, {}, g_highHeelScenes.Contains(a->id)};
}
bool SceneHasNpcPositionSource(RE::Actor* a) { return !a->sceneOffsets.empty(); }
bool IsRegisteredAppearanceDisplayActive(RE::FormID id) { return RE::TESForm::LookupByID<RE::Actor>(id)->active; }
bool RememberHighHeelAttachmentRoots(RE::Actor*,
    const std::vector<RE::NiPointer<RE::NiAVObject>>& nodes, RE::FormID, bool fp, const SceneObservation*) {
  return !fp && std::ranges::any_of(nodes, [](const auto& n) { return n.get()->hasHeel; });
}
void QueuePendingMorphSync(RE::FormID) {}
namespace sfs::virtual_tokens {
void ObserveRegisteredAppearanceWig(RE::Actor*, RE::TESObjectARMO*, RE::NiAVObject*) {}
}
namespace sfs::native {
bool IsDisplayedFittingArmor(RE::Actor* a, const RE::TESObjectARMO* armor) { return a->displayed.contains(armor->id); }
bool IsArmorShownForActor(RE::Actor* a, const RE::TESObjectARMO* armor) { return a->displayed.contains(armor->id) || a->actualVisible.contains(armor->id); }
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
  g_highHeelFrameRetries.clear(); g_hasHighHeelFrameRetries = false;
  g_morphFrameRetries.clear(); g_hasMorphFrameRetries = false;
  g_sceneHeightWatches.clear(); g_hasSceneHeightWatches = false; g_sceneHeightCheckQueued = false;
  a = RE::Actor{a.id};
  RE::TESForm::forms[a.id] = &a;
  g_registeredAppearanceHighHeelActors.clear();
  g_highHeelScenes.Clear();
  g_highHeelAttachmentActors = {a.id};
  g_queuedHighHeelSyncs.Clear(); g_pendingHighHeelResyncs.clear();
  SKSE::tasks.pending.clear();
  RE::BSScript::Internal::vm.pending.clear();
  RE::BSScript::Internal::vm.calls.clear();
  RE::BSScript::Internal::vm.fail.clear();
  RE::BSScript::Internal::vm.invalidResult.clear();
  g_registeredAppearanceAttachmentRoots.clear();
  g_registeredAppearanceNodes.clear();
  sfs::native::dye::requests = 0;
}
void DrainAll() {
  auto& vm = RE::BSScript::Internal::vm;
  for (int i = 0; i != 32 && (!SKSE::tasks.pending.empty() || !vm.pending.empty() ||
      g_hasHighHeelFrameRetries); ++i) {
    DispatchHighHeelFrameRetries(); // one simulated Present per iteration
    SKSE::tasks.Drain(); vm.Drain();
  }
  Check(SKSE::tasks.pending.empty() && vm.pending.empty() && !g_hasHighHeelFrameRetries,
        "actor-local work terminates after bounded frame retries");
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
    Check(a.resolverCalls == 1 && a.fullUpdates == 1 && a.displayedHeight == 0,
          "hidden old attachment cannot exhaust retries while retaining automatic height");
    Reset(a); a.sceneOffsets = {12}; queue(); queue(); DrainAll();
    Check(a.fullUpdates == 2, "pending event coalesces into one subsequent pass");
    Reset(a); a.sceneOffsets = {12}; queue(); g_queuedHighHeelSyncs.Clear(); DrainAll();
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
  vm.Drain(); // Add neutral scale completed, cleanup is queued before the full scan.
  a.selectedOffset = 7.0F; a.sceneOffsets = {12, 0}; queue(); DrainAll();
  Check(a.displayedHeight == 7, "pending refresh resolves edited appearance on game task after legacy chain");
  for (const auto* failure : {"UpdateAllReferenceTransforms", "HasNodeTransformPosition", "AddNodeTransformPosition", "UpdateNodeTransform"}) {
    Reset(a); a.sceneOffsets = {12}; vm.fail = failure; queue(); DrainAll();
    Check(!a.temporary && !g_registeredAppearanceHighHeelActors.contains(a.id),
          "legacy dispatch failure removes neutral bootstrap and does not report success");
  }
  Reset(a); a.sceneOffsets = {12};
  const std::array<RE::FormID, 1> sceneActors{a.id};
  g_highHeelScenes.Update(0x120, sceneActors);
  vm.fail = "GetNodeTransformPosition"; queue(); DrainAll();
  Check(!a.temporary && !g_registeredAppearanceHighHeelActors.contains(a.id),
        "legacy scene-query failure cleans bootstrap without publishing false success");
  vm.fail.clear(); queue(); DrainAll();
  Check(a.displayedHeight == 0, "legacy scene-query failure does not permanently disable the next valid synchronization");
  Reset(a); a.selectedOffset.reset(); a.internalPosition = 12.0F;
  g_registeredAppearanceHighHeelActors.insert(a.id);
  vm.invalidResult = "HasNodeTransformPosition"; queue(); DrainAll();
  Check(g_registeredAppearanceHighHeelActors.contains(a.id) && !a.temporary,
        "invalid legacy Boolean cannot be mistaken for confirmed internal-position removal");
  vm.invalidResult.clear(); queue(); DrainAll();
  Check(!g_registeredAppearanceHighHeelActors.contains(a.id),
        "valid cleanup after unreadable legacy result retires old height ownership");
  Reset(a); a.sceneOffsets = {12}; vm.invalidResult = "GetNodeTransformPosition";
  queue(); DrainAll();
  Check(!g_registeredAppearanceHighHeelActors.contains(a.id) && !a.temporary &&
        g_queuedHighHeelSyncs.Size() == 0,
        "invalid legacy applied-position readback cannot report success or retain work");
  vm.invalidResult.clear(); queue(); DrainAll();
  Check(g_registeredAppearanceHighHeelActors.contains(a.id),
        "a malformed readback does not disable subsequent valid height synchronization");
  Reset(a); a.sceneOffsets = {12}; queue(); SKSE::tasks.Drain();
  g_queuedHighHeelSyncs.Clear(); DrainAll();
  Check(a.fullUpdates == 0 && !g_registeredAppearanceHighHeelActors.contains(a.id),
        "old legacy callbacks cannot continue into a new load generation");

  // An unload cancels one actor, not the other actor's pending height update.
  // Reuse the same FormID immediately, before the old Papyrus callback arrives.
  for (auto version : {1U, 2U, 3U, 4U, 99U}) {
    g_transformInterfaceVersion = version;
    g_transformInterface = version < 3 ? nullptr : &skee::transform;
    Reset(a); a.sceneOffsets = {12}; queue();
    if (version < 3) { SKSE::tasks.Drain(); }
    g_queuedHighHeelSyncs.Forget(a.id);
    g_pendingHighHeelResyncs.erase(a.id);
    RE::Actor other{0xA001}; other.sceneOffsets = {12};
    sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&other);
    a.selectedOffset = 17.0F; a.sceneOffsets = {17}; queue();
    DrainAll();
    Check(a.displayedHeight == 17 && other.displayedHeight == 12 &&
              g_queuedHighHeelSyncs.Size() == 0,
          "actor cancellation/reload keeps new and other actor work, including legacy callbacks");
  }
  std::printf("%u high-heel source regression checks passed; NOT in-game evidence.\n", checks);
}
