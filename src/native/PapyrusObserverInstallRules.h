#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sfs::native::papyrus_observer::rules {
[[nodiscard]] constexpr char AsciiLower(const char a_value) noexcept {
  return a_value >= 'A' && a_value <= 'Z'
             ? static_cast<char>(a_value + ('a' - 'A'))
             : a_value;
}

[[nodiscard]] constexpr bool EqualNoCase(const std::string_view a_left,
                                         const std::string_view a_right) noexcept {
  if (a_left.size() != a_right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < a_left.size(); ++index) {
    if (AsciiLower(a_left[index]) != AsciiLower(a_right[index])) {
      return false;
    }
  }
  return true;
}

// Papyrus can publish a linked type before every member/state array is safe
// to traverse. Keep the generic delayed inspection global-only. The P+
// member natives are the sole exception, and are revisited only after the
// exact sslActorAlias type has reached its fully linked state.
[[nodiscard]] constexpr bool ShouldInspectPostLinkMembers(
    const std::string_view a_className) noexcept {
  return EqualNoCase(a_className, "sslActorAlias");
}

[[nodiscard]] constexpr bool ShouldRetryPostLinkMemberInspection(
    const std::string_view a_className, const std::size_t a_discoveredCount,
    const std::uint8_t a_attempt,
    const std::uint8_t a_maxAttempts) noexcept {
  return ShouldInspectPostLinkMembers(a_className) &&
         a_discoveredCount == 0 && a_attempt + 1 < a_maxAttempts;
}
} // namespace sfs::native::papyrus_observer::rules
