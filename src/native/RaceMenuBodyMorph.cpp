#include "native/RaceMenuBodyMorph.h"

#include "native/ArmorSkinning.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Windows.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace {
namespace skee {
class IPluginInterface {
public:
  virtual ~IPluginInterface() = default;

  virtual std::uint32_t GetVersion() = 0;
  virtual void Revert() = 0;
};

class IInterfaceMap {
public:
  virtual IPluginInterface *QueryInterface(const char *a_name) = 0;
  virtual bool AddInterface(const char *a_name,
                            IPluginInterface *a_pluginInterface) = 0;
  virtual IPluginInterface *RemoveInterface(const char *a_name) = 0;
};

struct InterfaceExchangeMessage {
  enum : std::uint32_t { kExchangeInterface = 0x9E3779B9 };

  IInterfaceMap *interfaceMap{nullptr};
};

class IAddonAttachmentInterface {
public:
  virtual void OnAttach(RE::TESObjectREFR *, RE::TESObjectARMO *,
                        RE::TESObjectARMA *, RE::NiAVObject *, bool,
                        RE::NiNode *, RE::NiNode *) = 0;
};

class IBodyMorphInterface : public IPluginInterface {
public:
  class MorphKeyVisitor {
  public:
    virtual void Visit(const char *, float) = 0;
  };

  class StringVisitor {
  public:
    virtual void Visit(const char *) = 0;
  };

  class ActorVisitor {
  public:
    virtual void Visit(RE::TESObjectREFR *) = 0;
  };

  class MorphValueVisitor {
  public:
    virtual void Visit(RE::TESObjectREFR *, const char *, const char *,
                       float) = 0;
  };

  class MorphVisitor {
  public:
    virtual void Visit(RE::TESObjectREFR *, const char *) = 0;
  };

  virtual void SetMorph(RE::TESObjectREFR *, const char *, const char *,
                        float) = 0;
  virtual float GetMorph(RE::TESObjectREFR *, const char *, const char *) = 0;
  virtual void ClearMorph(RE::TESObjectREFR *, const char *, const char *) = 0;
  virtual float GetBodyMorphs(RE::TESObjectREFR *, const char *) = 0;
  virtual void ClearBodyMorphNames(RE::TESObjectREFR *, const char *) = 0;
  virtual void VisitMorphs(RE::TESObjectREFR *, MorphVisitor &) = 0;
  virtual void VisitKeys(RE::TESObjectREFR *, const char *,
                         MorphKeyVisitor &) = 0;
  virtual void VisitMorphValues(RE::TESObjectREFR *, MorphValueVisitor &) = 0;
  virtual void ClearMorphs(RE::TESObjectREFR *) = 0;
  virtual void ApplyVertexDiff(RE::TESObjectREFR *, RE::NiAVObject *,
                               bool = false) = 0;
  virtual void ApplyBodyMorphs(RE::TESObjectREFR *, bool = true) = 0;
  virtual void UpdateModelWeight(RE::TESObjectREFR *, bool = false) = 0;
  virtual void SetCacheLimit(std::size_t) = 0;
  virtual bool HasMorphs(RE::TESObjectREFR *) = 0;
  virtual std::uint32_t EvaluateBodyMorphs(RE::TESObjectREFR *) = 0;
  virtual bool HasBodyMorph(RE::TESObjectREFR *, const char *,
                            const char *) = 0;
  virtual bool HasBodyMorphName(RE::TESObjectREFR *, const char *) = 0;
  virtual bool HasBodyMorphKey(RE::TESObjectREFR *, const char *) = 0;
  virtual void ClearBodyMorphKeys(RE::TESObjectREFR *, const char *) = 0;
  virtual void VisitStrings(StringVisitor &) = 0;
  virtual void VisitActors(ActorVisitor &) = 0;
  virtual std::size_t ClearMorphCache() = 0;
};

class IActorUpdateManager : public IPluginInterface {
public:
  // RaceMenu 0.4.16's public ActorUpdateManager ABI exposes these three
  // virtuals immediately after IPluginInterface. The Add*Update helpers in
  // RaceMenu's concrete class are non-virtual and must not be represented
  // here, or every following vtable index becomes incorrect.
  virtual void AddInterface(IAddonAttachmentInterface *) = 0;
  virtual void RemoveInterface(IAddonAttachmentInterface *) = 0;
  virtual void OnAttach(RE::TESObjectREFR *, RE::TESObjectARMO *,
                        RE::TESObjectARMA *, RE::NiAVObject *, bool,
                        RE::NiNode *, RE::NiNode *) = 0;
};
} // namespace skee

struct RegisteredAppearanceNode {
  RE::NiPointer<RE::NiAVObject> object;
  RE::FormID armorFormID{0};
  RE::FormID addonFormID{0};
  bool firstPerson{false};

