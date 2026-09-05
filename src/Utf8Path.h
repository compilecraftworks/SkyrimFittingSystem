#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace sfs::utf8 {
inline std::string Sanitize(std::string_view a_text) {
  std::string result;
  result.reserve(a_text.size());
  constexpr std::string_view replacement{"\xEF\xBF\xBD", 3};
  for (std::size_t index = 0; index < a_text.size();) {
    const auto lead = static_cast<unsigned char>(a_text[index]);
    if (lead < 0x80) {
      result.push_back(static_cast<char>(lead));
      ++index;
      continue;
    }

    std::size_t length = 0;
    if (lead >= 0xC2 && lead <= 0xDF) {
      length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      length = 3;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      length = 4;
    }
    bool valid = length != 0 && index + length <= a_text.size();
    for (std::size_t offset = 1; valid && offset < length; ++offset) {
      const auto continuation =
          static_cast<unsigned char>(a_text[index + offset]);
      valid = continuation >= 0x80 && continuation <= 0xBF;
    }
    if (valid && length == 3) {
      const auto second = static_cast<unsigned char>(a_text[index + 1]);
      valid = !((lead == 0xE0 && second < 0xA0) ||
                (lead == 0xED && second >= 0xA0));
    } else if (valid && length == 4) {
      const auto second = static_cast<unsigned char>(a_text[index + 1]);
      valid = !((lead == 0xF0 && second < 0x90) ||
                (lead == 0xF4 && second > 0x8F));
    }
    if (!valid) {
      result.append(replacement);
      ++index;
      continue;
    }
    result.append(a_text.substr(index, length));
    index += length;
  }
  return result;
}

inline std::u8string ToU8String(std::string_view a_text) {
  const auto sanitized = Sanitize(a_text);
  std::u8string result;
  result.reserve(sanitized.size());
  for (const auto character : sanitized) {
    result.push_back(static_cast<char8_t>(character));
  }
  return result;
}

inline std::string ToString(std::u8string_view a_text) {
  std::string result;
  result.reserve(a_text.size());
  for (const auto character : a_text) {
    result.push_back(static_cast<char>(character));
  }
  return result;
}

inline std::filesystem::path PathFromUtf8(std::string_view a_text) {
  return std::filesystem::path(ToU8String(a_text));
}

inline std::string PathToUtf8String(const std::filesystem::path &a_path) {
  return ToString(a_path.u8string());
}

inline std::string PathToUtf8GenericString(const std::filesystem::path &a_path) {
  return ToString(a_path.generic_u8string());
}
} // namespace sfs::utf8
