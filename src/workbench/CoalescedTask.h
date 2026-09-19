#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

namespace sfs::workbench {
// One pending callback, with constant-size ownership state. Cancellation does
// not recycle tickets: an old callback cannot finish a replacement callback.
// World-generation checks remain the caller's responsibility.
class CoalescedTask {
public:
  [[nodiscard]] std::optional<std::uint64_t> Start() {
    auto ticket = next_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (ticket == 0) {
      ticket = next_.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    std::uint64_t empty = 0;
    if (!pending_.compare_exchange_strong(empty, ticket)) {
      return std::nullopt;
    }
    return ticket;
  }

  bool Finish(std::uint64_t ticket) {
    return ticket != 0 && pending_.compare_exchange_strong(ticket, 0);
  }

  void Cancel() { pending_.store(0); }

private:
  std::atomic<std::uint64_t> next_{0};
  std::atomic<std::uint64_t> pending_{0};
};
} // namespace sfs::workbench
