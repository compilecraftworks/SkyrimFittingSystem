#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <vector>

namespace sfs::workbench {
struct VariantWorkbenchRow;
}

namespace sfs::devious_devices {
void InitializeDeviousDevicesHider();
void ResetDeviousDevicesHider();
[[nodiscard]] bool
IsDeviousDevicesRenderedDevice(const RE::TESObjectARMO *a_armor);
// DD rendered devices and their inventory/hider counterparts own their
// strip/redress lifetime.  They must not also enter the generic external
// equipment transaction ledger, which otherwise waits for a removed device
// itself to be redressed and leaves its linked fitting appearances suppressed.
[[nodiscard]] bool IsDeviousDevicesEquipmentTransactionArmor(
    const RE::TESObjectARMO *a_armor);
void ObserveDeviousDevicesRenderedDeviceEquipEvent(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor, bool a_equipped);
void ReconcileDeviousDevicesRenderedDeviceVisibility(RE::Actor *a_actor);
[[nodiscard]] bool IsDeviousDevicesRenderedDeviceOrdinaryVisible(
    RE::FormID a_actorFormID, RE::FormID a_armorFormID);
void SetDeviousDevicesRenderedDeviceUserVisible(
    RE::FormID a_actorFormID, RE::FormID a_armorFormID, bool a_visible);
void ClearDeviousDevicesRenderedDeviceOrdinaryVisibility(
    RE::FormID a_actorFormID);
void ReleaseDeviousDevicesHiderSuppressionForSourceMask(
    RE::FormID a_actorFormID, std::uint64_t a_sourceSlotMask);
void ReleaseDeviousDevicesHiderSuppressionForAppearanceMask(
    RE::FormID a_actorFormID, std::uint64_t a_appearanceSlotMask);
bool RefreshDeviousDevicesHiderSettings();
bool UpdateDeviousDevicesHiderSettings(
    const std::vector<std::int32_t> &a_slotMaskFilters,
    std::int32_t a_setting);

[[nodiscard]] std::uint32_t
GetDeviousDevicesHiderSuppressedFittingSlotMask(RE::Actor *a_actor);

[[nodiscard]] std::uint32_t CalculateDeviousDevicesHiderSuppressedFittingSlotMask(
    RE::Actor *a_actor,
    const std::vector<sfs::workbench::VariantWorkbenchRow> *a_previewRows,
    bool a_previewReplacesRows, std::uint32_t a_suppressedFittingSlotMask);
} // namespace sfs::devious_devices
