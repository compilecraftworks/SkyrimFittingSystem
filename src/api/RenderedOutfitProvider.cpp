#include "api/RenderedOutfitProvider.h"

#ifndef SFS_RENDERED_OUTFIT_TEST
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#endif
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace sfs::api::rendered {
namespace {
std::mutex mutex;
State state;
std::unordered_map<std::uint32_t, Value> prepared;
struct Observation {
  std::uintptr_t root{0}; // Compare only; never dereferenced or serialized.
  bool awaitingSkinning{false};
  bool suspended{false};
  bool unavailable{false};
  std::uint64_t publishedScene{0};
};
std::unordered_map<std::uint32_t, Observation> observations;
std::atomic_bool gameReady{false}, pumpQueued{false}, workPending{false};
std::atomic<DWORD> gameThread{0};
DecisionObserver decisionObserver{nullptr};
constexpr std::size_t kPublicationBatch = 64;

void ObserveRoot(const std::uint32_t id, const std::uintptr_t root,
                 const bool unavailable) {
  // Caller holds mutex. Notifications drive publication; Query also validates
  // its own actor's root, without a background scan of all subscribed actors.
  const auto* entry = state.Find(id);
  auto& observation = observations[id];
  if (observation.unavailable != unavailable) {
    observation.unavailable = unavailable;
    state.Touch(id);
    workPending.store(true);
  }
  if (observation.root == root) { return; }
  if (!root || (observation.root && entry->scene == observation.publishedScene)) {
    prepared.erase(id);
  }
  observation.root = root;
  state.Touch(id, true);
  workPending.store(true);
}

void Pump() {
  // Only SKSE game tasks call this, not the presentation/render thread.
  gameThread.store(::GetCurrentThreadId());
  pumpQueued.store(false);
  std::vector<std::uint32_t> actors;
  {
    std::lock_guard lock(mutex);
    if (gameReady.load()) { actors = state.PendingActors(kPublicationBatch); }
  }
  for (const auto id : actors) {
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
    const auto root = actor ? reinterpret_cast<std::uintptr_t>(actor->Get3D(false)) : 0;
    std::uint64_t ticket = 0, epoch = 0;
    bool waiting = false, suspended = false;
    {
      std::lock_guard lock(mutex);
      const auto* entry = state.Find(id);
      if (!entry || !gameReady.load()) { continue; }
      if (!actor) {
        state.Erase(id, abi::Status::InvalidActor);
        observations.erase(id);
        prepared.erase(id);
        continue;
      }
      ObserveRoot(id, root, !root || actor->IsDisabled() || actor->IsDeleted());
      const auto& observation = observations[id];
      if (!entry->dirty) { continue; }
      waiting = observation.awaitingSkinning;
      suspended = observation.suspended;
      ticket = entry->ticket; epoch = state.Epoch();
    }
    Value value;
    if (root && !suspended && !actor->IsDisabled() && !actor->IsDeleted()) {
      value = CaptureValue(id);
    }
    {
      std::lock_guard lock(mutex);
      if (gameReady.load()) {
        // DAVE can retain unchanged attachments. Compare in place, rather than
        // allocating a second copy of the last published outfit for each actor.
        const auto* previous = state.Find(id);
        if (waiting && value.status == abi::Status::Ready &&
            (!previous || !(value == previous->value))) { value = {}; }
        state.Publish(id, epoch, ticket, std::move(value));
        if (const auto* current = state.Find(id); current && !current->dirty &&
            current->value.status == abi::Status::Ready) {
          observations[id].publishedScene = current->scene;
        }
      }
    }
  }
  std::vector<abi::Changed> events;
  {
    std::lock_guard lock(mutex);
    events = state.Drain(kPublicationBatch);
    workPending.store(state.HasWork());
  }
  // Synchronous consumers can query/reenter, invalidate, or trigger a load.
  // Never hold our mutex or keep an engine/actor pointer across Dispatch.
  for (auto& event : events) {
    DecisionObserver observer = nullptr;
    {
      std::lock_guard lock(mutex);
      if (event.epoch != state.Epoch()) { break; }
      const auto* current = state.Find(event.actorFormID);
      if (current && (current->dirty || current->revision != event.revision)) {
        continue;
      }
      if (event.actorFormID && (event.reasons &
          (abi::StateChanged | abi::AvailabilityChanged)) &&
          (event.status == abi::Status::Ready || event.status == abi::Status::NotManaged)) {
        observer = decisionObserver;
      }
    }
    if (observer) { observer(event.actorFormID); }
    if (auto* messaging = SKSE::GetMessagingInterface()) {
      messaging->Dispatch(abi::kChangedMessage, &event, sizeof(event), nullptr);
    }
  }
}

class Events final : public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
                     public RE::BSTEventSink<RE::TESFormDeleteEvent> {
  RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,
      RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {
    if (event) {
      std::lock_guard lock(mutex);
      // Warm producer data is also discarded for actors not queried yet.
      if (!event->loaded) { prepared.erase(event->formID); }
      if (state.Contains(event->formID)) {
        auto& observation = observations[event->formID];
        observation.suspended = !event->loaded;
        observation.awaitingSkinning = false;
        state.Touch(event->formID, true);
        workPending.store(true);
      }
    }
    return RE::BSEventNotifyControl::kContinue;
  }
  RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* event,
      RE::BSTEventSource<RE::TESFormDeleteEvent>*) override {
    if (event) {
      std::lock_guard lock(mutex);
      state.Erase(event->formID, abi::Status::InvalidActor);
      observations.erase(event->formID);
      prepared.erase(event->formID);
      workPending.store(state.HasWork());
    }
    return RE::BSEventNotifyControl::kContinue;
  }
public:
  static Events& Get() { static Events result; return result; }
};
} // namespace

