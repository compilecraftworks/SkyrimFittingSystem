#include "../src/kit_generator/Generator.cpp"
#include <iostream>

namespace sfs::body_family {
Mask ClassifyCatalogArmor(const RE::TESObjectARMO*) {
  throw std::runtime_error("Offline corpus audit cannot snapshot live engine forms");
}
}

std::set<std::string> IdentitySet(const std::vector<ArmorRecord> &items) {
  std::set<std::string> result;
  for (const auto &item : items)
    result.insert(sfs::kit_generator::community::Key(item.pluginName, item.editorID));
  return result;
}

// Independently check selected records against competing outfit-wide labels.
// Labels are alternatives only if both occur on two component slots and never
// coexist on an input record. Common parts without a label are allowed.
std::size_t CheckCoordinatedChoices(const OutfitGroup &group,
                                  const std::vector<KitCandidate> &candidates) {
  if (!group.variants.empty()) {
    std::size_t checked = 0;
    std::set<std::set<std::string>> expected, actual;
    for (const auto& variant : group.variants) {
      const auto original = BuildCandidates(variant, {}, {}, 1);
      checked += CheckCoordinatedChoices(variant, original);
      for (const auto& candidate : original) expected.insert(IdentitySet(candidate.items));
      if (!original.empty() && std::ranges::none_of(candidates, [&](const auto& candidate) {
            return std::ranges::any_of(original, [&](const auto& leaf) { return IdentitySet(leaf.items) == IdentitySet(candidate.items); });
          })) throw std::runtime_error("Family lost an explicit variant: " + variant.name);
    }
    for (const auto& candidate : candidates) actual.insert(IdentitySet(candidate.items));
    for (const auto& members : actual)
      if (!expected.contains(members)) throw std::runtime_error("Family mixed explicit memberships: " + group.name);
    if (expected.size() <= kMaximumGeneratedCandidateProfiles && actual != expected)
      throw std::runtime_error("Family lost original candidate compositions: " + group.name);
    return checked;
  }
  std::vector<SlotCandidate> profiles;
  for (const auto &item : group.items)
    profiles.push_back({item, ExtractProfile(group.name, item, group.rootFamilyExpanded), {}});
  NormalizeOutfitWideProfiles(profiles);
  std::map<std::string, std::uint32_t> slots;
  std::map<std::uint32_t, std::unordered_set<std::string>> labels;
  for (const auto &item : profiles) {
    auto tokens = HardProfileTokens(item.profile);
    for (const auto &token : tokens) slots[token] |= item.item.layoutSlotMask;
    labels.emplace(item.item.localFormID, std::move(tokens));
  }
  std::vector<std::pair<std::string, std::string>> alternatives;
  for (const auto &[left, leftSlots] : slots)
    for (const auto &[right, rightSlots] : slots) {
      if (left >= right || std::popcount(leftSlots & rightSlots) < 2) continue;
      if (std::ranges::any_of(labels, [&](const auto &entry) {
            return entry.second.contains(left) && entry.second.contains(right);
          })) continue;
      alternatives.emplace_back(left, right);
    }
  if (alternatives.empty()) return 0;
  for (const auto &candidate : candidates) {
    std::unordered_set<std::string> selected;
    for (const auto &item : candidate.items) {
      const auto &tokens = labels.at(item.localFormID);
      selected.insert(tokens.begin(), tokens.end());
    }
    for (const auto &[left, right] : alternatives)
      if (selected.contains(left) && selected.contains(right))
        throw std::runtime_error("Mixed coordinated variations: " + group.name + " / " + left + " / " + right);
  }
  return candidates.size();
}

