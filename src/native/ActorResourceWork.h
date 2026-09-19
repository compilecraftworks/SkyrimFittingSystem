#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace sfs::native::resource_work {
// Callers hold their existing subsystem mutex. Tickets are never recycled on
// erase/reset: a callback for an old scene cannot finish a new scene's work.
class ActorTasks {
public:
  std::optional<std::uint64_t> Start(std::uint32_t actor) {
    if (tasks_.contains(actor)) { return std::nullopt; }
    const auto ticket = ++next_;
    tasks_.emplace(actor, ticket);
    return ticket;
  }
  bool Current(std::uint32_t actor, std::uint64_t ticket) const {
    const auto it = tasks_.find(actor);
    return it != tasks_.end() && ticket != 0 && it->second == ticket;
  }
  bool Finish(std::uint32_t actor, std::uint64_t ticket) {
    if (!Current(actor, ticket)) { return false; }
    tasks_.erase(actor);
    return true;
  }
  void Forget(std::uint32_t actor) { tasks_.erase(actor); }
  void Clear() { tasks_.clear(); }
  bool Contains(std::uint32_t actor) const { return tasks_.contains(actor); }
  std::size_t Size() const { return tasks_.size(); }
private:
  std::uint64_t next_{0};
  std::unordered_map<std::uint32_t, std::uint64_t> tasks_;
};

// Several components of one actor may be built concurrently (UI and game
// task). Unload cancels all of that actor's builds, not another component's
// ordinary edit. Entries exist only while a build is in flight.
class ActorBuilds {
public:
  std::uint64_t Start(std::uint32_t actor) {
    const auto ticket = ++next_;
    builds_[actor].insert(ticket);
    return ticket;
  }
  bool Current(std::uint32_t actor, std::uint64_t ticket) const {
    const auto it = builds_.find(actor);
    return it != builds_.end() && it->second.contains(ticket);
  }
  void Finish(std::uint32_t actor, std::uint64_t ticket) {
    const auto it = builds_.find(actor);
    if (it == builds_.end()) { return; }
    it->second.erase(ticket);
    if (it->second.empty()) { builds_.erase(it); }
  }
  void Forget(std::uint32_t actor) { builds_.erase(actor); }
  void Clear() { builds_.clear(); }
  std::size_t Size() const { return builds_.size(); }
private:
  std::uint64_t next_{0};
  std::unordered_map<std::uint32_t, std::unordered_set<std::uint64_t>> builds_;
};
} // namespace sfs::native::resource_work
