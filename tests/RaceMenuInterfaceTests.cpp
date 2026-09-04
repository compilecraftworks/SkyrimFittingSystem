#include "native/RaceMenuInterfaces.h"
#include "native/RegisteredAppearanceMorphRules.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace race_menu_test {
namespace abi = sfs::native::racemenu::abi;
void ExerciseBodyMorph(abi::IBodyMorphInterface *, RE::TESObjectREFR *,
                       RE::NiAVObject *);
void ExerciseTransform(abi::INiTransformInterface *, RE::TESObjectREFR *, bool);
}

namespace {
namespace abi = sfs::native::racemenu::abi;
namespace rules = sfs::native::racemenu::rules;
using Status = abi::AttachmentRegistrationStatus;

void Expect(bool a_ok, const char *a_message) {
  if (!a_ok) {
    std::cerr << "FAIL: " << a_message << '\n';
    std::exit(1);
  }
}

[[noreturn]] void UnexpectedSlot() {
  // A wrong slot must fail deterministically, never corrupt a neighboring
  // mutex or silently pass because the mock shares SFS's mistaken vtable.
  std::cerr << "FAIL: called an unverified RaceMenu vtable slot\n";
  std::exit(2);
}

// Independent provider-side MSVC x64 ABI fixture. Literal slots come from
// upstream headers/binary analysis, NOT offsetof/consumer member declarations.
// No game DLL is loaded or executed. Unused slots deliberately trap.
struct Provider {
  std::uintptr_t *vptr;
  std::array<std::uintptr_t, 28> slots;
  std::uint32_t version;
  int versionCalls{0};
  int registerCalls{0};
  std::atomic_bool *published{nullptr};
  abi::IAddonAttachmentInterface *observer{nullptr};
  bool throwOnRegister{false};
  RE::TESObjectREFR *actor{nullptr};
  RE::NiAVObject *node{nullptr};
  bool female{false};
  std::vector<int> calls;

  explicit Provider(std::uint32_t a_version) : vptr(slots.data()), version(a_version) {
    slots.fill(reinterpret_cast<std::uintptr_t>(&UnexpectedSlot));
    slots[1] = reinterpret_cast<std::uintptr_t>(&Version);
  }
  Provider(const Provider &) = delete;
  Provider &operator=(const Provider &) = delete;

  abi::IPluginInterface *Plugin() {
    return reinterpret_cast<abi::IPluginInterface *>(this);
  }

  static std::uint32_t Version(Provider *a_self) {
    ++a_self->versionCalls;
    return a_self->version;
  }
  static void Register(Provider *a_self, abi::IAddonAttachmentInterface *a_observer) {
    Expect(a_self->versionCalls == 1, "GetVersion must precede the versioned cast/call");
    Expect(a_self->published && !a_self->published->load(),
           "Registration must not be published before AddInterface returns");
    ++a_self->registerCalls;
    if (a_self->throwOnRegister) {
      throw std::runtime_error("provider registration failure");
    }
    a_self->observer = a_observer;
  }

