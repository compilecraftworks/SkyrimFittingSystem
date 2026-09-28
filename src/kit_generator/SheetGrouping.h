#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sfs::kit_generator::sheet {
struct SheetAlias { std::string_view text; };
struct SheetFamily {
  std::string_view plugin;
  std::string_view name;
  std::size_t offset;
  std::size_t count;
};
#include "SheetFamilies.inc"

inline std::string Fold(std::string_view text) {
  std::string result(text);
  for (auto &ch : result) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
  return result;
}

// Keep numbers as whole tokens (FOX27 != FOX270), while recognizing ordinary
// CamelCase EDIDs and acronym-to-word transitions (BDORDurandalArmor).
inline std::string Words(std::string_view text) {
  const auto upper = [](unsigned char ch) { return ch >= 'A' && ch <= 'Z'; };
  const auto lower = [](unsigned char ch) { return ch >= 'a' && ch <= 'z'; };
  const auto digit = [](unsigned char ch) { return ch >= '0' && ch <= '9'; };
  const auto letter = [&](unsigned char ch) { return upper(ch) || lower(ch) || ch >= 128; };
  std::string out = " ";
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto ch = static_cast<unsigned char>(text[i]);
    if (ch == '+' && i && digit(static_cast<unsigned char>(text[i - 1]))) {
      out.push_back('+'); // "300+" is an explicit set, not any outfit numbered 300.
      continue;
    }
    if (!letter(ch) && !digit(ch)) {
      if (out.back() != ' ') out.push_back(' ');
      continue;
    }
    if (i && out.back() != ' ') {
      const auto prev = static_cast<unsigned char>(text[i - 1]);
      const unsigned char next = i + 1 < text.size() ? static_cast<unsigned char>(text[i + 1]) : 0;
      if ((digit(ch) && letter(prev)) || (letter(ch) && digit(prev)) ||
          (upper(ch) && lower(prev)) || (upper(ch) && upper(prev) && lower(next)))
        out.push_back(' ');
    }
    out.push_back(upper(ch) ? static_cast<char>(ch + ('a' - 'A')) : static_cast<char>(ch));
  }
  if (out.back() != ' ') out.push_back(' ');
  return out;
}

struct IndexedAlias { std::string words; std::size_t family; };
struct Index {
  std::unordered_map<std::string, std::vector<IndexedAlias>> byPlugin;
  Index() {
    for (std::size_t i = 0; i < std::size(kSheetFamilies); ++i) {
      const auto &family = kSheetFamilies[i];
      auto &aliases = byPlugin[Fold(family.plugin)];
      for (const auto &alias : std::span(kSheetAliases).subspan(family.offset, family.count)) {
        auto words = Words(alias.text);
        if (words.size() > 2 && std::ranges::none_of(aliases, [&](const auto &other) {
              return other.family == i && other.words == words;
            })) aliases.push_back({std::move(words), i});
      }
    }
  }
};

inline const Index &GetIndex() {
  static const Index index;
  return index;
}

inline std::vector<std::string> DisplayRoots(std::string_view text) {
  std::vector<std::string> roots{Words(text)};
  // Bracketed maker tags are not an outfit root. Do not otherwise search in
  // the middle of a display name: "Emma Swimsuit" is not the "Swimsuit" set.
  while (true) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos || text[first] != '[') break;
    const auto last = text.find(']', first + 1);
    if (last == std::string_view::npos) break;
    text.remove_prefix(last + 1);
    roots.push_back(Words(text));
  }
  return roots;
}

inline std::optional<std::size_t> Resolve(std::string_view plugin,
                                        std::string_view displayName,
                                        std::string_view editorID) {
  const auto &index = GetIndex();
  const auto found = index.byPlugin.find(Fold(plugin));
  if (found == index.byPlugin.end()) return {};
  // A clear display-name match takes precedence over an unrelated internal
  // naming convention; EDIDs also support translated/absent display names.
  for (const bool byEditorID : {false, true}) {
    const auto roots = byEditorID ? std::vector<std::string>{Words(editorID)} : DisplayRoots(displayName);
    std::size_t bestLength = 0;
    std::optional<std::size_t> best;
    bool ambiguous = false;
    for (const auto &alias : found->second) {
      if (alias.words.size() < bestLength || std::ranges::none_of(roots, [&](const auto &words) {
            return byEditorID ? words.find(alias.words) != std::string::npos : words.starts_with(alias.words);
          })) continue;
      if (alias.words.size() > bestLength) {
        bestLength = alias.words.size();
        best = alias.family;
        ambiguous = false;
      } else if (best != alias.family) {
        ambiguous = true;
      }
    }
    // Equal-strength, contradictory sheet labels are not permission to fall
    // through to a less reliable EDID and guess one of those families.
    if (bestLength) return ambiguous ? std::nullopt : best;
  }
  return {};
}
} // namespace sfs::kit_generator::sheet
