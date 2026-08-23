#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace sfs::workbench {
// Global policy for reacting to equipment changes initiated by external
// Papyrus mods. ModSettingsSlots uses the virtual worn-token lifecycle, while
// VanillaSlots follows the automatically classified vanilla equipment anchor.
enum class ExternalModStripLinkMode : std::uint8_t {
  Disabled = 0,
  ModSettingsSlots = 1,
  VanillaSlots = 2,
  Custom = 3,
  // Internal custom-popup base. It is never exposed as a fifth global option.
  DirectSlots = 4
};

[[nodiscard]] ExternalModStripLinkMode GetExternalModStripLinkMode();
void SetExternalModStripLinkMode(ExternalModStripLinkMode a_mode);
// Custom keeps one automatic policy while overlaying explicit per-slot
// exceptions, or uses DirectSlots where every untouched card defaults to a
// same-slot 1:1 mapping. These are shared user settings; runtime state remains
// actor-local.
[[nodiscard]] ExternalModStripLinkMode
GetCustomExternalModStripLinkBaseMode();
void SetCustomExternalModStripLinkBaseMode(ExternalModStripLinkMode a_mode);
[[nodiscard]] ExternalModStripLinkMode GetEffectiveExternalModStripLinkMode();
// True for the normal mod-settings policy and for DirectSlots when its
// selected automatic base is ModSettingsSlots. Direct editing changes only
// the queried slot: real worn armor wins when present, otherwise the existing
// virtual-token lifecycle represents the empty target slot.
[[nodiscard]] bool IsModSettingsStripLinkPolicyActive();
// True only for the pure vanilla policy and DirectSlots with a vanilla base.
// DirectSlots with a mod-settings base remains entirely owned by the
// mod-settings real-or-virtual transaction pipeline.
[[nodiscard]] bool IsActualEquipmentStripLinkPolicyActive();
[[nodiscard]] ExternalModStripLinkMode
GetCustomDirectStripLinkAutomaticBaseMode();
void SetCustomDirectStripLinkAutomaticBaseMode(
    ExternalModStripLinkMode a_mode);
[[nodiscard]] std::uint64_t GetCustomStripLinkDisabledAppearanceSlotMask();
void SetCustomStripLinkDisabledAppearanceSlotMask(std::uint64_t a_slotMask);
[[nodiscard]] bool
IsExternalModStripLinkAppearanceEnabled(std::uint64_t a_appearanceSlotMask);
[[nodiscard]] std::array<std::uint8_t, 32>
GetCustomDirectStripLinkMappings();
void SetCustomDirectStripLinkMappings(
    const std::array<std::uint8_t, 32> &a_mappings);
[[nodiscard]] std::array<bool, 32> GetCustomDirectStripLinkOverrides();
void SetCustomDirectStripLinkOverrides(
    const std::array<bool, 32> &a_overrides);
// A returned zero mask is an explicit "Do Not Link" override. No value means
// the appearance must continue following the selected automatic base policy.
// DirectSlots has no automatic fallback: an untouched slot resolves 1:1.
[[nodiscard]] std::optional<std::uint64_t>
ResolveCustomDirectStripLinkAnchorSlotMask(
    std::uint64_t a_appearanceSlotMask);
// Direct editor mod-settings counterpart. A returned zero mask means that this
// appearance is explicitly disabled. A concrete 30-61 mask is the remapped
// query slot; its real-versus-virtual ownership is resolved per actor at call
// time. No value keeps the mod-settings base's original 1:1 query slots.
[[nodiscard]] std::optional<std::uint64_t>
ResolveCustomDirectStripLinkTokenSlotMask(
    std::uint64_t a_appearanceSlotMask);

enum class AutomaticEquipmentVisibilityMode : std::uint8_t {
  Disabled = 0,
  VanillaSlots = 1,
  ModdingSlots = 2,
  // Internal binding marker retained for compatibility. Custom mappings are
  // per-slot exceptions now and are not exposed as a global UI mode.
  CustomSlots = 3
};

inline constexpr std::uint32_t kAutomaticEquipmentFirstSlot = 30;
inline constexpr std::uint32_t kAutomaticEquipmentLastSlot = 61;
inline constexpr std::size_t kAutomaticEquipmentSlotCount =
    kAutomaticEquipmentLastSlot - kAutomaticEquipmentFirstSlot + 1;