// Exact 1.7.1 appearance deduplication, kept here solely to score the baseline
// without the patch's preservation of reference member identities.
std::vector<ArmorRecord> BaselineDeduplicate(const std::vector<ArmorRecord> &items, bool preserveCommunity = false) {
  std::vector<ArmorRecord> result;
  for (const auto &item : items) {
    auto found = std::ranges::find_if(result, [&](const auto &other) {
      namespace community = sfs::kit_generator::community;
      if (preserveCommunity && community::Key(other.pluginName, other.editorID) != community::Key(item.pluginName, item.editorID) &&
          (community::IsReferenced(other.pluginName, other.editorID) || community::IsReferenced(item.pluginName, item.editorID))) return false;
      if (other.sourceSlotMask != item.sourceSlotMask) return false;
      if (!other.armorAddonFormIDs.empty() && !item.armorAddonFormIDs.empty())
        return other.armorAddonFormIDs == item.armorAddonFormIDs;
      return NormalizeKey(other.DisplayName()) == NormalizeKey(item.DisplayName()) &&
             LowerAscii(other.editorID) == LowerAscii(item.editorID);
    });
    if (found == result.end()) result.push_back(item);
    else if (std::tuple{!item.adultVariant, !item.enchanted, -static_cast<std::int64_t>(item.sourceOrder)} >
             std::tuple{!found->adultVariant, !found->enchanted, -static_cast<std::int64_t>(found->sourceOrder)}) *found = item;
  }
  std::ranges::sort(result, {}, &ArmorRecord::sourceOrder);
  return result;
}

// Frozen v1 path, for measuring the sheet pass against the previous custom patch.
std::vector<OutfitGroup> CommunityPatch1(const std::vector<ArmorRecord> &items) {
  namespace c = sfs::kit_generator::community;
  const auto matches = c::MatchGroups(items, [] {});
  if (matches.empty()) return BuildHeuristicOutfitGroups(items);
  std::vector<OutfitGroup> result;
  std::unordered_map<std::string, std::size_t> used;
  for (const auto &match : matches) {
    OutfitGroup group{std::string(c::kCommunityReferences[match.reference].name), {}};
    for (auto index : match.items) { group.items.push_back(items[index]); ++used[items[index].Identifier()]; }
    result.push_back(std::move(group));
  }
  if (used.size() == items.size()) return result;
  for (auto &group : BuildHeuristicOutfitGroups(items)) {
    if (std::ranges::none_of(group.items, [&](const auto &item) { return !used.contains(item.Identifier()); })) continue;
    std::erase_if(group.items, [&](const auto &item) { auto it = used.find(item.Identifier()); return it != used.end() && it->second == 1; });
    if (group.items.size() >= kMinimumGroupSize) result.push_back(std::move(group));
  }
  return result;
}

