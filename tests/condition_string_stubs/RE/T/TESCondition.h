#pragma once
#include <RE/B/BSFixedString.h>
#include <cstdlib>
namespace RE {
struct TESConditionItem {
  TESConditionItem *next{};
  BSFixedString *text{};
};
struct TESCondition {
  TESConditionItem *head{};
  ~TESCondition() {
    while (head) {
      if (!BSFixedString::live.contains(head->text)) { std::abort(); }
      auto *next = head->next;
      delete head;
      head = next;
    }
  }
};
}
