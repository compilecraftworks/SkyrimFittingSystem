#pragma once

#include "workbench/ConditionDrop.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace sfs::workbench::action_drop {
struct Entry {
  condition_drop::Target target;
  std::vector<std::uint32_t> formIDs;
};

struct Request {
  condition_drop::Target target;
  std::uint32_t formID{0};
  std::optional<condition_drop::Target> source;
};

namespace detail {
struct FindResult {
  const Entry *entry{nullptr};
  bool ambiguous{false};
};

[[nodiscard]] inline FindResult FindUnique(const std::span<const Entry> a_entries,
                                           const condition_drop::Target &a_target) {
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

[[nodiscard]] inline bool
IsActionTarget(const condition_drop::Target &a_target) noexcept {
  return a_target.uiIdentity != 0 &&
         a_target.kind != condition_drop::TargetKind::Catalog;
}
} // namespace detail

// Validates both endpoints before the domain model stages any mutation. The
// actual row/rule conversion is then applied to a private workbench copy and
// committed only if target replacement and source cleanup both succeed.
[[nodiscard]] inline condition_drop::Status
ValidateRequest(const std::span<const Entry> a_entries,
                const Request &a_request) {
  if (!detail::IsActionTarget(a_request.target) || a_request.formID == 0) {
    return condition_drop::Status::InvalidRequest;
  }

  const auto target = detail::FindUnique(a_entries, a_request.target);
  if (target.ambiguous) {
    return condition_drop::Status::AmbiguousIdentity;
  }
  if (target.entry == nullptr) {
    return condition_drop::Status::TargetNotFound;
  }

  if (!a_request.source.has_value()) {
    return condition_drop::Status::Applied;
  }
  if (!detail::IsActionTarget(*a_request.source)) {
    return condition_drop::Status::InvalidRequest;
  }
  if (*a_request.source == a_request.target) {
    return condition_drop::Status::NoChange;
  }

  const auto source = detail::FindUnique(a_entries, *a_request.source);
  if (source.ambiguous) {
    return condition_drop::Status::AmbiguousIdentity;
  }
  if (source.entry == nullptr) {
    return condition_drop::Status::SourceNotFound;
  }
  if (std::ranges::find(source.entry->formIDs, a_request.formID) ==
      source.entry->formIDs.end()) {
    return condition_drop::Status::StaleSource;
  }
  return condition_drop::Status::Applied;
}
} // namespace sfs::workbench::action_drop
