// Compile the real queue state declaration, enrollment, cancellation and
// dispatch bodies. Game state and the main-thread task runner are controlled
// boundaries; condition evaluation/rendering themselves are not simulated.
#include <atomic>
#include <array>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static unsigned failures{}, polls{}, refreshes{};
static void Check(bool ok, const char *message) {
  if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
namespace RE {
using FormID = std::uint32_t;
enum class BSEventNotifyControl { kContinue };
struct TESEquipEvent {};
struct TESActorLocationChangeEvent {};
struct TESCellFullyLoadedEvent {};
struct TESCombatEvent {};
struct TESFastTravelEndEvent {};
template<class T> struct BSTEventSource {};
template<class T> struct BSTEventSink {
  virtual ~BSTEventSink() = default;
  virtual BSEventNotifyControl ProcessEvent(const T *, BSTEventSource<T> *) = 0;
};
}
namespace SKSE {
struct ModCallbackEvent {};
bool available = true;
std::function<void()> onLookup;
struct Tasks {
  std::vector<std::function<void()>> pending;
  void AddTask(std::function<void()> task) { pending.push_back(std::move(task)); }
  void Drain() {
    auto batch = std::move(pending); pending.clear();
    for (auto &task : batch) { task(); }
  }
} tasks;
Tasks *GetTaskInterface() {
  if (onLookup) { auto callback = std::move(onLookup); onLookup = {}; callback(); }
  return available ? &tasks : nullptr;
}
}
namespace logger { template<class... T> void warn(T&&...) {} }
namespace sfs {
bool ready = true;
std::function<void()> onRefresh;
struct Menu {
  static Menu *GetSingleton() { static Menu menu; return &menu; }
  bool IsGameDataLoaded() const { return ready; }
  int GetConditions() { return 0; }
  struct Workbench {
    void RefreshNativeArmorOverrides(int, int, bool) {
      ++refreshes;
      if (onRefresh) { auto callback = std::move(onRefresh); onRefresh = {}; callback(); }
    }
  } workbench;
  Workbench &GetWorkbench() { return workbench; }
};
}
#include "workbench/EquipmentRefreshEventSink.h"
namespace {
#include "EquipmentQueueGlobals.production.inc"
void ClearAutomaticEquipmentStateEvents() {}
}
namespace sfs::workbench {
#include "EquipmentQueue.production.inc"
void EquipmentRefreshEventSink::PollConditionState() { ++polls; }
// Unexercised virtual event entry points only satisfy the concrete singleton.
#define EMPTY_EVENT(Event) \
RE::BSEventNotifyControl EquipmentRefreshEventSink::ProcessEvent( \
    const Event *, RE::BSTEventSource<Event> *) { return RE::BSEventNotifyControl::kContinue; }
EMPTY_EVENT(RE::TESEquipEvent)
EMPTY_EVENT(RE::TESActorLocationChangeEvent)
EMPTY_EVENT(RE::TESCellFullyLoadedEvent)
EMPTY_EVENT(RE::TESCombatEvent)
EMPTY_EVENT(SKSE::ModCallbackEvent)
#if defined(EXCLUSIVE_SKYRIM_FLAT)
EMPTY_EVENT(RE::TESFastTravelEndEvent)
#endif
#undef EMPTY_EVENT
}

static void TestConcurrentTickets() {
  sfs::workbench::CoalescedTask task;
  std::barrier rendezvous{5};
  std::array<std::uint64_t, 4> tickets{};
  std::vector<std::thread> producers;
  for (unsigned i = 0; i != tickets.size(); ++i) {
    producers.emplace_back([&, i] {
      for (unsigned cycle = 0; cycle != 128; ++cycle) {
        rendezvous.arrive_and_wait();
        tickets[i] = task.Start().value_or(0);
        rendezvous.arrive_and_wait();
      }
    });
  }
  for (unsigned cycle = 0; cycle != 128; ++cycle) {
    rendezvous.arrive_and_wait(); rendezvous.arrive_and_wait();
    unsigned owners = 0;
    for (auto ticket : tickets) { owners += ticket != 0; }
    Check(owners == 1, "concurrent producers have exactly one pending owner");
    task.Cancel();
    const auto replacement = task.Start();
    Check(replacement.has_value(), "cancellation permits new enrollment immediately");
    for (auto ticket : tickets) { Check(!task.Finish(ticket), "old/empty tickets cannot finish new work"); }
    Check(!task.Start().has_value(), "new work still coalesces after stale completions");
    Check(replacement && task.Finish(*replacement) && !task.Finish(*replacement), "current callback finishes exactly once");
  }
  for (auto &producer : producers) { producer.join(); }
}

int main() {
  TestConcurrentTickets();
  using Sink = sfs::workbench::EquipmentRefreshEventSink;
  auto *sink = Sink::GetSingleton();
  for (unsigned cycle = 0; cycle != 128; ++cycle) {
    Sink::CancelQueuedRefreshes();
    const auto beforePolls = polls, beforeRefreshes = refreshes;
    sink->QueueRefresh(); sink->QueueRefresh(); sink->TickConditionState();
    Check(SKSE::tasks.pending.size() == 2, "one pending task per queue");
    auto oldTasks = std::move(SKSE::tasks.pending); SKSE::tasks.pending.clear();
    Sink::CancelQueuedRefreshes();
    sink->QueueRefresh(); sink->TickConditionState();
    for (auto &old : oldTasks) { old(); }
    Check(polls == beforePolls && refreshes == beforeRefreshes, "pre-reset tasks never execute in the new world");
    sink->QueueRefresh();
    Check(SKSE::tasks.pending.size() == 2, "old completion cannot release new queue ownership");
    SKSE::tasks.Drain();
    Check(polls == beforePolls + 1 && refreshes == beforeRefreshes + 1, "new-world tasks execute exactly once");
    for (auto &old : oldTasks) { old(); }
    Check(polls == beforePolls + 1 && refreshes == beforeRefreshes + 1, "replayed stale callbacks stay inert");
    Check(SKSE::tasks.pending.empty(), "drained work leaves no callbacks or polling retries");
  }
  {
    Sink::CancelQueuedRefreshes();
    const auto before = refreshes;
    sink->QueueRefresh();
    sfs::onRefresh = [&] { sink->QueueRefresh(); sink->QueueRefresh(); };
    SKSE::tasks.Drain();
    Check(SKSE::tasks.pending.size() == 1, "legitimate changes during refresh may request one follow-up");
    SKSE::tasks.Drain();
    Check(refreshes == before + 2 && SKSE::tasks.pending.empty(), "follow-up completes without perpetual scheduling");
  }
  {
    Sink::CancelQueuedRefreshes();
    SKSE::available = false; sink->QueueRefresh(); sink->TickConditionState();
    Check(SKSE::tasks.pending.empty(), "unavailable SKSE interface queues nothing");
    SKSE::available = true; sink->QueueRefresh();
    Check(SKSE::tasks.pending.size() == 1, "failed enrollment cannot permanently block refresh");
    SKSE::tasks.Drain();
  }
  {
    Sink::CancelQueuedRefreshes();
    const auto beforePolls = polls, beforeRefreshes = refreshes;
    sink->QueueRefresh(); sink->TickConditionState();
    sfs::ready = false; SKSE::tasks.Drain();
    Check(polls == beforePolls && refreshes == beforeRefreshes, "unready game suppresses engine access");
    sfs::ready = true; Sink::CancelQueuedRefreshes();
    sink->QueueRefresh(); sink->TickConditionState(); SKSE::tasks.Drain();
    Check(polls == beforePolls + 1 && refreshes == beforeRefreshes + 1, "ready game resumes both queues");
  }
  // Cancellation between registration and AddTask cannot adopt the new world.
  for (bool condition : {false, true}) {
    Sink::CancelQueuedRefreshes();
    const auto beforePolls = polls, beforeRefreshes = refreshes;
    SKSE::onLookup = [] { Sink::CancelQueuedRefreshes(); };
    if (condition) { sink->TickConditionState(); } else { sink->QueueRefresh(); }
    sink->QueueRefresh(); sink->TickConditionState(); SKSE::tasks.Drain();
    Check(polls == beforePolls + 1 && refreshes == beforeRefreshes + 1, "registration/reset interleaving retains only current work");
  }
  if (failures) { std::fprintf(stderr, "%u queue checks failed\n", failures); return 1; }
  std::puts("EquipmentRefreshQueueTests passed: production state/dispatch, 128 load boundaries, stale/reentrant/unavailable cases (not in-game timing).");
}
