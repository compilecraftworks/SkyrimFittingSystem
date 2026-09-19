#pragma once

#include "workbench/CoalescedTask.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace sfs::workbench {

class EquipmentRefreshEventSink final
    : public RE::BSTEventSink<RE::TESEquipEvent>,
      public RE::BSTEventSink<RE::TESActorLocationChangeEvent>,
      public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>,
      public RE::BSTEventSink<RE::TESCombatEvent>,
      public RE::BSTEventSink<SKSE::ModCallbackEvent>
#if defined(EXCLUSIVE_SKYRIM_FLAT)
    ,
      public RE::BSTEventSink<RE::TESFastTravelEndEvent>
#endif
{
public:
  static EquipmentRefreshEventSink *GetSingleton();
  static void Register();
  static void CancelQueuedRefreshes();

  RE::BSEventNotifyControl
  ProcessEvent(const RE::TESEquipEvent *a_event,
               RE::BSTEventSource<RE::TESEquipEvent> *a_eventSource) override;
  RE::BSEventNotifyControl
  ProcessEvent(const RE::TESActorLocationChangeEvent *a_event,
               RE::BSTEventSource<RE::TESActorLocationChangeEvent>
                   *a_eventSource) override;
  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESCellFullyLoadedEvent *a_event,
      RE::BSTEventSource<RE::TESCellFullyLoadedEvent> *a_eventSource) override;
  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESCombatEvent *a_event,
      RE::BSTEventSource<RE::TESCombatEvent> *a_eventSource) override;
  RE::BSEventNotifyControl ProcessEvent(
      const SKSE::ModCallbackEvent *a_event,
      RE::BSTEventSource<SKSE::ModCallbackEvent> *a_eventSource) override;

#if defined(EXCLUSIVE_SKYRIM_FLAT)
  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESFastTravelEndEvent *a_event,
      RE::BSTEventSource<RE::TESFastTravelEndEvent> *a_eventSource) override;
#endif

  void QueueRefresh();
  void QueueActorRefresh(RE::FormID a_actorFormID,
                         bool a_reconcileState = true);
  // Called from the render hook only to schedule a bounded main-thread poll.
  // The poll itself reads Skyrim state and queues actor-local refresh tasks.
  void TickConditionState();

private:
  EquipmentRefreshEventSink() = default;
  ~EquipmentRefreshEventSink() override = default;

  EquipmentRefreshEventSink(const EquipmentRefreshEventSink &) = delete;
  EquipmentRefreshEventSink(EquipmentRefreshEventSink &&) = delete;
  EquipmentRefreshEventSink &
  operator=(const EquipmentRefreshEventSink &) = delete;
  EquipmentRefreshEventSink &operator=(EquipmentRefreshEventSink &&) = delete;

  void RunRefresh();
  void PollConditionState();

  struct ActorConditionSignature {
    bool inCombat{false};
    bool sneaking{false};
    bool weaponDrawn{false};
    bool swimming{false};
    // Truth values of all condition rows assigned to this actor.  Custom
    // condition functions have no universal game event, so the bounded poll
    // compares this aggregate and refreshes only when it changes.
    std::uint64_t conditionTruthHash{0};

    friend bool operator==(const ActorConditionSignature &, const ActorConditionSignature &) = default;
  };

  struct WorldConditionSignature {
    std::int32_t minuteOfDay{-1};
    bool raining{false};
    bool snowing{false};
    RE::FormID weatherFormID{0};

    friend bool operator==(const WorldConditionSignature &, const WorldConditionSignature &) = default;
  };

  CoalescedTask refreshWork_;
  CoalescedTask conditionPollWork_;
  std::atomic<std::int64_t> nextConditionPollMillis_{0};
  std::unordered_map<RE::FormID, ActorConditionSignature>
      actorConditionSignatures_;
  WorldConditionSignature worldConditionSignature_{};
  bool hasWorldConditionSignature_{false};
};

} // namespace sfs::workbench
