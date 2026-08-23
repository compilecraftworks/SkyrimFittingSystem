#pragma once

#include "ui/ConditionData.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sfs::ui::condition_editor {
[[nodiscard]] bool
IsBooleanComparator(sfs::conditions::Comparator a_comparator);
[[nodiscard]] std::string BuildSuggestedConditionName(
    const std::vector<sfs::conditions::Definition> &a_conditions, int a_seed,
    const std::function<bool(std::string_view)> &a_extraConflict = {});
[[nodiscard]] std::string BuildUniqueConditionName(
    std::string_view a_baseName,
    const std::vector<sfs::conditions::Definition> &a_conditions,
    const std::function<bool(std::string_view)> &a_extraConflict = {});
[[nodiscard]] std::string ValidateConditionDraft(
    const sfs::conditions::Definition &a_definition,
    const std::vector<sfs::conditions::Definition> &a_conditions);
[[nodiscard]] bool ParseBooleanComparand(std::string_view a_text,
                                         bool a_defaultValue);
} // namespace sfs::ui::condition_editor
