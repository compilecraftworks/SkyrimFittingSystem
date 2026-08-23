#include "papyrus/FittingPapyrus.h"

#include "native/ArmorSkinning.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/FittingSlotState.h"
#include "ui/Menu.h"

#include <string>

namespace {
constexpr auto kScriptName = "SkyrimFittingSystemNative"sv;

[[nodiscard]] std::int32_t ToPapyrusMask(const std::uint32_t a_mask) {
  return static_cast<std::int32_t>(a_mask);
}

[[nodiscard]] std::uint32_t FromPapyrusMask(const std::int32_t a_mask) {
  return static_cast<std::uint32_t>(a_mask);
}

void TraceMissingActor(RE::BSScript::IVirtualMachine *a_vm,
                       const std::uint32_t a_stackID,
                       const std::string_view a_functionName) {
  if (!a_vm) {
    return;
  }
  auto message = std::string(a_functionName);
  message += " requires a non-None Actor.";
  a_vm->TraceStack(message.c_str(), a_stackID,
                   RE::BSScript::IVirtualMachine::Severity::kError);
}

std::int32_t GetDisplayedFittingSlotMask(
    RE::BSScript::IVirtualMachine *a_vm, const std::uint32_t a_stackID,
    RE::StaticFunctionTag *, RE::Actor *a_actor) {
  if (!a_actor) {
    TraceMissingActor(a_vm, a_stackID, "GetDisplayedFittingSlotMask");
    return 0;
  }
  return ToPapyrusMask(sfs::native::GetDisplayedFittingSlotMask(a_actor));
}

bool IsRealEquipmentHiddenForActorSlots(
    RE::BSScript::IVirtualMachine *a_vm, const std::uint32_t a_stackID,
    RE::StaticFunctionTag *, RE::Actor *a_actor,
    const std::int32_t a_slotMask) {
  if (!a_actor) {
    TraceMissingActor(a_vm, a_stackID,
                      "IsRealEquipmentHiddenForActorSlots");
    return false;
  }
  const auto slotMask = FromPapyrusMask(a_slotMask);
  return slotMask != 0 &&
         sfs::native::IsRealEquipmentHiddenForActorSlots(a_actor, slotMask);
}

[[nodiscard]] std::uint32_t ResolveHeadgearToggleFittingSlotMask(
    RE::Actor *a_actor, const std::uint32_t a_controllerSlotMask) {
  if (!a_actor || a_controllerSlotMask == 0) {
    return 0;
  }
  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return 0;
  }
  return menu->GetWorkbench().GetHeadgearToggleFittingSlotMaskForActor(
      a_actor->GetFormID(), a_controllerSlotMask);
}

void SetHeadgearToggleFittingSlotsHidden(
    RE::BSScript::IVirtualMachine *a_vm, const std::uint32_t a_stackID,
    RE::StaticFunctionTag *, RE::Actor *a_actor,
    const std::int32_t a_slotMask, const bool a_hidden) {
  if (!a_actor) {
    TraceMissingActor(a_vm, a_stackID,
                      "SetHeadgearToggleFittingSlotsHidden");
    return;
  }

  const auto slotMask = FromPapyrusMask(a_slotMask);
  const auto previousMask =
      sfs::native::GetHeadgearToggleSuppressedFittingSlotMask(a_actor);
  if (!a_hidden) {
    // A shown transition is authoritative even when HT2's array has already
    // been cleared by an unequip. Clear all of this actor's temporary HT2
    // state rather than leaving a stale cosmetic suppression behind.
    sfs::native::ClearHeadgearToggleFittingSlotsSuppressed(a_actor);
  } else {
    if (slotMask == 0) {
      return;
    }
    const auto fittingSlotMask =
        ResolveHeadgearToggleFittingSlotMask(a_actor, slotMask);
    if (fittingSlotMask == 0) {
      return;
    }
    sfs::native::SetHeadgearToggleFittingSlotsSuppressed(
        a_actor, fittingSlotMask, true);
  }
  const auto currentMask =
      sfs::native::GetHeadgearToggleSuppressedFittingSlotMask(a_actor);
  if (previousMask != currentMask) {
    sfs::native::QueueArmorRefreshFor(a_actor);
  }
}

