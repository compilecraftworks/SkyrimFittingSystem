#pragma once

#include <optional>

namespace sfs::ui::depth_of_field {
// ENB parameters are touched only in its own render callbacks. The lease lasts
// one frame, and is also released before preset saves or renderer teardown.
class FrameLease {
public:
  template<class Read, class Write>
  void Begin(bool requested, Read read, Write write) {
    if (!requested || original_) { return; }
    const auto value = read();
    if (value && *value && write(false)) { original_ = *value; }
  }

  template<class Read, class Write>
  void End(Read read, Write write) {
    if (!original_) { return; }
    const auto value = read();
    // Preserve a newer provider/editor value. If the provider is temporarily
    // unavailable, retain the pending restoration for its next valid callback.
    if (value && (*value || write(*original_))) { original_.reset(); }
  }

  // A freshly loaded preset supersedes the old parameter, even with SFS open.
  void PresetLoaded() { original_.reset(); }
  [[nodiscard]] bool PendingRestore() const { return original_.has_value(); }
private:
  std::optional<bool> original_;
};
} // namespace sfs::ui::depth_of_field
