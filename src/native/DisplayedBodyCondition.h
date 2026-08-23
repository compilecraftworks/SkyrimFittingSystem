#pragma once

namespace sfs::native {
// Extends Skyrim's engine condition-table WornHasKeyword evaluation with the
// same actor-local final-rendered body state used by the Papyrus compatibility
// path. The original condition function is always chained.
void InstallDisplayedBodyConditionHook();
} // namespace sfs::native