int main(int argc, char **argv) {
  try {
    if (argc != 3) throw std::runtime_error("Usage: CommunityGroupingAudit corpus.json result.json");
    std::ifstream input(argv[1]);
    const auto corpus = nlohmann::json::parse(input);
    nlohmann::json output{{"plugins", nlohmann::json::array()}, {"references", nlohmann::json::array()}};
    std::size_t baselineExact = 0, patchedExact = 0, measured = 0, candidateChecks = 0, coordinatedChecks = 0;
    double baselineJaccard = 0, patchedJaccard = 0;
    output["sheetFamilies"] = nlohmann::json::array();
    output["sheetResidualChecks"] = nlohmann::json::array();
    output["screenshots"] = nlohmann::json::array();
    std::size_t screenshotCount = 0, screenshotBeforeExact = 0, screenshotAfterExact = 0;
    std::size_t sheetFamilies = 0, sheetBeforeExact = 0, sheetAfterExact = 0, sheetRecords = 0;
    std::size_t sheetBeforeTogether = 0, sheetAfterTogether = 0;
    for (const auto &plugin : corpus.at("plugins")) {
      const auto pluginName = plugin.at("name").get<std::string>();
      if (kBaseGamePlugins.contains(LowerAscii(pluginName))) continue;
      std::vector<ArmorRecord> records;
      for (const auto &json : plugin.at("armors")) {
        ArmorRecord item;
        item.pluginName = pluginName;
        item.editorID = json.at("editorID").get<std::string>();
        item.name = json.at("name").get<std::string>();
        if (json.contains("nameBytes")) {
          const auto bytes = json.at("nameBytes").get<std::vector<unsigned char>>();
          item.name.assign(bytes.begin(), bytes.end());
        }
        item.runtimeFormID = item.localFormID = json.at("formID").get<std::uint32_t>();
        item.sourceSlotMask = json.at("slots").get<std::uint32_t>();
        item.visualSlotMask = InferVisualSlot(item.editorID + " " + item.name, item.sourceSlotMask);
        item.layoutSlotMask = SelectPrimaryLayoutSlot(item.sourceSlotMask);
        item.armorAddonFormIDs = json.at("addons").get<std::vector<std::uint32_t>>();
        std::ranges::sort(item.armorAddonFormIDs);
        item.armorAddonFormIDs.erase(std::unique(item.armorAddonFormIDs.begin(), item.armorAddonFormIDs.end()), item.armorAddonFormIDs.end());
        item.armorModelPaths = json.at("models").get<std::vector<std::string>>();
        item.enchanted = json.at("enchanted").get<bool>();
        item.sourceOrder = records.size();
        records.push_back(std::move(item));
      }
      std::size_t excluded = 0;
      auto classified = PreferBaseVariants(records, excluded, true);
      // Real part-fragmentation examples evaluated without any community,
      // spreadsheet or plugin-specific name table, using an unlisted plugin.
      const auto checkGenericParts = [&](std::string_view prefix, std::size_t expectedCount) {
        std::vector<ArmorRecord> parts;
        for (auto item : classified) {
          if (!LowerAscii(item.editorID).starts_with(prefix)) continue;
          item.pluginName = "GenericPartAudit.esp";
          parts.push_back(std::move(item));
        }
        if (parts.size() != expectedCount) throw std::runtime_error("Part audit input changed");
        const auto groups = BuildHeuristicOutfitGroups(parts);
        if (groups.size() != 1 || groups.front().items.size() != parts.size())
          throw std::runtime_error("Generic part fragmentation remains: " + std::string(prefix));
        const auto candidates = BuildCandidates(groups.front(), {}, {}, 1);
        std::uint32_t availableSlots = 0, bestCoverage = 0;
        for (const auto &item : parts) availableSlots |= item.sourceSlotMask;
        for (const auto &candidate : candidates) {
          if (CountSlotConflicts(candidate.items)) throw std::runtime_error("Generic part candidate conflict");
          std::uint32_t mask = 0;
          for (const auto &item : candidate.items) mask |= item.sourceSlotMask;
          if (std::popcount(mask) > std::popcount(bestCoverage)) bestCoverage = mask;
        }
        output["genericPartChecks"].push_back({{"plugin", pluginName}, {"editorPrefix", prefix},
          {"records", parts.size()}, {"groups", groups.size()}, {"candidates", candidates.size()},
          {"availableSlots", std::popcount(availableSlots)}, {"bestCandidateSlots", std::popcount(bestCoverage)}});
      };
      if (pluginName == "TULLIUS COLLECTION 01.esp") checkGenericParts("ahbreezeoutfit", 9);
      if (pluginName == "TULLIUS COLLECTION 02.esp") checkGenericParts("luscious_", 12);
      const auto baseline = BuildHeuristicOutfitGroups(BaselineDeduplicate(classified));
      const auto usable = DeduplicateAppearanceRecords(classified);
      const auto patched = BuildFinalOutfitGroups(usable);
      const auto previous = CommunityPatch1(BaselineDeduplicate(classified, true));
      namespace screenshot = sfs::kit_generator::screenshot;
      const auto screenshotIDs = [](const auto &items) {
        std::set<std::string> result;
        for (const auto &item : items) result.insert(screenshot::Key(item.pluginName, item.editorID, item.localFormID));
        return result;
      };
      for (const auto &match : screenshot::MatchGroups(usable, [] {})) {
        std::vector<ArmorRecord> members;
        for (const auto index : match.items) members.push_back(usable[index]);
        const auto expected = screenshotIDs(members);
        const auto name = screenshot::kScreenshotReferences[match.reference].name;
        const bool before = std::ranges::any_of(previous, [&](const auto &group) { return screenshotIDs(group.items) == expected; });
        const bool after = std::ranges::any_of(patched, [&](const auto &group) { return group.name == name && screenshotIDs(group.items) == expected; });
        ++screenshotCount; screenshotBeforeExact += before; screenshotAfterExact += after;
        output["screenshots"].push_back({{"plugin", pluginName}, {"png", name}, {"parts", members.size()},
                                       {"previousExact", before}, {"patchedExact", after}});
      }
      for (const auto &group : previous) {
        std::set<std::size_t> known;
        nlohmann::json residualNames = nlohmann::json::array();
        for (const auto &member : group.items) {
          if (const auto id = sfs::kit_generator::sheet::Resolve(member.pluginName, member.DisplayName(), member.editorID)) known.insert(*id);
          else residualNames.push_back({{"editorID", member.editorID}, {"name", sfs::utf8::Sanitize(member.name)}});
        }
        if (known.size() == 1 && !residualNames.empty())
          output["sheetResidualChecks"].push_back({{"plugin", pluginName}, {"oldGroup", group.name},
            {"sheetFamily", sfs::kit_generator::sheet::kSheetFamilies[*known.begin()].name}, {"unresolved", residualNames}});
      }
      std::set<std::string> communityUsed;
      for (const auto &match : sfs::kit_generator::community::MatchGroups(usable, [] {}))
        for (auto index : match.items) communityUsed.insert(usable[index].Identifier());
      std::map<std::size_t, std::vector<ArmorRecord>> sheetBuckets;
      for (const auto &item : usable) {
        if (communityUsed.contains(item.Identifier())) continue;
        if (const auto family = sfs::kit_generator::sheet::Resolve(item.pluginName, item.DisplayName(), item.editorID))
          sheetBuckets[*family].push_back(item);
      }
      for (const auto &[family, members] : sheetBuckets) {
        if (members.size() < 2) continue;
        ++sheetFamilies;
        sheetRecords += members.size();
        const auto expected = IdentitySet(members);
        const auto coverage = [&](const auto &groups) {
          bool exact = false, together = false;
          std::size_t intersecting = 0;
          for (const auto &group : groups) {
            const auto actual = IdentitySet(group.items);
            std::size_t common = 0;
            for (const auto &id : expected) common += actual.contains(id);
            intersecting += common > 0;
            together |= common == expected.size();
            exact |= actual == expected;
          }
          return std::tuple{exact, together, intersecting};
        };
        const auto [before, beforeTogether, beforeGroups] = coverage(previous);
        const auto [after, afterTogether, afterGroups] = coverage(patched);
        sheetBeforeExact += before; sheetAfterExact += after;
        sheetBeforeTogether += beforeTogether; sheetAfterTogether += afterTogether;
        nlohmann::json itemNames = nlohmann::json::array();
        for (const auto &member : members) itemNames.push_back({{"editorID", member.editorID}, {"name", member.name}});
        output["sheetFamilies"].push_back({{"plugin", pluginName}, {"family", sfs::kit_generator::sheet::kSheetFamilies[family].name},
          {"members", itemNames}, {"beforeExact", before}, {"afterExact", after}, {"beforeTogether", beforeTogether},
          {"afterTogether", afterTogether}, {"beforeIntersectingGroups", beforeGroups}, {"afterIntersectingGroups", afterGroups}});
      }
      const auto available = IdentitySet(records);
      output["plugins"].push_back({{"plugin", pluginName}, {"armorCount", records.size()},
                                  {"baselineGroups", baseline.size()}, {"patchedGroups", patched.size()}});
      for (const auto &reference : corpus.at("references")) {
        std::set<std::string> expected;
        for (const auto &item : reference.at("items")) {
          if (!item.at("equipped").get<bool>() || LowerAscii(item.at("plugin").get<std::string>()) != LowerAscii(pluginName)) continue;
          const auto key = sfs::kit_generator::community::Key(pluginName, item.at("editorID").get<std::string>());
          if (available.contains(key)) expected.insert(key);
        }
        if (expected.size() < 2) continue;
        auto score = [&](const auto &groups) {
          double best = 0;
          for (const auto &group : groups) {
            const auto measurePool = [&](const auto& pool) {
              const auto actual = IdentitySet(pool.items);
              std::size_t common = 0;
              for (const auto &key : expected) common += actual.contains(key);
              const double ratio = static_cast<double>(common) / (expected.size() + actual.size() - common);
              best = (std::max)(best, ratio);
            };
            if (group.variants.empty()) measurePool(group);
            else for (const auto& variant : group.variants) measurePool(variant);
          }
          return best;
        };
        const auto before = score(baseline), after = score(patched);
        ++measured;
        baselineExact += before == 1;
        patchedExact += after == 1;
        baselineJaccard += before;
        patchedJaccard += after;
        output["references"].push_back({{"kit", reference.at("name")}, {"plugin", pluginName},
          {"availableEquippedArmor", expected.size()}, {"baselineJaccard", before}, {"patchedJaccard", after}});
      }
      for (const auto &group : patched) {
        const auto members = IdentitySet(group.items);
        const auto candidates = BuildCandidates(group, {}, {}, 1);
        if (candidates.size() > kMaximumGeneratedCandidateProfiles)
          throw std::runtime_error("Candidate profile budget exceeded: " + group.name);
        std::set<std::uint32_t> represented;
        for (const auto &candidate : candidates)
          for (const auto &item : candidate.items) represented.insert(item.localFormID);
        output["candidateGroups"].push_back({{"plugin", pluginName}, {"name", group.name},
          {"sourceRecords", group.items.size()}, {"representedRecords", represented.size()},
          {"explicitVariants", group.variants.size()},
          {"candidates", candidates.size()}});
        coordinatedChecks += CheckCoordinatedChoices(group, candidates);
        for (const auto &candidate : candidates) {
          if (CountSlotConflicts(candidate.items)) throw std::runtime_error("Candidate slot conflict: " + group.name);
          for (const auto &key : IdentitySet(candidate.items))
            if (!members.contains(key)) throw std::runtime_error("Candidate escaped its group");
          ++candidateChecks;
        }
      }
    }
    output["summary"] = {{"measuredReferences", measured}, {"baselineExact", baselineExact}, {"patchedExact", patchedExact},
      {"baselineMeanJaccard", baselineJaccard / measured}, {"patchedMeanJaccard", patchedJaccard / measured},
      {"conflictFreeCandidatesChecked", candidateChecks}, {"coordinatedCandidatesChecked", coordinatedChecks}};
    output["sheetSummary"] = {{"measuredFamilies", sheetFamilies}, {"classifiedArmorRecords", sheetRecords},
      {"patch1Exact", sheetBeforeExact}, {"patch2Exact", sheetAfterExact},
      {"patch1Together", sheetBeforeTogether}, {"patch2Together", sheetAfterTogether}};
    output["screenshotSummary"] = {{"availableGroups", screenshotCount}, {"previousExact", screenshotBeforeExact}, {"patchedExact", screenshotAfterExact}};
    std::ofstream report(argv[2]);
    report << output.dump(2);
    std::cout << output["summary"].dump() << '\n';
    std::cout << output["sheetSummary"].dump() << '\n';
    std::cout << output["screenshotSummary"].dump() << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
