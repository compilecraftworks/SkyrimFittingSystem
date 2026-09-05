#include "native/GenitalArmorResolver.h"

#include "native/ArmorSkinning.h"
#include "native/GenitalCompatibility.h"

#include <mutex>
#include <unordered_map>

namespace {
struct ResolverState {
  std::uint64_t generation{0};
  RE::FormID armorFormID{0};
  bool pending{false};
  bool attempted{false};
};

std::mutex g_resolverMutex;
std::unordered_map<RE::FormID, ResolverState> g_resolverStates;

void CompleteResolution(const RE::FormID a_actorFormID,
                        const std::uint64_t a_generation,
                        const RE::FormID a_armorFormID) {
  bool changed = false;
  {
    std::lock_guard lock(g_resolverMutex);
    const auto stateIt = g_resolverStates.find(a_actorFormID);
    if (stateIt == g_resolverStates.end() ||
        stateIt->second.generation != a_generation) {
      return;
    }

    auto &state = stateIt->second;
    state.pending = false;
    state.attempted = true;
    changed = state.armorFormID != a_armorFormID;
    state.armorFormID = a_armorFormID;
  }

  logger::debug(
      "SFS genital resolver: actor={:08X} generation={} armor={:08X} changed={}",
      a_actorFormID, a_generation, a_armorFormID, changed);
  if (changed) {
    auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
    sfs::native::QueueArmorRefreshFor(actor);
  }
}

class GenitalArmorCallback final
    : public RE::BSScript::IStackCallbackFunctor {
public:
  GenitalArmorCallback(const RE::FormID a_actorFormID,
                       const std::uint64_t a_generation)
      : actorFormID_(a_actorFormID), generation_(a_generation) {}

  void operator()(RE::BSScript::Variable a_result) override {
    const auto *armor = a_result.Unpack<RE::TESObjectARMO *>();
    CompleteResolution(actorFormID_, generation_,
                       armor ? armor->GetFormID() : 0);
  }

  void SetObject(
      const RE::BSTSmartPointer<RE::BSScript::Object> &) override {}

private:
  RE::FormID actorFormID_{0};
  std::uint64_t generation_{0};
};

class ActiveAddonCallback final
    : public RE::BSScript::IStackCallbackFunctor {
public:
  ActiveAddonCallback(const RE::FormID a_actorFormID,
                      const std::uint64_t a_generation)
      : actorFormID_(a_actorFormID), generation_(a_generation) {}

  void operator()(RE::BSScript::Variable a_result) override {
    auto *addon = a_result.Unpack<RE::TESQuest *>();
    auto *vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!addon || !vm) {
      CompleteResolution(actorFormID_, generation_, 0);
      return;
    }

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
        new GenitalArmorCallback(actorFormID_, generation_));
    if (!vm->DispatchStaticCall(
            "SOS_Data", "GetGenitalArmor",
            RE::MakeFunctionArguments(static_cast<RE::TESForm *>(addon)),
            callback)) {
      CompleteResolution(actorFormID_, generation_, 0);
    }
  }

  void SetObject(
      const RE::BSTSmartPointer<RE::BSScript::Object> &) override {}

private:
  RE::FormID actorFormID_{0};
  std::uint64_t generation_{0};
};
} // namespace

namespace sfs::native {
void ClearResolvedGenitalArmors() {
  std::lock_guard lock(g_resolverMutex);
  g_resolverStates.clear();
}

void RememberGenitalArmor(RE::Actor *a_actor,
                          const RE::TESObjectARMO *a_armor) {
  if (!a_actor || !a_armor) {
    return;
  }

  std::lock_guard lock(g_resolverMutex);
  auto &state = g_resolverStates[a_actor->GetFormID()];
  ++state.generation;
  state.armorFormID = a_armor->GetFormID();
  state.pending = false;
  state.attempted = true;
}

void ForgetGenitalArmor(RE::Actor *a_actor) {
  if (!a_actor) {
    return;
  }

  std::lock_guard lock(g_resolverMutex);
  auto &state = g_resolverStates[a_actor->GetFormID()];
  ++state.generation;
  state.armorFormID = 0;
  state.pending = false;
  state.attempted = true;
}

void RequestGenitalArmorResolution(RE::Actor *a_actor, const bool a_force) {
  if (!a_actor ||
      !sfs::native::genital_compatibility::IsSosInstalled()) {
    return;
  }

  const auto actorFormID = a_actor->GetFormID();
  std::uint64_t generation = 0;
  {
    std::lock_guard lock(g_resolverMutex);
    auto &state = g_resolverStates[actorFormID];
    if (state.pending || (!a_force && state.attempted)) {
      return;
    }

    generation = ++state.generation;
    state.pending = true;
    state.attempted = false;
  }

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  auto *apiForm =
      dataHandler
          ? dataHandler->LookupForm(0x1EDA4, "Schlongs of Skyrim.esp")
          : nullptr;
  if (!apiForm) {
    apiForm = RE::TESForm::LookupByEditorID<RE::TESQuest>("SOS_Misc");
  }
  auto *vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
  auto *handlePolicy = vm ? vm->GetObjectHandlePolicy() : nullptr;
  if (!apiForm || !vm || !handlePolicy) {
    CompleteResolution(actorFormID, generation, 0);
    return;
  }

  const auto handle =
      handlePolicy->GetHandleForObject(apiForm->GetFormType(), apiForm);
  if (handle == handlePolicy->EmptyHandle()) {
    CompleteResolution(actorFormID, generation, 0);
    return;
  }

  RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
      new ActiveAddonCallback(actorFormID, generation));
  if (!vm->DispatchMethodCall(
          handle, "SOS_API", "GetSchlong",
          RE::MakeFunctionArguments(static_cast<RE::Actor *>(a_actor)),
          callback)) {
    CompleteResolution(actorFormID, generation, 0);
  }
}

const RE::TESObjectARMO *GetResolvedGenitalArmor(RE::Actor *a_actor) {
  if (!a_actor) {
    return nullptr;
  }

  RE::FormID armorFormID = 0;
  {
    std::lock_guard lock(g_resolverMutex);
    const auto stateIt = g_resolverStates.find(a_actor->GetFormID());
    if (stateIt == g_resolverStates.end()) {
      return nullptr;
    }
    armorFormID = stateIt->second.armorFormID;
  }

  return RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID);
}
} // namespace sfs::native
