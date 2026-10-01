#pragma once

#include "native/GenitalCompatibilityRules.h"

namespace RE { class TESForm; }

namespace sfs::native::genital_compatibility {

// Detects the loaded runtime implementations once after TES data is ready.
// SOS and TNG remain independent so a compatibility keyword or custom slot-52
// skin cannot accidentally enable the other system's renderer behavior.
void InitializeEnvironment();

[[nodiscard]] rules::Environment GetEnvironment();
[[nodiscard]] bool IsSosInstalled();
[[nodiscard]] bool IsTngInstalled();
// Resolves the same form detected at DataLoaded; does not independently probe
// plugins or EditorIDs, and retains no engine pointer across calls.
[[nodiscard]] RE::TESForm *GetSosApiForm();

} // namespace sfs::native::genital_compatibility
