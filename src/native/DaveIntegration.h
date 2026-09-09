#pragma once

#include <RE/Skyrim.h>

#include <string>
#include <vector>

namespace sfs::native::dave {
[[nodiscard]] bool IsDynamicArmorVariantsLoaded();
[[nodiscard]] bool HasNativeApi(bool a_forceRetry = false);
[[nodiscard]] bool IsApiReady();
void LockToNativeFallback();
[[nodiscard]] bool RefreshActor(RE::Actor *a_actor);

void MarkHiddenRealEquipmentDirty(RE::Actor *a_actor);
void SyncHiddenRealEquipment(
    RE::Actor *a_actor,
    const std::vector<std::string> &a_sourceArmorAddonIdentifiers);
void ClearHiddenRealEquipment(RE::Actor *a_actor);
void ForgetHiddenRealEquipmentState();
} // namespace sfs::native::dave
