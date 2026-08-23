#pragma once

#include <RE/Skyrim.h>

namespace sfs::native {
void ClearResolvedGenitalArmors();
void RememberGenitalArmor(RE::Actor *a_actor,
                          const RE::TESObjectARMO *a_armor);
void ForgetGenitalArmor(RE::Actor *a_actor);
void RequestGenitalArmorResolution(RE::Actor *a_actor,
                                   bool a_force = false);
[[nodiscard]] const RE::TESObjectARMO *
GetResolvedGenitalArmor(RE::Actor *a_actor);
} // namespace sfs::native
