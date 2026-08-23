#pragma once

#include "ui/ConditionData.h"

#include <string>

namespace sfs::ui::conditions::editor {
struct State {
  int windowSlot{0};
  std::string sourceConditionId;
  ui::conditions::Definition draft;
  std::string error;
  bool isNew{false};
  bool open{true};
  bool focusOnNextDraw{false};
};
} // namespace sfs::ui::conditions::editor
