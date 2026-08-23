#include "native/DisplayedBodyCondition.h"

#include "ArmorUtils.h"
#include "native/ArmorSkinning.h"

#include <atomic>
#include <mutex>

namespace {
using ConditionFunction = RE::SCRIPT_FUNCTION::Condition_t;

std::once_flag g_installOnce;
std::atomic<ConditionFunction *> g_originalWornHasKeyword{nullptr};

bool WornHasKeywordCondition(RE::TESObjectREFR *a_thisObj, void *a_param1,
                             void *a_param2, double &a_result) {
  const auto original =
      g_originalWornHasKeyword.load(std::memory_order_acquire);
  if (!original) {
    return false;
  }

  const bool evaluated = original(a_thisObj, a_param1, a_param2, a_result);
  if (!evaluated) {
    return false;
  }

  auto *actor = a_thisObj ? a_thisObj->As<RE::Actor>() : nullptr;
  auto *form = static_cast<RE::TESForm *>(a_param1);
  auto *keyword = form ? form->As<RE::BGSKeyword>() : nullptr;
  const auto displayedState =
      sfs::native::GetDisplayedBodyKeywordState(actor, keyword);
  if (!displayedState.has_value()) {
    return true;
  }

  const bool originalState = a_result != 0.0;
  a_result = *displayedState ? 1.0 : 0.0;
  if (originalState != *displayedState) {
    logger::debug(
        "SFS engine displayed-body compatibility changed WornHasKeyword "
        "{} -> {} actor={:08X} keyword={}",
        originalState ? "true" : "false",
        *displayedState ? "true" : "false", actor ? actor->GetFormID() : 0,
        keyword ? sfs::armor::GetEditorID(keyword) : std::string{});
  }
  return true;
}

void InstallHook() {
  auto *command = RE::SCRIPT_FUNCTION::LocateScriptCommand("WornHasKeyword");
  if (!command || !command->conditionFunction || command->numParams == 0) {
    logger::warn(
        "Could not install SFS engine displayed-body compatibility: "
        "WornHasKeyword condition command is unavailable");
    return;
  }

  auto **slot = std::addressof(command->conditionFunction);
  const auto original = *slot;
  const auto replacement = &WornHasKeywordCondition;
  if (original == replacement) {
    logger::info("SFS engine displayed-body compatibility already installed");
    return;
  }

  g_originalWornHasKeyword.store(original, std::memory_order_release);
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(slot),
                       std::addressof(replacement), sizeof(replacement),
                       std::addressof(original), sizeof(original))) {
    g_originalWornHasKeyword.store(nullptr, std::memory_order_release);
    logger::warn(
        "Could not install SFS engine displayed-body compatibility because "
        "the WornHasKeyword condition pointer changed");
    return;
  }

  logger::info(
      "Installed SFS engine displayed-body WornHasKeyword compatibility");
}
} // namespace

namespace sfs::native {
void InstallDisplayedBodyConditionHook() {
  std::call_once(g_installOnce, InstallHook);
}
} // namespace sfs::native