  [[nodiscard]] bool operator==(const RegisteredAppearanceNode &a_other) const {
    return object.get() == a_other.object.get() &&
           armorFormID == a_other.armorFormID &&
           addonFormID == a_other.addonFormID &&
           firstPerson == a_other.firstPerson;
  }
};

constexpr std::size_t kApplyBodyMorphsVtableIndex = 13;
using ApplyBodyMorphsFn = void (*)(skee::IBodyMorphInterface *,
                                   RE::TESObjectREFR *, bool);
using UpdateModelWeightTaskRunFn = void (*)(void *);

std::atomic<skee::IBodyMorphInterface *> g_bodyMorphInterface{nullptr};
std::atomic<ApplyBodyMorphsFn> g_originalApplyBodyMorphs{nullptr};
std::atomic<UpdateModelWeightTaskRunFn>
    g_originalUpdateModelWeightTaskRun{nullptr};
std::atomic_bool g_applyBodyMorphsHookInstalled{false};
std::atomic_bool g_updateModelWeightTaskHookInstalled{false};
std::atomic_bool g_attachmentObserverRegistered{false};
std::atomic_bool g_bodyMorphInitializationComplete{false};
std::atomic_uint32_t g_bodyMorphInitializationAttempts{0};
std::mutex g_initializeMutex;
std::mutex g_nodeMutex;
std::unordered_map<RE::FormID, std::vector<RegisteredAppearanceNode>>
    g_registeredAppearanceNodes;
std::unordered_set<RE::FormID> g_activeRegisteredAppearanceActors;
std::unordered_set<RE::FormID> g_observedModelWeightMorphActors;
std::unordered_set<RE::FormID> g_observedEmptyModelWeightMorphActors;
thread_local std::uint32_t g_updateModelWeightTaskDepth{0};

class ScopedUpdateModelWeightTask final {
public:
  ScopedUpdateModelWeightTask() { ++g_updateModelWeightTaskDepth; }
  ~ScopedUpdateModelWeightTask() { --g_updateModelWeightTaskDepth; }

  ScopedUpdateModelWeightTask(const ScopedUpdateModelWeightTask &) = delete;
  ScopedUpdateModelWeightTask &
  operator=(const ScopedUpdateModelWeightTask &) = delete;
};

struct ModuleSection {
  std::byte *begin{nullptr};
  std::size_t size{0};
  std::uint32_t characteristics{0};

  [[nodiscard]] bool Contains(const void *a_address,
                              const std::size_t a_size = 1) const {
    if (!begin || !a_address || a_size > size) {
      return false;
    }
    const auto *address = static_cast<const std::byte *>(a_address);
    return address >= begin && address <= begin + size - a_size;
  }

  [[nodiscard]] bool IsReadableData() const {
    return (characteristics & IMAGE_SCN_MEM_READ) != 0 &&
           (characteristics & IMAGE_SCN_MEM_EXECUTE) == 0;
  }

  [[nodiscard]] bool IsExecutable() const {
    return (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
  }
};

struct LoadedModuleImage {
  std::byte *base{nullptr};
  std::size_t size{0};
  std::vector<ModuleSection> sections;

  [[nodiscard]] bool Contains(const void *a_address,
                              const std::size_t a_size = 1) const {
    if (!base || !a_address || a_size > size) {
      return false;
    }
    const auto *address = static_cast<const std::byte *>(a_address);
    return address >= base && address <= base + size - a_size;
  }

  [[nodiscard]] bool IsExecutableAddress(const void *a_address) const {
    return std::ranges::any_of(sections, [&](const ModuleSection &a_section) {
      return a_section.IsExecutable() && a_section.Contains(a_address);
    });
  }
};

struct RttiCompleteObjectLocator64 {
  std::uint32_t signature{0};
  std::uint32_t offset{0};
  std::uint32_t constructorDisplacementOffset{0};
  std::int32_t typeDescriptorRva{0};
  std::int32_t classDescriptorRva{0};
  std::int32_t selfRva{0};
};

[[nodiscard]] std::optional<LoadedModuleImage>
GetLoadedModuleImage(const wchar_t *a_moduleName) {
  auto *module = reinterpret_cast<std::byte *>(GetModuleHandleW(a_moduleName));
  if (!module) {
    return std::nullopt;
  }

  const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
    return std::nullopt;
  }
  const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
      module + static_cast<std::size_t>(dos->e_lfanew));
  if (nt->Signature != IMAGE_NT_SIGNATURE ||
      nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      nt->OptionalHeader.SizeOfImage == 0) {
    return std::nullopt;
  }

  LoadedModuleImage image{.base = module,
                          .size = nt->OptionalHeader.SizeOfImage};
  const auto *section = IMAGE_FIRST_SECTION(nt);
  image.sections.reserve(nt->FileHeader.NumberOfSections);
  for (std::uint16_t index = 0; index < nt->FileHeader.NumberOfSections;
       ++index) {
    const auto sectionSize = std::max<std::size_t>(
        section[index].Misc.VirtualSize, section[index].SizeOfRawData);
    if (sectionSize == 0 ||
        section[index].VirtualAddress >= image.size ||
        sectionSize > image.size - section[index].VirtualAddress) {
      continue;
    }
    image.sections.push_back(
        {.begin = module + section[index].VirtualAddress,
         .size = sectionSize,
         .characteristics = section[index].Characteristics});
  }
  return image;
}

