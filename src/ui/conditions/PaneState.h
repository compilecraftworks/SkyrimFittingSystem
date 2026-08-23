#pragma once

#include "ui/conditions/EditorState.h"

#include <vector>

namespace sfs::ui::conditions {
struct PaneState {
  int focusedEditorWindowSlot{0};
  float libraryPaneHeight{220.0f};
  std::vector<editor::State> editors;
};
} // namespace sfs::ui::conditions
