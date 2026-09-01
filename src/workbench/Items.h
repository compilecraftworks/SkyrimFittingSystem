#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <string>

namespace sfs::workbench {
enum class EquipmentWidgetItemKind : std::uint8_t { Armor, Slot };

struct EquipmentWidgetItem {
  EquipmentWidgetItemKind kind{EquipmentWidgetItemKind::Armor};
  RE::FormID formID{0};
  std::string key;
  std::string name;
  std::string slotText;
  std::uint64_t slotMask{0};
  bool hasArmorAddons{true};
  bool hidden{false};
  // Persisted actor-local registration lock. A locked appearance remains in
  // place when another catalog/outfit/kit is applied and ignores automatic
  // external visibility suppression. Manual eye state and conditions retain
  // their existing meaning.
  bool locked{false};
  // Persisted binding selected for this registered appearance. Suppression
  // and user-visible override are transient results derived per actor.
  std::uint8_t automaticEquipmentBindingMode{0};
  std::uint64_t automaticEquipmentAnchorSlotMask{0};
  RE::FormID automaticEquipmentAnchorFormID{0};
  bool automaticEquipmentSuppressed{false};
  bool automaticEquipmentUserVisible{false};

  [[nodiscard]] bool
  operator==(const EquipmentWidgetItem &a_other) const = default;

  [[nodiscard]] bool IsSlot() const {
    return kind == EquipmentWidgetItemKind::Slot;
  }

  [[nodiscard]] bool HasForm() const { return formID != 0; }

  [[nodiscard]] bool SupportsInfoTooltip() const {
    return !IsSlot() && HasForm();
  }

  [[nodiscard]] bool SupportsArmorReplacement() const {
    return IsSlot() || hasArmorAddons;
  }
};
} // namespace sfs::workbench
