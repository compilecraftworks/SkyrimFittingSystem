#include <iostream>

#define private public
#if defined(SFS_INTEGRATED_KIT_GENERATOR_TEST)
#include "../src/kit_generator/Generator.cpp"
#else
#include "../src/Generator.cpp"
#endif
#undef private

#include "catalog/KitJsonRules.h"

// The offline generator test never snapshots live engine forms. Keep that
// boundary explicit; body classification/aggregation has its own rule tests.
namespace sfs::body_family {
Mask ClassifyCatalogArmor(const RE::TESObjectARMO*) {
  throw std::runtime_error("Live armor classification is not available in offline generator tests");
}
}

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

void TestCommunityReferencePipeline() {
  namespace community = sfs::kit_generator::community;
  std::vector<ArmorRecord> records;
  for (const auto &ref : community::kCommunityReferences) {
    if (ref.name != "[DX] Dark Knight Armor (Long)" &&
        ref.name != "[DX] Dark Knight Armor (Short)") continue;
    for (const auto &member : std::span(community::kCommunityMembers).subspan(ref.offset, ref.count)) {
      if (std::ranges::any_of(records, [&](const auto &r) { return r.editorID == member.editorID; })) continue;
      auto armor = MakeArmor(static_cast<std::uint32_t>(records.size() + 5000),
                             "번역된 의상 " + std::to_string(records.size()), {32}, records.size());
      armor.pluginName = std::string(ref.plugin);
      armor.editorID = std::string(member.editorID);
      armor.armorAddonFormIDs = {1234}; // Same model must not erase separate identities.
      records.push_back(std::move(armor));
    }
  }
  const auto deduplicated = DeduplicateAppearanceRecords(records);
  Require(deduplicated.size() == records.size(), "Reference identities must survive identical ARMA records");
  const auto groups = BuildFinalOutfitGroups(deduplicated);
  Require(groups.size() == 1 && groups.front().variants.size() == 2,
          "Long/Short must share one kit while retaining two separate membership pools");
  for (const auto &group : groups) {
    Require(group.name == "[DX] Dark Knight Armor",
            "The family must retain the full reference outfit name");
    const auto candidates = BuildCandidates(group, {}, {}, 1);
    Require(!candidates.empty(), "Reference groups still use the existing candidate builder");
    for (const auto &candidate : candidates) {
      Require(CountSlotConflicts(candidate.items) == 0, "Reference candidates must obey real slots");
      const auto source = std::ranges::find_if(group.variants, [&](const auto& variant) {
        return std::ranges::all_of(candidate.items, [&](const auto& item) {
          return std::ranges::any_of(variant.items, [&](const auto& member) { return member.Identifier() == item.Identifier(); });
        });
      });
      Require(source != group.variants.end(), "Every candidate must remain within one explicit reference");
    }
  }
  auto unmatched = MakeArmor(6001, "Unknown Outfit Body", {32}, 0);
  auto feet = MakeArmor(6002, "Unknown Outfit Boots", {37}, 1);
  const auto oldGroups = BuildHeuristicOutfitGroups({unmatched, feet});
  const auto newGroups = BuildFinalOutfitGroups({unmatched, feet});
  Require(oldGroups.size() == newGroups.size() && oldGroups.front().name == newGroups.front().name &&
              newGroups.front().items.size() == 2,
          "Unlisted plugins must retain their original heuristic path");
  std::stop_source stop;
  stop.request_stop();
  bool cancelled = false;
  try { (void)BuildFinalOutfitGroups(records, stop.get_token()); }
  catch (const ScanCancelled &) { cancelled = true; }
  Require(cancelled, "Cancellation must propagate through the integrated reference phase");
}

