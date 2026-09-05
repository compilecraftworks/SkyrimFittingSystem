#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sfs::catalog::kit_json {
inline bool IsItemEquipped(const nlohmann::json &a_itemData) noexcept {
  if (!a_itemData.is_object()) {
    // Legacy SVS/Modex-compatible files did not always store item metadata.
    return true;
  }

  const auto equipped = a_itemData.find("Equipped");
  if (equipped == a_itemData.end()) {
    return true;
  }
  if (equipped->is_boolean()) {
    return equipped->get<bool>();
  }
  if (equipped->is_number_integer()) {
    return equipped->get<std::int64_t>() != 0;
  }

  // Invalid explicit values must not silently add an appearance.
  return false;
}

inline std::optional<std::string>
GetItemPluginName(const nlohmann::json &a_itemData) {
  if (!a_itemData.is_object()) {
    return std::nullopt;
  }
  const auto plugin = a_itemData.find("Plugin");
  if (plugin == a_itemData.end() || !plugin->is_string()) {
    return std::nullopt;
  }
  auto value = plugin->get<std::string>();
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

inline std::string LowerAscii(std::string_view a_value) {
  std::string result(a_value);
  std::ranges::transform(result, result.begin(), [](const unsigned char a_ch) {
    return a_ch < 128 ? static_cast<char>(std::tolower(a_ch))
                      : static_cast<char>(a_ch);
  });
  return result;
}

inline bool PluginNamesEqual(const std::string_view a_left,
                             const std::string_view a_right) {
  return LowerAscii(a_left) == LowerAscii(a_right);
}

inline std::string MakePluginEditorIDKey(const std::string_view a_plugin,
                                         const std::string_view a_editorID) {
  auto key = LowerAscii(a_plugin);
  key.push_back('|');
  key.append(LowerAscii(a_editorID));
  return key;
}
} // namespace sfs::catalog::kit_json
