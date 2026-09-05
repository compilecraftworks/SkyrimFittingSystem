#pragma once

#include <cstdint>
#include <string_view>

namespace sfs::armor::rules {

inline constexpr std::uint64_t kGenitalSlotMask = std::uint64_t{1} << 22;

// TNG_GenitalCover is a non-playable, invisible slot-52 blocker used by TNG
// to hide the genital partition. It is control state, not wearable equipment
// or the genital addon itself. Inputs are normalized to lowercase by the
// runtime adapter so this rule stays engine-independent and testable.
[[nodiscard]] constexpr bool IsTngGenitalCoverIdentity(
    const std::uint64_t a_slotMask, const std::string_view a_pluginName,
    const std::string_view a_editorID, const std::string_view a_displayName) {
  if ((a_slotMask & kGenitalSlotMask) == 0) {
    return false;
  }

  // The exact EditorID survives ordinary plugin overrides and merged patches.
  if (a_editorID == "tng_genitalcover") {
    return true;
  }

  // Keep fallbacks scoped to TNG itself so user-made slot-52 armors containing
  // the words "genital cover" are never silently removed from SFS.
  const bool fromTng =
      a_pluginName.find("thenewgentleman") != std::string_view::npos;
  return fromTng &&
         (a_editorID.starts_with("tng_genitalcover") ||
          a_displayName == "tng genitalcover" ||
          a_displayName == "tng genital cover");
}

struct GenitalCoverProjection {
  bool hideEquippedCover{false};
  bool occupyGenitalSlot{false};
};

[[nodiscard]] constexpr bool ShouldApplyGenitalCoverProjection(
    const bool a_tngInstalled, const bool a_actorSkinUsesGenitalSlot,
    const bool a_tngCoverEquipped) noexcept {
  return a_tngInstalled &&
         (a_actorSkinUsesGenitalSlot || a_tngCoverEquipped);
}

// The player is always eligible for SFS's actual-equipment display policy.
// NPCs remain actor-local and are refreshed only when they already have a
// fitting, hidden-actual, conditional, or preview display state.
[[nodiscard]] constexpr bool ShouldRefreshForGenitalCoverEvent(
    const bool a_tngInstalled, const bool a_isPlayer,
    const bool a_hasFittingState,
    const bool a_hasWorkbenchDisplayState) noexcept {
  return a_tngInstalled &&
         (a_isPlayer || a_hasFittingState || a_hasWorkbenchDisplayState);
}

// SFS only overrides TNG's actor-local blocker while an effective registered
// or hidden-equipment appearance requires genital correction. Otherwise TNG
// remains the sole owner of its normal actual-equipment behavior.
[[nodiscard]] constexpr GenitalCoverProjection
ResolveGenitalCoverProjection(const bool a_correctionActive,
                              const bool a_concealGenitals) {
  if (!a_correctionActive) {
    return {};
  }
  return {.hideEquippedCover = !a_concealGenitals,
          .occupyGenitalSlot = a_concealGenitals};
}

} // namespace sfs::armor::rules