void TestNumberedVariantFamilies() {
  std::vector<ArmorRecord> records;
  for (unsigned variant = 1; variant <= 16; ++variant) {
    for (const auto part : {"Bikini", "Suit"}) {
      auto armor = MakeArmor(11000 + static_cast<unsigned>(records.size()),
          std::format("{:02} Birth Lingerie {}", variant, part),
          part == std::string_view("Bikini") ? std::initializer_list<int>{32} : std::initializer_list<int>{44}, records.size());
      armor.pluginName = "[YoerkSun] Birth Lingerie.esp";
      armor.editorID = part == std::string_view("Suit") && variant == 1 ? "BirthLingerie01Suit" :
          std::format("BirthLingerie{}{:02}", part, variant);
      armor.armorAddonFormIDs = {armor.runtimeFormID + 1000};
      armor.armorModelPaths = {"armor/bandit/body1m_1.nif",
          std::format("yoerksun/birth lingerie/{}_1.nif", part)};
      records.push_back(std::move(armor));
    }
  }
  const auto groups = BuildFinalOutfitGroups(records);
  Require(groups.size() == 1 && groups.front().name == "Birth Lingerie" &&
          groups.front().variants.size() == 16 && groups.front().items.size() == 32,
          "Birth 01-16 must become one family with 16 separate paired variants");
  const auto candidates = BuildCandidates(groups.front(), {}, {}, 1);
  Require(candidates.size() == 16, "Numbered pairs must yield 16 candidates, not 16 kits or 256 mixed pairs");
  for (const auto& candidate : candidates) {
    Require(candidate.items.size() == 2 && !CountSlotConflicts(candidate.items), "Both complementary pieces must survive");
    Require(candidate.items[0].name.substr(0, 2) == candidate.items[1].name.substr(0, 2),
            "Bikini and Suit numbers must never cross");
  }
  std::ranges::reverse(records);
  const auto reversed = BuildCandidates(BuildFinalOutfitGroups(records).front(), {}, {}, 2);
  Require(reversed.size() == candidates.size(), "Source order and worker budget must not lose versions");
  auto negative = records;
  for (auto& record : negative) record.editorID = std::format("Different{}Body", record.runtimeFormID);
  Require(BuildFinalOutfitGroups(negative).size() == 16,
          "Display numbering alone cannot join unrelated EDID outfit identities");
  auto longNumber = MakeArmor(12000, "1001 Nights Body", {32}, 0);
  auto longNumberFeet = MakeArmor(12001, "1001 Nights Boots", {37}, 1);
  auto another = MakeArmor(12002, "1002 Nights Body", {32}, 2);
  auto anotherFeet = MakeArmor(12003, "1002 Nights Boots", {37}, 3);
  Require(BuildFinalOutfitGroups({longNumber, longNumberFeet, another, anotherFeet}).size() == 2,
          "Long numeric outfit titles must not become leading-number variants");
}

