#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace sfs::native::racemenu::rules {
// Scene membership is independent of equipment ownership/redress. In particular,
// redress-OFF must end animation compensation without restoring any appearance.
class HighHeelScenes {
public:
  [[nodiscard]] bool Contains(std::uint32_t actor) const {
    return owners_.contains(actor);
  }
  std::unordered_set<std::uint32_t> Update(
      std::uint32_t owner, std::span<const std::uint32_t> actors) {
    std::unordered_set<std::uint32_t> changed;
    for (auto it = owners_.begin(); it != owners_.end();) {
      if (it->second == owner) {
        changed.insert(it->first);
        it = owners_.erase(it);
      } else { ++it; }
    }
    for (auto actor : actors) {
      if (owner && actor) { owners_[actor] = owner; changed.insert(actor); }
    }
    return changed;
  }
  void Forget(std::uint32_t actor) { owners_.erase(actor); }
  void Clear() { owners_.clear(); }
private:
  std::unordered_map<std::uint32_t, std::uint32_t> owners_;
};
} // namespace sfs::native::racemenu::rules
