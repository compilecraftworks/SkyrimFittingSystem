#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace sfs::native::racemenu::rules {
// Scene membership is independent of equipment ownership/redress. In particular,
// redress-OFF must end animation compensation without restoring any appearance.
// With removal OFF, only physically bare feet request barefoot alignment.
// Merely hiding actual boots must not be mistaken for stripping them.
class HighHeelScenes {
public:
  [[nodiscard]] bool Contains(std::uint32_t actor) const {
    return owners_.contains(actor);
  }
  [[nodiscard]] bool ShouldSuppress(std::uint32_t actor, bool hasActualFootwear) const {
    const auto it = owners_.find(actor);
    return it != owners_.end() && (it->second.removeHeelEffect || !hasActualFootwear);
  }
  std::unordered_set<std::uint32_t> Update(
      std::uint32_t owner, std::span<const std::uint32_t> actors,
      std::optional<bool> removeHeelEffect = true) {
    std::unordered_set<std::uint32_t> changed;
    std::unordered_map<std::uint32_t, bool> previousPolicy;
    for (auto it = owners_.begin(); it != owners_.end();) {
      if (it->second.owner == owner) {
        if (!removeHeelEffect.has_value()) {
          previousPolicy.emplace(it->first, it->second.removeHeelEffect);
        }
        changed.insert(it->first);
        it = owners_.erase(it);
      } else { ++it; }
    }
    for (auto actor : actors) {
      if (owner && actor) {
        const auto previous = previousPolicy.find(actor);
        owners_[actor] = {owner, removeHeelEffect.value_or(
            previous != previousPolicy.end() && previous->second)};
        changed.insert(actor);
      }
    }
    return changed;
  }
  void Forget(std::uint32_t actor) { owners_.erase(actor); }
  void Clear() { owners_.clear(); }
private:
  struct Member { std::uint32_t owner; bool removeHeelEffect; };
  std::unordered_map<std::uint32_t, Member> owners_;
};
} // namespace sfs::native::racemenu::rules