// Retain the v1.2.x Helmet Toggle 2 entry points for saves that still carry
// the old patch PEX.  The former patch only sent its fixed 30|42 controller
// mask; resolve that mask through the current actor's selected SFS linking
// policy rather than restoring the old global suppression state.
void SuppressHeadgearToggleFittingSlots(
    RE::BSScript::IVirtualMachine *a_vm, const std::uint32_t a_stackID,
    RE::StaticFunctionTag *a_tag, RE::Actor *a_actor,
    const std::int32_t a_slotMask) {
  SetHeadgearToggleFittingSlotsHidden(a_vm, a_stackID, a_tag, a_actor,
                                      a_slotMask, true);
}

void RestoreHeadgearToggleFittingSlotMask(
    RE::BSScript::IVirtualMachine *a_vm, const std::uint32_t a_stackID,
    RE::StaticFunctionTag *, RE::Actor *a_actor,
    const std::int32_t a_slotMask) {
  if (!a_actor) {
    TraceMissingActor(a_vm, a_stackID,
                      "RestoreHeadgearToggleFittingSlotMask");
    return;
  }
  const auto fittingSlotMask = ResolveHeadgearToggleFittingSlotMask(
      a_actor, FromPapyrusMask(a_slotMask));
  if (fittingSlotMask == 0) {
    return;
  }
  const auto previousMask =
      sfs::native::GetHeadgearToggleSuppressedFittingSlotMask(a_actor);
  sfs::native::SetHeadgearToggleFittingSlotsSuppressed(a_actor,
                                                        fittingSlotMask, false);
  if (previousMask !=
      sfs::native::GetHeadgearToggleSuppressedFittingSlotMask(a_actor)) {
    sfs::native::QueueArmorRefreshFor(a_actor);
  }
}

void RestoreHeadgearToggleFittingSlots(
    RE::BSScript::IVirtualMachine *a_vm, const std::uint32_t a_stackID,
    RE::StaticFunctionTag *a_tag, RE::Actor *a_actor) {
  SetHeadgearToggleFittingSlotsHidden(a_vm, a_stackID, a_tag, a_actor, 0,
                                      false);
}

} // namespace

namespace sfs::papyrus {
bool Register(RE::BSScript::IVirtualMachine *a_vm) {
  if (!a_vm) {
    return false;
  }
  if (!sfs::native::external_equipment::RegisterPapyrusObserver(a_vm)) {
    logger::warn("External equipment transaction observer is unavailable");
  }
  a_vm->RegisterFunction("GetDisplayedFittingSlotMask", kScriptName,
                         GetDisplayedFittingSlotMask);
  a_vm->RegisterFunction("IsRealEquipmentHiddenForActorSlots", kScriptName,
                         IsRealEquipmentHiddenForActorSlots);
  a_vm->RegisterFunction("SetHeadgearToggleFittingSlotsHidden", kScriptName,
                         SetHeadgearToggleFittingSlotsHidden);
  a_vm->RegisterFunction("SuppressHeadgearToggleFittingSlots", kScriptName,
                         SuppressHeadgearToggleFittingSlots);
  a_vm->RegisterFunction("RestoreHeadgearToggleFittingSlotMask", kScriptName,
                         RestoreHeadgearToggleFittingSlotMask);
  a_vm->RegisterFunction("RestoreHeadgearToggleFittingSlots", kScriptName,
                         RestoreHeadgearToggleFittingSlots);
  logger::info("Registered minimal SFS compatibility Papyrus API");
  return true;
}
} // namespace sfs::papyrus
