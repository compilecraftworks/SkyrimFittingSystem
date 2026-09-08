#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sfs::workbench::condition_drop {
enum class TargetKind : std::uint8_t {
  Catalog,
  ConditionalRow,
  ConditionalVisibilityRule,
};

struct Target {
  TargetKind kind{TargetKind::Catalog};
  std::uint64_t uiIdentity{0};

  [[nodiscard]] bool operator==(const Target &) const = default;
};

struct Entry {
  Target target;
  std::string conditionId;
};

struct Request {
  Target target;
  std::string conditionId;
  std::optional<Target> source;
};

struct Change {
  Target target;
  std::string conditionId;
};

enum class Status : std::uint8_t {
  Applied,
  NoChange,
  InvalidRequest,
  SourceNotFound,
  TargetNotFound,
  StaleSource,
  AmbiguousIdentity,
  Conflict,
};

[[nodiscard]] inline constexpr std::string_view
StatusName(const Status a_status) noexcept {
  switch (a_status) {
  case Status::Applied:
    return "applied";
  case Status::NoChange:
    return "no-change";
  case Status::InvalidRequest:
    return "invalid-request";
  case Status::SourceNotFound:
    return "source-not-found";
  case Status::TargetNotFound:
    return "target-not-found";
  case Status::StaleSource:
    return "stale-source";
  case Status::AmbiguousIdentity:
    return "ambiguous-identity";
  case Status::Conflict:
    return "conflict";
  }
  return "unknown";
}

struct Plan {
  Status status{Status::InvalidRequest};
  std::vector<Change> changes;

  [[nodiscard]] bool Changed() const {
    return status == Status::Applied && !changes.empty();
  }
};

namespace detail {
struct FindResult {
  const Entry *entry{nullptr};
  bool ambiguous{false};
};

[[nodiscard]] inline FindResult FindUnique(const std::span<const Entry> a_entries,
                                           const Target &a_target) {
  FindResult result;
  for (const auto &entry : a_entries) {
    if (entry.target != a_target) {
      continue;
    }
    if (result.entry != nullptr) {
      result.ambiguous = true;
      result.entry = nullptr;
      return result;
    }
    result.entry = &entry;
  }
  return result;
}

[[nodiscard]] inline bool IsStateTarget(const Target &a_target) {
  return a_target.uiIdentity != 0 &&
         a_target.kind != TargetKind::Catalog;
}
} // namespace detail

// Builds the complete condition-card mutation before any workbench state is
// changed. Moving onto an occupied card is a swap; moving onto an empty card
// clears the source. The caller may run additional domain-conflict validation
// against the planned final state before committing every change atomically.
[[nodiscard]] inline Plan BuildPlan(const std::span<const Entry> a_entries,
                                    const Request &a_request) {
  if (!detail::IsStateTarget(a_request.target) ||
      a_request.conditionId.empty()) {
    return {.status = Status::InvalidRequest};
  }

  const auto target = detail::FindUnique(a_entries, a_request.target);
  if (target.ambiguous) {
    return {.status = Status::AmbiguousIdentity};
  }
  if (target.entry == nullptr) {
    return {.status = Status::TargetNotFound};
  }

  if (!a_request.source.has_value()) {
    if (target.entry->conditionId == a_request.conditionId) {
      return {.status = Status::NoChange};
    }
    return {.status = Status::Applied,
            .changes = {{a_request.target, a_request.conditionId}}};
  }

  if (!detail::IsStateTarget(*a_request.source)) {
    return {.status = Status::InvalidRequest};
  }
  if (*a_request.source == a_request.target) {
    return {.status = Status::NoChange};
  }

  const auto source = detail::FindUnique(a_entries, *a_request.source);
  if (source.ambiguous) {
    return {.status = Status::AmbiguousIdentity};
  }
  if (source.entry == nullptr) {
    return {.status = Status::SourceNotFound};
  }
  if (source.entry->conditionId != a_request.conditionId) {
    return {.status = Status::StaleSource};
  }
  if (target.entry->conditionId == a_request.conditionId) {
    return {.status = Status::NoChange};
  }

  return {
      .status = Status::Applied,
      .changes = {{*a_request.source, target.entry->conditionId},
                  {a_request.target, a_request.conditionId}},
  };
}
} // namespace sfs::workbench::condition_drop
