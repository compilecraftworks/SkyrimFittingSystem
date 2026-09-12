#include "conditions/FormTokens.h"

#include "ArmorUtils.h"
#include "conditions/FormTokenRules.h"

#include <unordered_set>

namespace sfs::conditions {
namespace {
std::vector<RE::TESForm *> SnapshotRegisteredForms(FormTokenPredicate a_accept) {
  std::vector<RE::TESForm *> result;
  const auto [forms, lock] = RE::TESForm::GetAllForms();
  const RE::BSReadLockGuard guard{lock};
  if (forms) {
    for (const auto &[id, form] : *forms) {
      if (form && !form->IsDeleted() && !form->IsIgnored() && a_accept(form)) {
        result.push_back(form);
      }
    }
  }
  return result;
}
} // namespace

std::vector<RE::TESForm *> CollectCellForms() {
  std::vector<RE::TESForm *> result;
  auto *handler = RE::TESDataHandler::GetSingleton();
  if (!handler) { return result; }
  std::unordered_set<RE::FormID> seen;
  const auto append = [&](RE::TESObjectCELL *a_cell) {
    if (a_cell && !a_cell->IsDeleted() && !a_cell->IsIgnored() &&
        a_cell->GetFormID() != 0 && seen.insert(a_cell->GetFormID()).second) {
      result.push_back(a_cell);
    }
  };
  for (auto *cell : handler->GetFormArray<RE::TESObjectCELL>()) { append(cell); }
  for (auto *cell : handler->interiorCells) { append(cell); }
  // The registered form table also contains exterior/persistent CELL records.
  // Snapshot under the engine's read lock instead of walking streaming world's
  // mutable cellMap on the render thread. No cell loads or EditorID callbacks
  // are performed while holding the table lock.
  for (auto *form : SnapshotRegisteredForms([](RE::TESForm *a_form) {
         return a_form->As<RE::TESObjectCELL>() != nullptr;
       })) {
    append(form->As<RE::TESObjectCELL>());
  }
  return result;
}

RE::TESForm *LookupFormTokenIf(const std::string &a_token,
                              const FormTokenPredicate a_accept,
                              const bool a_searchRecords) {
  const auto token = sfs::strings::TrimText(a_token);
  if (token.empty()) { return nullptr; }
  const auto accept = [&](RE::TESForm *a_form) -> RE::TESForm * {
    return a_form && !a_form->IsDeleted() && !a_form->IsIgnored() &&
                   a_accept(a_form) ? a_form : nullptr;
  };

  auto *handler = RE::TESDataHandler::GetSingleton();
  if (token.find('|') != std::string::npos) {
    const auto parsed = ParsePluginFormToken(token);
    return parsed && handler
               ? accept(handler->LookupForm(parsed->localID, parsed->plugin))
               : nullptr;
  }
  // Preserve EditorID precedence for IDs made entirely from hexadecimal letters.
  if (auto *form = accept(RE::TESForm::LookupByEditorID(token))) {
    return form;
  }
  if (const auto id = ParseFormIDToken(token)) {
    if (auto *form = accept(RE::TESForm::LookupByID(*id))) { return form; }
  }
  if (!handler || !a_searchRecords) { return nullptr; }
  const auto matches = [&](RE::TESForm *a_form) {
    return accept(a_form) && sfs::strings::EqualsInsensitive(
                                 sfs::armor::GetEditorID(a_form), token);
  };
  // Includes EditorIDs preserved by po3 that are absent from the engine index.
  for (const auto &array : handler->formArrays) {
    for (auto *form : array) {
      if (matches(form)) { return form; }
    }
  }
  for (auto *form : SnapshotRegisteredForms(a_accept)) {
    if (matches(form)) { return form; }
  }
  for (auto *cell : handler->interiorCells) {
    if (matches(cell)) { return cell; }
  }
  return nullptr;
}
std::string GetFormTokenEditorID(const std::string &a_token) {
  const auto *form = LookupFormToken(a_token, false);
  return form ? sfs::armor::GetEditorID(form) : std::string{};
}
} // namespace sfs::conditions
