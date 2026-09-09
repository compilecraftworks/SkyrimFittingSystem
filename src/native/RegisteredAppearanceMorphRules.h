#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace sfs::native::racemenu::rules {

enum class HighHeelTransformRoute : std::uint8_t {
  Unavailable,
  LegacyPapyrus,
  PublicInterface,
};

enum class LegacyHighHeelCompletion : std::uint8_t {
  Failed,
  RemoveInternalPosition,
  Synchronized,
};

[[nodiscard]] inline constexpr LegacyHighHeelCompletion
ResolveLegacyHighHeelCompletion(bool a_updateSucceeded,
                               bool a_clearInternalPosition) noexcept {
  if (!a_updateSucceeded) {
    return LegacyHighHeelCompletion::Failed;
  }
  return a_clearInternalPosition ? LegacyHighHeelCompletion::RemoveInternalPosition
                                : LegacyHighHeelCompletion::Synchronized;
}

// RaceMenu's public INiTransformInterface was introduced at version 3.
// Released version-2 builds expose the same HH_OFFSET behavior through the
// long-standing NiOverride Papyrus API, but their concrete C++ vtable is not
// ABI-compatible with the public interface. Never cast those objects to v3.
// Higher interface versions use this last public prefix, with the runtime
// caller checking callable memory and logging the compatibility assumption.
[[nodiscard]] inline constexpr HighHeelTransformRoute
ResolveHighHeelTransformRoute(const std::uint32_t a_version) noexcept {
  if (a_version >= 3) {
    return HighHeelTransformRoute::PublicInterface;
  }
  if (a_version == 1 || a_version == 2) {
    return HighHeelTransformRoute::LegacyPapyrus;
  }
  return HighHeelTransformRoute::Unavailable;
}

// BodyMorph v4 introduced the public wrapper ABI used by SFS. Older
// BodyMorph v3 releases still expose NiOverride Papyrus transforms, but their
// concrete C++ object must not be treated as the public interface.
[[nodiscard]] inline constexpr bool
IsPublicBodyMorphInterfaceCompatible(
    const std::uint32_t a_version) noexcept {
  // Use the last compatible public prefix, not a package/version allowlist.
  // v5 only appends a callback. Higher versions optimistically retain v4's
  // used prefix; the caller validates callable slots and logs this assumption.
  // This is forward-compatibility policy, not proof of a future ABI's meaning.
  return a_version >= 4;
}

[[nodiscard]] inline constexpr bool ShouldTrackRegisteredAppearanceNodes(
    const bool a_displayActive,
    const std::size_t a_displayArmorCount) noexcept {
  return a_displayActive && a_displayArmorCount != 0;
}

// Tracking later updates is independent of the initial attach-time application.
// RaceMenu owns initial morphing for replacement previews; recording their
// roots must not cause another immediate vertex reset during the attach pass.
[[nodiscard]] inline constexpr bool ShouldApplyInitialNativeMorphs(
    const bool a_previewReplacesRows) noexcept {
  return !a_previewReplacesRows;
}

// Event-driven, actor-local requests. Remember that an update was requested even
// if its first pass finds no attached nodes. A later attachment/reactivation can
// then replay CURRENT values. No polling and no morph-value snapshots.
class ActorMorphRequests {
public:
  void Request(const std::uint32_t a_actor) { states_[a_actor].requested = true; }
  [[nodiscard]] bool HasRequest(const std::uint32_t a_actor) const {
    const auto it = states_.find(a_actor);
    return it != states_.end() && it->second.requested;
  }
  [[nodiscard]] std::optional<std::uint64_t> Schedule(
      const std::uint32_t a_actor) {
    auto &state = states_[a_actor];
    if (state.ticket != 0) {
      return std::nullopt;
    }
    state.ticket = ++nextTicket_;
    return state.ticket;
  }
  [[nodiscard]] bool Begin(const std::uint32_t a_actor,
                           const std::uint64_t a_ticket) {
    const auto it = states_.find(a_actor);
    if (it == states_.end() || a_ticket == 0 || it->second.ticket != a_ticket) {
      return false;
    }
    it->second.ticket = 0;
    return true;
  }
  void Forget(const std::uint32_t a_actor) { states_.erase(a_actor); }
  // Do not reset the ticket counter: an already queued task must never match a
  // new actor/request after a save transition, even with the same FormID.
  void Clear() { states_.clear(); }

private:
  struct State { bool requested{false}; std::uint64_t ticket{0}; };
  std::unordered_map<std::uint32_t, State> states_;
  std::uint64_t nextTicket_{0};
};

class ActorMorphActivity {
public:
  [[nodiscard]] bool IsActive(const std::uint32_t a_actorFormID) const {
    return activeActors_.contains(a_actorFormID);
  }

  void SetActive(const std::uint32_t a_actorFormID, const bool a_active) {
    if (a_active) {
      activeActors_.insert(a_actorFormID);
      return;
    }
    activeActors_.erase(a_actorFormID);
    observedMorphActors_.erase(a_actorFormID);
    observedEmptyActors_.erase(a_actorFormID);
  }

  [[nodiscard]] bool MarkObserved(const std::uint32_t a_actorFormID,
                                  const bool a_hadMorphNodes) {
    return a_hadMorphNodes
               ? observedMorphActors_.insert(a_actorFormID).second
               : observedEmptyActors_.insert(a_actorFormID).second;
  }

  void Clear() {
    activeActors_.clear();
    observedMorphActors_.clear();
    observedEmptyActors_.clear();
  }

private:
  std::unordered_set<std::uint32_t> activeActors_;
  std::unordered_set<std::uint32_t> observedMorphActors_;
  std::unordered_set<std::uint32_t> observedEmptyActors_;
};

} // namespace sfs::native::racemenu::rules
