#pragma once

#include <format>
#include <string>
#include <string_view>

namespace sfs::kit_generator {
class Localization {
public:
  static Localization &Get();

  void SyncWithHost() const {}
  [[nodiscard]] std::string Text(std::string_view a_key) const;

  template <class... Args>
  [[nodiscard]] std::string Format(const std::string_view a_key,
                                   const Args &...a_args) const {
    return std::vformat(Text(a_key), std::make_format_args(a_args...));
  }
};
} // namespace sfs::kit_generator