void TestFamilyCompositionBoundaries() {
  const auto make = [](std::string name, unsigned id, std::string plugin = "Family.esp") {
    auto body = MakeArmor(id, name + " Body", {32}, id);
    auto feet = MakeArmor(id + 1, name + " Boots", {37}, id + 1);
    body.pluginName = feet.pluginName = plugin;
    return OutfitGroup{std::move(name), {std::move(body), std::move(feet)}};
  };
  for (int cycle = 0; cycle < 128; ++cycle) {
    const auto groups = GroupCommunityFamilies({
        make("[Maker] Outfit (Black)", 13000), make("[Maker] Outfit (White)", 13010),
        make("[Maker] Different Outfit (Black)", 13020),
        make("[Maker] Outfit (Red)", 13030, "Other.esp")}, {});
    Require(groups.size() == 3 && groups[0].variants.size() == 2,
            "Same-family references combine without crossing semantic title or plugin");
    const auto candidates = BuildCandidates(groups[0], {}, {}, 1);
    Require(candidates.size() == 2 && candidates[0].items.size() == 2 && candidates[1].items.size() == 2,
            "Reference outfits must stay whole, not expand to cross-color products");
    for (const auto& candidate : candidates)
      Require(candidate.items[0].localFormID / 10 == candidate.items[1].localFormID / 10,
              "Every explicit candidate must retain the original paired pieces");
  }
  std::vector<OutfitGroup> many;
  for (unsigned i = 0; i < 256; ++i) many.push_back(make(std::format("[Maker] Outfit ({})", i), 14000 + 10 * i));
  const auto grouped = GroupCommunityFamilies(many, {});
  Require(grouped.size() == 1 && BuildCandidates(grouped.front(), {}, {}, 1).size() == 256,
          "Every explicit version must retain one candidate at the budget boundary");
  many.push_back(make("[Maker] Outfit (256)", 17000));
  Require(GroupCommunityFamilies(many, {}).size() == 257,
          "An oversized explicit family must remain accessible as separate kits, never silently discard versions");
  std::stop_source stop;
  stop.request_stop();
  bool cancelled = false;
  try { static_cast<void>(BuildCandidates(grouped.front(), {}, stop.get_token(), 1)); }
  catch (const ScanCancelled&) { cancelled = true; }
  Require(cancelled, "Cancellation must be honored before any family candidate work");
}

void TestSheetGroupingPipeline() {
  const auto make = [](std::uint32_t id, std::string name, int slot) {
    auto item = MakeArmor(id, std::move(name), {slot}, id);
    item.pluginName = "[SunJeong] Ninirim Collection.esp";
    return item;
  };
  std::vector<ArmorRecord> items{
    make(7100, "검은색 크리스탈 씨쓰루 상의", 32),
    make(7101, "크리스탈 시스루 신발", 37),
    make(7102, "크리스탈 시스루 장갑", 33),
    make(7103, "고타 렌사 상의", 32),
    make(7104, "고타 렌사 신발", 37)};
  const auto groups = BuildFinalOutfitGroups(items);
  Require(groups.size() == 2, "Sheet-connected aliases must form two complete, separate outfits");
  std::size_t count = 0;
  for (const auto &group : groups) {
    count += group.items.size();
    Require(group.rootFamilyExpanded, "Sheet families must retain variant candidate profiles");
    const auto candidates = BuildCandidates(group, {}, {}, 1);
    Require(!candidates.empty(), "Sheet group must reach the candidate builder");
    for (const auto &candidate : candidates) Require(!CountSlotConflicts(candidate.items), "Sheet candidates must remain slot safe");
  }
  Require(count == items.size(), "Every classified part must remain in exactly one sheet family");
  auto first = items.front(), second = items.back();
  first.sourceSlotMask = second.sourceSlotMask = SlotMask(32);
  first.armorAddonFormIDs = second.armorAddonFormIDs = {100};
  Require(DeduplicateAppearanceRecords({first, second}).size() == 2,
          "Identical ARMA records from distinct sheet families must not erase a family member");
  const auto singles = BuildFinalOutfitGroups({items.front(), items.back()});
  Require(singles.empty(), "Separate explicit families cannot be merged just to manufacture a two-item kit");
  std::stop_source stop;
  stop.request_stop();
  bool cancelled = false;
  try { (void)BuildFinalOutfitGroups(items, stop.get_token()); } catch (const ScanCancelled &) { cancelled = true; }
  Require(cancelled, "Sheet grouping must honor scan cancellation");
}