void SetGameReady(const bool ready) {
  std::lock_guard lock(mutex);
  if (!ready && gameReady.exchange(false)) {
    state.Reset(); observations.clear(); prepared.clear();
    workPending.store(true);
  } else if (ready) { gameReady.store(true); }
}
void RegisterEvents() {
  static std::once_flag once;
  std::call_once(once, [] {
    auto* source = RE::ScriptEventSourceHolder::GetSingleton();
    if (source) {
      source->AddEventSink<RE::TESObjectLoadedEvent>(&Events::Get());
      source->AddEventSink<RE::TESFormDeleteEvent>(&Events::Get());
    }
  });
}
void SetDecisionObserver(DecisionObserver observer) {
  std::lock_guard lock(mutex);
  decisionObserver = observer;
}
std::shared_ptr<const Value> AcquirePublished(const std::uint32_t id) {
  if (!id || !gameReady.load()) { return {}; }
  std::lock_guard lock(mutex);
  if (!gameReady.load()) { return {}; }
  return state.Acquire(id);
}
void QueuePump() {
  // Idle frames take only an atomic check: no mutex, actor walk or game task.
  // One initial task establishes the game-thread identity.
  if (gameThread.load() != 0 && !workPending.load()) { return; }
  if (pumpQueued.exchange(true)) { return; }
  if (auto* tasks = SKSE::GetTaskInterface()) {
    tasks->AddTask([] {
      try { Pump(); }
      catch (const std::exception& e) {
        pumpQueued.store(false);
        logger::error("SFS rendered outfit publication failed: {}", e.what());
      }
    });
  } else { pumpQueued.store(false); }
}
void NotifyRefresh(const std::uint32_t id, const bool awaitsSkinning) {
  std::lock_guard lock(mutex);
  // No enrollment/scanning of unrelated NPCs. A first Query subscribes an actor.
  if (state.Contains(id)) {
    state.Touch(id);
    observations[id].awaitingSkinning = awaitsSkinning;
    workPending.store(true);
  }
}
void NotifySkinning(const std::uint32_t id) {
  std::lock_guard lock(mutex);
  if (state.Contains(id)) {
    state.Touch(id, true);
    observations[id].awaitingSkinning = false;
    workPending.store(true);
  }
}

Value CaptureValue(const std::uint32_t id) {
  {
    std::lock_guard lock(mutex);
    if (const auto found = prepared.find(id); found != prepared.end()) {
      return found->second;
    }
  }
  return {HasDisplayConfiguration(id) ? abi::Status::NotReady : abi::Status::NotManaged,
          0, {}};
}
void PrepareValue(const std::uint32_t id, Value& value) {
  value.Normalize();
  std::lock_guard lock(mutex);
  if (!gameReady.load()) { return; }
  auto found = prepared.find(id);
  if (found != prepared.end() && found->second == value) { return; }
  prepared.insert_or_assign(id, value);
  if (decisionObserver || state.Contains(id)) {
    state.Touch(id); workPending.store(true);
  }
}
void ForgetPreparedValue(const std::uint32_t id) {
  std::lock_guard lock(mutex);
  if (state.Contains(id)) {
    const auto found = prepared.find(id);
    if (found != prepared.end() && found->second.status == abi::Status::NotManaged) { return; }
    prepared.insert_or_assign(id, Value{abi::Status::NotManaged, 0, {}});
    state.Touch(id);
    workPending.store(true);
  } else { prepared.erase(id); }
}

