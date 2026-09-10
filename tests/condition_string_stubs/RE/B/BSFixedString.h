#pragma once
#include <string>
#include <unordered_set>
namespace RE {
// Instrument only the engine-owned pool boundary; production ownership code
// is compiled unchanged. This does not emulate engine condition evaluation.
struct BSFixedString {
  inline static std::unordered_set<const BSFixedString *> live;
  std::string text;
  explicit BSFixedString(const char *value) : text(value) { live.insert(this); }
  ~BSFixedString() { live.erase(this); }
  const char *c_str() const { return text.c_str(); }
};
}