void TestCoordinatedReferenceVariations() {
  // Reference titles can be English or PNG labels while equipment is translated.
  // The same candidate builder serves community, screenshot and spreadsheet groups.
  for (const std::string title : {"Community Outfit", "ADD Photo 02", "Sheet Family"}) {
    OutfitGroup group{title, {
      MakeArmor(8100, "번역 의상 상의 [Black]", {32}, 0),
      MakeArmor(8101, "번역 의상 상의 [Red]", {32}, 1),
      MakeArmor(8102, "번역 의상 장갑 [레드]", {33}, 2),
      MakeArmor(8103, "번역 의상 장갑 [검은색]", {33}, 3),
      MakeArmor(8104, "번역 의상 신발 [黑色]", {37}, 4),
      MakeArmor(8105, "번역 의상 신발 [빨간색]", {37}, 5),
      MakeArmor(8106, "번역 의상 목걸이", {35}, 6)}, true};
    const auto candidates = BuildCandidates(group, {}, {}, 1);
    Require(candidates.size() == 2, "Equivalent localized color names must yield two complete palettes");
    for (const auto &candidate : candidates) {
      Require(candidate.items.size() == 4 && !CountSlotConflicts(candidate.items),
              "Every palette must have all matching parts plus the common necklace without slot overlap");
      const bool black = HasName(candidate, "번역 의상 상의 [Black]");
      Require(HasName(candidate, black ? "번역 의상 장갑 [검은색]" : "번역 의상 장갑 [레드]") &&
              HasName(candidate, black ? "번역 의상 신발 [黑色]" : "번역 의상 신발 [빨간색]"),
              "A candidate must coordinate colors across languages and input ordering");
    }
    group.items = {
      MakeArmor(8200, "번역 의상 Celestial 상의", {32}, 0),
      MakeArmor(8201, "번역 의상 Infernal 상의", {32}, 1),
      MakeArmor(8202, "번역 의상 Infernal 신발", {37}, 2),
      MakeArmor(8203, "번역 의상 Celestial 신발", {37}, 3)};
    for (const auto slot : {33, 35, 42, 46, 49, 53})
      group.items.push_back(MakeArmor(8300 + slot, "번역 의상 공용 장식", {slot}, group.items.size()));
    const auto styles = BuildCandidates(group, {}, {}, 1);
    Require(styles.size() == 2, "Repeated named styles must survive many common accessory slots");
    for (const auto &candidate : styles) {
      Require(candidate.items.size() == 8 && !CountSlotConflicts(candidate.items),
              "Style candidates must retain common parts and real slot safety");
      const bool celestial = HasName(candidate, "번역 의상 Celestial 상의");
      Require(HasName(candidate, celestial ? "번역 의상 Celestial 신발" : "번역 의상 Infernal 신발"),
              "Styles must coordinate even when reference title does not match translated equipment names");
    }
    group.items = {
      MakeArmor(8400, "번역 의상 상의 [Black]", {32}, 0),
      MakeArmor(8401, "번역 의상 상의 [오닉스]", {32}, 1),
      MakeArmor(8402, "번역 의상 신발 [오닉스]", {37}, 2),
      MakeArmor(8403, "번역 의상 신발 [Black]", {37}, 3)};
    const auto namedPalettes = BuildCandidates(group, {}, {}, 1);
    Require(namedPalettes.size() == 2, "Named palettes and known colors must not form invented mixed combinations");
    for (const auto &candidate : namedPalettes)
      Require(HasName(candidate, "번역 의상 상의 [Black]") == HasName(candidate, "번역 의상 신발 [Black]"),
              "A custom palette must not mix with a competing known color");
  }
}

