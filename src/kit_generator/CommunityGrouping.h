#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sfs::kit_generator::community {
struct CommunityMember {
  std::string_view editorID;
  bool anchor;
};
struct CommunityReference {
  std::string_view name;
  std::string_view plugin;
  std::size_t offset;
  std::size_t count;
};

#include "CommunityKitReferences.inc"

inline std::string Fold(std::string_view value) {
  std::string result(value);
  for (auto &ch : result) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch + ('a' - 'A'));
    }
  }
  return result;
}

// Only for the curated reference titles, not arbitrary armor/plugin names.
// Keep author and semantic outfit title intact; trailing annotations/numbers
// describe selectable versions of that title. Plugin scoping is mandatory.
inline std::string FamilyName(std::string_view name) {
  std::string result(name);
  const auto trim = [&] {
    while (!result.empty() && result.back() == ' ') result.pop_back();
  };
  trim();
  for (;;) {
    if (result.ends_with(')')) {
      const auto start = result.find_last_of('(');
      if (start == std::string::npos || start == 0 || result[start - 1] != ' ') break;
      auto prefixEnd = start;
      while (prefixEnd > 0 && result[prefixEnd - 1] == ' ') --prefixEnd;
      if (prefixEnd == 0 || result[prefixEnd - 1] == ']') break;
      result.resize(start);
      trim();
      continue;
    }
    const auto folded = Fold(result);
    bool robeWeight = false;
    for (const std::string_view suffix : {" heavy robe", " light robe", " unarmored robe"}) {
      if (folded.ends_with(suffix)) {
        result.resize(result.size() - suffix.size());
        result += " Robe";
        robeWeight = true;
        break;
      }
    }
    if (robeWeight) break;
    const auto space = result.find_last_of(' ');
    if (space == std::string::npos) break;
    auto tail = Fold(std::string_view(result).substr(space + 1));
    const bool roman = tail == "i" || tail == "ii" || tail == "iii" ||
        tail == "iv" || tail == "v" || tail == "vi" || tail == "vii" ||
        tail == "viii" || tail == "ix" || tail == "x";
    if (tail.starts_with('v')) tail.erase(0, 1);
    const bool number = !tail.empty() && tail.size() <= 2 &&
        std::ranges::all_of(tail, [](char c) { return c >= '0' && c <= '9'; });
    const bool weight = tail == "heavy" || tail == "light" || tail == "unarmored";
    const bool color = tail == "dark" || tail == "black" || tail == "white" ||
        tail == "red" || tail == "blue" || tail == "green" || tail == "brown" ||
        tail == "pink" || tail == "purple" || tail == "gold" || tail == "silver";
    if (!roman && !number && !weight && !color) break;
    // An author tag alone is not an outfit identity.
    const auto prefix = std::string_view(result).substr(0, space);
    if (prefix.empty() || prefix.ends_with(']')) break;
    result.resize(space);
    trim();
  }
  return result.empty() ? std::string(name) : result;
}

inline std::string Key(std::string_view plugin, std::string_view editorID) {
  auto result = Fold(plugin);
  result.push_back('\0');
  result += Fold(editorID);
  return result;
}

struct Index {
  std::unordered_map<std::string, std::vector<std::size_t>> referencesByPlugin;
  std::unordered_set<std::string> memberKeys;
  Index() {
    for (std::size_t i = 0; i < std::size(kCommunityReferences); ++i) {
      const auto &ref = kCommunityReferences[i];
      referencesByPlugin[std::string(ref.plugin)].push_back(i);
      for (const auto &member : std::span(kCommunityMembers).subspan(ref.offset, ref.count)) {
        memberKeys.insert(Key(ref.plugin, member.editorID));
      }
    }
  }
};

inline const Index &GetIndex() {
  static const Index index;
  return index;
}

inline bool IsReferenced(std::string_view plugin, std::string_view editorID) {
  return GetIndex().memberKeys.contains(Key(plugin, editorID));
}

struct Match {
  std::size_t reference;
  std::vector<std::size_t> items;
};

// The caller supplies a cancellation check, so construction on scan/assessment
// workers shares their existing lifetime and never touches live game objects.
template <class Record, class Check>
std::vector<Match> MatchGroups(const std::vector<Record> &items, Check check) {
  const auto &index = GetIndex();
  std::unordered_map<std::string, std::size_t> lookup;
  std::unordered_set<std::string> ambiguous;
  std::unordered_set<std::string> plugins;
  for (std::size_t i = 0; i < items.size(); ++i) {
    check();
    const auto &item = items[i];
    if (item.editorID.empty()) {
      continue;
    }
    auto key = Key(item.pluginName, item.editorID);
    if (!lookup.emplace(key, i).second) {
      ambiguous.insert(std::move(key));
    }
    plugins.insert(Fold(item.pluginName));
  }
  std::vector<std::size_t> references;
  for (const auto &plugin : plugins) {
    if (auto found = index.referencesByPlugin.find(plugin); found != index.referencesByPlugin.end()) {
      references.insert(references.end(), found->second.begin(), found->second.end());
    }
  }
  std::ranges::sort(references);
  std::vector<Match> result;
  for (const auto id : references) {
    check();
    const auto &reference = kCommunityReferences[id];
    Match match{id, {}};
    bool hasAnchor = false;
    for (const auto &member : std::span(kCommunityMembers).subspan(reference.offset, reference.count)) {
      check();
      const auto key = Key(reference.plugin, member.editorID);
      const auto found = lookup.find(key);
      if (found != lookup.end() && !ambiguous.contains(key)) {
        match.items.push_back(found->second);
        hasAnchor |= member.anchor;
      }
    }
    // A shared glove/boot pair alone must not invent every absent color/length
    // variant. At least half of the reference and one of its least-shared
    // identities must actually be present. Single-piece shards never qualify.
    if (hasAnchor && match.items.size() >= 2 && match.items.size() * 2 >= reference.count) {
      result.push_back(std::move(match));
    }
  }
  return result;
}
} // namespace sfs::kit_generator::community
