#pragma once

#include "VariantWorkbench.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace sfs::ui::workbench_conflicts {
struct ConflictEntry {
  std::string primaryName;
  std::string secondaryName;
  bool isOverride{false};
  std::string targetLabel;
};

struct RowConflictInfo {
  std::vector<std::string> targetWidgetIds;
  std::vector<ConflictEntry> targetDescriptions;
};

struct OverrideConflictInfo {
  std::vector<std::string> targetWidgetIds;
  std::vector<ConflictEntry> targetDescriptions;
};

struct ConflictState {
  std::unordered_map<std::string, RowConflictInfo> rowConflicts;
  std::unordered_map<std::string, OverrideConflictInfo> overrideConflicts;
};

[[nodiscard]] ConflictState BuildConflictState(
    const std::vector<::sfs::workbench::VariantWorkbenchRow> &a_rows);
} // namespace sfs::ui::workbench_conflicts