// Every vanilla biped slot which can carry a distinct registered appearance is
// available as an anchor. Multi-slot actual equipment retains every occupied
// slot so head, hair, and circlet links follow the same rule as other slots.
inline constexpr std::array<std::uint32_t, 11>
    kAutomaticEquipmentVanillaAnchorSlots{30, 31, 32, 33, 34, 35,
                                          36, 37, 38, 39, 42};

// Index 0 represents appearance slot 30. Each value is either 0 (this
// appearance slot does not participate) or an actual-equipment slot 30-61.
using AutomaticEquipmentSlotMappings =
    std::array<std::uint8_t, kAutomaticEquipmentSlotCount>;
// A separate flag is required because mapping value 0 is an explicit custom
// choice (Do Not Use), while an untouched slot must continue using vanilla
// keyword classification.
using AutomaticEquipmentSlotOverrides =
    std::array<bool, kAutomaticEquipmentSlotCount>;

// Legacy pre-release actor-scoped popup settings. They remain serializable so
// existing development co-saves can be consumed safely, but the global
// external-mod policy no longer reads them at runtime.
struct ActorAutomaticEquipmentVisibilitySettings {
  AutomaticEquipmentVisibilityMode mode{
      AutomaticEquipmentVisibilityMode::Disabled};
  AutomaticEquipmentSlotMappings customMappings{};
  AutomaticEquipmentSlotOverrides customOverrides{};
};

using AutomaticEquipmentVisibilitySettingsByActor =
    std::unordered_map<RE::FormID, ActorAutomaticEquipmentVisibilitySettings>;

inline constexpr std::size_t kMaximumVanillaAnchorCandidates = 3;

struct VanillaAnchorPriority {
  std::array<std::uint64_t, kMaximumVanillaAnchorCandidates> slotMasks{};
  std::size_t count{0};

  [[nodiscard]] bool empty() const { return count == 0; }
};

struct AutomaticEquipmentStateOverride {
  RE::FormID armorFormID{0};
  bool equipped{false};
};

[[nodiscard]] ActorAutomaticEquipmentVisibilitySettings
GetActorAutomaticEquipmentVisibilitySettings(RE::FormID a_actorFormID);
void SetActorAutomaticEquipmentVisibilitySettings(
    RE::FormID a_actorFormID,
    const ActorAutomaticEquipmentVisibilitySettings &a_settings);
[[nodiscard]] AutomaticEquipmentVisibilitySettingsByActor
GetAllActorAutomaticEquipmentVisibilitySettings();
void ReplaceAllActorAutomaticEquipmentVisibilitySettings(
    const AutomaticEquipmentVisibilitySettingsByActor &a_settingsByActor);
void SerializeActorAutomaticEquipmentVisibilitySettings(
    SKSE::SerializationInterface *a_skse);
void DeserializeActorAutomaticEquipmentVisibilitySettings(
    SKSE::SerializationInterface *a_skse);
void RevertActorAutomaticEquipmentVisibilitySettings();

// TESEquipEvent may be delivered before Actor::GetWornArmor reflects the
// completed biped change. Keep only accepted external-transaction event states
// which the engine snapshot has not caught up with yet. Ordinary inventory UI
// actions never enter this ledger.
void RecordAutomaticEquipmentStateEvent(RE::FormID a_actorFormID,
                                        RE::FormID a_armorFormID,
                                        bool a_equipped);
[[nodiscard]] std::vector<AutomaticEquipmentStateOverride>
ReconcileAutomaticEquipmentStateEvents(
    RE::FormID a_actorFormID,
    std::span<const RE::FormID> a_observedWornArmorFormIDs);
void ClearAutomaticEquipmentStateEvents();
void ClearAutomaticEquipmentStateEvents(RE::FormID a_actorFormID);

// Builds the actual-equipment occupancy used by automatic appearance links.
// ARMO and every ARMA slot are combined so vanilla Forearms (34) and Calves
// (38) partitions remain observable. SFS's existing non-genital 32+49
// normalization is applied after that union.
[[nodiscard]] std::uint64_t
GetAutomaticEquipmentControlSlotMask(const RE::TESObjectARMO *a_armor);

// Returns an ordered list of ordinary Skyrim equipment slots which naturally
// controls the supplied registered appearance. Pure 31/34/38 appearances keep
// their own Hair/Forearms/Calves anchors instead of being folded into a nearby
// primary slot.
[[nodiscard]] VanillaAnchorPriority
GetVanillaAnchorPriority(const RE::TESObjectARMO *a_appearance);
} // namespace sfs::workbench