[[nodiscard]] std::byte *FindBytes(const ModuleSection &a_section,
                                   const std::string_view a_bytes) {
  if (!a_section.begin || a_bytes.empty() || a_bytes.size() > a_section.size) {
    return nullptr;
  }
  const auto *needle = reinterpret_cast<const std::byte *>(a_bytes.data());
  const auto result = std::search(a_section.begin,
                                  a_section.begin + a_section.size, needle,
                                  needle + a_bytes.size());
  return result == a_section.begin + a_section.size ? nullptr : result;
}

[[nodiscard]] std::uintptr_t *FindRttiVtable(
    const LoadedModuleImage &a_image, const std::string_view a_typeName) {
  // MSVC x64 TypeDescriptor stores two pointers immediately before its
  // decorated class name. Locate that descriptor without relying on a
  // RaceMenu version-specific RVA.
  std::byte *typeNameAddress = nullptr;
  for (const auto &section : a_image.sections) {
    if (!section.IsReadableData()) {
      continue;
    }
    typeNameAddress = FindBytes(section, a_typeName);
    if (typeNameAddress) {
      break;
    }
  }
  if (!typeNameAddress ||
      !a_image.Contains(typeNameAddress - 2 * sizeof(std::uintptr_t),
                        2 * sizeof(std::uintptr_t) + a_typeName.size())) {
    return nullptr;
  }

  const auto *typeDescriptor =
      typeNameAddress - 2 * sizeof(std::uintptr_t);
  const auto typeDescriptorOffset = typeDescriptor - a_image.base;
  if (typeDescriptorOffset < 0 ||
      static_cast<std::uint64_t>(typeDescriptorOffset) >
          std::numeric_limits<std::uint32_t>::max()) {
    return nullptr;
  }
  const auto typeDescriptorRva =
      static_cast<std::int32_t>(typeDescriptorOffset);

  std::vector<const RttiCompleteObjectLocator64 *> objectLocators;
  for (const auto &section : a_image.sections) {
    if (!section.IsReadableData() ||
        section.size < sizeof(RttiCompleteObjectLocator64)) {
      continue;
    }
    for (std::size_t offset = 0;
         offset + sizeof(RttiCompleteObjectLocator64) <= section.size;
         offset += alignof(std::uint32_t)) {
      const auto *locator = reinterpret_cast<
          const RttiCompleteObjectLocator64 *>(section.begin + offset);
      if (locator->signature != 1 || locator->offset != 0 ||
          locator->typeDescriptorRva != typeDescriptorRva ||
          locator->selfRva !=
              static_cast<std::int32_t>(
                  reinterpret_cast<const std::byte *>(locator) -
                  a_image.base)) {
        continue;
      }
      objectLocators.push_back(locator);
    }
  }

  for (const auto *locator : objectLocators) {
    const auto locatorAddress = reinterpret_cast<std::uintptr_t>(locator);
    for (const auto &section : a_image.sections) {
      if (!section.IsReadableData() || section.size < 3 * sizeof(void *)) {
        continue;
      }
      for (std::size_t offset = 0;
           offset + 3 * sizeof(void *) <= section.size;
           offset += alignof(void *)) {
        auto *locatorSlot = reinterpret_cast<std::uintptr_t *>(
            section.begin + offset);
        if (*locatorSlot != locatorAddress) {
          continue;
        }
        auto *vtable = locatorSlot + 1;
        if (a_image.IsExecutableAddress(
                reinterpret_cast<const void *>(vtable[0])) &&
            a_image.IsExecutableAddress(
                reinterpret_cast<const void *>(vtable[1]))) {
          return vtable;
        }
      }
    }
  }
  return nullptr;
}

[[nodiscard]] bool IsRegisteredAppearanceDisplayActive(
    const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_nodeMutex);
  return g_activeRegisteredAppearanceActors.contains(a_actorFormID);
}

