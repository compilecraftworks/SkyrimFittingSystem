#pragma once

namespace sfs::hooks {
void Install();
// On menu close, finish only presses already consumed by SFS; new presses pass.
void ResetInputFilterState(bool a_finishKeyboardPresses = false);
[[nodiscard]] bool IsWindowShutdownObserved();
} // namespace sfs::hooks