  static void Vertex(Provider *a_self, RE::TESObjectREFR *a_actor,
                     RE::NiAVObject *a_node, bool a_erase) {
    Expect(a_actor == a_self->actor && a_node == a_self->node && !a_erase,
           "ApplyVertexDiff slot 12 must preserve actor/node and erase=false");
    a_self->calls.push_back(12);
  }
  static void Body(Provider *a_self, RE::TESObjectREFR *a_actor, bool a_defer) {
    Expect(a_actor == a_self->actor && a_defer,
           "ApplyBodyMorphs slot 13 must preserve actor and deferUpdate");
    a_self->calls.push_back(13);
  }
  static void CheckTransform(Provider *a_self, RE::TESObjectREFR *a_actor,
                             bool a_first, bool a_female, const char *a_node) {
    Expect(a_actor == a_self->actor && !a_first && a_female == a_self->female &&
               std::string_view(a_node) == "NPC",
           "NiTransform must preserve actor/first-person/gender/node arguments");
  }
  static void AddPosition(Provider *a_self, RE::TESObjectREFR *a_actor,
                          bool a_first, bool a_female, const char *a_node,
                          const char *a_key, const float *a_xyz) {
    CheckTransform(a_self, a_actor, a_first, a_female, a_node);
    Expect(std::string_view(a_key) == "SFS_HH_SYNC" &&
               a_xyz[0] == 0 && a_xyz[1] == 0 && a_xyz[2] == 0,
           "Position ABI must be three floats and the bootstrap must be neutral");
    a_self->calls.push_back(7);
  }
  static bool RemovePosition(Provider *a_self, RE::TESObjectREFR *a_actor,
                             bool a_first, bool a_female, const char *a_node,
                             const char *a_key) {
    CheckTransform(a_self, a_actor, a_first, a_female, a_node);
    Expect(std::string_view(a_key) == "SFS_HH_SYNC", "Remove only the SFS bootstrap key");
    a_self->calls.push_back(15);
    return true;
  }
  static void UpdateAll(Provider *a_self, RE::TESObjectREFR *a_actor) {
    Expect(a_actor == a_self->actor, "UpdateNodeAllTransforms actor isolation");
    a_self->calls.push_back(22);
  }
  static void UpdateNode(Provider *a_self, RE::TESObjectREFR *a_actor,
                         bool a_first, bool a_female, const char *a_node) {
    CheckTransform(a_self, a_actor, a_first, a_female, a_node);
    a_self->calls.push_back(24);
  }
};

class Observer final : public abi::IAddonAttachmentInterface {
public:
  std::array<const void *, 6> received{};
  bool firstPerson{false};
  void OnAttach(RE::TESObjectREFR *a_ref, RE::TESObjectARMO *a_armor,
                RE::TESObjectARMA *a_addon, RE::NiAVObject *a_object,
                bool a_first, RE::NiNode *a_skeleton, RE::NiNode *a_root) override {
    received = {a_ref, a_armor, a_addon, a_object, a_skeleton, a_root};
    firstPerson = a_first;
  }
};

void TestRegistrationAndAttachment() {
  for (auto version : {0U, 1U, 2U}) {
    Provider provider(version);
    // Legacy concrete class: 3. Public v1/v2: 11. Keeping these independent
    // catches the exact v1.5.3 startup regression.
    provider.slots[version == 0 ? 3 : 11] =
        reinterpret_cast<std::uintptr_t>(&Provider::Register);
    std::atomic_bool registered{false};
    provider.published = &registered;
    Observer observer;
    const auto result = abi::RegisterAttachmentObserver(provider.Plugin(), &observer, registered);
    Expect(result.status == Status::Registered && result.version == version &&
               registered && provider.registerCalls == 1 && provider.observer == &observer,
           "ActorUpdateManager 0/1/2 must register on the correct slot");
    Expect(abi::RegisterAttachmentObserver(provider.Plugin(), &observer, registered).status ==
               Status::AlreadyRegistered && provider.registerCalls == 1 && provider.versionCalls == 1,
           "DataLoaded retry must not register the process-lifetime observer twice");

    // Provider calls the callback at its own ABI slot 0; a virtual destructor
    // added to SFS's observer declaration would shift this and fail the test.
    std::array<std::uintptr_t, 6> objects{};
    const auto callback = reinterpret_cast<void (*)(void *, RE::TESObjectREFR *,
        RE::TESObjectARMO *, RE::TESObjectARMA *, RE::NiAVObject *, bool,
        RE::NiNode *, RE::NiNode *)>(
            (*reinterpret_cast<std::uintptr_t **>(provider.observer))[0]);
    for (const bool firstPerson : {false, true}) {
      callback(provider.observer, reinterpret_cast<RE::TESObjectREFR *>(&objects[0]),
          reinterpret_cast<RE::TESObjectARMO *>(&objects[1]),
          reinterpret_cast<RE::TESObjectARMA *>(&objects[2]),
          reinterpret_cast<RE::NiAVObject *>(&objects[3]), firstPerson,
          reinterpret_cast<RE::NiNode *>(&objects[4]), reinterpret_cast<RE::NiNode *>(&objects[5]));
      for (std::size_t i = 0; i < objects.size(); ++i) {
        Expect(observer.received[i] == &objects[i], "OnAttach must preserve every pointer argument");
      }
      Expect(observer.firstPerson == firstPerson, "OnAttach first-person ABI");
    }
    std::cout << "ActorUpdateManager v" << version << " registration/OnAttach passed\n";
  }
}

void TestFailureAndThreadedRetry() {
  Observer observer;
  std::atomic_bool registered{false};
  Expect(abi::RegisterAttachmentObserver(nullptr, &observer, registered).status ==
             Status::Unavailable && !registered, "Missing provider must remain retryable");
  for (auto version : {3U, 99U, UINT32_MAX}) {
    Provider unknown(version);
    const auto result = abi::RegisterAttachmentObserver(unknown.Plugin(), &observer, registered);
    Expect(result.status == Status::UnsupportedVersion && result.version == version &&
               unknown.versionCalls == 1 && !registered,
           "Unverified versions must never call beyond the stable prefix");
  }
  Provider provider(0);
  provider.slots[3] = reinterpret_cast<std::uintptr_t>(&Provider::Register);
  provider.published = &registered;
  Expect(abi::RegisterAttachmentObserver(provider.Plugin(), nullptr, registered).status ==
             Status::Unavailable && provider.versionCalls == 0, "Missing observer must not call provider");
  std::mutex initializationMutex;
  provider.throwOnRegister = true;
  try {
    std::lock_guard lock(initializationMutex);
    (void)abi::RegisterAttachmentObserver(provider.Plugin(), &observer, registered);
    Expect(false, "Expected provider C++ exception");
  } catch (const std::runtime_error &) {
    Expect(!registered, "A failed registration must not publish success");
  }
  provider.throwOnRegister = false;
  provider.versionCalls = 0;
  // Match startup: PostPostLoad and DataLoaded may run on different threads.
  std::promise<Status> promise;
  auto completion = promise.get_future();
  std::thread retry([&]() {
    std::lock_guard lock(initializationMutex);
    promise.set_value(abi::RegisterAttachmentObserver(provider.Plugin(), &observer, registered).status);
  });
  Expect(completion.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
         "Initialization mutex must be available to the next startup thread");
  Expect(completion.get() == Status::Registered, "Failed registration must retry successfully");
  retry.join();
  std::array<std::thread, 4> repeat;
  for (auto &thread : repeat) {
    thread = std::thread([&]() {
      std::lock_guard lock(initializationMutex);
      Expect(abi::RegisterAttachmentObserver(provider.Plugin(), &observer, registered).status ==
                 Status::AlreadyRegistered, "Repeated concurrent startup must be idempotent");
    });
  }
  for (auto &thread : repeat) thread.join();
  Expect(provider.registerCalls == 2, "Only the failed attempt and one retry may call AddInterface");
}

void TestIndependentMorphAndTransformVersions() {
  Expect(abi::kApplyBodyMorphsVtableIndex == 13 &&
             abi::kUpdateModelWeightRunSlot == 0 &&
             abi::kUpdateModelWeightFormIDOffset == 8,
         "Hook slots/member access must match the independently verified x64 layout");
  for (bool clearInternal : {false, true}) {
    Expect(rules::ResolveLegacyHighHeelCompletion(false, clearInternal) ==
               rules::LegacyHighHeelCompletion::Failed,
           "Bootstrap cleanup must not turn a failed Papyrus update into success");
    Expect(rules::ResolveLegacyHighHeelCompletion(true, clearInternal) ==
               (clearInternal ? rules::LegacyHighHeelCompletion::RemoveInternalPosition
                              : rules::LegacyHighHeelCompletion::Synchronized),
           "Only a successful transform update may advance the legacy HH chain");
  }
  std::array<std::uintptr_t, 4> objects{};
  for (auto morphVersion : {0U, 1U, 2U, 3U, 4U, 5U, 6U, UINT32_MAX}) {
    for (auto transformVersion : {0U, 1U, 2U, 3U, 4U, UINT32_MAX}) {
      Expect(rules::IsPublicBodyMorphInterfaceCompatible(morphVersion) ==
                 (morphVersion == 4 || morphVersion == 5), "Independent BodyMorph version policy");
      const auto route = rules::ResolveHighHeelTransformRoute(transformVersion);
      Expect(route == (transformVersion == 3 ? rules::HighHeelTransformRoute::PublicInterface :
              transformVersion == 1 || transformVersion == 2 ? rules::HighHeelTransformRoute::LegacyPapyrus :
              rules::HighHeelTransformRoute::Unavailable), "Independent NiTransform version policy");
    }
  }
  for (auto version : {4U, 5U}) {
    Provider provider(version);
    provider.slots[12] = reinterpret_cast<std::uintptr_t>(&Provider::Vertex);
    provider.slots[13] = reinterpret_cast<std::uintptr_t>(&Provider::Body);
    for (auto actorIndex : {0U, 1U}) {
      provider.actor = reinterpret_cast<RE::TESObjectREFR *>(&objects[actorIndex]);
      provider.node = reinterpret_cast<RE::NiAVObject *>(&objects[actorIndex + 2]);
      race_menu_test::ExerciseBodyMorph(static_cast<abi::IBodyMorphInterface *>(provider.Plugin()),
                                       provider.actor, provider.node);
    }
    Expect(provider.calls == std::vector<int>({12, 13, 12, 13}), "BodyMorph v4/v5 slot dispatch");
    std::cout << "BodyMorph v" << version << " dispatch passed\n";
  }
  Provider transform(3);
  transform.slots[7] = reinterpret_cast<std::uintptr_t>(&Provider::AddPosition);
  transform.slots[15] = reinterpret_cast<std::uintptr_t>(&Provider::RemovePosition);
  transform.slots[22] = reinterpret_cast<std::uintptr_t>(&Provider::UpdateAll);
  transform.slots[24] = reinterpret_cast<std::uintptr_t>(&Provider::UpdateNode);
  for (auto actorIndex : {0U, 1U}) {
    transform.actor = reinterpret_cast<RE::TESObjectREFR *>(&objects[actorIndex]);
    transform.female = actorIndex == 1;
    race_menu_test::ExerciseTransform(static_cast<abi::INiTransformInterface *>(transform.Plugin()),
                                     transform.actor, transform.female);
  }
  Expect(transform.calls == std::vector<int>({7, 22, 15, 24, 7, 22, 15, 24}),
         "NiTransform v3 position/update slots must preserve actor-local call order");
}
} // namespace

int main() {
  static_assert(sizeof(void *) == 8, "Tests model RaceMenu's SE/AE x64 ABI");
  static_assert(sizeof(abi::INiTransformInterface::Position) == 12);
  static_assert(sizeof(abi::INiTransformInterface::Rotation) == 12);
  static_assert(sizeof(abi::InterfaceExchangeMessage) == 8);
  TestRegistrationAndAttachment();
  TestFailureAndThreadedRetry();
  TestIndependentMorphAndTransformVersions();
  std::cout << "RaceMenu interface ABI regression tests passed\n";
}