[[nodiscard]] std::vector<RE::NiPointer<RE::NiAVObject>>
ResolveRegisteredAppearanceNodes(RE::Actor *a_actor) {
  std::vector<RE::NiPointer<RE::NiAVObject>> resolved;
  if (!a_actor) {
    return resolved;
  }

  std::lock_guard lock(g_nodeMutex);
  const auto nodesIt =
      g_registeredAppearanceNodes.find(a_actor->GetFormID());
  if (nodesIt == g_registeredAppearanceNodes.end()) {
    return resolved;
  }

  // A DAVE refresh may retain an unchanged attachment instead of emitting a
  // replacement OnAttach event. Keep remembered nodes across refreshes and
  // prune them here only after they have actually left this actor's scene or
  // no longer belong to the actor's current registered appearance set.
  const auto isCurrentNode = [&](const RegisteredAppearanceNode &a_entry) {
    if (!a_entry.object) {
      return false;
    }
    const auto *armor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(a_entry.armorFormID);
    if (!armor || !sfs::native::IsDisplayedFittingArmor(a_actor, armor)) {
      return false;
    }
    auto *root = a_actor->Get3D(a_entry.firstPerson);
    for (auto *object = a_entry.object.get(); object;
         object = object->parent) {
      if (object == root) {
        return true;
      }
    }
    return false;
  };

  auto &rememberedNodes = nodesIt->second;
  std::erase_if(rememberedNodes, [&](const RegisteredAppearanceNode &a_entry) {
    return !isCurrentNode(a_entry);
  });
  std::unordered_set<RE::NiAVObject *> seen;
  for (const auto &entry : rememberedNodes) {
    if (entry.object && seen.insert(entry.object.get()).second) {
      resolved.push_back(entry.object);
    }
  }
  return resolved;
}

void CollectSceneObjects(RE::NiAVObject *a_object,
                         std::unordered_set<RE::NiAVObject *> &a_objects) {
  if (!a_object || !a_objects.insert(a_object).second) {
    return;
  }
  auto *node = a_object->AsNode();
  if (!node) {
    return;
  }
  for (const auto &child : node->GetChildren()) {
    CollectSceneObjects(child.get(), a_objects);
  }
}

[[nodiscard]] bool ContainsExtraData(RE::NiAVObject *a_object,
                                     const RE::BSFixedString &a_name) {
  if (!a_object) {
    return false;
  }
  if (a_object->GetExtraData(a_name)) {
    return true;
  }
  auto *node = a_object->AsNode();
  if (!node) {
    return false;
  }
  return std::ranges::any_of(node->GetChildren(), [&](const auto &a_child) {
    return ContainsExtraData(a_child.get(), a_name);
  });
}

[[nodiscard]] std::vector<RE::NiPointer<RE::NiAVObject>>
FindNewAttachmentRoots(
    RE::NiAVObject *a_root,
    const std::unordered_set<RE::NiAVObject *> &a_beforeObjects) {
  std::unordered_set<RE::NiAVObject *> afterObjects;
  CollectSceneObjects(a_root, afterObjects);

  std::vector<RE::NiPointer<RE::NiAVObject>> roots;
  for (auto *object : afterObjects) {
    if (!object || a_beforeObjects.contains(object)) {
      continue;
    }
    if (!object->parent || a_beforeObjects.contains(object->parent)) {
      roots.emplace_back(object);
    }
  }
  return roots;
}

void RememberAndMorphNewNodes(
    skee::IBodyMorphInterface *a_interface, RE::Actor *a_actor,
    const std::vector<RE::NiPointer<RE::NiAVObject>> &a_nodes,
    const RE::FormID a_armorFormID, const bool a_firstPerson) {
  if (!a_interface || !a_actor || a_nodes.empty()) {
    return;
  }

  std::size_t remembered = 0;
  static const RE::BSFixedString bodyTriName{"BODYTRI"};
  static const RE::BSFixedString shapeDataName{"SHAPEDATA"};
  for (const auto &node : a_nodes) {
    if (!node || !ContainsExtraData(node.get(), bodyTriName)) {
      continue;
    }

    // RaceMenu's own OnAttach path leaves SHAPEDATA on processed geometry. If
    // it already saw this attachment, record the node without adding the same
    // vertex delta twice. Otherwise mirror OnAttach with attaching=true.
    if (!ContainsExtraData(node.get(), shapeDataName)) {
      a_interface->ApplyVertexDiff(a_actor, node.get(), true);
    }

    RegisteredAppearanceNode entry{.object = node,
                                   .armorFormID = a_armorFormID,
                                   .addonFormID = 0,
                                   .firstPerson = a_firstPerson};
    std::lock_guard lock(g_nodeMutex);
    auto &registered =
        g_registeredAppearanceNodes[a_actor->GetFormID()];
    if (std::ranges::find(registered, entry) == registered.end()) {
      registered.push_back(std::move(entry));
      ++remembered;
    }
  }

  if (remembered != 0) {
    logger::debug(
        "Applied RaceMenu morphs to {} newly attached SFS node(s) actor={:08X} armor={:08X} firstPerson={}",
        remembered, a_actor->GetFormID(), a_armorFormID, a_firstPerson);
  }
}

