#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace sfs::native::external_equipment::rules {

using ActorFormID = std::uint32_t;
using ArmorFormID = std::uint32_t;
using SuppressedEquipmentByActor = std::unordered_map<
    ActorFormID, std::unordered_map<ArmorFormID, std::uint64_t>>;

struct PendingEquipmentMutation {
  std::uint64_t sequence{0};
  ActorFormID actorFormID{0};
  ArmorFormID armorFormID{0};
  std::uint64_t slotMask{0};
  bool equipped{false};
};

[[nodiscard]] inline std::uint64_t ResolveSuppressedSlotMask(
    const ActorFormID a_actorFormID,
    const SuppressedEquipmentByActor &a_durableState,
    const std::span<const PendingEquipmentMutation> a_pendingMutations) {
  if (a_actorFormID == 0) {
    return 0;
  }

  std::unordered_map<ArmorFormID, std::uint64_t> effective;
  if (const auto actor = a_durableState.find(a_actorFormID);
      actor != a_durableState.end()) {
    effective = actor->second;
  }

  std::vector<const PendingEquipmentMutation *> actorMutations;
  actorMutations.reserve(a_pendingMutations.size());
  for (const auto &mutation : a_pendingMutations) {
    if (mutation.actorFormID == a_actorFormID && mutation.armorFormID != 0 &&
        mutation.slotMask != 0) {
      actorMutations.push_back(std::addressof(mutation));
    }
  }
  std::ranges::sort(actorMutations, {},
                    &PendingEquipmentMutation::sequence);
  for (const auto *mutation : actorMutations) {
    if (mutation->equipped) {
      effective.erase(mutation->armorFormID);
    } else {
      effective.insert_or_assign(mutation->armorFormID, mutation->slotMask);
    }
  }

  std::uint64_t result = 0;
  for (const auto &[_, slotMask] : effective) {
    result |= slotMask;
  }
  return result;
}

} // namespace sfs::native::external_equipment::rules
