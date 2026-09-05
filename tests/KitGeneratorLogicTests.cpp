#include <iostream>

#define private public
#if defined(SFS_INTEGRATED_KIT_GENERATOR_TEST)
#include "../src/kit_generator/Generator.cpp"
#else
#include "../src/Generator.cpp"
#endif
#undef private

#include "catalog/KitJsonRules.h"

#if defined(SFS_INTEGRATED_KIT_GENERATOR_TEST)
namespace kit_generator_under_test = sfs::kit_generator;
#else
namespace kit_generator_under_test = sfs_kit_generator;
#endif

namespace {
ArmorRecord MakeArmor(const std::uint32_t a_id, std::string a_name,
                      const std::initializer_list<int> a_slots,
                      const std::size_t a_order) {
  ArmorRecord record;
  record.pluginName = "Regression.esp";
  record.runtimeFormID = a_id;
  record.localFormID = a_id;
  record.editorID = std::format("Regression{:04X}", a_id);
  record.name = std::move(a_name);
  for (const auto slot : a_slots) {
    record.sourceSlotMask |= SlotMask(slot);
  }
  record.visualSlotMask = InferVisualSlot(
      record.editorID + " " + record.name, record.sourceSlotMask);
  record.layoutSlotMask = SelectPrimaryLayoutSlot(record.sourceSlotMask);
  record.sourceOrder = a_order;
  return record;
}

void Require(const bool a_condition, const std::string_view a_message) {
  if (!a_condition) {
    throw std::runtime_error(std::string(a_message));
  }
}

bool HasName(const KitCandidate &a_candidate, const std::string_view a_name) {
  return std::ranges::any_of(a_candidate.items, [&](const auto &item) {
    return item.DisplayName() == a_name;
  });
}

void TestAngelSynchronizedChoices() {
  OutfitGroup group{"Angel",
                    {MakeArmor(1, "Angel Body", {32}, 0),
                     MakeArmor(2, "Angel Boots", {37}, 1),
                     MakeArmor(3, "Angel High Heels", {37}, 2),
                     MakeArmor(4, "Angel Gauntlets", {33}, 3),
                     MakeArmor(5, "Angel Gauntlets With Claws", {33}, 4),
                     MakeArmor(6, "Angel Circle", {42}, 5),
                     MakeArmor(7, "Angel Circle 3RD", {42}, 6)}};
  const auto candidates = BuildCandidates(group, {}, {}, 1);
  Require(candidates.size() == 2,
          "Angel's three two-way component axes must produce two synchronized candidates");
  Require(std::ranges::all_of(candidates, [](const auto &candidate) {
            return candidate.items.size() == 4;
          }),
          "Every Angel candidate must contain body, feet, hands, and circlet");
}

void TestIndependentUnderwearChoices() {
  OutfitGroup group{"Angel Secrets",
                    {MakeArmor(11, "Angel Secrets Body", {32}, 0),
                     MakeArmor(12, "Angel Secrets Bra", {46}, 1),
                     MakeArmor(13, "Angel Secrets Corset", {46}, 2),
                     MakeArmor(14, "Angel Secrets Pantie", {52}, 3),
                     MakeArmor(15, "Angel Secrets Panties", {52}, 4)}};
  const auto candidates = BuildCandidates(group, {}, {}, 1);
  Require(candidates.size() == 4,
          "Bra/Corset and Pantie/Panties must remain two independent choice axes");
  Require(std::ranges::all_of(candidates, [](const auto &candidate) {
            return candidate.items.size() == 3;
          }),
          "Every underwear candidate must contain one item from each conflict slot");
}

void TestAltColorFallback() {
  OutfitGroup group{"Example",
                    {MakeArmor(21, "Example Body", {32}, 0),
                     MakeArmor(22, "Example Gloves", {33}, 1),
                     MakeArmor(23, "Example Alt Body", {32}, 2),
                     MakeArmor(24, "Example Red Body", {32}, 3),
                     MakeArmor(25, "Example Alt Red Body", {32}, 4)}};
  const auto candidates = BuildCandidates(group, {}, {}, 1);
  Require(candidates.size() == 4,
          "Base, Alt, Red, and Alt+Red must produce four distinct candidates");
  Require(std::ranges::all_of(candidates, [](const auto &candidate) {
            return HasName(candidate, "Example Gloves");
          }),
          "Unqualified common parts must fill every Alt/color candidate");
}

void TestStandaloneFull() {
  OutfitGroup group{"Standalone",
                    {MakeArmor(31, "Standalone Body", {32}, 0),
                     MakeArmor(32, "Standalone Gloves", {33}, 1),
                     MakeArmor(33, "Standalone Full", {32, 33}, 2)}};
  const auto candidates = BuildCandidates(group, {}, {}, 1);
  const auto full = std::ranges::find_if(candidates, [](const auto &candidate) {
    return candidate.profile.find("full") != std::string::npos;
  });
  Require(full != candidates.end(), "Full must produce a standalone candidate");
  Require(full->items.size() == 1 && HasName(*full, "Standalone Full"),
          "Full candidate must not inherit common parts");
}

void TestDefaultPriority() {
  OutfitGroup smpGroup{"SMP Test",
                       {MakeArmor(41, "SMP Test Body", {32}, 0),
                        MakeArmor(42, "SMP Test SMP Body", {32}, 1)}};
  const auto smpCandidates = BuildCandidates(smpGroup, {}, {}, 1);
  Require(smpCandidates.size() == 2 &&
              HasName(smpCandidates.front(), "SMP Test SMP Body"),
          "SMP candidate must be selected first");

  OutfitGroup exposureGroup{
      "Exposure Test",
      {MakeArmor(51, "Exposure Test Closed Body", {32}, 0),
       MakeArmor(52, "Exposure Test Transparent Body", {32}, 1)}};
  const auto exposureCandidates = BuildCandidates(exposureGroup, {}, {}, 1);
  Require(exposureCandidates.size() == 2 &&
              HasName(exposureCandidates.front(),
                      "Exposure Test Transparent Body"),
          "Higher exposure must be selected first when SMP is absent");
}

void TestSafetyAndDuplicateRules() {
  std::size_t excluded = 0;
  auto records = std::vector<ArmorRecord>{
      MakeArmor(61, "Safety Open Jacket", {32}, 0),
      MakeArmor(62, "Safety Open Crotch", {32}, 1),
      MakeArmor(63, "Safety R-18", {32}, 2)};
  const auto classified = PreferBaseVariants(records, excluded, true);
  Require(classified.size() == 3 && excluded == 0,
          "Adult variants must remain selectable during generation");
  Require(!classified[0].nsfw && classified[1].nsfw && classified[2].nsfw,
          "Open Jacket is exposure-only, while Open Crotch and R-18 are NSFW");

  GeneratedKit safetySelection{
      "Safety Selection",
      {{"sfw", {classified[0]}, 0}, {"nsfw", {classified[1]}, 0}},
      classified};
  safetySelection.selectedCandidate = 1;
  Require(safetySelection.IsSelectedCandidateNsfw(),
          "Automatic safety classification must follow the selected candidate");
  safetySelection.ToggleSafetyPrefix();
  Require(!safetySelection.IsSelectedCandidateNsfw(),
          "An explicit safety override must still win over automatic classification");

  auto enchanted = MakeArmor(64, "Duplicate Enchanted", {32}, 0);
  enchanted.armorAddonFormIDs = {9001};
  enchanted.enchanted = true;
  auto ordinary = MakeArmor(65, "Duplicate Ordinary", {32}, 1);
  ordinary.armorAddonFormIDs = {9001};
  const auto deduplicated =
      DeduplicateAppearanceRecords({enchanted, ordinary});
  Require(deduplicated.size() == 1 && !deduplicated.front().enchanted,
          "Exact appearance duplicates must prefer the unenchanted record");

#if defined(SFS_INTEGRATED_KIT_GENERATOR_TEST)
  auto ordinaryX =
      MakeArmor(66, "BDOR Lephria Shoes NonStocking EX", {37}, 0);
  ordinaryX.editorID = "BDOR_Lephria_Shoes_NonStocking_X";
  excluded = 0;
  const auto ordinaryXClassified =
      PreferBaseVariants({ordinaryX}, excluded, true);
  Require(ordinaryXClassified.size() == 1 &&
              !ordinaryXClassified.front().nsfw,
          "An ordinary _X editor suffix must not classify an outfit as NSFW");

  auto explicitX = MakeArmor(67, "Tagged Outfit [X]", {32}, 0);
  excluded = 0;
  const auto explicitXClassified =
      PreferBaseVariants({explicitX}, excluded, true);
  Require(explicitXClassified.size() == 1 &&
              explicitXClassified.front().nsfw,
          "An explicit [X] tag must remain available as an NSFW marker");
#endif
}

void TestCandidateThreshold() {
  OutfitGroup group{"Threshold", {}};
  for (std::uint32_t index = 1; index <= 31; ++index) {
    group.items.push_back(MakeArmor(
        100 + index, std::format("Threshold {:02} Body", index), {32},
        index - 1));
  }
  const auto candidates = BuildCandidates(group, {}, {}, 1);
  Require(candidates.size() > 30,
          "A 31-variant outfit must exceed the red suitability threshold");
}

#if defined(SFS_INTEGRATED_KIT_GENERATOR_TEST)
void TestDistinctCandidateDisplayProfiles() {
  OutfitGroup group{
      "Wardrobe",
      {MakeArmor(151, "Wardrobe Black Shirt", {32}, 0),
       MakeArmor(152, "Wardrobe Black Top", {32}, 1),
       MakeArmor(153, "Wardrobe Blue Shirt", {32}, 2),
       MakeArmor(154, "Wardrobe Blue Top", {32}, 3)}};
  const auto candidates = BuildCandidates(group, {}, {}, 1);
  std::unordered_set<std::string> profiles;
  for (const auto &candidate : candidates) {
    profiles.insert(NormalizeKey(candidate.profile));
  }
  Require(profiles.size() == candidates.size(),
          "Candidates with the same color must have distinct display profiles");
  Require(std::ranges::any_of(candidates, [](const auto &candidate) {
            return NormalizeKey(candidate.profile).find("black shirt") !=
                   std::string::npos;
          }),
          "A duplicate color profile must expose its differing outfit piece");
}

void TestObservedKitNameGroupingRules() {
  const auto key = [](const std::string_view name) {
    return NormalizeKey(NormalizeKitName(std::string(name)));
  };
  Require(key("하이힐_드레스") == key("하이힐 드레스0"),
          "An attached dress variant number must not split the high-heel dress kit");
  Require(key("하루") == key("하루 스웨터"),
          "A sweater component must remain in the Haru outfit kit");
  Require(key("하트비트") == key("하트비트 스트링스"),
          "A strings component must remain in the Heartbeat outfit kit");
  Require(key("하와와 세라") == key("하와와 스커트 A"),
          "Sera and skirt components must share the Hawawa outfit root");

  const auto fox17 = key("폭스 콜렉션 17");
  const auto fox50 = key("폭스 콜렉션 50");
  Require(fox17 != fox50 && fox17.ends_with("17") && fox50.ends_with("50"),
          "Fox Collection numbers must remain distinct kit identities");
  Require(key("폭스 컬렉션 33").ends_with("33"),
          "The alternate Korean Collection spelling must preserve its number");
  Require(key("FOX 27") != key("FOX 46") && key("FOX 27").ends_with("27"),
          "FOX catalog names without a Collection token must still preserve their number");

  const auto hawawaGroups = MergeGroups(
      {{"하와와 세라",
        {MakeArmor(551, "하와와 세라 톱 Black", {32}, 0),
         MakeArmor(552, "하와와 세라 톱 White", {32}, 1)}},
       {"하와와 스커트 A",
        {MakeArmor(553, "하와와 스커트 A Gray", {49}, 2),
         MakeArmor(554, "하와와 스커트 A Pink", {49}, 3)}}});
  Require(hawawaGroups.size() == 1 && hawawaGroups.front().items.size() == 4,
          "Observed Hawawa component groups must merge into one source outfit group");

  auto genericGroups = MergeGroups(
      {{"루나 낯선상의어",
        {MakeArmor(561, "루나 낯선상의어 Black", {32}, 0),
         MakeArmor(562, "루나 낯선상의어 White", {32}, 1)}},
       {"루나 낯선부속어",
        {MakeArmor(563, "루나 낯선부속어 Shoes", {37}, 2),
         MakeArmor(564, "루나 낯선부속어 Gloves", {33}, 3)}}});
  genericGroups = MergeRelatedSiblingGroups(std::move(genericGroups));
  Require(genericGroups.size() == 1 && genericGroups.front().name == "루나",
          "Unknown component wording must still merge by common root and complementary slots");

  auto numberedFamilies = MergeGroups(
      {{"폭스 콜렉션 17",
        {MakeArmor(571, "폭스 콜렉션 17 Body", {32}, 0),
         MakeArmor(572, "폭스 콜렉션 17 Gloves", {33}, 1)}},
       {"폭스 콜렉션 50",
        {MakeArmor(573, "폭스 콜렉션 50 Shoes", {37}, 2),
         MakeArmor(574, "폭스 콜렉션 50 Circlet", {42}, 3)}}});
  numberedFamilies =
      MergeRelatedSiblingGroups(std::move(numberedFamilies));
  Require(numberedFamilies.size() == 2,
          "Numbered collection identities must not merge through the complementary-slot fallback");
}

void TestBareRootFamilyGroupingRules() {
  // ADD-style packs often provide a few items with the bare family name and
  // then use arbitrary author labels for the rest. The bare root is stronger
  // evidence than a language-specific part dictionary, but its tail must be
  // retained as a candidate profile to keep variants from cross-mixing.
  auto mysticGroups = MergeRelatedSiblingGroups(
      { {"미스틱", {MakeArmor(1501, "미스틱", {32}, 0),
                     MakeArmor(1502, "미스틱 부츠", {37}, 1)}},
        {"미스틱 레이스", {MakeArmor(1503, "미스틱 레이스", {32}, 2),
                          MakeArmor(1504, "미스틱 레이스 반지", {60}, 3)}},
        {"미스틱 타이 하이 삭스",
         {MakeArmor(1505, "미스틱 타이 하이 삭스", {49}, 4),
          MakeArmor(1506, "미스틱 타이 하이 삭스 장식", {55}, 5)}} });
  Require(mysticGroups.size() == 1 && mysticGroups.front().rootFamilyExpanded,
          "A bare one-word root must absorb arbitrary same-family tails");
  const auto mysticCandidates = BuildCandidates(mysticGroups.front(), {}, {}, 1);
  Require(mysticCandidates.size() >= 2 &&
              std::ranges::any_of(mysticCandidates, [](const auto &candidate) {
                return NormalizeKey(candidate.profile).find("레이스") !=
                       std::string::npos;
              }),
          "Expanded-family tails must stay as candidate profiles, not mixed parts");

  auto starfishGroups = MergeRelatedSiblingGroups(
      {{"Starfish", {MakeArmor(1511, "Starfish", {32}, 0),
                      MakeArmor(1512, "Starfish Shoes", {37}, 1)}},
       {"Starfish Arsenic Remix",
        {MakeArmor(1513, "Starfish Arsenic Remix", {32}, 2),
         MakeArmor(1514, "Starfish Arsenic Remix Ring", {60}, 3)}},
       {"Starfish Bottom Arsenic Remix",
        {MakeArmor(1515, "Starfish Bottom Arsenic Remix", {49}, 4),
         MakeArmor(1516, "Starfish Bottom Arsenic Remix Anklet", {55}, 5)}}});
  Require(starfishGroups.size() == 1 &&
              starfishGroups.front().rootFamilyExpanded,
          "Multi-word arbitrary tails under a bare root must also merge");

  const auto key = [](const std::string_view name) {
    return NormalizeKey(NormalizeKitName(std::string(name)));
  };
  Require(key("토른 다크 진") == key("토른 라이트 진") &&
              key("토른 다크 진") == "토른",
          "Jeans/denim component words must not split color siblings");
}

void TestHierarchicalSetNameGroupingRules() {
  const auto key = [](const std::string_view name) {
    return NormalizeKey(NormalizeKitName(std::string(name)));
  };

  // The published outfit catalogue uses this hierarchy extensively:
  // <set> <numbered part/category> <local variation> <detail> <part>.
  // The detail must be the group identity, while the numbered category and
  // A/B variation remain candidate information.
  const auto brigandinTorso =
      key("MORDHAU 01Torso A: Brigandin Chest A");
  const auto brigandinLegs =
      key("MORDHAU 05Legs B: Brigandin Tassets B");
  Require(brigandinTorso == brigandinLegs &&
              brigandinTorso == "mordhau brigandin",
          "Numbered part prefixes and local A/B labels must not split a detail family");
  Require(brigandinTorso != key("MORDHAU 00Head A: Armet Dome A"),
          "Different detail families under one modular set must stay separate");
  Require(key("MORDHAU Shoulder A: Brigandin Chest") == brigandinTorso,
          "An unnumbered part prefix before the detail name must be handled too");

  const auto hierarchicalGroups = MergeGroups(
      {{"MORDHAU 01Torso A: Brigandin Chest A",
        {MakeArmor(581, "MORDHAU 01Torso A: Brigandin Chest A", {32}, 0),
         MakeArmor(582, "MORDHAU 01Torso B: Brigandin Coat B", {32}, 1)}},
       {"MORDHAU 05Legs A: Brigandin Tassets A",
        {MakeArmor(583, "MORDHAU 05Legs A: Brigandin Tassets A", {38}, 2),
         MakeArmor(584, "MORDHAU 05Legs B: Brigandin Boots B", {37}, 3)}},
       {"MORDHAU 00Head A: Armet Dome A",
        {MakeArmor(585, "MORDHAU 00Head A: Armet Dome A", {30}, 4),
         MakeArmor(586, "MORDHAU 00Head B: Armet Dome B", {30}, 5)}}});
  Require(hierarchicalGroups.size() == 2 &&
              std::ranges::any_of(hierarchicalGroups, [](const auto &group) {
                return NormalizeKey(group.name) == "mordhau brigandin" &&
                       group.items.size() == 4;
              }),
          "The merge stage must join matching detail families across part categories only");

  // A colour can be part of a formal set title rather than a candidate
  // variation.  Preserve it when it precedes a meaningful identity word.
  Require(key("C5Kev Black Rose Leggings") == "c5kev black rose",
          "Formal colour names must not collapse into an unrelated set root");
  Require(key("BDOR DK 0172 Armor") == "bdor dk 0172",
          "A long set-number title before a component must remain an identity");
  Require(key("Wardrobe Studded Leather Gauntlet") !=
              key("Wardrobe Studded Steel Gauntlet"),
          "Standalone material names must remain separate kit identities");
  Require(key("Wardrobe Gala ArmorXtra") ==
              key("Wardrobe Gala GlovesHDT"),
          "Attached rendering and fit variants must not split an outfit identity");
  Require(key("BDOR Checkmate ArmorB") == key("BDOR Checkmate CloakB") &&
              key("BDOR Checkmate ArmorGlossy") == "bdor checkmate",
          "Part-attached short, colour, and glossy variants must remain in one set");
  Require(key("BDOR Nova Dobart 의상R") ==
              key("BDOR Nova Dobart 베일W"),
          "Part-attached variants must work for non-ASCII part names too");
}

void TestStableOutfitIdentityGrouping() {
  // The supplied answer sources are authoritative within their listed ESPs.
  // They must win over generic token heuristics, while never affecting the
  // same display name in another plugin.
  auto namedCatalogPiece = [](std::string plugin, std::string name,
                              std::string editorID) {
    auto item = MakeArmor(579, std::move(name), {32}, 0);
    item.pluginName = std::move(plugin);
    item.editorID = std::move(editorID);
    return item;
  };
  Require(FindKnownCatalogFamilyRoot(
              namedCatalogPiece("TULLIUS COLLECTION 01.esp", "하루 스웨터", "Test"))
                  .value_or("") == "하루" &&
              FindKnownCatalogFamilyRoot(namedCatalogPiece(
                  "[Kirax] BDOR 2024 Female Collection.esp",
                  "BDOR Darkborne Rose Verdiant Dec", "Test"))
                      .value_or("") == "Darkborne Rose" &&
              FindKnownCatalogFamilyRoot(namedCatalogPiece(
                  "[Kirax] BDO Reborn 2026 Female Collection.esp",
                  "BDOR Angelic Chorus Ornament", "Test"))
                      .value_or("") == "Angelic Chorus" &&
              FindKnownCatalogFamilyRoot(namedCatalogPiece(
                  "BDOR Pack 3 ver.Tullgall.esp", "BDOR Nova Dobart Veil", "Test"))
                      .value_or("") == "Nova Dobart" &&
              FindKnownCatalogFamilyRoot(namedCatalogPiece(
                  "LadyHorus_Tera_Normal.esp", "TERA Orphic Hauberk", "Test"))
                      .value_or("") == "Orphic" &&
              FindKnownCatalogFamilyRoot(namedCatalogPiece(
                  "LadyHorus_Tera_Normal.esp", "TERA Solace Robe of the Cruel Healer", "Test"))
                      .value_or("") == "Solace" &&
              FindKnownCatalogFamilyRoot(namedCatalogPiece(
                  "LadyHorus_Tera_Normal.esp", "테라 Orphic Hauberk", "Test"))
                      .value_or("") == "테라 Orphic",
          "Catalogue and supplied-pack answer keys must override generic grouping");
  Require(FindKnownCatalogFamilyRoot(namedCatalogPiece(
              "TULLIUS COLLECTION 01.esp", "하루 스웨터 노멀", "Test"))
                  .value_or("") == "하루" &&
              FindKnownCatalogFamilyRoot(namedCatalogPiece(
                  "[SunJeong] Lovers Lab Collection 3-0.esp",
                  "DEM 의식용 가운 SMP", "Test"))
                      .value_or("") == "DEM 의식용" &&
              FindKnownCatalogFamilyRoot(namedCatalogPiece(
                  "DM BDOR Pack by Team TAL.esp", "BDOR 벤슬라 방어구 노출", "Test"))
                      .value_or("") == "BDOR 벤슬라",
          "Spreadsheet answer keys must win for each exact installed ESP");
  Require(!FindKnownCatalogFamilyRoot(
              namedCatalogPiece("Unrelated Pack.esp", "하루 스웨터", "Test")),
          "Catalogue roots must never leak into another ESP");

  // ADD01's bundled screenshot/list uses X-Fighter as the exact family name,
  // while its real record commonly keeps the readable identity only in the
  // EditorID.  The curated ADD answer key must still bind all pieces.
  auto addPiece = [](const std::uint32_t id, std::string editorID,
                     const int slot, const std::size_t order) {
    auto item = MakeArmor(id, "전사 장비", {slot}, order);
    item.pluginName = "ADD 01.esp";
    item.editorID = std::move(editorID);
    return item;
  };
  const auto addGroups = BuildStableOutfitGroups(
      {addPiece(570, "0XFighterBody", 32, 0),
       addPiece(571, "0XFighterGloves", 33, 1),
       addPiece(572, "0XFighterBoots", 37, 2)});
  Require(addGroups.size() == 1 && addGroups.front().name == "X-Fighter" &&
              addGroups.front().items.size() == 3,
          "ADD screenshot/list answer keys must match localized records through EditorID");

  auto catalogPiece = [](const std::uint32_t id, std::string name,
                         const int slot, const std::size_t order) {
    auto item = MakeArmor(id, std::move(name), {slot}, order);
    item.pluginName = "[Kirax] BDOR 2024 Female Collection.esp";
    return item;
  };
  const auto catalogGroups = BuildStableOutfitGroups(
      {catalogPiece(580, "BDOR Darkborne Rose Upper", 32, 0),
       catalogPiece(581, "BDOR Darkborne Rose Verdiant Dec", 36, 1),
       catalogPiece(582, "BDOR Darkborne Rose Gloves", 33, 2)});
  Require(catalogGroups.size() == 1 &&
              NormalizeKey(catalogGroups.front().name) == "darkborne rose" &&
              catalogGroups.front().items.size() == 3,
          "Answer-key pieces with arbitrary tails must form their documented kit");

  // The source file may place a set's cloak/accessory far from its main pieces
  // in FormID order.  The grouping pass must not depend on contiguous records.
  const auto groups = BuildStableOutfitGroups(
      {MakeArmor(591, "Aquila Armor", {32}, 0),
       MakeArmor(592, "Blood Countess Armor", {32}, 1),
       MakeArmor(593, "Aquila Gloves", {33}, 2),
       MakeArmor(594, "Blood Countess Shoes", {37}, 3),
       MakeArmor(595, "Aquila Cloak", {47}, 4)});
  Require(groups.size() == 2 &&
              std::ranges::any_of(groups, [](const auto &group) {
                return NormalizeKey(group.name) == "aquila" &&
                       group.items.size() == 3;
              }),
          "Non-contiguous named outfit pieces must remain in one stable group");

  const std::array<std::string_view, 25> answerGroups{
      "Angelic Chorus", "Aquila",          "Blood Countess",
      "Cavaro",         "Checkmate",       "Secrua",
      "Selaine",        "Sephia",          "Shudad",
      "Ynixtra",        "Crimson Flame",   "Darkborne Rose",
      "Everbloom Waltz", "Flamekissed",     "Gotha Rensa F",
      "Nereid",         "Nouverikant",     "Pale Rose",
      "Rosa Cassius",   "Roslyn",          "Highnoon",
      "Imperium",       "Kharoxia",        "Lethena",
      "Marod Star"};
  std::vector<ArmorRecord> records;
  records.reserve(answerGroups.size() * 2);
  for (std::size_t index = 0; index < answerGroups.size(); ++index) {
    records.push_back(MakeArmor(static_cast<std::uint32_t>(700 + index),
                                std::format("{} Armor", answerGroups[index]),
                                {32}, index));
  }
  for (std::size_t index = 0; index < answerGroups.size(); ++index) {
    records.push_back(MakeArmor(static_cast<std::uint32_t>(800 + index),
                                std::format("{} Gloves", answerGroups[index]),
                                {33}, answerGroups.size() + index));
  }
  const auto answerKeyGroups = BuildStableOutfitGroups(records);
  Require(answerKeyGroups.size() == 25 &&
              std::ranges::all_of(answerKeyGroups, [](const auto &group) {
                return group.items.size() == 2;
              }),
          "Distinct named sets must stay separate even when their parts are interleaved");

  // The preceding 2024 collection has the same authoring pattern but a
  // different 26-set catalogue.  Keep it as a second data-shaped regression
  // fixture so the rule remains about name/part structure, never one release.
  const std::array<std::string_view, 26> legacyGroups{
      "Adamant",       "Anemos",       "Aquila",       "Blood Countess",
      "Brilliance",    "Coco",         "Darkborne Rose", "DK0172",
      "Enslar",        "Exclaire",     "Hemomancer",   "Kharoxia",
      "Kibelius",      "Lathrakan",    "Lethena",      "Marod Star",
      "Nightveil",     "Parthenoa",    "Rosa Cassius", "Ryfina",
      "Salanar",       "Sephia",       "Summer Suit",  "Tempia",
      "Wisteria",      "Ynixtra"};
  records.clear();
  for (std::size_t index = 0; index < legacyGroups.size(); ++index) {
    records.push_back(MakeArmor(static_cast<std::uint32_t>(900 + index),
                                std::format("{} Main", legacyGroups[index]),
                                {32}, index));
  }
  for (std::size_t index = 0; index < legacyGroups.size(); ++index) {
    records.push_back(MakeArmor(static_cast<std::uint32_t>(1000 + index),
                                std::format("{} Cloak", legacyGroups[index]),
                                {47}, legacyGroups.size() + index));
  }
  const auto legacyKeyGroups = BuildStableOutfitGroups(records);
  Require(legacyKeyGroups.size() == legacyGroups.size() &&
              std::ranges::all_of(legacyKeyGroups, [](const auto &group) {
                return group.items.size() == 2;
              }),
          "A second interleaved catalogue must preserve every named set");

  // A real-world pack may mix both authoring styles in one ESP: direct
  // part-attached B/R/Gloss suffixes, colours before the part word, and
  // gender/class variants between the named set and its component.  These
  // must form one kit each while retaining distinct candidate profiles.
  auto packThreeGroups = BuildStableOutfitGroups(
      {MakeArmor(1101, "BDOR Checkmate ArmorB", {32}, 0),
       MakeArmor(1102, "BDOR Checkmate CloakB", {47}, 1),
       MakeArmor(1103, "BDOR Checkmate ArmorGlossy", {32}, 2),
       MakeArmor(1104, "BDOR Nova Dobart 의상R", {32}, 3),
       MakeArmor(1105, "BDOR Nova Dobart 베일W", {42}, 4),
       MakeArmor(1106, "BDOR MuSae Black Dress", {32}, 5),
       MakeArmor(1107, "BDOR MuSae Black Boots", {37}, 6),
       MakeArmor(1108, "BDOR MuSae White Dress", {32}, 7),
       MakeArmor(1109, "BDOR MuSae White Boots", {37}, 8),
       MakeArmor(1110, "BDOR Cavaro Warrior Main", {32}, 9),
       MakeArmor(1111, "BDOR Cavaro Warrior Hands", {33}, 10),
       MakeArmor(1112, "BDOR Cavaro Ranger Main", {32}, 11),
       MakeArmor(1113, "BDOR Cavaro Ranger Hands", {33}, 12),
       MakeArmor(1114, "BDOR Cavaro Warrior Shield", {39}, 13),
       MakeArmor(1115, "BDOR Adamas Armor", {32}, 14),
       MakeArmor(1116, "BDOR Adamas UW", {49}, 15)});
  packThreeGroups = MergeGroups(packThreeGroups);
  packThreeGroups = MergeRelatedSiblingGroups(std::move(packThreeGroups));
  const auto findGroup = [&](const std::string_view name) {
    return std::ranges::find_if(packThreeGroups, [&](const auto &group) {
      return NormalizeKey(group.name) == NormalizeKey(name);
    });
  };
  const auto checkmate = findGroup("BDOR Checkmate");
  const auto dobart = findGroup("BDOR Nova Dobart");
  const auto musae = findGroup("BDOR MuSae");
  const auto cavaro = findGroup("BDOR Cavaro");
  const auto adamas = findGroup("BDOR Adamas");
  Require(checkmate != packThreeGroups.end() && checkmate->items.size() == 3 &&
              dobart != packThreeGroups.end() && dobart->items.size() == 2 &&
              musae != packThreeGroups.end() && musae->items.size() == 4 &&
              cavaro != packThreeGroups.end() && cavaro->items.size() == 5 &&
              adamas != packThreeGroups.end() && adamas->items.size() == 2,
          "Attached, colour, and contextual variants must preserve Pack 3 set boundaries");
  const auto warrior = std::ranges::find_if(cavaro->items, [](const auto &item) {
    return NormalizeKey(item.DisplayName()).find("warrior") != std::string::npos;
  });
  const auto ranger = std::ranges::find_if(cavaro->items, [](const auto &item) {
    return NormalizeKey(item.DisplayName()).find("ranger") != std::string::npos;
  });
  Require(warrior != cavaro->items.end() && ranger != cavaro->items.end() &&
              ExtractProfile(cavaro->name, *warrior).find("warrior") !=
                  std::string::npos &&
              ExtractProfile(cavaro->name, *ranger).find("ranger") !=
                  std::string::npos,
          "Contextual sibling variants must remain distinct candidate profiles");
}

void TestCrossPluginDuplicateCollapse() {
  auto makePluginArmor = [](const std::uint32_t id, std::string name,
                            const int slot, const std::size_t order,
                            std::string plugin) {
    auto item = MakeArmor(id, std::move(name), {slot}, order);
    item.pluginName = std::move(plugin);
    return item;
  };

  auto tulliusItems = std::vector<ArmorRecord>{
      makePluginArmor(601, "X-Fighter Body 8", 32, 0,
                      "TULLIUS COLLECTION 03.esp"),
      makePluginArmor(602, "X-Fighter Gloves 8", 33, 1,
                      "TULLIUS COLLECTION 03.esp"),
      makePluginArmor(603, "X-Fighter Heels 8", 37, 2,
                      "TULLIUS COLLECTION 03.esp"),
      makePluginArmor(604, "X-Fighter Helmet 8", 42, 3,
                      "TULLIUS COLLECTION 03.esp")};
  auto addItems = std::vector<ArmorRecord>{
      makePluginArmor(611, "X-파이터 의상 01", 32, 0, "ADD 01.esp"),
      makePluginArmor(612, "X-파이터 장갑 01", 33, 1, "ADD 01.esp"),
      makePluginArmor(613, "X-파이터 힐 01", 37, 2, "ADD 01.esp"),
      makePluginArmor(614, "X-파이터 헬멧 01", 42, 3, "ADD 01.esp"),
      makePluginArmor(615, "X-파이터 견갑 01", 57, 4, "ADD 01.esp")};
  std::vector<GeneratedKit> kits{
      {"X Fighter", {{"base", tulliusItems, 0}}, tulliusItems},
      {"X 파이터", {{"base", addItems, 0}}, addItems}};
  const auto removed = CollapseCrossPluginDuplicateKits(kits);
  Require(removed == 1 && kits.size() == 1 &&
              GeneratedKitPluginKeys(kits.front()).contains("add 01.esp"),
          "Cross-ESP X Fighter copies must keep only the version with more occupied slots");

  auto firstCopy = std::vector<ArmorRecord>{
      makePluginArmor(621, "Moon Body", 32, 0, "Copy A.esp"),
      makePluginArmor(622, "Moon Gloves", 33, 1, "Copy A.esp"),
      makePluginArmor(623, "Moon Boots", 37, 2, "Copy A.esp")};
  auto localizedCopy = std::vector<ArmorRecord>{
      makePluginArmor(631, "달빛 의상", 32, 0, "Copy B.esp"),
      makePluginArmor(632, "달빛 장갑", 33, 1, "Copy B.esp"),
      makePluginArmor(633, "달빛 신발", 37, 2, "Copy B.esp")};
  for (std::size_t index = 0; index < firstCopy.size(); ++index) {
    const auto modelPath =
        std::format("meshes/copied_outfit/piece{}.nif", index + 1);
    firstCopy[index].armorModelPaths = {modelPath};
    localizedCopy[index].armorModelPaths = {modelPath};
  }
  std::vector<GeneratedKit> localizedKits{
      {"Moon Warrior", {{"base", firstCopy, 0}}, firstCopy},
      {"달빛 전사", {{"base", localizedCopy, 0}}, localizedCopy}};
  Require(CollapseCrossPluginDuplicateKits(localizedKits) == 1 &&
              localizedKits.size() == 1,
          "Copied outfits with localized names must collapse through their shared model paths and slots");

  auto renamedCopy = std::vector<ArmorRecord>{
      makePluginArmor(626, "Translated Body", 32, 0, "Copy C.esp"),
      makePluginArmor(627, "Translated Gloves", 33, 1, "Copy C.esp"),
      makePluginArmor(628, "Translated Shoes", 37, 2, "Copy C.esp"),
      makePluginArmor(629, "Translated Circlet", 42, 3, "Copy C.esp")};
  for (std::size_t index = 0; index < firstCopy.size(); ++index) {
    renamedCopy[index].armorModelPaths = firstCopy[index].armorModelPaths;
  }
  renamedCopy.back().armorModelPaths = {"meshes/copied_outfit/extra.nif"};
  std::vector<GeneratedKit> differentlyNamedCopies{
      {"Moon Warrior", {{"base", firstCopy, 0}}, firstCopy},
      {"Completely Different Translation", {{"base", renamedCopy, 0}},
       renamedCopy}};
  Require(CollapseCrossPluginDuplicateKits(differentlyNamedCopies) == 1 &&
              differentlyNamedCopies.size() == 1 &&
              GeneratedKitPluginKeys(differentlyNamedCopies.front())
                  .contains("copy c.esp"),
          "Differently named copies with mostly identical meshes must retain the more complete kit");

  // A matching display name alone is not enough: unrelated outfits can reuse
  // a short set name in another ESP.  Their individual piece-slot structure
  // must still agree before the more complete copy is allowed to replace one.
  const auto firstNamedSet = std::vector<ArmorRecord>{
      makePluginArmor(631, "Shared Set Body", 32, 0, "First.esp"),
      makePluginArmor(632, "Shared Set Gloves", 33, 1, "First.esp"),
      makePluginArmor(633, "Shared Set Shoes", 37, 2, "First.esp")};
  const auto unrelatedNamedSet = std::vector<ArmorRecord>{
      makePluginArmor(641, "Shared Set Circlet", 42, 0, "Second.esp"),
      makePluginArmor(642, "Shared Set Cape", 46, 1, "Second.esp"),
      makePluginArmor(643, "Shared Set Ring", 60, 2, "Second.esp")};
  std::vector<GeneratedKit> sameNameDifferentPieces{
      {"Shared Set", {{"base", firstNamedSet, 0}}, firstNamedSet},
      {"Shared Set", {{"base", unrelatedNamedSet, 0}}, unrelatedNamedSet}};
  Require(CollapseCrossPluginDuplicateKits(sameNameDifferentPieces) == 0 &&
              sameNameDifferentPieces.size() == 2,
          "Same-name outfits with different internal pieces must remain separate");
}

void TestGeneratedKitMultiDeleteIndices() {
  std::vector<GeneratedKit> kits{{"First"}, {"Second"}, {"Third"},
                                 {"Fourth"}};
  const auto deleted = EraseGeneratedKitsAtIndices(kits, {3, 1, 1, 99});
  Require(deleted == 2 && kits.size() == 2 && kits[0].name == "First" &&
              kits[1].name == "Third",
          "Multi-delete must remove exactly the selected original kit indices");
}

void TestInitialCandidateSelection() {
  auto exposed = std::vector<ArmorRecord>{
      MakeArmor(651, "Selection Transparent Body", {32}, 0),
      MakeArmor(652, "Selection Transparent Gloves", {33}, 1),
      MakeArmor(653, "Selection Transparent Boots", {37}, 2),
      MakeArmor(654, "Selection Transparent Circlet", {42}, 3),
      MakeArmor(655, "Selection Transparent Panty", {49}, 4),
      MakeArmor(656, "Selection Transparent Cape", {46}, 5)};
  auto alternate = std::vector<ArmorRecord>{
      MakeArmor(661, "Selection Alt Body", {32}, 0),
      MakeArmor(662, "Selection Alt Gloves", {33}, 1),
      MakeArmor(663, "Selection Alt Boots", {37}, 2),
      MakeArmor(664, "Selection Alt Circlet", {42}, 3),
      MakeArmor(665, "Selection Alt Panty", {49}, 4)};
  auto base = std::vector<ArmorRecord>{
      MakeArmor(671, "Selection Body", {32}, 0),
      MakeArmor(672, "Selection Gloves", {33}, 1),
      MakeArmor(673, "Selection Boots", {37}, 2),
      MakeArmor(674, "Selection Circlet", {42}, 3),
      MakeArmor(675, "Selection Panty", {49}, 4)};
  auto baseSmp = std::vector<ArmorRecord>{
      MakeArmor(681, "Selection SMP Body", {32}, 0),
      MakeArmor(682, "Selection SMP Gloves", {33}, 1),
      MakeArmor(683, "Selection SMP Boots", {37}, 2),
      MakeArmor(684, "Selection SMP Circlet", {42}, 3),
      MakeArmor(685, "Selection SMP Panty", {49}, 4)};

  GeneratedKit kit{"Selection",
                   {{"transparent", exposed, 100},
                    {"alt", alternate, 90},
                    {"base", base, 80},
                    {"base smp", baseSmp, 70}},
                   {}};
  SelectInitialCandidate(kit);
  Require(kit.selectedCandidate == 3 && kit.draftCandidate == 3,
          "Initial selection must reject exposed candidates, maximize slot coverage, and prefer an SMP base candidate on a tie");
}

void TestParallelWorkerBudgetsAndMonotonicProgress() {
  const auto budgets = DistributeWorkerBudgets(15, 4);
  Require(budgets.size() == 4 &&
              std::accumulate(budgets.begin(), budgets.end(),
                              std::size_t{}) == 15 &&
              std::ranges::all_of(budgets,
                                  [](const auto budget) { return budget > 0; }),
          "Parallel ESP CPU budgets must be positive and stay within the global worker budget");
  const auto singleBudget = DistributeWorkerBudgets(15, 1);
  Require(singleBudget.size() == 1 && singleBudget.front() == 15,
          "A single selected ESP must retain the full safe worker budget");

  std::vector<float> progress(3, 0.0F);
  float sum = 0.0F;
  const auto first = UpdateMonotonicPluginProgress(progress, 0, 0.8F, sum);
  const auto second = UpdateMonotonicPluginProgress(progress, 1, 0.1F, sum);
  const auto stale = UpdateMonotonicPluginProgress(progress, 0, 0.2F, sum);
  const auto final = UpdateMonotonicPluginProgress(progress, 2, 1.0F, sum);
  Require(first <= second && second <= stale && stale <= final,
          "Aggregated parallel ESP progress must never move backwards");
  Require(progress[0] == 0.8F,
          "A stale worker update must not lower its ESP progress");

  OutfitGroup deterministicGroup{
      "Parallel",
      {MakeArmor(171, "Parallel Black Body", {32}, 0),
       MakeArmor(172, "Parallel Blue Body", {32}, 1),
       MakeArmor(173, "Parallel Black Gloves", {33}, 2),
       MakeArmor(174, "Parallel Blue Gloves", {33}, 3),
       MakeArmor(175, "Parallel Black Boots", {37}, 4),
       MakeArmor(176, "Parallel Blue Boots", {37}, 5)}};
  const auto serialCandidates =
      BuildCandidates(deterministicGroup, {}, {}, 1);
  const auto parallelCandidates =
      BuildCandidates(deterministicGroup, {}, {}, 4);
  Require(serialCandidates.size() == parallelCandidates.size(),
          "Parallel candidate resolution must preserve candidate count");
  for (std::size_t index = 0; index < serialCandidates.size(); ++index) {
    Require(serialCandidates[index].profile == parallelCandidates[index].profile,
            "Parallel candidate resolution must preserve candidate ordering and names");
    std::vector<std::uint32_t> serialForms;
    std::vector<std::uint32_t> parallelForms;
    for (const auto &item : serialCandidates[index].items) {
      serialForms.push_back(item.runtimeFormID);
    }
    for (const auto &item : parallelCandidates[index].items) {
      parallelForms.push_back(item.runtimeFormID);
    }
    Require(serialForms == parallelForms,
            "Parallel candidate resolution must preserve selected outfit pieces");
  }

  std::stop_source cancelledSource;
  cancelledSource.request_stop();
  bool cancelled = false;
  try {
    std::size_t excluded = 0;
    static_cast<void>(PreferBaseVariants(
        deterministicGroup.items, excluded, true,
        cancelledSource.get_token()));
  } catch (const ScanCancelled &) {
    cancelled = true;
  }
  Require(cancelled,
          "Cancelled suitability assessment preprocessing must stop promptly");
}

void TestParallelMultiSlotDynamicProgramming() {
  std::vector<SlotCandidate> candidates;
  candidates.reserve(14);
  for (std::size_t index = 0; index < 14; ++index) {
    const auto firstSlot = static_cast<int>(30 + index * 2);
    auto item = MakeArmor(static_cast<std::uint32_t>(301 + index),
                          std::format("DP Multi Slot {}", index + 1),
                          {firstSlot, firstSlot + 1}, index);
    candidates.push_back({std::move(item), {}, {}});
  }

  const auto serial =
      SelectSlots(candidates, {}, {}, "DP Regression", 1, 1,
                  std::stop_token{}, 1);
  const auto parallel =
      SelectSlots(candidates, {}, {}, "DP Regression", 1, 1,
                  std::stop_token{}, 4);
  Require(serial.score == parallel.score &&
              serial.items.size() == parallel.items.size(),
          "Parallel multi-slot DP must preserve the serial optimum");
  for (std::size_t index = 0; index < serial.items.size(); ++index) {
    Require(serial.items[index].runtimeFormID ==
                parallel.items[index].runtimeFormID,
            "Parallel multi-slot DP must preserve deterministic item ordering");
  }
}

void TestIsolatedSlotsStayOutsideMultiSlotDynamicProgramming() {
  std::vector<SlotCandidate> candidates;
  candidates.reserve(20);
  for (std::size_t index = 0; index < 19; ++index) {
    const auto slot = static_cast<int>(30 + index);
    auto item = MakeArmor(static_cast<std::uint32_t>(401 + index),
                          std::format("Independent Slot {}", slot), {slot},
                          index);
    candidates.push_back({std::move(item), {}, {}});
  }
  auto overlapping =
      MakeArmor(450, "Overlapping Slots 48 49", {48, 49}, 19);
  candidates.push_back({std::move(overlapping), {}, {}});

  const auto serial =
      SelectSlots(candidates, {}, {}, "Isolated DP Regression", 1, 1,
                  std::stop_token{}, 1);
  const auto parallel =
      SelectSlots(candidates, {}, {}, "Isolated DP Regression", 1, 1,
                  std::stop_token{}, 4);
  Require(serial.items.size() == 19 && parallel.items.size() == 19,
          "Independent slots must be selected directly while only the "
          "overlapping 48/49 component enters DP");
  Require(serial.score == parallel.score,
          "Isolated-slot reduction must preserve the serial optimum");
  for (std::size_t index = 0; index < serial.items.size(); ++index) {
    Require(serial.items[index].runtimeFormID ==
                parallel.items[index].runtimeFormID,
            "Isolated-slot reduction must preserve deterministic ordering");
  }
  Require(std::ranges::any_of(serial.items, [](const auto &item) {
            return item.runtimeFormID == 450;
          }),
          "The exact DP must still prefer the two-slot 48/49 piece");
}
#endif

void TestMultiMerge() {
  auto &generator = kit_generator_under_test::Generator::Get();
  generator.state_.store(kit_generator_under_test::ScanState::Complete,
                         std::memory_order_release);
  auto body = MakeArmor(201, "Merge Body", {32}, 0);
  auto gloves = MakeArmor(202, "Merge Gloves", {33}, 1);
  auto boots = MakeArmor(203, "Merge Boots Open Crotch", {37}, 2);
  boots.nsfw = true;
  generator.generatedKits_ = {
      {"First", {{"base", {body}, 0}}, {body}},
      {"Second", {{"base", {gloves}, 0}}, {gloves}},
      {"Third", {{"base", {boots}, 0}}, {boots}}};
  std::string error;
  Require(generator.MergeGeneratedKits({0, 1, 2}, error),
          "Result-list merge must accept every checked kit");
  Require(generator.generatedKits_.size() == 1 &&
              generator.generatedKits_.front().name == "Merge" &&
              generator.generatedKits_.front().candidates.size() == 1 &&
              generator.generatedKits_.front().candidates.front().items.size() ==
                  3,
          "Three checked kits must be rebuilt as one complete candidate group");
  Require(generator.generatedKits_.front().IsSelectedCandidateNsfw(),
          "Merged kit safety classification must be recalculated");

  auto replacementBody = MakeArmor(204, "Merge Alternate Body", {32}, 3);
  auto circlet = MakeArmor(205, "Merge Circlet", {42}, 4);
  generator.generatedKits_ = {
      {"Candidate Merge",
       {{"base", {body, gloves}, 0},
        {"alt", {replacementBody, boots}, 0},
        {"red", {circlet}, 0}},
       {body, gloves, replacementBody, boots, circlet}}};
  Require(generator.MergeKitCandidates(0, {0, 1, 2}, error),
          "Candidate-list merge must accept every checked candidate");
  const auto &merged = generator.generatedKits_.front().candidates.front();
  Require(generator.generatedKits_.front().candidates.size() == 1 &&
              merged.items.size() == 4 && HasName(merged, "Merge Body") &&
              !HasName(merged, "Merge Alternate Body"),
          "Candidate merge must keep one deterministic item per overlapping real slot");
  std::uint32_t occupiedSlots = 0;
  for (const auto &item : merged.items) {
    Require((occupiedSlots & item.sourceSlotMask) == 0,
            "Merged candidate must not contain duplicate real slots");
    occupiedSlots |= item.sourceSlotMask;
  }
  generator.generatedKits_.clear();
  generator.state_.store(kit_generator_under_test::ScanState::Ready,
                         std::memory_order_release);
}

void TestUnicodeKitOutputPath() {
  std::string malformedName = "Armor ";
  malformedName.push_back(static_cast<char>(0xFF));
  malformedName += " 이름";
  const auto sanitized = sfs::utf8::Sanitize(malformedName);
  Require(sanitized.find("\xEF\xBF\xBD") != std::string::npos,
          "Invalid plugin text bytes must be replaced before Windows path conversion");
  const auto path = SafeFilenameStem(malformedName);
  Require(!path.empty(),
          "Malformed plugin text must still produce a usable kit filename");

  const auto longPath = SafeFilenameStem(std::string(400, 'A'));
#if defined(_WIN32)
  Require(longPath.native().size() <= 140,
          "Generated kit filename stems must stay within the Windows path budget");
#endif
  const auto collisionPath = WithCollisionSuffix(longPath, 2);
  Require(collisionPath != longPath &&
              sfs::utf8::PathToUtf8String(collisionPath).ends_with(" 2"),
          "A long generated filename must retain its collision identity");
}

void TestModexKitItemCompatibilityRules() {
  using namespace sfs::catalog::kit_json;

  Require(IsItemEquipped(nlohmann::json::object()),
          "Legacy kit items without Equipped must remain enabled");
  Require(IsItemEquipped({{"Equipped", true}}) &&
              !IsItemEquipped({{"Equipped", false}}) &&
              !IsItemEquipped({{"Equipped", "false"}}),
          "Modex Equipped=false and invalid explicit states must not become appearances");
  Require(GetItemPluginName({{"Plugin", "Armor Pack.esp"}})
                  .value_or("") == "Armor Pack.esp" &&
              !GetItemPluginName(nlohmann::json::object()),
          "Modex item plugin identity must be retained when present");
  Require(MakePluginEditorIDKey("Armor Pack.ESP", "SharedArmor") ==
              MakePluginEditorIDKey("armor pack.esp", "sharedarmor") &&
              MakePluginEditorIDKey("Other.esp", "SharedArmor") !=
                  MakePluginEditorIDKey("Armor Pack.esp", "SharedArmor"),
          "Kit lookup keys must be case-insensitive without crossing plugins");
}
} // namespace

int main() {
  try {
    TestAngelSynchronizedChoices();
    TestIndependentUnderwearChoices();
    TestAltColorFallback();
    TestStandaloneFull();
    TestDefaultPriority();
    TestSafetyAndDuplicateRules();
    TestCandidateThreshold();
#if defined(SFS_INTEGRATED_KIT_GENERATOR_TEST)
    TestDistinctCandidateDisplayProfiles();
    TestObservedKitNameGroupingRules();
    TestBareRootFamilyGroupingRules();
    TestHierarchicalSetNameGroupingRules();
    TestStableOutfitIdentityGrouping();
    TestCrossPluginDuplicateCollapse();
    TestGeneratedKitMultiDeleteIndices();
    TestInitialCandidateSelection();
    TestParallelWorkerBudgetsAndMonotonicProgress();
    TestParallelMultiSlotDynamicProgramming();
    TestIsolatedSlotsStayOutsideMultiSlotDynamicProgramming();
#endif
    TestMultiMerge();
    TestUnicodeKitOutputPath();
    TestModexKitItemCompatibilityRules();
    std::cout << "Generator logic regression tests passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "Generator logic regression test failed: "
              << exception.what() << '\n';
    return 1;
  }
}
