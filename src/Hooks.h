#pragma once

namespace sfs::hooks {
void Install();
void ResetInputFilterState();
[[nodiscard]] bool IsWindowShutdownObserved();
} // namespace sfs::hooks
