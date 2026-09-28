#pragma once

#include "CommunityGrouping.h"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace sfs::kit_generator::screenshot {
struct ScreenshotMember {
  std::string_view editorID;
  std::uint32_t localFormID;
  bool anchor;
};
struct ScreenshotReference {
  std::string_view plugin;
  std::string_view name;
  std::size_t offset;
  std::size_t count;
};
#include "AddScreenshotReferences.inc"

inline std::string Key(std::string_view plugin, std::string_view editorID, std::uint32_t localID) {
  auto key = community::Key(plugin, editorID);
  key.push_back('\0');
  key += std::to_string(localID);
  return key;
}

struct Index {
  std::unordered_map<std::string, std::vector<std::size_t>> byPlugin;
  std::unordered_set<std::string> members;
  Index() {
    for (std::size_t i = 0; i < std::size(kScreenshotReferences); ++i) {
      const auto &ref = kScreenshotReferences[i];
      byPlugin[std::string(ref.plugin)].push_back(i);
      for (const auto &member : std::span(kScreenshotMembers).subspan(ref.offset, ref.count))
        members.insert(Key(ref.plugin, member.editorID, member.localFormID));
    }
  }
};
inline const Index &GetIndex() { static const Index index; return index; }
template <class Record> bool IsReferenced(const Record &item) {
  return GetIndex().members.contains(Key(item.pluginName, item.editorID, item.localFormID));
}

struct Match { std::size_t reference; std::vector<std::size_t> items; };
template <class Record, class Check>
std::vector<Match> MatchGroups(const std::vector<Record> &items, Check check) {
  const auto &index = GetIndex();
  std::unordered_map<std::string, std::size_t> lookup;
  std::unordered_set<std::string> ambiguous, plugins;
  for (std::size_t i = 0; i < items.size(); ++i) {
    check();
    const auto &item = items[i];
    auto key = Key(item.pluginName, item.editorID, item.localFormID);
    if (!lookup.emplace(key, i).second) ambiguous.insert(std::move(key));
    plugins.insert(community::Fold(item.pluginName));
  }
  std::vector<std::size_t> references;
  for (const auto &plugin : plugins)
    if (auto found = index.byPlugin.find(plugin); found != index.byPlugin.end())
      references.insert(references.end(), found->second.begin(), found->second.end());
  std::ranges::sort(references);
  std::vector<Match> result;
  for (const auto id : references) {
    check();
    const auto &ref = kScreenshotReferences[id];
    Match match{id, {}};
    bool anchor = false;
    for (const auto &member : std::span(kScreenshotMembers).subspan(ref.offset, ref.count)) {
      check();
      auto key = Key(ref.plugin, member.editorID, member.localFormID);
      auto found = lookup.find(key);
      if (found != lookup.end() && !ambiguous.contains(key)) {
        match.items.push_back(found->second);
        anchor |= member.anchor;
      }
    }
    // Explicit screenshot kits may be a single complete suit. Shared boots
    // alone cannot create all missing numbered outfits. Local IDs are guarded
    // by EDID too: duplicate EDIDs in ADD must not select the wrong record.
    if (anchor && !match.items.empty()) result.push_back(std::move(match));
  }
  return result;
}
} // namespace sfs::kit_generator::screenshot
