#include "conditions/NativeConditionStorage.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>

void Expect(bool value, const char *message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
  using sfs::conditions::NativeConditionStorage;
  static_assert(!std::is_copy_constructible_v<NativeConditionStorage>);
  static_assert(!std::is_move_constructible_v<NativeConditionStorage>);
  for (int cycle = 0; cycle < 100; ++cycle) {
    auto owner = std::make_shared<NativeConditionStorage>();
    std::shared_ptr<RE::TESCondition> evaluator(owner, &owner->condition);
    auto *name = static_cast<RE::BSFixedString *>(owner->StoreText("iState"));
    auto *numericName = static_cast<RE::BSFixedString *>(owner->StoreText("0012"));
    auto *vmName = static_cast<RE::BSFixedString *>(owner->StoreText("::MyProperty_var"));
    // VM predicates keep the reference/quest in slot 0 and the string object
    // address in slot 1. They must not normalize the name or read slot 0 as text.
    int targetFormFixture = 42;
    void *vmParams[2]{&targetFormFixture, vmName};
    Expect(*static_cast<int *>(vmParams[0]) == 42 &&
           std::string(static_cast<RE::BSFixedString *>(vmParams[1])->c_str()) == "::MyProperty_var",
           "VM argument 2 is a fixed-string object, separate from target form");
    owner->condition.head = new RE::TESConditionItem{nullptr, name};
    for (int i = 0; i < 1024; ++i) {
      std::string temporary = "Variable_" + std::to_string(i);
      auto *stored = static_cast<RE::BSFixedString *>(owner->StoreText(temporary));
      temporary.assign(2048, 'x');
      Expect(std::string(stored->c_str()) == "Variable_" + std::to_string(i),
             "draft string changes cannot change stored argument");
    }
    Expect(std::string(name->c_str()) == "iState", "expansion keeps BSFixedString object address stable");
    Expect(std::string(numericName->c_str()) == "0012", "numeric-looking names stay text");
    const auto count = owner->strings.size();
    Expect(owner->StoreText(std::string("bad\0suffix", 10)) == nullptr,
           "embedded NUL is rejected instead of truncated");
    Expect(owner->strings.size() == count, "failed argument adds no allocation");
    std::weak_ptr<NativeConditionStorage> weak = owner;
    auto secondEvaluator = evaluator;
    owner.reset(); // Cache invalidation / failed rebuild while evaluation holds old condition.
    evaluator.reset();
    Expect(!weak.expired() && RE::BSFixedString::live.contains(name),
           "last active evaluator keeps strings alive after cache invalidation");
    Expect(std::string(secondEvaluator->head->text->c_str()) == "iState", "condition parameter remains usable");
    Expect(std::string(static_cast<RE::BSFixedString *>(vmParams[1])->c_str()) == "::MyProperty_var",
           "VM property name survives cache invalidation and expansion");
    secondEvaluator.reset();
    Expect(weak.expired() && RE::BSFixedString::live.empty(), "last evaluator releases all strings and nodes");
  }
  {
    auto partial = std::make_shared<NativeConditionStorage>();
    (void)partial->StoreText("partial");
    // Simulates failure of a later parameter/comparand before publishing.
  }
  Expect(RE::BSFixedString::live.empty(), "failed emission releases partial string storage");
  std::cout << "Condition string lifetime/rebuild/destruction tests passed\n";
}
