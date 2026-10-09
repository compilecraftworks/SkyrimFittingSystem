#pragma once
#include <cstdint>
#include <string_view>

namespace sfs::native::ostim::rules {
constexpr std::string_view kActivePrefix = "OStim:";
constexpr std::string_view kEndedPrefix = "OStimEnded:";
[[nodiscard]] constexpr bool IsOwned(std::string_view source) {
  return source.starts_with(kActivePrefix) || source.starts_with(kEndedPrefix);
}
[[nodiscard]] constexpr std::string_view SessionSuffix(std::string_view source) {
  if (source.starts_with(kActivePrefix)) { return source.substr(kActivePrefix.size()); }
  if (source.starts_with(kEndedPrefix)) { return source.substr(kEndedPrefix.size()); }
  return {};
}
[[nodiscard]] constexpr bool SameSession(std::string_view source, std::string_view session) {
  const auto suffix = SessionSuffix(source);
  return !suffix.empty() && suffix == SessionSuffix(session);
}
[[nodiscard]] constexpr bool CanStrip(std::uint32_t controlMask,
    std::uint32_t requestedMask, bool noStrip, bool wig, bool undressWigs) {
  return (controlMask & requestedMask) != 0 && !noStrip && (!wig || undressWigs);
}
// A multi-slot appearance remains atomic, just like an actual ARMO.
[[nodiscard]] constexpr bool CanRestore(std::uint32_t controlMask,
                                      std::uint32_t requestedMask) {
  return (controlMask & requestedMask) != 0;
}
} // namespace sfs::native::ostim::rules
