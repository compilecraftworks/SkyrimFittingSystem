#pragma once

#include <string_view>

namespace sfs::native::dye::rules {
// Returns true only for character base-body/helper components that must never
// appear in the Fitting Dye component list. The check is deliberately based
// on the already loaded renderer strings and performs no disk or actor scan.
[[nodiscard]] bool IsCharacterBaseComponent(std::string_view a_shapeName,
                                            std::string_view a_diffuseTexture);
} // namespace sfs::native::dye::rules
