#pragma once

#include <cstdint>

namespace sfs::native::ied::rules {

// IED's custom-skin hook forwards a concrete InitWornVisitor reference. SFS's
// filtering visitor has a different concrete layout and must never be passed
// back through that hook. This decision is intentionally independent of actor
// startup timing: an opaque chain receives the original concrete game visitor,
// with scoped filtering at its engine callbacks, while an exact known IED
// chain is bypassed and refreshed later through IED's
// public actor-level API. Neither decision disables additional SFS attachments.
enum class VisitorRoute : std::uint8_t {
  CurrentTargetUnfiltered,
  CurrentTargetWithOriginalVisitorFilter,
  CurrentTargetWithSfsFilter,
  OriginalEngineWithSfsFilterThenIedEvaluate,
};

[[nodiscard]] inline constexpr VisitorRoute ResolveVisitorRoute(
    const bool a_filterRequired, const std::uintptr_t a_currentTarget,
    const std::uintptr_t a_iedChainTarget,
    const std::uintptr_t a_passthroughChainTarget = 0) noexcept {
  if (a_passthroughChainTarget != 0 &&
      a_currentTarget == a_passthroughChainTarget) {
    return VisitorRoute::CurrentTargetWithOriginalVisitorFilter;
  }
  if (!a_filterRequired) {
    return VisitorRoute::CurrentTargetUnfiltered;
  }
  if (a_iedChainTarget != 0 && a_currentTarget == a_iedChainTarget) {
    return VisitorRoute::OriginalEngineWithSfsFilterThenIedEvaluate;
  }
  return VisitorRoute::CurrentTargetWithSfsFilter;
}

} // namespace sfs::native::ied::rules
