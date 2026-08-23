#pragma once

namespace sfs::native::oar {
// Registers SFS's read-only final-rendered equipment conditions when Open
// Animation Replacer is installed and exposes a compatible Conditions API.
// Safe to call unconditionally during SKSE PostLoad.
void RegisterConditions();
} // namespace sfs::native::oar