class RegisteredAppearanceAttachmentObserver final
    : public skee::IAddonAttachmentInterface {
public:
  void OnAttach(RE::TESObjectREFR *a_reference,
                RE::TESObjectARMO *a_armor, RE::TESObjectARMA *a_addon,
                RE::NiAVObject *a_object, const bool a_firstPerson,
                [[maybe_unused]] RE::NiNode *a_skeleton,
                [[maybe_unused]] RE::NiNode *a_root) override {
    auto *actor = a_reference ? a_reference->As<RE::Actor>() : nullptr;
    if (!actor || !a_armor || !a_addon || !a_object ||
        !IsRegisteredAppearanceDisplayActive(actor->GetFormID()) ||
        !sfs::native::IsDisplayedFittingArmor(actor, a_armor)) {
      return;
    }

    RegisteredAppearanceNode node{
                                  .object = RE::NiPointer<RE::NiAVObject>{a_object},
                                  .armorFormID = a_armor->GetFormID(),
                                  .addonFormID = a_addon->GetFormID(),
                                  .firstPerson = a_firstPerson};
    std::lock_guard lock(g_nodeMutex);
    auto &nodes = g_registeredAppearanceNodes[actor->GetFormID()];
    if (std::ranges::find(nodes, node) == nodes.end()) {
      nodes.push_back(std::move(node));
    }
  }
};

RegisteredAppearanceAttachmentObserver g_attachmentObserver;

[[nodiscard]] std::size_t ApplyMorphsToRegisteredAppearanceNodes(
    skee::IBodyMorphInterface *a_interface, RE::Actor *a_actor);

void QueueUpdateModelWeightAppearanceSync(const RE::FormID a_actorFormID) {
  if (a_actorFormID == 0 ||
      !IsRegisteredAppearanceDisplayActive(a_actorFormID)) {
    return;
  }

  auto *taskInterface = SKSE::GetTaskInterface();
  if (!taskInterface) {
    logger::warn(
        "Skipped deferred RaceMenu registered-appearance synchronization for actor {:08X}: SKSE task interface unavailable",
        a_actorFormID);
    return;
  }

  taskInterface->AddTask([a_actorFormID]() {
    if (!IsRegisteredAppearanceDisplayActive(a_actorFormID)) {
      return;
    }

    try {
      auto *bodyMorph = g_bodyMorphInterface.load();
      auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
      if (!bodyMorph || !actor || !actor->Is3DLoaded()) {
        return;
      }

      const auto appliedCount =
          ApplyMorphsToRegisteredAppearanceNodes(bodyMorph, actor);
      bool firstObservedUpdate = false;
      {
        std::lock_guard lock(g_nodeMutex);
        if (appliedCount != 0) {
          firstObservedUpdate =
              g_observedModelWeightMorphActors.insert(a_actorFormID).second;
        } else {
          firstObservedUpdate =
              g_observedEmptyModelWeightMorphActors.insert(a_actorFormID)
                  .second;
        }
      }
      if (firstObservedUpdate) {
        logger::info(
            "Observed original RaceMenu UpdateModelWeight task for active SFS actor {:08X}; deferred synchronization applied to {} registered appearance node(s)",
            a_actorFormID, appliedCount);
      } else {
        logger::debug(
            "RaceMenu UpdateModelWeight deferred synchronization applied to {} registered appearance node(s) actor={:08X}",
            appliedCount, a_actorFormID);
      }
    } catch (const std::exception &error) {
      logger::error(
          "Deferred RaceMenu UpdateModelWeight registered-appearance synchronization failed actor={:08X}: {}",
          a_actorFormID, error.what());
    } catch (...) {
      logger::error(
          "Deferred RaceMenu UpdateModelWeight registered-appearance synchronization failed actor={:08X}: unknown exception",
          a_actorFormID);
    }
  });
}

[[nodiscard]] std::size_t ApplyMorphsToRegisteredAppearanceNodes(
    skee::IBodyMorphInterface *a_interface, RE::Actor *a_actor) {
  if (!a_interface || !a_actor || !a_actor->Is3DLoaded() ||
      !IsRegisteredAppearanceDisplayActive(a_actor->GetFormID())) {
    return 0;
  }

  auto nodes = ResolveRegisteredAppearanceNodes(a_actor);
  if (nodes.empty()) {
    return 0;
  }

  std::size_t submittedCount = 0;
  for (const auto &node : nodes) {
    if (node) {
      a_interface->ApplyVertexDiff(a_actor, node.get(), false);
      ++submittedCount;
    }
  }

  if (submittedCount != 0) {
    logger::debug(
        "Applied current RaceMenu morphs to {} registered SFS appearance node(s) actor={:08X}",
        submittedCount, a_actor->GetFormID());
  }
  return submittedCount;
}

