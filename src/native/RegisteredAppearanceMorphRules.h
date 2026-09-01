#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace sfs::native::racemenu::rules {

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
