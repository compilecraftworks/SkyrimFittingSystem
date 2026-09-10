#pragma once

#include <RE/Skyrim.h>

#include <string>
#include <vector>

namespace sfs::conditions {
using FormTokenPredicate = bool (*)(RE::TESForm *);

[[nodiscard]] RE::TESForm *LookupFormTokenIf(
    const std::string &a_token, FormTokenPredicate a_accept,
    bool a_searchRecords = true);
[[nodiscard]] std::vector<RE::TESForm *> CollectCellForms();

template <class T = RE::TESForm>
[[nodiscard]] T *LookupFormToken(const std::string &a_token,
                                  const bool a_searchRecords = true) {
  auto *form = LookupFormTokenIf(a_token, [](RE::TESForm *a_form) {
    return a_form->As<T>() != nullptr;
  }, a_searchRecords);
  return form ? form->As<T>() : nullptr;
}
} // namespace sfs::conditions
