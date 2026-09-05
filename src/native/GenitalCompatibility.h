#pragma once

#include "native/GenitalCompatibilityRules.h"

namespace sfs::native::genital_compatibility {

// Detects the loaded runtime implementations once after TES data is ready.
// SOS and TNG remain independent so a compatibility keyword or custom slot-52
// skin cannot accidentally enable the other system's renderer behavior.
void InitializeEnvironment();

[[nodiscard]] rules::Environment GetEnvironment();
[[nodiscard]] bool IsSosInstalled();
[[nodiscard]] bool IsTngInstalled();

} // namespace sfs::native::genital_compatibility