void ApplyBodyMorphsHook(skee::IBodyMorphInterface *a_interface,
                         RE::TESObjectREFR *a_reference,
                         const bool a_deferUpdate) {
  const auto original = g_originalApplyBodyMorphs.load();
  if (original) {
    original(a_interface, a_reference, a_deferUpdate);
  }

  auto *actor = a_reference ? a_reference->As<RE::Actor>() : nullptr;
  if (!actor) {
    return;
  }

  // The deferred UpdateModelWeight task is handled after its original Run
  // returns. Suppress this public-interface post pass while inside that task
  // so compiler/version differences cannot apply the same vertex deltas twice.
  if (g_updateModelWeightTaskDepth == 0) {
    try {
      (void)ApplyMorphsToRegisteredAppearanceNodes(a_interface, actor);
    } catch (const std::exception &error) {
      logger::error(
          "RaceMenu public BodyMorph synchronization failed actor={:08X}: {}",
          actor->GetFormID(), error.what());
    } catch (...) {
      logger::error(
          "RaceMenu public BodyMorph synchronization failed actor={:08X}: unknown exception",
          actor->GetFormID());
    }
  }
}

void UpdateModelWeightTaskRunHook(void *a_task) {
  // NIOVTaskUpdateModelWeight contains the TaskDelegate vptr followed by the
  // actor FormID. Read it before Run; RaceMenu disposes the task separately.
  RE::FormID actorFormID{0};
  if (a_task) {
    std::memcpy(std::addressof(actorFormID),
                static_cast<const std::byte *>(a_task) + sizeof(void *),
                sizeof(actorFormID));
  }

  const auto original = g_originalUpdateModelWeightTaskRun.load();
  if (original) {
    const ScopedUpdateModelWeightTask taskScope;
    original(a_task);
  }

  if (actorFormID == 0 ||
      !IsRegisteredAppearanceDisplayActive(actorFormID)) {
    return;
  }

  QueueUpdateModelWeightAppearanceSync(actorFormID);
}

[[nodiscard]] bool
InstallApplyBodyMorphsHook(skee::IBodyMorphInterface *a_bodyMorph) {
  if (!a_bodyMorph) {
    return false;
  }
  if (g_applyBodyMorphsHookInstalled.load()) {
    return true;
  }

  auto *vtable = *reinterpret_cast<std::uintptr_t **>(a_bodyMorph);
  if (!vtable) {
    return false;
  }
  auto *slot = std::addressof(vtable[kApplyBodyMorphsVtableIndex]);
  const auto originalAddress = *slot;
  const auto replacementAddress =
      reinterpret_cast<std::uintptr_t>(ApplyBodyMorphsHook);
  if (originalAddress == 0 || originalAddress == replacementAddress) {
    return false;
  }

  g_originalApplyBodyMorphs.store(
      reinterpret_cast<ApplyBodyMorphsFn>(originalAddress));
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(slot),
                       std::addressof(replacementAddress),
                       sizeof(replacementAddress),
                       std::addressof(originalAddress),
                       sizeof(originalAddress))) {
    g_originalApplyBodyMorphs.store(nullptr);
    logger::warn(
        "RaceMenu BodyMorph pipeline hook verification failed; registered appearance morph compatibility remains inactive");
    return false;
  }

  g_applyBodyMorphsHookInstalled.store(true);
  logger::info(
      "Installed actor-local RaceMenu BodyMorph pipeline hook for registered appearances");
  return true;
}

