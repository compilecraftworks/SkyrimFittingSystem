#pragma once

#include <RE/B/BSFixedString.h>
#include <RE/T/TESCondition.h>

#include <list>
#include <memory>
#include <string>

namespace sfs::conditions {
// Native kChar and kVMScriptVar conditions take BSFixedString*, not char*. Nodes must
// retain stable addresses even when CNF expansion adds more string arguments.
// Share the enclosing owner through an aliasing shared_ptr<TESCondition> so
// cache invalidation cannot release strings while an evaluator still uses it.
struct NativeConditionStorage {
  NativeConditionStorage() = default;
  NativeConditionStorage(const NativeConditionStorage &) = delete;
  NativeConditionStorage &operator=(const NativeConditionStorage &) = delete;
  NativeConditionStorage(NativeConditionStorage &&) = delete;
  NativeConditionStorage &operator=(NativeConditionStorage &&) = delete;

  std::list<RE::BSFixedString> strings;
  RE::TESCondition condition; // Destroy items BEFORE releasing interned strings.

  [[nodiscard]] void *StoreText(const std::string &a_text) {
    if (a_text.find('\0') != std::string::npos) {
      return nullptr; // Never silently evaluate a truncated imported argument.
    }
    strings.emplace_back(a_text.c_str());
    return std::addressof(strings.back());
  }
};
} // namespace sfs::conditions
