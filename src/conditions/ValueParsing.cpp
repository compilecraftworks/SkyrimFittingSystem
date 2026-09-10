#include "conditions/ValueParsing.h"

#include <RE/A/ActorValueList.h>
#include <algorithm>
#include <cctype>
#include <utility>

namespace sfs::conditions {
std::optional<std::int32_t> ParseActorValueArgument(std::string_view a_text) {
  std::string token(TrimValueToken(a_text));
  if (token.empty() || token.find('\0') != std::string::npos) {
    return std::nullopt;
  }
  auto value = RE::ActorValueList::LookupActorValueByName(token.c_str());
  if (value == RE::ActorValue::kNone) {
    std::ranges::transform(token, token.begin(), [](unsigned char c) {
      return static_cast<char>(std::toupper(c));
    });
    value = RE::ActorValueList::LookupActorValueByName(token.c_str());
  }
  if (value == RE::ActorValue::kNone) { return std::nullopt; }
  return static_cast<std::int32_t>(std::to_underlying(value));
}
} // namespace sfs::conditions
