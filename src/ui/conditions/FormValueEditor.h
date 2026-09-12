#pragma once

#include "ui/components/EditableCombo.h"

namespace sfs::ui::condition_editor {
// Keep the exact stored reference when merely drawing/focusing/accepting its
// readable alias. Only an actual text or selection change updates the draft.
inline bool DrawFormArgumentDropdown(
    const char *a_id, const char *a_hint, std::string &a_token,
    std::span<const std::string> a_options, const float a_width,
    const std::string_view a_editorID) {
  const std::string initial = a_editorID.empty() ? a_token : std::string(a_editorID);
  auto display = initial;
  const bool changed = components::DrawSearchableStringDropdown(
      a_id, a_hint, display, a_options, a_width, true);
  if (!changed || display == initial) {
    return false;
  }
  a_token = std::move(display);
  return true;
}
} // namespace sfs::ui::condition_editor
