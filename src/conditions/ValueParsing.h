#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>

namespace sfs::conditions {
inline std::string_view TrimValueToken(std::string_view a_text) {
  constexpr std::string_view whitespace = " \t\r\n\f\v";
  const auto first = a_text.find_first_not_of(whitespace);
  if (first == std::string_view::npos) { return {}; }
  return a_text.substr(first, a_text.find_last_not_of(whitespace) - first + 1);
}

inline std::optional<std::int32_t> TryParseInt(std::string_view a_text) {
  auto token = TrimValueToken(a_text);
  if (token.empty()) { return std::nullopt; }
  // Preserve the leading plus sign accepted by the former UI stoi path.
  if (token.front() == '+') {
    token.remove_prefix(1);
    if (token.empty() || token.front() == '-' || token.front() == '+') {
      return std::nullopt;
    }
  }
  std::int32_t value{};
  const auto end = token.data() + token.size();
  const auto [ptr, error] = std::from_chars(token.data(), end, value);
  if (error == std::errc{} && ptr == end) { return value; }
  return std::nullopt;
}

inline std::optional<float> TryParseFloat(std::string_view a_text) {
  const std::string token(TrimValueToken(a_text));
  if (token.empty()) { return std::nullopt; }
  try {
    std::size_t parsed{};
    const float value = std::stof(token, &parsed);
    // Native conditions store a float, including their comparison value.
    if (parsed == token.size() && std::isfinite(value)) { return value; }
  } catch (const std::exception &) {
  }
  return std::nullopt;
}

inline std::optional<std::int32_t> ParseAxisArgument(std::string_view a_text) {
  const auto token = TrimValueToken(a_text);
  if (token.size() != 1) { return std::nullopt; }
  switch (token.front()) {
  case 'X': case 'x': return 0;
  case 'Y': case 'y': return 1;
  case 'Z': case 'z': return 2;
  default: return std::nullopt;
  }
}

// Uses the loaded engine's name lookup, never a compile-time enum bound.
[[nodiscard]] std::optional<std::int32_t>
ParseActorValueArgument(std::string_view a_text);
} // namespace sfs::conditions