abi::Status QueryImpl(const std::uint32_t actorID, abi::Snapshot* output,
    abi::Item* items, const std::uint32_t capacity, const std::uint32_t itemSize,
    const bool fromGameTask) {
  if (!output || output->structSize < sizeof(abi::Snapshot) ||
      itemSize != sizeof(abi::Item) || (!items && capacity != 0)) {
    return abi::Status::InvalidArgument;
  }
  *output = {sizeof(abi::Snapshot), abi::kVersion, 0, 0, 0, actorID,
             abi::Status::NotReady, 0, 0, 0, 0};
  if (!actorID) { return output->status = abi::Status::InvalidActor; }
  if (!fromGameTask) {
    // Preserve the original export's contract for existing consumers. An OS
    // thread ID cannot identify the SKSE task phase: successive task batches
    // may execute on different threads. Task-aware consumers use the explicit
    // OnGameTask export, and must enforce that call-site precondition themselves.
    const auto thread = gameThread.load();
    if (!thread) { return output->status; }
    if (thread != ::GetCurrentThreadId()) {
      return output->status = abi::Status::WrongThread;
    }
  }
  if (!gameReady.load()) {
    std::lock_guard lock(mutex); output->epoch = state.Epoch(); return output->status;
  }
  const auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
  if (!actor || actor->IsDeleted()) { return output->status = abi::Status::InvalidActor; }
  const auto root = reinterpret_cast<std::uintptr_t>(actor->Get3D(false));
  const auto unavailable = !root || actor->IsDisabled();
  std::lock_guard lock(mutex);
  if (!gameReady.load()) { output->epoch = state.Epoch(); return output->status; }
  if (!state.Contains(actorID)) { state.Touch(actorID); workPending.store(true); }
  ObserveRoot(actorID, root, unavailable);
  if (observations[actorID].suspended || unavailable) {
    output->epoch = state.Epoch();
    return output->status;
  }
  return state.Copy(actorID, *output, items, capacity);
}

abi::Status Query(const std::uint32_t actorID, abi::Snapshot* output,
    abi::Item* items, const std::uint32_t capacity, const std::uint32_t itemSize,
    const bool fromGameTask = false) noexcept {
  try { return QueryImpl(actorID, output, items, capacity, itemSize, fromGameTask); }
  catch (...) {
    if (output && output->structSize >= sizeof(*output)) {
      *output = {sizeof(*output), abi::kVersion, 0, 0, 0,
                 actorID, abi::Status::NotReady, 0, 0, 0, 0};
    }
    return abi::Status::NotReady;
  }
}
} // namespace sfs::api::rendered

extern "C" __declspec(dllexport) std::uint32_t __cdecl
SkyrimFittingSystem_GetRenderedOutfitAPIVersion() {
  return sfs::rendered_outfit_api::kVersion;
}
extern "C" __declspec(dllexport) sfs::rendered_outfit_api::Status __cdecl
SkyrimFittingSystem_QueryRenderedOutfit(const std::uint32_t actorID,
    sfs::rendered_outfit_api::Snapshot* output, sfs::rendered_outfit_api::Item* items,
    const std::uint32_t capacity, const std::uint32_t itemSize) {
  return sfs::api::rendered::Query(actorID, output, items, capacity, itemSize);
}

// Same v1 data/ownership ABI, with an explicit SKSE AddTask execution contract.
// Not a worker/render-thread API. Root, availability, dirty-state and generation
// validation remain identical to the original export.
extern "C" __declspec(dllexport) sfs::rendered_outfit_api::Status __cdecl
SkyrimFittingSystem_QueryRenderedOutfitOnGameTask(const std::uint32_t actorID,
    sfs::rendered_outfit_api::Snapshot* output, sfs::rendered_outfit_api::Item* items,
    const std::uint32_t capacity, const std::uint32_t itemSize) {
  return sfs::api::rendered::Query(actorID, output, items, capacity, itemSize, true);
}
