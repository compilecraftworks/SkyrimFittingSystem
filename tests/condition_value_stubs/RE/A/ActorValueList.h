#pragma once
#include <cstdint>
#include <string_view>
namespace RE {
enum class ActorValue : std::int32_t { kNone = -1, kAggression = 0, kHealth = 24 };
struct ActorValueList {
  static ActorValue LookupActorValueByName(const char *name) {
    const std::string_view token(name);
    if (token == "Health" || token == "HEALTH") { return ActorValue::kHealth; }
    if (token == "AGGRESSION") { return ActorValue::kAggression; }
    if (token == "RUNTIMEEXTENSION") { return static_cast<ActorValue>(16384); }
    return ActorValue::kNone;
  }
};
}
