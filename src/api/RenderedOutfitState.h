#pragma once

#include "../../extras/SkyrimFittingSystemRenderedOutfitAPI.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

namespace sfs::api::rendered {
namespace abi = sfs::rendered_outfit_api;
struct Value {
  abi::Status status{abi::Status::NotReady};
  std::uint32_t bodyFlags{0};
  std::vector<abi::Item> items;

  void Normalize() {
    if (status != abi::Status::Ready) { items.clear(); bodyFlags = 0; }
    std::ranges::sort(items, {}, [](const auto& i) {
      return std::pair{i.formID, i.source};
    });
    std::size_t count = 0;
    for (const auto& item : items) {
      if (count && items[count - 1].formID == item.formID &&
          items[count - 1].source == item.source) {
        auto& previous = items[count - 1];
        previous.visibleSlots |= item.visibleSlots;
        previous.declaredSlots |= item.declaredSlots;
        previous.flags |= item.flags;
        if (previous.originalFormID != item.originalFormID ||
            (previous.flags & abi::OriginalUnknown)) {
          previous.originalFormID = 0;
          previous.flags |= abi::OriginalUnknown;
        }
      } else { items[count++] = item; }
    }
    items.resize(count); // Compact in place; no second allocation/copy.
  }
  bool operator==(const Value& rhs) const {
    return status == rhs.status && bodyFlags == rhs.bodyFlags &&
        items.size() == rhs.items.size() &&
        (items.empty() || std::memcmp(items.data(), rhs.items.data(),
                                     items.size() * sizeof(abi::Item)) == 0);
  }
};

// Pure publication state; synchronization belongs to the provider. Tokens make
// a computation obsolete if a refresh/load/unload happened during its capture.
class State {
public:
  struct Entry {
    Value value;
    // Allocated only when an internal worker consumer first requests this value.
    // A held view stays immutable across publication/reset; it owns no RE objects.
    mutable std::shared_ptr<const Value> workerView;
    std::uint64_t ticket{0}, revision{0}, scene{0};
    bool dirty{true};
    std::uint32_t reasons{abi::AvailabilityChanged};
  };
  std::uint64_t Epoch() const { return epoch_; }
  std::uint64_t Touch(std::uint32_t actor, bool scene = false) {
    auto& e = entries_[actor];
    e.ticket = ++ticket_;
    e.dirty = true;
    pending_.insert(actor);
    if (scene) { ++e.scene; e.reasons |= abi::SceneChanged; }
    return e.ticket;
  }
  bool Contains(std::uint32_t actor) const { return entries_.contains(actor); }
  bool HasWork() const { return !pending_.empty() || !events_.empty(); }
  const Entry* Find(std::uint32_t actor) const {
    const auto it = entries_.find(actor);
    return it == entries_.end() ? nullptr : &it->second;
  }
  std::vector<std::uint32_t> PendingActors(std::size_t limit) {
    std::vector<std::uint32_t> result;
    const auto count = (std::min)(limit, pending_.size());
    result.reserve(count);
    if (!count) { return result; }
    auto next = pending_.upper_bound(lastServed_);
    for (std::size_t i = 0; i < count; ++i) {
      if (next == pending_.end()) { next = pending_.begin(); }
      result.push_back(*next++);
    }
    // Round-robin prevents continuously changing low FormIDs from starving
    // other actors when a burst is split across multiple game tasks.
    lastServed_ = result.back();
    return result;
  }
  // Input is canonicalized once by PrepareValue, before comparison/publication.
  bool Publish(std::uint32_t actor, std::uint64_t epoch,
               std::uint64_t ticket, Value value) {
    auto it = entries_.find(actor);
    if (epoch != epoch_ || it == entries_.end() || it->second.ticket != ticket) {
      return false;
    }
    auto& e = it->second;
    const bool different = !(e.value == value);
    const bool statusChanged = e.value.status != value.status;
    e.dirty = false;
    pending_.erase(actor);
    if (!different && e.reasons == 0) { return false; }
    e.revision = ++revision_;
    const auto pending = events_.find(actor);
    const auto reasons = e.reasons | (different ? abi::StateChanged : 0u) |
                         (statusChanged ? abi::AvailabilityChanged : 0u) |
                         (pending != events_.end() ? pending->second.reasons : 0u);
    if (different) { e.workerView.reset(); }
    e.value = std::move(value);
    events_[actor] = {sizeof(abi::Changed), abi::kVersion, epoch_, e.revision,
                     e.scene, actor, e.value.status, reasons, 0};
    e.reasons = 0;
    return true;
  }
  void Erase(std::uint32_t actor, abi::Status status) {
    const auto it = entries_.find(actor);
    if (it == entries_.end()) { return; }
    events_[actor] = {sizeof(abi::Changed), abi::kVersion, epoch_, ++revision_,
                     it->second.scene, actor, status, abi::AvailabilityChanged, 0};
    entries_.erase(it);
    pending_.erase(actor);
  }
  void Reset() {
    ++epoch_;
    entries_.clear(); pending_.clear(); events_.clear();
    lastServed_ = 0; lastEventServed_ = 0;
    events_[0] = {sizeof(abi::Changed), abi::kVersion, epoch_, ++revision_, 0, 0,
                  abi::Status::NotReady, abi::EpochChanged, 0};
  }
  std::vector<abi::Changed> Drain(std::size_t limit = 64) {
    std::vector<abi::Changed> result;
    const auto count = (std::min)(limit, events_.size());
    result.reserve(count);
    // Invalidate the consumer's previous epoch BEFORE any new actor values.
    if (count && events_.contains(0)) {
      result.push_back(events_.at(0));
      events_.erase(0);
    }
    auto next = events_.upper_bound(lastEventServed_);
    for (std::size_t i = result.size(); i < count; ++i) {
      if (next == events_.end()) { next = events_.begin(); }
      result.push_back(next->second);
      next = events_.erase(next);
    }
    if (!result.empty()) { lastEventServed_ = result.back().actorFormID; }
    return result;
  }
  abi::Status Copy(std::uint32_t actor, abi::Snapshot& out, abi::Item* items,
                   std::uint32_t capacity) const {
    out = {sizeof(abi::Snapshot), abi::kVersion, epoch_, 0, 0, actor,
           abi::Status::NotReady, 0, 0, 0, 0};
    const auto* e = Find(actor);
    if (!e) { return out.status; }
    out.revision = e->revision; out.sceneGeneration = e->scene;
    out.status = e->dirty ? abi::Status::NotReady : e->value.status;
    if (out.status != abi::Status::Ready) { return out.status; }
    out.requiredCount = static_cast<std::uint32_t>(e->value.items.size());
    out.bodyFlags = e->value.bodyFlags;
    for (const auto& item : e->value.items) { out.visibleSlots |= item.visibleSlots; }
    if (capacity < out.requiredCount) { return out.status = abi::Status::BufferTooSmall; }
    if (out.requiredCount) {
      std::memcpy(items, e->value.items.data(), out.requiredCount * sizeof(abi::Item));
    }
    return out.status;
  }
  std::shared_ptr<const Value> Acquire(std::uint32_t actor) const {
    const auto* e = Find(actor);
    if (!e || e->dirty || e->value.status != abi::Status::Ready) { return {}; }
    if (!e->workerView) { e->workerView = std::make_shared<const Value>(e->value); }
    return e->workerView;
  }
private:
  std::uint64_t epoch_{1}, revision_{0}, ticket_{0};
  std::uint32_t lastServed_{0}, lastEventServed_{0};
  std::map<std::uint32_t, Entry> entries_;
  std::set<std::uint32_t> pending_;
  std::map<std::uint32_t, abi::Changed> events_;
};
} // namespace sfs::api::rendered