[[nodiscard]] bool InstallUpdateModelWeightTaskHook() {
  if (g_updateModelWeightTaskHookInstalled.load()) {
    return true;
  }

  const auto image = GetLoadedModuleImage(L"skee64.dll");
  if (!image.has_value()) {
    logger::warn(
        "RaceMenu skee64.dll image was unavailable; deferred UpdateModelWeight task hook remains inactive");
    return false;
  }

  constexpr std::string_view typeName{
      ".?AVNIOVTaskUpdateModelWeight@@"};
  auto *vtable = FindRttiVtable(*image, typeName);
  if (!vtable) {
    logger::warn(
        "RaceMenu deferred UpdateModelWeight task RTTI was unavailable; registered appearance morph synchronization remains on the public interface fallback");
    return false;
  }

  auto *slot = std::addressof(vtable[0]);
  const auto originalAddress = *slot;
  const auto replacementAddress =
      reinterpret_cast<std::uintptr_t>(UpdateModelWeightTaskRunHook);
  if (originalAddress == 0 || originalAddress == replacementAddress) {
    return false;
  }

  g_originalUpdateModelWeightTaskRun.store(
      reinterpret_cast<UpdateModelWeightTaskRunFn>(originalAddress));
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(slot),
                       std::addressof(replacementAddress),
                       sizeof(replacementAddress),
                       std::addressof(originalAddress),
                       sizeof(originalAddress))) {
    g_originalUpdateModelWeightTaskRun.store(nullptr);
    logger::warn(
        "RaceMenu deferred UpdateModelWeight task hook verification failed; registered appearance morph synchronization remains on the public interface fallback");
    return false;
  }

  g_updateModelWeightTaskHookInstalled.store(true);
  logger::info(
      "Installed RTTI-resolved original RaceMenu UpdateModelWeight task hook for registered appearances (vtable RVA {:X}, Run RVA {:X}, actor FormID offset {})",
      reinterpret_cast<std::byte *>(vtable) - image->base,
      reinterpret_cast<const std::byte *>(
          reinterpret_cast<const void *>(originalAddress)) -
          image->base,
      sizeof(void *));
  return true;
}
} // namespace

