#pragma once

#include "StringUtils.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sfs::conditions {
// Form IDs are hexadecimal, with an optional console-style 0x prefix.
// Reject partial parses, overflow and zero rather than resolving another form.
inline std::optional<std::uint32_t> ParseFormIDToken(std::string_view a_text) {
  auto text = sfs::strings::TrimText(a_text);
  if (text.starts_with("0x") || text.starts_with("0X")) {
    text.erase(0, 2);
  }
  if (text.empty() || text.size() > 8) {
    return std::nullopt;
  }
  std::uint32_t value = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value, 16);
  if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
    return std::nullopt;
  }
  return value;
}

struct PluginFormToken {
  std::string plugin;
  std::uint32_t localID;
};

inline std::optional<PluginFormToken>
ParsePluginFormToken(std::string_view a_text) {
  const auto separator = a_text.find('|');
  if (separator == std::string_view::npos ||
      a_text.find('|', separator + 1) != std::string_view::npos) {
    return std::nullopt;
  }
  auto plugin = sfs::strings::TrimText(a_text.substr(0, separator));
  const auto localID = ParseFormIDToken(a_text.substr(separator + 1));
  if (plugin.empty() || !localID || *localID > 0xFFFFFF) {
    return std::nullopt;
  }
  return PluginFormToken{std::move(plugin), *localID};
}

} // namespace sfs::conditions