void TestGenericProfileNormalizationAndBudgets() {
  const auto translated = MakeArmor(9000, "Aster Boots 그레이", {37}, 0);
  const bool earlyColor = CanonicalProfile(Tokenize(ExtractProfile("Aster", translated))) == "gray" &&
      NormalizeKitName("Aster Coat 그레이") == "Aster";

  std::vector<LocalChoiceDimension> dimensions;
  for (int axis = 0; axis < 10; ++axis)
    dimensions.push_back({std::format("localchoice{}v1", axis), std::format("localchoice{}v2", axis)});
  const auto bounded = ChooseProfiles({{MakeArmor(9001, "Aster Body", {32}, 0), {}, "body"}}, dimensions, {});
  const bool completeAxes = bounded.size() <= kMaximumGeneratedCandidateProfiles &&
      std::ranges::all_of(bounded, [&](const auto &profile) {
        const auto tokens = ProfileTokens(profile);
        return std::ranges::all_of(dimensions, [&](const auto &dimension) {
          return std::ranges::count_if(dimension, [&](const auto &option) { return tokens.contains(option); }) == 1;
        });
      });
  const bool everyAxisOption = std::ranges::all_of(dimensions, [&](const auto &dimension) {
    return std::ranges::all_of(dimension, [&](const auto &option) {
      return std::ranges::any_of(bounded, [&](const auto &profile) { return ProfileTokens(profile).contains(option); });
    });
  });
  OutfitGroup group{"Unlisted Family", {}, true};
  for (int style = 0; style < 24; ++style)
    group.items.push_back(MakeArmor(9100 + style, std::format("Unlisted Family Body [Style{} Black]", style), {32}, style));
  for (int option = 0; option < 4; ++option)
    group.items.push_back(MakeArmor(9200 + option, "Unlisted Family Boots", {37}, 24 + option));
  for (int option = 0; option < 3; ++option)
    group.items.push_back(MakeArmor(9300 + option, "Unlisted Family Gloves", {33}, 28 + option));
  for (int option = 0; option < 2; ++option)
    group.items.push_back(MakeArmor(9400 + option, "Unlisted Family Amulet", {35}, 31 + option));
  const auto candidates = BuildCandidates(group, {}, {}, 1);
  std::set<std::uint32_t> coveredBodies;
  for (const auto &candidate : candidates) {
    Require(candidate.items.size() == 4 && !CountSlotConflicts(candidate.items),
            "Budgeted profiles must still produce complete, slot-safe candidates");
    for (const auto &item : candidate.items)
      if (item.sourceSlotMask & SlotMask(32)) coveredBodies.insert(item.localFormID);
  }
  OutfitGroup manyChoices{"Aster", {MakeArmor(9500, "Aster Body", {32}, 0)}};
  for (int i = 0; i < 7; ++i)
    manyChoices.items.push_back(MakeArmor(9501 + i, "Aster Boots", {37}, i + 1));
  const auto sevenChoices = BuildCandidates(manyChoices, {}, {}, 1);
  const auto unrelated = BuildHeuristicOutfitGroups({MakeArmor(9600, "Falcon Body", {32}, 0),
                                                    MakeArmor(9601, "Orchid Boots", {37}, 1)});
  std::cout << "Generic regression: early-color=" << earlyColor << " complete-axes=" << completeAxes
            << " all-axis-options=" << everyAxisOption << " styles=" << coveredBodies.size()
            << "/24 local-choices=" << sevenChoices.size() << "/7 unrelated-groups=" << unrelated.size() << "\n";
  Require(earlyColor, "Color aliases must normalize before grouping and profile recognition, including unbracketed local parts");
  Require(completeAxes && everyAxisOption, "The profile cap must retain every axis and cover each individual choice when capacity allows");
  Require(coveredBodies.size() == 24, "Observed styles must be represented before local combinations consume the profile budget");
  Require(sevenChoices.size() == 7, "Five or more alternatives must not silently collapse to a single local choice");
  Require(unrelated.empty(), "Different reliable outfit identities cannot be merged merely to meet the minimum group size");
  Require(NormalizeKitName("Aster Black Rose Coat") != NormalizeKitName("Aster White Rose Coat"),
          "Colors inside actual outfit titles must remain distinct identities");
  std::stop_source stop;
  stop.request_stop();
  bool cancelled = false;
  try { (void)ChooseProfiles({}, dimensions, stop.get_token()); }
  catch (const ScanCancelled &) { cancelled = true; }
  Require(cancelled, "Bounded profile enumeration must honor cancellation even without source items");
}

