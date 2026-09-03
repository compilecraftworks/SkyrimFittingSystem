#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace sfs::native::racemenu::rules {

enum class HighHeelTransformRoute : std::uint8_t {
  Unavailable,
  LegacyPapyrus,
  PublicInterface,
};

// RaceMenu's public INiTransformInterface was introduced at version 3.
// Released version-2 builds expose the same HH_OFFSET behavior through the
// long-standing NiOverride Papyrus API, but their concrete C++ vtable is not
// ABI-compatible with the public interface. Never cast those objects to v3.
[[nodiscard]] inline constexpr HighHeelTransformRoute
ResolveHighHeelTransformRoute(const std::uint32_t a_version) noexcept {
  if (a_version >= 3) {
    return HighHeelTransformRoute::PublicInterface;
  }
  if (a_version != 0) {
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
  return a_version >= 4;
}

[[nodiscard]] inline constexpr bool ShouldTrackRegisteredAppearanceNodes(
    const bool a_previewReplacesRows, const bool a_displayActive,
    const std::size_t a_displayArmorCount) noexcept {
  return !a_previewReplacesRows && a_displayActive && a_displayArmorCount != 0;
}

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
