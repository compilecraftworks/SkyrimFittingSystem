#pragma once

#include <RE/Skyrim.h>

#include <string>

namespace sfs::ui::workbench {
struct FilterState {
  RE::FormID actorFormID{0};
};

struct FilterOption {
  std::string label;
  RE::FormID actorFormID{0};
};
} // namespace sfs::ui::workbench