void TestGenericFamilyFragmentation() {
  auto firstBody = MakeArmor(9700, "[Maker] Aster Obsidian Body", {32}, 0);
  auto firstBoots = MakeArmor(9701, "[Maker] Aster Obsidian Boots", {37}, 1);
  auto secondBody = MakeArmor(9702, "[Maker] Aster Porcelain Body", {32}, 2);
  auto secondBoots = MakeArmor(9703, "[Maker] Aster Porcelain Boots", {37}, 3);
  firstBody.armorModelPaths = {"armor/bandit/body1m_1.nif", "meshes/aster/body.nif"};
  secondBody.armorModelPaths = {"ARMOR\\BANDIT\\BODY1M_1.NIF", "ASTER\\BODY.NIF"};
  firstBoots.armorModelPaths = secondBoots.armorModelPaths = {"aster/boots.nif"};
  const auto combined = BuildHeuristicOutfitGroups({firstBody, firstBoots, secondBody, secondBoots});
  Require(combined.size() == 1 && combined.front().items.size() == 4,
          "Same-family named variants with matching body and footwear models must become candidates of one kit");
  const auto candidates = BuildCandidates(combined.front(), {}, {}, 1);
  Require(candidates.size() == 2, "Merged named styles must remain two coherent candidates");
  for (const auto &candidate : candidates) {
    Require(candidate.items.size() == 2 && !CountSlotConflicts(candidate.items), "Merged styles must keep complete real-slot-safe candidates");
    Require(HasName(candidate, firstBody.name) == HasName(candidate, firstBoots.name), "Merged family must not mix the named styles");
  }
  secondBody.armorModelPaths.back() = "different/body.nif";
  Require(BuildHeuristicOutfitGroups({firstBody, firstBoots, secondBody, secondBoots}).size() == 2,
          "A shared male fallback and shared shoe cannot merge outfits with different body models");
  Require(BuildHeuristicOutfitGroups({MakeArmor(9710, "[Maker] Falcon Body", {32}, 0),
                                     MakeArmor(9711, "[Maker] Orchid Boots", {37}, 1)}).empty(),
          "An author tag alone must not manufacture a shared outfit identity");
}

