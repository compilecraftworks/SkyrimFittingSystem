#include "kit_generator/AddScreenshotGrouping.h"
#include <future>
#include <iostream>
#include <stdexcept>

namespace s = sfs::kit_generator::screenshot;
struct Record { std::string pluginName; std::string editorID; std::uint32_t localFormID; };
void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
std::vector<Record> Records(std::string_view plugin, std::string_view name) {
  std::vector<Record> result;
  for (const auto &ref : s::kScreenshotReferences) {
    if (ref.plugin != plugin || ref.name != name) continue;
    for (const auto &m : std::span(s::kScreenshotMembers).subspan(ref.offset, ref.count))
      result.push_back({std::string(ref.plugin), std::string(m.editorID), m.localFormID});
  }
  Require(!result.empty(), "Fixture PNG has no verified membership");
  return result;
}
int main() {
  try {
    auto first = Records("add 03.esp", "다크 나이트 01");
    auto second = Records("add 03.esp", "다크 나이트 02");
    auto records = first;
    for (const auto &item : second)
      if (std::ranges::none_of(records, [&](const auto &other) { return item.localFormID == other.localFormID; })) records.push_back(item);
    auto matches = s::MatchGroups(records, [] {});
    Require(matches.size() == 2, "PNG 01 and 02 must remain distinct; shared accessories cannot create absent kits");
    auto wrong = first;
    for (auto &item : wrong) item.pluginName = "ADD 04.esp";
    Require(s::MatchGroups(wrong, [] {}).empty(), "Same name/EDID in a different ADD plugin is not a match");
    wrong = first;
    for (auto &item : wrong) item.localFormID += 0x500000;
    Require(s::MatchGroups(wrong, [] {}).empty(), "Changed local IDs must not resolve duplicate EDIDs arbitrarily");
    wrong = first;
    for (auto &item : wrong) item.editorID += "Changed";
    Require(s::MatchGroups(wrong, [] {}).empty(), "A reused FormID must pass the EDID identity guard");
    auto duplicates = first;
    duplicates.insert(duplicates.end(), first.begin(), first.end());
    Require(s::MatchGroups(duplicates, [] {}).empty(), "Ambiguous duplicate snapshot identities must fail closed");
    auto suit = Records("add 03.esp", "말리부 수영복 06");
    Require(s::MatchGroups(std::vector<Record>{suit.front()}, [] {}).size() == 1,
            "An explicit PNG may describe a complete one-piece outfit");
    const auto pose1 = Records("add 01.esp", "몬노 비키니 01");
    const auto pose2 = Records("add 01.esp", "몬노 비키니 02");
    Require(pose1.size() == pose2.size() && std::ranges::equal(pose1, pose2, [](const auto &a, const auto &b) {
              return a.editorID == b.editorID && a.localFormID == b.localFormID;
            }), "Photo/BodySlide variants must retain the same complete outfit pool under separate PNG names");
    bool cancelled = false;
    try { s::MatchGroups(records, [] { throw std::runtime_error("cancelled"); }); }
    catch (const std::runtime_error &) { cancelled = true; }
    Require(cancelled, "Screenshot matching must support cancellation");
    std::vector<std::future<std::size_t>> tasks;
    for (int i = 0; i < 8; ++i) tasks.push_back(std::async(std::launch::async, [&] { return s::MatchGroups(records, [] {}).size(); }));
    for (auto &task : tasks) Require(task.get() == 2, "Parallel scans must preserve PNG boundaries");
    std::unordered_set<std::string> names;
    for (const auto &ref : s::kScreenshotReferences) {
      Require(ref.count > 0, "Every generated PNG reference must have a resolved outfit pool");
      Require(ref.offset + ref.count <= std::size(s::kScreenshotMembers), "Invalid generated membership range");
      Require(names.insert(sfs::kit_generator::community::Key(ref.plugin, ref.name)).second, "PNG identities must not collapse by normalization");
    }
    std::cout << "ADD screenshot identity, numeric boundary, duplicate EDID, singleton, cancellation and concurrency tests passed\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