namespace sfs::native::racemenu {
namespace {
struct ExchangeResult {
  skee::IInterfaceMap *interfaceMap{nullptr};
  skee::IBodyMorphInterface *bodyMorph{nullptr};
  std::string_view route;
};

std::optional<ExchangeResult>
TryInterfaceExchange(const SKSE::MessagingInterface *a_messaging,
                     const std::uint32_t a_attempt, const char *a_receiver,
                     const std::string_view a_route) {
  if (!a_messaging) {
    return std::nullopt;
  }

  // Use the same direct named-receiver exchange as OBody NG. This isolates
  // RaceMenu's official "skee" listener and avoids sharing the mutable
  // exchange payload with unrelated broadcast listeners.
  skee::InterfaceExchangeMessage message{};
  const bool dispatched = a_messaging->Dispatch(
      skee::InterfaceExchangeMessage::kExchangeInterface,
      std::addressof(message), sizeof(skee::InterfaceExchangeMessage),
      a_receiver);

  auto *bodyMorph =
      message.interfaceMap
          ? static_cast<skee::IBodyMorphInterface *>(
                message.interfaceMap->QueryInterface("BodyMorph"))
          : nullptr;
  const auto bodyMorphVersion = bodyMorph ? bodyMorph->GetVersion() : 0;

  logger::info(
      "RaceMenu interface exchange attempt {} via {}: dispatched={}, interfaceMap={}, BodyMorph={}, version={}",
      a_attempt, a_route, dispatched,
      static_cast<const void *>(message.interfaceMap),
      static_cast<const void *>(bodyMorph), bodyMorphVersion);

  if (!dispatched || !message.interfaceMap || !bodyMorph ||
      bodyMorphVersion < 4) {
    return std::nullopt;
  }
  return ExchangeResult{message.interfaceMap, bodyMorph, a_route};
}
} // namespace

void InitializeBodyMorphInterface() {
  std::lock_guard initializeLock(g_initializeMutex);
  if (g_bodyMorphInitializationComplete.load()) {
    return;
  }

  auto *messaging = SKSE::GetMessagingInterface();
  if (!messaging) {
    logger::debug("RaceMenu BodyMorph interface unavailable: SKSE messaging "
                  "interface missing");
    return;
  }

  const auto attempt = g_bodyMorphInitializationAttempts.fetch_add(1) + 1;

  // Match OBody NG's public RaceMenu exchange path exactly: request the
  // interface from RaceMenu's registered SKSE receiver name after PostPostLoad.
  // The second call from DataLoaded provides one bounded retry for load-order
  // differences without polling or replacing skee64.dll.
  auto exchange = TryInterfaceExchange(
      messaging, attempt, "skee", "OBody-compatible named receiver 'skee'");
  if (!exchange) {
    if (attempt == 1) {
      logger::info(
          "RaceMenu interfaces are not available yet; registered appearance morph compatibility will retry at DataLoaded");
    } else {
      logger::warn(
          "RaceMenu interfaces remained unavailable after {} initialization attempts; registered appearance morph compatibility is inactive for this game session",
          attempt);
    }
    return;
  }
  logger::info("Received supported RaceMenu interfaces via {}",
               exchange->route);

  auto *bodyMorph = exchange->bodyMorph;
  g_bodyMorphInterface.store(bodyMorph);

  const bool publicHookInstalled = InstallApplyBodyMorphsHook(bodyMorph);
  if (!publicHookInstalled) {
    logger::warn(
        "RaceMenu BodyMorph public pipeline hook could not be installed; external direct ApplyBodyMorphs callers will use attachment-time synchronization only");
  }
  const bool updateTaskHookInstalled = InstallUpdateModelWeightTaskHook();
  if (!updateTaskHookInstalled) {
    logger::warn(
        "RaceMenu UpdateModelWeight task hook could not be installed; NiOverride Papyrus live synchronization will remain inactive");
  }

  auto *actorUpdateManager = static_cast<skee::IActorUpdateManager *>(
      exchange->interfaceMap->QueryInterface("ActorUpdateManager"));
  if (actorUpdateManager &&
      !g_attachmentObserverRegistered.exchange(true)) {
    actorUpdateManager->AddInterface(std::addressof(g_attachmentObserver));
    logger::info(
        "Registered official RaceMenu ActorUpdateManager attachment observer for SFS appearance morphs (reported version {})",
        actorUpdateManager->GetVersion());
  } else if (!actorUpdateManager) {
    logger::warn(
        "RaceMenu ActorUpdateManager interface is unavailable; SFS will retain native attachment-scene capture only");
  }

  logger::info("Connected to RaceMenu BodyMorph interface version {}",
               bodyMorph->GetVersion());
  g_bodyMorphInitializationComplete.store(true);
}

bool IsBodyMorphInterfaceReady() {
  return g_bodyMorphInterface.load() != nullptr &&
         g_bodyMorphInitializationComplete.load();
}

AttachmentSceneSnapshot CaptureAttachmentScene(RE::Actor *a_actor) {
  AttachmentSceneSnapshot snapshot;
  if (!a_actor || !IsBodyMorphInterfaceReady()) {
    return snapshot;
  }
  auto *thirdPersonRoot = a_actor->Get3D(false);
  auto *firstPersonRoot = a_actor->Get3D(true);
  CollectSceneObjects(thirdPersonRoot, snapshot.thirdPersonObjects);
  if (firstPersonRoot != thirdPersonRoot) {
    CollectSceneObjects(firstPersonRoot, snapshot.firstPersonObjects);
  }
  return snapshot;
}

void MorphNewRegisteredAppearanceNodes(
    RE::Actor *a_actor, const AttachmentSceneSnapshot &a_before,
    const RE::FormID a_armorFormID) {
  auto *bodyMorph = g_bodyMorphInterface.load();
  // Replacing kit previews are deliberately left to RaceMenu's official
  // OnAttach callback. Tracking the same short-lived attachment here would
  // make the later SFS synchronization reset its vertex buffer a second time
  // during the same skinning pass. Saved appearances remain tracked so they
  // continue to receive explicit RaceMenu body-morph updates.
  if (!bodyMorph || !a_actor || !IsBodyMorphInterfaceReady() ||
      !IsRegisteredAppearanceDisplayActive(a_actor->GetFormID())) {
    return;
  }

  auto *thirdPersonRoot = a_actor->Get3D(false);
  auto *firstPersonRoot = a_actor->Get3D(true);
  RememberAndMorphNewNodes(
      bodyMorph, a_actor,
      FindNewAttachmentRoots(thirdPersonRoot, a_before.thirdPersonObjects),
      a_armorFormID, false);
  if (firstPersonRoot != thirdPersonRoot) {
    RememberAndMorphNewNodes(
        bodyMorph, a_actor,
        FindNewAttachmentRoots(firstPersonRoot,
                               a_before.firstPersonObjects),
        a_armorFormID, true);
  }
}

void SetRegisteredAppearanceDisplayActive(RE::Actor *a_actor,
                                          const bool a_active) {
  if (!a_actor) {
    return;
  }
  const auto actorFormID = a_actor->GetFormID();
  std::lock_guard lock(g_nodeMutex);
  if (a_active) {
    g_activeRegisteredAppearanceActors.insert(actorFormID);
  } else {
    g_activeRegisteredAppearanceActors.erase(actorFormID);
    g_registeredAppearanceNodes.erase(actorFormID);
    g_observedModelWeightMorphActors.erase(actorFormID);
    g_observedEmptyModelWeightMorphActors.erase(actorFormID);
  }
}

void ForgetRegisteredAppearanceNodes(RE::Actor *a_actor) {
  if (!a_actor) {
    return;
  }
  std::lock_guard lock(g_nodeMutex);
  g_registeredAppearanceNodes.erase(a_actor->GetFormID());
}

void ForgetAllRegisteredAppearanceNodes() {
  std::lock_guard lock(g_nodeMutex);
  g_registeredAppearanceNodes.clear();
  g_activeRegisteredAppearanceActors.clear();
  g_observedModelWeightMorphActors.clear();
  g_observedEmptyModelWeightMorphActors.clear();
}
} // namespace sfs::native::racemenu