void TestGenericPartAssembly() {
  const std::vector<std::string> names{"은하상의", "은하 치마", "Aurora 손장식", "별빛 신발"};
  const std::vector<std::string> ids{"AuroraSetTop1SMP", "AuroraSetSkirt2aSMP", "AuroraSetGloves", "AuroraSetBoots"};
  const std::vector<int> slots{32, 49, 33, 37};
  std::vector<ArmorRecord> parts;
  for (std::size_t i = 0; i < names.size(); ++i) {
    auto item = MakeArmor(9800 + i, names[i], {slots[i]}, i);
    item.editorID = ids[i];
    item.armorModelPaths = {std::format("author/auroraset/part{}.nif", i)};
    parts.push_back(std::move(item));
  }
  const auto groups = BuildHeuristicOutfitGroups(parts);
  Require(groups.size() == 1 && groups.front().items.size() == 4,
          "Translated and differently spaced body/lower/hands/feet must assemble from EDID-family and shared asset evidence");
  const auto candidates = BuildCandidates(groups.front(), {}, {}, 1);
  Require(!candidates.empty() && std::ranges::all_of(candidates, [](const auto &candidate) {
    return candidate.items.size() == 4 && !CountSlotConflicts(candidate.items);
  }), "Assembled parts must reach a complete candidate, not four separate kits");
  Require(IsComponentPartToken("톱1smp") && IsComponentPartToken("Skirt2aSMP"),
          "Attached numbered physics suffixes must remain part variants");
  Require(EditorOutfitFamily("MORDHAU01TorsoBrigandinChest").empty(),
          "An EDID with its actual set identity after the component must not be truncated to an author/category prefix");
  Require(EditorOutfitFamily("AuroraSet1Top") != EditorOutfitFamily("AuroraSet2Gloves"),
          "Design numbers before the component must stay in the EDID family identity");
  auto withAccessory = parts;
  withAccessory.push_back(MakeArmor(9810, "은하상의 귀걸이", {36}, 4));
  const auto retained = BuildHeuristicOutfitGroups(withAccessory);
  Require(retained.size() == 1 && retained.front().items.size() == 5,
          "An existing named earring must survive when asset evidence bridges the larger outfit");
  parts.back().editorID = "DifferentSetBoots";
  const auto separated = BuildHeuristicOutfitGroups(parts);
  Require(std::ranges::none_of(separated, [](const auto &group) { return group.items.size() == 4; }),
          "A shared model directory alone must not join different EDID families");
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

void TestExplicitCheckedExport() {
  using kit_generator_under_test::Generator;
  using kit_generator_under_test::ScanState;
  const auto originalDirectory = std::filesystem::current_path();
  const auto testRoot = std::filesystem::temp_directory_path() /
      std::format("sfs-checked-export-{}", std::chrono::steady_clock::now().time_since_epoch().count());
  Require(std::filesystem::create_directory(testRoot), "Isolated output test directory must be new");
  std::filesystem::current_path(testRoot);
  const auto restoreDirectory = [&]() { std::filesystem::current_path(originalDirectory); };
  try {
    Generator generator;
    generator.state_.store(ScanState::Complete);
    for (std::size_t i = 0; i < 3; ++i) {
      GeneratedKit kit;
      kit.name = std::format("CheckedExport{}", i);
      kit.candidates.push_back(KitCandidate{.items = {MakeArmor(100 + static_cast<std::uint32_t>(i), "Body", {32}, i)}});
      generator.generatedKits_.push_back(std::move(kit));
    }
    std::string error;
    Require(generator.CreateKitFiles({}, error) == 0 && error.empty() &&
            !std::filesystem::exists("Data"), "Empty selection must not create any directory or kit");
    Require(generator.CreateKitFiles({0, 99}, error) == 0 && !error.empty() &&
            !std::filesystem::exists("Data"), "Invalid selection must be rejected before any write");
    Require(generator.CreateKitFiles({2, 0, 2}, error) == 2 && error.empty(),
            "Create only checked indices and deduplicate repeated indices");
    const auto output = testRoot / "Data/Interface/SkyrimFittingSystem/user/kits";
    std::vector<std::filesystem::path> createdFiles;
    for (const auto& entry : std::filesystem::directory_iterator(output)) {
      std::ifstream stream(entry.path());
      const auto json = nlohmann::json::parse(stream);
      const auto data = json.dump();
      Require(data.find("CheckedExport1") == std::string::npos,
              "Unchecked single-candidate kit must never be emitted");
      createdFiles.push_back(entry.path());
    }
    Require(createdFiles.size() == 2 && generator.generatedKits_.size() == 3,
            "Export must not mutate the result list or create unchecked outputs");
    restoreDirectory();
    for (const auto& path : createdFiles) std::filesystem::remove(path);
    // Only remove this test's now-empty directories, never recursively.
    for (auto directory = output; directory != testRoot.parent_path(); directory = directory.parent_path())
      Require(std::filesystem::remove(directory), "Test output directory must be empty before removal");
  } catch (...) {
    restoreDirectory();
    throw;
  }
}

int main() {
  try {
    TestExplicitCheckedExport();
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
    TestCommunityReferencePipeline();
    TestNumberedVariantFamilies();
    TestFamilyCompositionBoundaries();
    TestSheetGroupingPipeline();
    TestCoordinatedReferenceVariations();
    TestGenericProfileNormalizationAndBudgets();
    TestGenericFamilyFragmentation();
    TestGenericPartAssembly();
    std::cout << "Generator logic regression tests passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "Generator logic regression test failed: "
              << exception.what() << '\n';
    return 1;
  }
}
