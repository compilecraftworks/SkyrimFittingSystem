#pragma once

#include "ui/workbench/FilterState.h"

namespace sfs::workbench {
struct InitialFilterSelection {
  ui::workbench::FilterState filter{};
};

[[nodiscard]] bool IsSelectableWorkbenchActor(RE::Actor *a_actor,
                                              RE::Actor *a_player);

[[nodiscard]] InitialFilterSelection
BuildInitialFilterSelection(bool a_includeCrosshairActor);
} // namespace sfs::workbench