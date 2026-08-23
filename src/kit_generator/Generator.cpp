#include "Generator.h"

#include "Localization.h"

#if defined(SFS_PERSONAL_KIT_COMPLETION)
#include "../../private/personal_kit_completion/PersonalKitCompletion.h"
#endif

#include <bit>
#include <chrono>
#include <filesystem>
#include <format>
#include <map>
#include <numeric>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace {
using sfs::kit_generator::ArmorRecord;
using sfs::kit_generator::GeneratedKit;
using sfs::kit_generator::KitCandidate;
using sfs::kit_generator::Localization;
using sfs::kit_generator::PluginSource;

constexpr std::size_t kMinimumGroupSize = 2;
constexpr std::size_t kMaximumGeneratedCandidateProfiles = 256;
constexpr std::array<int, 32> kVisualSlots{30, 31, 32, 33, 34, 35, 36, 37,
                                            38, 39, 40, 41, 42, 43, 44, 45,
                                            46, 47, 48, 49, 50, 51, 52, 53,
                                            54, 55, 56, 57, 58, 59, 60, 61};
constexpr std::array<int, 32> kInferenceOrder{
    49, 52, 55, 44, 42, 43, 41, 31, 30, 35, 45, 36, 57, 58, 59, 34,
    33, 53, 54, 38, 37, 47, 46, 56, 48, 60, 61, 40, 39, 32};

class ScanCancelled final {};

void ThrowIfScanCancelled(const std::stop_token &a_stopToken) {
  if (a_stopToken.stop_requested()) {
    throw ScanCancelled{};
  }
}

const std::unordered_set<std::string> kBaseGamePlugins{
    "skyrim.esm", "update.esm", "dawnguard.esm", "hearthfires.esm",
    "dragonborn.esm"};

std::size_t EraseGeneratedKitsAtIndices(
    std::vector<GeneratedKit> &a_kits,
    const std::vector<std::size_t> &a_indices) {
  std::vector<std::size_t> validIndices;
  validIndices.reserve(a_indices.size());
  for (const auto index : a_indices) {
    if (index < a_kits.size()) {
      validIndices.push_back(index);
    }
  }
  std::ranges::sort(validIndices, std::greater<>{});
  validIndices.erase(std::unique(validIndices.begin(), validIndices.end()),
                     validIndices.end());
  for (const auto index : validIndices) {
    a_kits.erase(a_kits.begin() + static_cast<std::ptrdiff_t>(index));
  }
  return validIndices.size();
}

// Curated outfit-family roots.  The generated table is sourced from the
// supplied catalogue spreadsheet (every tab) plus the supplied BDOR 2024,
// 2026, and Pack 3 set lists.  It is deliberately keyed by ESP: a broad name
// such as "Amazon" is only a family root inside the pack where it was
// catalogued, never a global rule that can merge unrelated mods.
struct KnownCatalogFamily {
  std::string_view plugin;
  std::string_view root;
};

constexpr std::array<KnownCatalogFamily, 2344> kKnownCatalogFamilies{
#include "KnownOutfitCatalog.inc"
#include "KnownAddOutfitCatalog.inc"
#include "KnownHaneulTaeOutfitCatalog.inc"
};

const std::unordered_set<std::string> kPartTokens{
    "armor", "armour", "cuirass", "body", "main", "top", "upper", "lower",
    "head", "torso", "chest", "shoulder", "shoulders", "waist",
    "feet", "foot", "thigh", "tasset", "tassets",
    "pants", "jean", "jeans", "denim", "skirt", "dress", "robe", "outfit", "suit", "clothes",
    "clothing", "cloth", "bikini", "bra", "panty", "pantie", "panties",
    "lingerie", "underwear", "swimsuit", "thong", "g-string", "gstring",
    "bodysuit", "corset", "bodice", "bustier", "camisole", "blouse",
    "pumps", "boots", "boot", "shoes", "shoe", "heels", "heel",
    "sandals", "sandal", "sneakers", "sneaker", "slippers", "slipper",
    "gloves", "glove", "gauntlets", "gauntlet", "hands", "hand", "arms",
    "arm", "forearms", "forearm", "wrist", "wrists", "wristband",
    "wristbands", "bracelet", "bracelets", "cuff", "cuffs", "sleeves",
    "sleeve", "pauldrons", "pauldron", "rerebraces", "rerebrace", "legs",
    "leg", "calves", "calf", "greaves", "greave", "cuisses", "tassets",
    "stockings", "stocking", "leggings", "legging", "garter", "garters",
    "garterbelt", "pantyhose", "tights", "hose", "thighhigh", "hood",
    "helmet", "helm", "hat", "mask", "veil", "hair", "wig", "wigs",
    "circlet", "tiara",
    "crown", "headdress", "headpiece", "hairpin", "hairclip", "amulet",
    "necklace", "collar", "choker", "scarf", "shawl", "ring", "earrings",
    "earring", "earcuff", "glasses", "eyeglasses", "sunglasses", "eyewear",
    "monocle", "ears", "tail", "cloak", "cape", "mantle", "wing", "wings",
    "backpack", "belt", "waistband", "sash", "loincloth", "apron",
    "ornament", "accessory", "charm", "trinket", "cover", "covers",
    "pasties", "pastie", "jacket", "coat", "vest", "shirt", "tshirt", "t-shirt", "tee", "tunic",
    "sweater", "sweaters", "strings", "inner", "under", "uw", "tattoo",
    "tatoo", "ribbon", "headband", "hairribbon", "shield", "buckler",
    "weapon", "sword", "dagger", "axe", "bow", "crossbow", "quiver",

    "상의", "하의", "갑옷", "아머", "몸통", "의상", "옷", "복장",
    "드레스", "비키니", "팬티", "브라", "속옷", "란제리", "수영복",
    "원피스", "부츠", "신발", "구두", "샌들", "힐", "바디",
    "바디슈트", "코르셋", "슈트", "셔츠", "블라우스", "튜닉", "펌프스",
    "하이힐", "장갑", "건틀렛", "팔", "팔뚝", "팔찌", "손목",
    "손목장식", "커프", "팔방어구", "방어구", "소매", "종아리", "다리",
    "바지", "청바지", "기모바지", "진", "데님", "그리브", "캘브즈", "스타킹", "레깅스", "가터", "가터벨트",
    "타이츠", "팬티스타킹", "니삭스", "오버니삭스", "머리", "헤어",
    "후드", "헬멧", "모자", "마스크", "베일", "서클렛", "머리장식",
    "헤어장식", "머리핀", "헤어핀", "목걸이", "아뮬렛", "카라", "스카프",
    "목도리", "숄", "반지", "귀", "귀걸이", "이어커프", "안경",
    "선글라스", "외눈안경", "꼬리", "망토", "케이프", "날개", "등",
    "백팩", "칼라", "넥클로스", "벨트", "허리띠", "허리장식", "앞치마",
    "장신구", "부착물", "파츠", "뿔", "견갑", "젖꼭지", "가리개", "커버",
    "재킷", "자켓", "코트", "조끼", "셔츠", "티셔츠", "긴소매", "튜닉", "스웨터",
    "스트링스", "스커트", "세라", "이너", "언더", "속", "타투", "문신",
    "리본", "헤드밴드", "머리띠", "방패", "검", "단검", "도끼", "활", "석궁",
    "화살통",

    "服装", "衣服", "上衣", "下装", "盔甲", "护甲", "装甲", "装备",
    "套装", "装扮", "胸甲", "衣领", "连衣裙", "连体衣", "长袍", "马甲",
    "背心", "外套", "短外套", "衬衫", "制服", "皮衣", "紧身衣",
    "紧身马甲", "内衣", "内裤", "胸罩", "文胸", "比基尼", "泳装",
    "泳衣", "裤", "短裤", "裙", "裙子", "靴", "靴子", "筒靴", "胫靴",
    "鞋", "鞋子", "高跟鞋", "凉鞋", "运动鞋", "拖鞋", "手套", "护手",
    "护腕", "护臂", "臂章", "臂甲", "手镯", "手环", "腕带", "袖口",
    "袖", "袖子", "护腿", "腿甲", "胫甲", "长袜", "丝袜", "袜",
    "吊袜带", "连裤袜", "裤袜", "过膝袜", "紧身裤", "兜帽", "头盔",
    "帽子", "面具", "面纱", "头饰", "头环", "发饰", "发夹", "发箍",
    "头冠", "项链", "护符", "项圈", "颈链", "围巾", "围脖", "披肩",
    "戒指", "指环", "耳环", "耳饰", "尾巴", "披风", "斗篷", "羽毛披风",
    "翅膀", "背包", "皮带", "腰带", "腰封", "腰链", "围裙", "肩甲",
    "肩饰", "饰品", "装饰", "配件", "附件", "挂件", "小物", "夹克",
    "外套", "大衣", "内衣", "内层", "纹身", "丝带", "发带", "盾", "剑",
    "匕首", "斧", "弓", "弩", "箭袋"};

const std::unordered_set<std::string> kVariantTokens{
    "gold", "silver", "white", "black", "red", "blue", "green",
    "yellow", "pink", "purple", "orange", "brown", "gray", "grey",
    "dark", "light", "original", "default", "baka", "chain", "toon",
    "separated", "slutty", "no", "bottom", "left", "right", "strap",
    "down", "up", "add", "addon", "alt", "alternative", "open", "closed",
    "long", "short", "smp", "hdt", "physics", "classic", "modest",
    "skimpy", "origin", "normal", "standard", "base", "vanilla",
    "xtra", "xxtra", "xxxtra", "extra", "tr", "trans", "wet", "glow",
    "raised", "lowered", "painted", "visor", "nohelm", "nohair",
    "hairless", "with", "without", "small", "wide", "pushed",
    "damaged", "broken", "ripped", "torn", "destroyed", "ench",
    "enchanted", "fur", "stocking", "stockings",
    "gloss", "glossy", "nonstocking", "nostocking", "full", "complete", "onepiece",
    "nude", "naked", "transparent", "translucent", "sheer", "nipless",
    "crotchless", "ruby",
    "sapphire", "aqua", "emerald", "cyan", "magenta", "ivory", "beige",
    "navy", "teal", "turquoise", "violet", "lavender", "burgundy",
    "골드", "실버", "화이트", "엑스트라", "엑스엑스트라", "엑스엑스엑스트라",
    "블랙", "레드", "블루", "그린", "핑크", "오리지널", "기본", "검정",
    "검은색", "흰색", "빨강", "빨간색", "파랑", "파란색", "녹색", "노랑",
    "노란색", "분홍", "보라", "보라색", "주황", "갈색", "회색", "다크",
    "라이트", "오픈", "닫힘", "열림", "긴", "짧은", "위", "왼쪽",
    "오른쪽", "완성형", "전체", "루비", "노멀", "기본형", "표준",
    "노출", "손상", "파손", "찢어진", "찢김", "젖음", "발광",
    "올림", "내림", "도색", "바이저", "머리없음", "머리카락없음",
    "장식없음", "포함", "미포함", "작은", "넓은", "밀어올림",
    "인챈트", "인챈티드", "모피", "스타킹없음",
    "사파이어", "아쿠아", "에메랄드", "시안", "마젠타", "아이보리",
    "베이지", "네이비", "틸", "터키석", "바이올렛", "라벤더", "버건디",
    "金", "金色",
    "银", "银色", "白", "白色", "黑", "黑色", "红", "红色", "蓝",
    "蓝色", "绿", "绿色", "黄", "黄色", "粉", "粉色", "粉红", "紫",
    "紫色", "橙", "橙色", "棕", "棕色", "灰", "灰色", "原版", "原始",
    "默认", "普通", "深色", "浅色", "左", "右", "开放", "打开", "关闭",
    "分离", "完整", "全套", "正常", "暴露", "损坏", "破损", "湿",
    "发光", "抬起", "放下", "绘制", "面罩", "无头发", "无装饰",
    "包含", "不含", "小", "宽", "附魔", "毛皮", "无丝袜"};

// These are alternate presentations of an already named outfit family, not
// universal variation words.  For example, "Cavaro Warrior" and "Cavaro
// Ranger" should remain separate candidate profiles under Cavaro, but a set
// called simply "Ranger" must remain a valid identity.  Callers therefore
// use this set only with surrounding-name context.
const std::unordered_set<std::string> kContextualVariantTokens{
    "male", "female", "man", "woman", "masculine", "feminine",
    "unisex", "warrior", "ranger", "knight", "archer", "assassin",
    "mage", "wizard", "witch", "sorcerer", "sorceress", "priest",
    "priestess", "rogue", "hunter", "barbarian", "berserker",
    "paladin", "maid", "butler", "maleonly", "femaleonly",
    "남", "여", "남성", "여성", "남자", "여자", "남녀공용",
    "전사", "레인저", "기사", "궁수", "암살자", "마법사", "마녀",
    "사제", "메이드", "집사", "男", "女", "男性", "女性", "男女",
    "战士", "游侠", "骑士", "弓手", "刺客", "法师", "女巫", "牧师"};

const std::unordered_set<std::string> kColorAndTypeTokens{
    "gold", "silver", "white", "black", "red", "blue", "green",
    "yellow", "pink", "purple", "orange", "brown", "gray", "grey",
    "dark", "light", "original", "default", "baka", "chain", "parasite",
    "fabricp", "fabric", "leather", "scale", "toon", "separated", "gloss", "glossy", "left",
    "right", "strap", "down", "up", "alt", "alternative", "open", "closed", "long", "short", "a", "b",
    "c", "d", "e", "f", "골드", "실버", "화이트", "블랙", "레드", "블루",
    "그린", "핑크", "오리지널", "기본", "검정", "검은색", "흰색", "빨강",
    "빨간색", "파랑", "파란색", "녹색", "노랑", "노란색", "분홍", "보라",
    "보라색", "주황", "갈색", "회색", "다크", "라이트", "오픈", "닫힘",
    "열림", "왼쪽", "오른쪽", "金", "金色", "银", "银色", "白", "白色",
    "黑", "黑色", "红", "红色", "蓝", "蓝色", "绿", "绿色", "黄", "黄色",
    "粉", "粉色", "粉红", "紫", "紫色", "橙", "橙色", "棕", "棕色", "灰",
    "灰色", "原版", "原始", "默认", "普通", "深色", "浅色", "皮革", "皮",
    "布", "布料", "鳞", "链", "链甲", "左", "右", "开放", "打开", "关闭",
    "分离"};

// Material words frequently identify a separate set family (for example
// "Studded Leather" versus "Studded Steel"), even though they are also
// useful visual descriptors.  A standalone material token therefore belongs
// to the group identity; an attached suffix such as ArmorLeather remains a
// local item/profile clue and is handled by the component parser.
const std::unordered_set<std::string> kMaterialIdentityTokens{
    "leather", "fabric", "cloth", "scale", "chain", "steel", "iron",
    "ebony", "glass", "daedric", "fur", "velvet", "silk", "lace",
    "latex", "rubber", "metal", "plate", "mail", "가죽", "천", "비늘",
    "사슬", "강철", "철", "흑단", "유리", "데이드릭", "모피", "벨벳",
    "실크", "레이스", "라텍스", "고무", "금속", "판금", "사슬갑옷",
    "皮", "皮革", "布", "布料", "鳞", "链", "链甲", "钢", "铁", "乌木",
    "玻璃", "魔族", "毛皮", "丝绸", "蕾丝", "乳胶", "橡胶", "金属", "板甲"};

// These tokens describe a local component choice more often than an outfit-wide
// profile.  They are promoted to a profile only when the same token repeats
// across a meaningful share of the group's visual slots.
const std::unordered_set<std::string> kLocalProfileTokens{
    "smp", "left", "right", "strap", "down", "separated", "alt",
    "alternative", "open", "closed", "long", "short", "a", "b", "c",
    "d", "e", "f", "왼쪽", "오른쪽", "오픈", "닫힘", "열림", "左",
    "右", "开放", "打开", "关闭", "分离"};

const std::unordered_set<std::string> kStandaloneProfileTokens{
    "full", "complete", "onepiece", "완성형", "전체", "完整", "全套"};

const std::unordered_set<std::string> kNumberedFamilyTokens{
    "collection", "series", "pack", "catalog", "catalogue", "컬렉션",
    "콜렉션", "시리즈", "팩", "카탈로그", "合集", "系列", "包"};

const std::unordered_set<std::string> kNonHumanoidTokens{
    "horse", "equine", "mount", "말", "马", "马匹", "坐骑"};

const std::unordered_set<std::string> kGemColorTokens{
    "ruby", "sapphire", "aqua", "emerald", "cyan", "magenta", "ivory",
    "beige", "navy", "teal", "turquoise", "violet", "lavender",
    "burgundy", "루비", "사파이어", "아쿠아", "에메랄드", "시안",
    "마젠타", "아이보리", "베이지", "네이비", "틸", "터키석",
    "바이올렛", "라벤더", "버건디"};

const std::unordered_set<std::string> kDisplayFriendlyTailTokens{
    "dress", "robe", "corset", "panty", "pantie", "panties", "bikini",
    "bodysuit", "드레스", "팬티", "비키니", "코르셋", "바디슈트", "服装",
    "衣服", "连衣裙", "长袍", "马甲", "比基尼", "紧身衣"};

const std::vector<std::string> kAdultFragments{
    "see-through", "see through", "transparent", "transparency",
    "translucent", "no panty", "no panties", "no bra", "no underwear",
    "open crotch", "open-crotch", "crotchless", "nipless", "no breast",
    "no breasts", "just bones", "no nipple", "no nipples",
    "damaged", "broken", "ripped", "tattered", "shredded", "destroyed",
    "futanari", "futa", "vagina", "vaginal", "pussy", "cunt", "nipple",
    "areola", "boob", "투명", "손상", "파손", "슬러티", "시스루",
    "반투명", "노출", "찢어진", "찢김", "헤진", "너덜너덜", "보지",
    "젖꼭지", "젖", "후타나리", "후타", "유두", "透视", "透明", "半透明",
    "裸露", "破损", "损坏", "破碎", "残破", "破烂", "毁坏", "撕裂",
    "撕破", "撕碎", "无内衣", "阴道", "小穴", "屄", "乳头", "乳首",
    "乳晕", "扶她", "扶他", "双性", "奶子", "乳房", "开裆", "露裆",
    "无胸", "无乳", "无乳头"};

const std::unordered_set<std::string> kAdultTokens{
    "nude", "naked", "lewd", "slutty", "damaged", "damage", "broken",
    "ripped", "tattered", "shredded", "destroyed", "transparent",
    "transparency", "translucent", "sheer", "torn", "exposed", "exposure",
    "nopanty", "nopanties", "nobra", "futanari", "futa", "vagina",
    "vaginal", "pussy", "cunt", "nipple", "nipples", "areola", "areolas",
    "boob", "boobs", "tit", "tits", "breast", "breasts", "crotchless",
    "nipless", "nobreast", "nobreasts", "justbones", "투명", "손상",
    "슬러티", "누드", "나체", "노출", "파손", "찢어진", "찢김", "헤진",
    "너덜너덜", "시스루", "반투명", "보지", "젖꼭지", "후타나리", "후타",
    "유두", "젖", "裸", "裸体", "裸露", "透", "透明", "透视", "半透明",
    "破损", "损坏", "破碎", "残破", "破烂", "毁坏", "撕裂", "撕破",
    "撕碎", "暴露", "成人", "阴道", "小穴", "屄", "乳头", "乳首", "乳晕",
    "扶她", "扶他", "双性", "双性人", "奶子", "乳房", "胸", "乳"};

const std::map<int, std::vector<std::string>> kSlotKeywords{
    {49, {"pelvis", "hip", "belt", "waist", "groin", "골반", "허리", "腰", "腰带"}},
    {52, {"pelvis secondary", "hip secondary", "belt secondary"}},
    {55, {"glasses", "eyeglasses", "sunglasses", "eyewear", "piercing", "안경", "眼镜"}},
    {44, {"mouth", "lip", "teeth", "fang", "입", "口", "嘴"}},
    {42, {"circlet", "tiara", "crown", "headdress", "headpiece", "서클릿", "왕관", "头饰"}},
    {43, {"ear", "ears", "earring", "earrings", "귀", "귀걸이", "耳环"}},
    {41, {"long hair", "longhair", "긴 머리", "장발", "长发"}},
    {31, {"hair", "wig", "hairstyle", "헤어", "가발", "头发", "假发"}},
    {30, {"head", "hood", "hat", "helm", "helmet", "mask", "머리", "후드", "모자", "头盔"}},
    {35, {"amulet", "necklace", "collar", "choker", "neck", "목걸이", "项链"}},
    {36, {"ring", "반지", "戒指"}},
    {57, {"shoulder", "pauldron", "어깨", "肩"}},
    {34, {"forearm", "gauntlet", "bracer", "arm", "wrist", "sleeve", "팔", "손목", "护臂"}},
    {33, {"hand", "hands", "glove", "gloves", "손", "장갑", "手套"}},
    {38, {"calf", "leg", "legging", "greave", "stocking", "garter", "다리", "스타킹", "长袜"}},
    {37, {"feet", "foot", "boot", "boots", "shoe", "shoes", "heel", "신발", "부츠", "鞋"}},
    {47, {"back", "cloak", "cape", "wing", "backpack", "망토", "날개", "披风"}},
    {46, {"chest", "breast", "bust", "torso", "가슴", "胸"}},
    {48, {"misc", "ornament", "accessory", "charm", "장식", "액세서리", "饰品"}},
    {40, {"tail", "꼬리", "尾巴"}},
    {39, {"shield", "buckler", "방패", "盾牌"}},
    {32, {"body", "cuirass", "armor", "armour", "dress", "robe", "outfit", "top", "shirt", "skirt", "몸통", "의상", "드레스", "服装", "连衣裙"}}};

std::uint32_t SlotMask(const int a_slot) {
  return a_slot >= 30 && a_slot <= 61 ? 1U << (a_slot - 30) : 0;
}

std::string LowerAscii(std::string a_value) {
  std::ranges::transform(a_value, a_value.begin(), [](const unsigned char ch) {
    return ch < 128 ? static_cast<char>(std::tolower(ch))
                    : static_cast<char>(ch);
  });
  return a_value;
}

std::string TrimSpaces(std::string a_value) {
  const auto isSpace = [](const unsigned char ch) { return std::isspace(ch); };
  while (!a_value.empty() && isSpace(a_value.front())) {
    a_value.erase(a_value.begin());
  }
  while (!a_value.empty() && isSpace(a_value.back())) {
    a_value.pop_back();
  }
  return a_value;
}

std::vector<std::string> Tokenize(const std::string_view a_value) {
  std::vector<std::string> tokens;
  std::string current;
  auto flush = [&]() {
    if (!current.empty()) {
      tokens.push_back(current);
      current.clear();
    }
  };
  for (const unsigned char ch : a_value) {
    if (ch >= 128 || std::isalnum(ch) ||
        ((ch == '\'' || ch == '.' || ch == '&') && !current.empty())) {
      current.push_back(static_cast<char>(ch));
    } else {
      flush();
    }
  }
  flush();
  return tokens;
}

std::string NormalizeKey(const std::string_view a_value) {
  std::string result;
  bool previousSpace = true;
  for (const unsigned char ch : a_value) {
    if (ch >= 128 || std::isalnum(ch)) {
      result.push_back(ch < 128 ? static_cast<char>(std::tolower(ch))
                               : static_cast<char>(ch));
      previousSpace = false;
    } else if (!previousSpace) {
      result.push_back(' ');
      previousSpace = true;
    }
  }
  return TrimSpaces(result);
}

std::string TrimDisplayName(std::string a_value) {
  a_value = TrimSpaces(std::move(a_value));
  while (!a_value.empty()) {
    const auto ch = static_cast<unsigned char>(a_value.back());
    if (ch >= 128 || std::isalnum(ch) || ch == ')' || ch == ']') {
      break;
    }
    a_value.pop_back();
    a_value = TrimSpaces(std::move(a_value));
  }

  std::string result;
  bool previousSpace = true;
  for (const unsigned char ch : a_value) {
    if (std::isspace(ch)) {
      if (!previousSpace) {
        result.push_back(' ');
      }
      previousSpace = true;
    } else {
      result.push_back(static_cast<char>(ch));
      previousSpace = false;
    }
  }
  return TrimSpaces(std::move(result));
}

std::string RemoveBracketedText(const std::string_view a_value) {
  std::string result;
  int squareDepth = 0;
  for (const char ch : a_value) {
    if (ch == '[') {
      ++squareDepth;
      continue;
    }
    if (ch == ']' && squareDepth > 0) {
      --squareDepth;
      continue;
    }
    if (squareDepth == 0) {
      result.push_back(ch);
    }
  }
  return result;
}

bool IsDisplayFriendlyTailToken(const std::string_view a_token) {
  return kDisplayFriendlyTailTokens.contains(NormalizeKey(a_token));
}

bool IsNumeric(const std::string_view a_value) {
  return !a_value.empty() &&
         std::ranges::all_of(a_value, [](const unsigned char ch) {
           return ch < 128 && std::isdigit(ch);
         });
}

bool IsComponentPartToken(const std::string_view a_token) {
  const auto normalized = NormalizeKey(a_token);
  if (kPartTokens.contains(normalized)) {
    return true;
  }

  auto baseLength = normalized.size();
  while (baseLength > 0) {
    const auto ch = static_cast<unsigned char>(normalized[baseLength - 1]);
    if (ch >= 128 || !std::isdigit(ch)) {
      break;
    }
    --baseLength;
  }
  if (baseLength > 0 && baseLength < normalized.size() &&
      kPartTokens.contains(normalized.substr(0, baseLength))) {
    return true;
  }

  // Many outfit packs attach a local variation directly to the part word:
  // ArmorB, HelmOP, ShoesBlack, 갑옷R, 베일W, etc.  Treat this as the
  // same part only when the remaining suffix is a short/known variation;
  // arbitrary longer words are still allowed to be part of a real set name.
  for (const auto &part : kPartTokens) {
    if (normalized.size() <= part.size() || !normalized.starts_with(part)) {
      continue;
    }
    const auto suffix = normalized.substr(part.size());
    const auto shortAsciiVariant =
        suffix.size() <= 2 &&
        std::ranges::all_of(suffix, [](const unsigned char ch) {
          return ch < 128 && std::isalpha(ch);
        });
    if (IsNumeric(suffix) || shortAsciiVariant ||
        kVariantTokens.contains(suffix) ||
        kColorAndTypeTokens.contains(suffix)) {
      return true;
    }
  }
  return false;
}

// A number directly attached to a recognised equipment-part word is an
// authoring/category label (for example "01Torso" or "05Legs"), not the
// identity of an outfit.  This is deliberately based on the word after the
// number rather than on a plugin-specific naming convention.
bool IsStructuralComponentPrefix(const std::string_view a_token) {
  const auto normalized = NormalizeKey(a_token);
  auto prefixLength = std::size_t{0};
  while (prefixLength < normalized.size() &&
         std::isdigit(static_cast<unsigned char>(normalized[prefixLength]))) {
    ++prefixLength;
  }
  return prefixLength > 0 && prefixLength < normalized.size() &&
         IsComponentPartToken(normalized.substr(prefixLength));
}

bool IsNumberedVariantToken(std::string_view a_value);

bool IsContextualVariantToken(const std::string_view a_token) {
  return kContextualVariantTokens.contains(NormalizeKey(a_token));
}

bool IsMaterialIdentityToken(const std::string_view a_token) {
  return kMaterialIdentityTokens.contains(NormalizeKey(a_token));
}

bool IsVariantMetadataToken(const std::string_view a_token) {
  const auto normalized = NormalizeKey(a_token);
  return !normalized.empty() &&
         (IsNumeric(normalized) || IsNumberedVariantToken(normalized) ||
          kVariantTokens.contains(normalized) ||
          (kColorAndTypeTokens.contains(normalized) &&
           !IsMaterialIdentityToken(normalized)) ||
          IsContextualVariantToken(normalized));
}

bool IsNumberedCollectionIdentity(const std::vector<std::string> &a_tokens,
                                  const std::size_t a_index) {
  if (a_index >= a_tokens.size()) {
    return false;
  }
  const auto number = NormalizeKey(a_tokens[a_index]);
  if (!IsNumeric(number)) {
    return false;
  }

  if (a_index > 0) {
    const auto previous = NormalizeKey(a_tokens[a_index - 1]);
    if (kNumberedFamilyTokens.contains(previous)) {
      return true;
    }
  }

  const auto root = NormalizeKey(a_tokens.front());
  return a_index == 1 && (root == "fox" || root == "폭스");
}

bool IsNumberedVariantToken(const std::string_view a_value) {
  if (a_value.empty() ||
      !std::isdigit(static_cast<unsigned char>(a_value.front()))) {
    return false;
  }
  auto index = std::size_t{0};
  while (index < a_value.size() &&
         std::isdigit(static_cast<unsigned char>(a_value[index]))) {
    ++index;
  }
  return index == a_value.size() ||
         (a_value.size() - index <= 2 &&
          std::ranges::all_of(a_value.substr(index),
                              [](const unsigned char ch) {
                                return ch < 128 && std::isalpha(ch);
                              }));
}

bool IsAsciiAlphaNumeric(const unsigned char a_ch) {
  return a_ch < 128 && std::isalnum(a_ch);
}

bool ContainsTrMarker(const std::string_view a_value) {
  for (std::size_t index = 0; index + 1 < a_value.size(); ++index) {
    if (index > 0 &&
        IsAsciiAlphaNumeric(static_cast<unsigned char>(a_value[index - 1]))) {
      continue;
    }
    const auto first = static_cast<unsigned char>(a_value[index]);
    const auto second = static_cast<unsigned char>(a_value[index + 1]);
    if (std::tolower(first) != 't' || std::tolower(second) != 'r') {
      continue;
    }
    auto end = index + 2;
    while (end < a_value.size() &&
           std::isdigit(static_cast<unsigned char>(a_value[end]))) {
      ++end;
    }
    if (end == a_value.size() ||
        !IsAsciiAlphaNumeric(static_cast<unsigned char>(a_value[end]))) {
      return true;
    }
  }
  return false;
}

bool ContainsR18Marker(const std::string_view a_value) {
  for (std::size_t index = 0; index < a_value.size(); ++index) {
    if (index > 0 &&
        IsAsciiAlphaNumeric(static_cast<unsigned char>(a_value[index - 1]))) {
      continue;
    }
    if (std::tolower(static_cast<unsigned char>(a_value[index])) != 'r') {
      continue;
    }
    auto marker = index + 1;
    while (marker < a_value.size() &&
           (std::isspace(static_cast<unsigned char>(a_value[marker])) ||
            a_value[marker] == '-' || a_value[marker] == '_')) {
      ++marker;
    }
    if (marker + 1 >= a_value.size() || a_value[marker] != '1' ||
        a_value[marker + 1] != '8') {
      continue;
    }
    const auto end = marker + 2;
    if (end == a_value.size() ||
        !IsAsciiAlphaNumeric(static_cast<unsigned char>(a_value[end]))) {
      return true;
    }
  }
  return false;
}

bool ContainsShortXMarker(const std::string_view a_value) {
  for (std::size_t index = 0; index < a_value.size(); ++index) {
    if (std::tolower(static_cast<unsigned char>(a_value[index])) != 'x' ||
        (index > 0 && IsAsciiAlphaNumeric(
                          static_cast<unsigned char>(a_value[index - 1])))) {
      continue;
    }
    const auto end = index + 1;
    if (end == a_value.size() ||
        !IsAsciiAlphaNumeric(static_cast<unsigned char>(a_value[end]))) {
      return true;
    }
  }
  return false;
}

bool IsAdultMarker(const std::string_view a_value, const bool a_allowShortX) {
  const auto lowered = LowerAscii(std::string(a_value));
  if (std::ranges::any_of(kAdultFragments, [&](const auto &fragment) {
        return lowered.find(fragment) != std::string::npos;
      }) ||
      ContainsTrMarker(a_value) ||
      (a_allowShortX && ContainsShortXMarker(a_value))) {
    return true;
  }
  return std::ranges::any_of(Tokenize(a_value), [](const auto &token) {
    return kAdultTokens.contains(NormalizeKey(token));
  });
}

struct VariantAnalysis {
  std::string baseName;
  bool adult{false};
};

std::string RemoveBracketTags(const std::string_view a_value,
                              bool &a_hasAdultTag) {
  a_hasAdultTag = false;
  std::string result;
  for (std::size_t index = 0; index < a_value.size();) {
    if (a_value[index] != '[') {
      result.push_back(a_value[index++]);
      continue;
    }
    const auto end = a_value.find(']', index + 1);
    if (end == std::string_view::npos) {
      result.push_back(a_value[index++]);
      continue;
    }
    const auto tag = a_value.substr(index + 1, end - index - 1);
    a_hasAdultTag |= IsAdultMarker(tag, true);
    result.push_back(' ');
    index = end + 1;
  }
  return result;
}

std::string RemoveAdultParentheticalTags(const std::string_view a_value,
                                         bool &a_hasAdultTag) {
  a_hasAdultTag = false;
  std::string result;
  constexpr std::string_view fullWidthOpen = "（";
  constexpr std::string_view fullWidthClose = "）";
  for (std::size_t index = 0; index < a_value.size();) {
    std::string_view open;
    std::string_view close;
    if (a_value[index] == '(') {
      open = "(";
      close = ")";
    } else if (a_value.substr(index).starts_with(fullWidthOpen)) {
      open = fullWidthOpen;
      close = fullWidthClose;
    } else {
      result.push_back(a_value[index++]);
      continue;
    }

    const auto end = a_value.find(close, index + open.size());
    if (end == std::string_view::npos) {
      result.append(open);
      index += open.size();
      continue;
    }
    const auto tag = a_value.substr(index + open.size(),
                                    end - index - open.size());
    if (IsAdultMarker(tag, true)) {
      a_hasAdultTag = true;
      result.push_back(' ');
    } else {
      result.append(a_value.substr(index, end + close.size() - index));
    }
    index = end + close.size();
  }
  return result;
}

std::string TrimVariantSeparator(std::string a_value) {
  a_value = TrimSpaces(std::move(a_value));
  while (!a_value.empty()) {
    const auto ch = a_value.back();
    if (ch != '-' && ch != '_' && ch != '.' && ch != '/' && ch != '\\' &&
        ch != '(' && ch != '[' && ch != '{') {
      break;
    }
    a_value.pop_back();
    a_value = TrimSpaces(std::move(a_value));
  }
  return a_value;
}

VariantAnalysis AnalyzeVariantName(const std::string &a_name) {
  // A bare X is common in ordinary outfit/editor identifiers (EX, _X,
  // revision X). Only bracket/parenthetical tags may use the explicit short
  // [X]/(X) adult marker; free-form names require an unambiguous marker.
  const auto wholeNameAdult = IsAdultMarker(a_name, false);
  bool hasAdultBracketTag = false;
  const auto withoutBracketTags =
      RemoveBracketTags(a_name, hasAdultBracketTag);
  bool hasAdultParentheticalTag = false;
  auto current = RemoveAdultParentheticalTags(
      withoutBracketTags, hasAdultParentheticalTag);
  current = TrimSpaces(std::move(current));
  bool strippedAny = false;

  while (!current.empty()) {
    const auto trimmed = TrimSpaces(current);
    const auto tokens = Tokenize(trimmed);
    if (tokens.empty() || !IsAdultMarker(tokens.back(), false)) {
      break;
    }
    const auto tokenIndex = trimmed.rfind(tokens.back());
    if (tokenIndex == std::string::npos) {
      break;
    }
    current = TrimVariantSeparator(trimmed.substr(0, tokenIndex));
    strippedAny = true;
  }

  const auto currentTokens = Tokenize(current);
  if (std::ranges::any_of(currentTokens,
                          [](const auto &token) {
                            return IsAdultMarker(token, false);
                          })) {
    std::string filtered;
    for (const auto &token : currentTokens) {
      if (IsAdultMarker(token, false)) {
        continue;
      }
      if (!filtered.empty()) {
        filtered.push_back(' ');
      }
      filtered.append(token);
    }
    current = std::move(filtered);
    strippedAny = true;
  }

  auto baseName = TrimDisplayName(current);
  if (baseName.empty()) {
    baseName = TrimDisplayName(a_name);
  }
  return {std::move(baseName), wholeNameAdult || hasAdultBracketTag ||
                                       hasAdultParentheticalTag || strippedAny};
}

std::uint32_t InferVisualSlot(const std::string &a_text,
                              const std::uint32_t a_sourceMask) {
  if (std::popcount(a_sourceMask) <= 1) {
    return a_sourceMask;
  }
  const auto normalized = NormalizeKey(a_text);
  const auto tokens = Tokenize(normalized);
  const std::unordered_set<std::string> tokenSet(tokens.begin(), tokens.end());
  const auto containsKeyword = [&](const std::string &a_keyword) {
    const auto keywordTokens = Tokenize(NormalizeKey(a_keyword));
    if (keywordTokens.empty()) {
      return false;
    }
    if (keywordTokens.size() == 1) {
      const auto &keyword = keywordTokens.front();
      const bool ascii = std::ranges::all_of(
          keyword, [](const unsigned char ch) { return ch < 128; });
      return ascii ? tokenSet.contains(keyword)
                   : normalized.find(keyword) != std::string::npos;
    }
    return std::ranges::search(tokens, keywordTokens).begin() != tokens.end();
  };
  for (const auto slot : kInferenceOrder) {
    const auto keywords = kSlotKeywords.find(slot);
    if (keywords == kSlotKeywords.end() ||
        (a_sourceMask & SlotMask(slot)) == 0) {
      continue;
    }
    for (const auto &keyword : keywords->second) {
      if (containsKeyword(keyword)) {
        return SlotMask(slot);
      }
    }
  }
  for (const auto slot : kInferenceOrder) {
    if ((a_sourceMask & SlotMask(slot)) != 0) {
      return SlotMask(slot);
    }
  }
  return 0;
}

std::uint32_t SelectPrimaryLayoutSlot(const std::uint32_t a_sourceMask) {
  for (const auto slot : kVisualSlots) {
    if ((a_sourceMask & SlotMask(slot)) != 0) {
      return SlotMask(slot);
    }
  }
  return 0;
}

std::vector<ArmorRecord>
PreferBaseVariants(const std::vector<ArmorRecord> &a_items,
                   std::size_t &a_excludedCount,
                   const bool a_retainAdultCandidates = false,
                   const std::stop_token &a_stopToken = {}) {
  std::vector<std::pair<std::string, std::vector<ArmorRecord>>> buckets;
  for (auto item : a_items) {
    ThrowIfScanCancelled(a_stopToken);
    const auto analysis = AnalyzeVariantName(std::string(item.DisplayName()));
    item.adultVariant = analysis.adult;
    item.nsfw = analysis.adult || IsAdultMarker(item.editorID, false) ||
                ContainsR18Marker(item.DisplayName()) ||
                ContainsR18Marker(item.editorID);
    auto key = NormalizeKey(analysis.baseName);
    if (key.empty()) {
      key = item.Identifier();
    }
    auto bucket = std::ranges::find_if(
        buckets, [&](const auto &entry) { return entry.first == key; });
    if (bucket == buckets.end()) {
      buckets.emplace_back(key, std::vector<ArmorRecord>{});
      bucket = std::prev(buckets.end());
    }
    bucket->second.push_back(std::move(item));
  }

  std::vector<ArmorRecord> result;
  a_excludedCount = 0;
  for (const auto &[_, candidates] : buckets) {
    ThrowIfScanCancelled(a_stopToken);
    const auto hasBase = std::ranges::any_of(
        candidates, [](const auto &item) { return !item.adultVariant; });
    for (const auto &candidate : candidates) {
      if (!a_retainAdultCandidates && hasBase && candidate.adultVariant) {
        ++a_excludedCount;
      } else {
        result.push_back(candidate);
      }
    }
  }
  return result;
}

bool SameAppearance(const ArmorRecord &a_left, const ArmorRecord &a_right) {
  if (a_left.sourceSlotMask != a_right.sourceSlotMask) {
    return false;
  }
  if (!a_left.armorAddonFormIDs.empty() &&
      !a_right.armorAddonFormIDs.empty()) {
    return a_left.armorAddonFormIDs == a_right.armorAddonFormIDs;
  }
  return NormalizeKey(a_left.DisplayName()) ==
             NormalizeKey(a_right.DisplayName()) &&
         LowerAscii(a_left.editorID) == LowerAscii(a_right.editorID);
}

std::vector<ArmorRecord>
DeduplicateAppearanceRecords(const std::vector<ArmorRecord> &a_items,
                             const std::stop_token &a_stopToken = {}) {
  std::vector<ArmorRecord> result;
  result.reserve(a_items.size());
  for (const auto &item : a_items) {
    ThrowIfScanCancelled(a_stopToken);
    auto duplicate = std::ranges::find_if(result, [&](const auto &existing) {
      ThrowIfScanCancelled(a_stopToken);
      return SameAppearance(existing, item);
    });
    if (duplicate == result.end()) {
      result.push_back(item);
      continue;
    }

    const auto existingPriority = std::tuple{!duplicate->adultVariant,
                                             !duplicate->enchanted,
                                             -static_cast<std::int64_t>(
                                                 duplicate->sourceOrder)};
    const auto replacementPriority = std::tuple{
        !item.adultVariant, !item.enchanted,
        -static_cast<std::int64_t>(item.sourceOrder)};
    if (replacementPriority > existingPriority) {
      *duplicate = item;
    }
  }
  std::ranges::sort(result, {}, &ArmorRecord::sourceOrder);
  return result;
}

struct OutfitGroup {
  std::string name;
  std::vector<ArmorRecord> items;
  // Set only when a bare set name (for example "Hepsy" or "Starfish")
  // absorbs otherwise unclassifiable sibling labels.  Candidate construction
  // then retains those labels as profiles instead of mixing their slots.
  bool rootFamilyExpanded{false};
};

std::string BuildGroupName(const std::vector<ArmorRecord> &a_items) {
  const auto firstNamed = std::ranges::find_if(a_items, [](const auto &item) {
    return !TrimSpaces(std::string(item.DisplayName())).empty();
  });
  if (firstNamed == a_items.end()) {
    return "Unnamed";
  }

  auto prefix = std::string(firstNamed->DisplayName());
  for (auto item = std::next(firstNamed); item != a_items.end(); ++item) {
    const auto name = std::string(item->DisplayName());
    if (TrimSpaces(name).empty()) {
      continue;
    }
    const auto maximum = (std::min)(prefix.size(), name.size());
    std::size_t count = 0;
    while (count < maximum) {
      const auto left = static_cast<unsigned char>(prefix[count]);
      const auto right = static_cast<unsigned char>(name[count]);
      const auto equal = left < 128 && right < 128
                             ? std::toupper(left) == std::toupper(right)
                             : left == right;
      if (!equal) {
        break;
      }
      ++count;
    }
    while (count > 0 && count < prefix.size() &&
           (static_cast<unsigned char>(prefix[count]) & 0xC0) == 0x80) {
      --count;
    }
    prefix.resize(count);
    if (prefix.empty()) {
      break;
    }
  }

  auto trimmed = TrimDisplayName(std::move(prefix));
  if (trimmed.empty()) {
    const auto tokens = Tokenize(firstNamed->DisplayName());
    return tokens.empty() ? "Unnamed" : tokens.front();
  }
  return trimmed;
}

bool IsBroadOutfitGroup(const std::vector<ArmorRecord> &a_items) {
  std::uint32_t mask = 0;
  for (const auto &item : a_items) {
    mask |= item.visualSlotMask;
  }
  int categories = 0;
  categories += (mask & SlotMask(32)) != 0;
  categories += (mask & (SlotMask(33) | SlotMask(34))) != 0;
  categories += (mask & (SlotMask(37) | SlotMask(38))) != 0;
  categories +=
      (mask & (SlotMask(30) | SlotMask(31) | SlotMask(41) | SlotMask(42) |
               SlotMask(43))) != 0;
  categories += (mask & (SlotMask(35) | SlotMask(36) | SlotMask(40))) != 0;
  return categories >= 2 &&
         ((mask & SlotMask(32)) != 0 || a_items.size() >= 5);
}

bool IsNonHumanoidOutfitGroup(const OutfitGroup &a_group) {
  const auto normalizedName = NormalizeKey(a_group.name);
  if (normalizedName.find("horse") != std::string::npos ||
      normalizedName.find("equine") != std::string::npos) {
    return true;
  }
  const auto tokens = Tokenize(a_group.name);
  return std::ranges::any_of(tokens, [](const auto &token) {
    return kNonHumanoidTokens.contains(NormalizeKey(token));
  });
}

struct TokenRun {
  std::string token;
  std::vector<ArmorRecord> items;
};

std::vector<TokenRun> RunsAtDepth(const std::vector<ArmorRecord> &a_items,
                                  const std::size_t a_depth,
                                  const std::stop_token &a_stopToken = {}) {
  std::vector<TokenRun> runs;
  for (const auto &item : a_items) {
    ThrowIfScanCancelled(a_stopToken);
    const auto tokens = Tokenize(item.DisplayName());
    const auto token =
        tokens.size() > a_depth ? NormalizeKey(tokens[a_depth]) : std::string{};
    if (runs.empty() || runs.back().token != token) {
      runs.push_back({token, {}});
    }
    runs.back().items.push_back(item);
  }
  return runs;
}

std::vector<OutfitGroup> SplitRun(const std::vector<ArmorRecord> &a_items,
                                  std::size_t a_depth,
                                  const std::stop_token &a_stopToken = {});

std::vector<OutfitGroup>
BuildGroupsAtDepth(const std::vector<ArmorRecord> &a_items,
                   const std::size_t a_depth,
                   const std::stop_token &a_stopToken = {}) {
  ThrowIfScanCancelled(a_stopToken);
  if (a_items.size() < kMinimumGroupSize) {
    return {};
  }
  const auto allShort = std::ranges::all_of(a_items, [&](const auto &item) {
    ThrowIfScanCancelled(a_stopToken);
    return Tokenize(item.DisplayName()).size() <= a_depth;
  });
  if (allShort) {
    return {{BuildGroupName(a_items), a_items}};
  }
  if (a_depth != 0) {
    return SplitRun(a_items, a_depth);
  }
  std::vector<OutfitGroup> result;
  for (const auto &run : RunsAtDepth(a_items, a_depth, a_stopToken)) {
    ThrowIfScanCancelled(a_stopToken);
    if (run.items.size() < kMinimumGroupSize) {
      continue;
    }
    auto children = SplitRun(run.items, a_depth + 1, a_stopToken);
    result.insert(result.end(), std::make_move_iterator(children.begin()),
                  std::make_move_iterator(children.end()));
  }
  return result;
}

std::vector<OutfitGroup> SplitRun(const std::vector<ArmorRecord> &a_items,
                                  const std::size_t a_depth,
                                  const std::stop_token &a_stopToken) {
  ThrowIfScanCancelled(a_stopToken);
  std::vector<std::vector<ArmorRecord>> children;
  for (const auto &run : RunsAtDepth(a_items, a_depth, a_stopToken)) {
    ThrowIfScanCancelled(a_stopToken);
    if (run.token.empty() || run.items.size() < 3) {
      continue;
    }
    if (kPartTokens.contains(run.token) && !IsBroadOutfitGroup(run.items)) {
      continue;
    }
    children.push_back(run.items);
  }
  if (children.empty() ||
      (children.size() == 1 && children.front().size() == a_items.size())) {
    return {{BuildGroupName(a_items), a_items}};
  }

  std::unordered_set<std::uint32_t> childForms;
  std::vector<OutfitGroup> result;
  for (const auto &child : children) {
    ThrowIfScanCancelled(a_stopToken);
    for (const auto &item : child) {
      childForms.insert(item.runtimeFormID);
    }
    auto nested = SplitRun(child, a_depth + 1, a_stopToken);
    result.insert(result.end(), std::make_move_iterator(nested.begin()),
                  std::make_move_iterator(nested.end()));
  }
  std::vector<ArmorRecord> residual;
  for (const auto &item : a_items) {
    ThrowIfScanCancelled(a_stopToken);
    if (!childForms.contains(item.runtimeFormID)) {
      residual.push_back(item);
    }
  }
  if (residual.size() >= kMinimumGroupSize) {
    result.push_back({BuildGroupName(residual), std::move(residual)});
  }
  return result;
}

std::string NormalizeKitName(const std::string &a_name) {
  auto tokens = Tokenize(RemoveBracketedText(a_name));
  if (tokens.empty()) {
    return TrimDisplayName(a_name);
  }

  // Equipment plugins often encode their hierarchy in the displayed name:
  //   <set> <part category> <variant> <detail name> <equipment part>
  // e.g. "MORDHAU 01Torso A: Brigandin Chest A".  The category and short
  // variation are metadata; "Brigandin" is the actual family to group.  At
  // the same time, a colour before a meaningful word may be an intentional
  // set name ("Black Rose"), so we only discard short/numbered variants.
  const auto hasFollowingSemanticToken = [&](const std::size_t a_index) {
    for (std::size_t index = a_index + 1; index < tokens.size(); ++index) {
      const auto normalized = NormalizeKey(tokens[index]);
      if (normalized.empty() || IsStructuralComponentPrefix(normalized) ||
          IsComponentPartToken(normalized) || IsNumeric(normalized) ||
          IsNumberedVariantToken(normalized) ||
          kVariantTokens.contains(normalized) ||
          kColorAndTypeTokens.contains(normalized)) {
        continue;
      }
      return true;
    }
    return false;
  };

  std::vector<std::string> kept;
  for (std::size_t index = 0; index < tokens.size(); ++index) {
    const auto &token = tokens[index];
    const auto normalized = NormalizeKey(token);
    if (!kept.empty() && normalized == "one" && index + 1 < tokens.size() &&
        NormalizeKey(tokens[index + 1]) == "piece" &&
        index + 2 == tokens.size()) {
      break;
    }

    if (IsStructuralComponentPrefix(normalized)) {
      continue;
    }
    if (!kept.empty() && IsComponentPartToken(normalized)) {
      // A part word before a real semantic name is a hierarchy prefix
      // ("Shoulder Brigandin").  A tail part remains the normal end of the
      // outfit name ("Haru Sweater").
      if (hasFollowingSemanticToken(index)) {
        continue;
      }
      break;
    }
    if (!kept.empty() &&
        (IsNumeric(normalized) || kVariantTokens.contains(normalized) ||
         kColorAndTypeTokens.contains(normalized))) {
      if (IsNumberedCollectionIdentity(tokens, index)) {
        kept.push_back(token);
        continue;
      }
      if (IsMaterialIdentityToken(normalized)) {
        kept.push_back(token);
        continue;
      }
      const auto hasSemanticTail = hasFollowingSemanticToken(index);
      // A long number directly before a component is often the stable title
      // of a named set ("DK 0172 Armor"), not a local A/B-style variation.
      // Keep it only in that unambiguous title position; structural numeric
      // component prefixes (01Torso) were removed above.
      const auto hasFollowingComponent =
          index + 1 < tokens.size() &&
          IsComponentPartToken(NormalizeKey(tokens[index + 1]));
      if (IsNumeric(normalized) && normalized.size() >= 3 &&
          hasFollowingComponent && !hasSemanticTail) {
        kept.push_back(token);
        continue;
      }
      if (!hasSemanticTail) {
        break;
      }
      // Single-letter and numbered tokens before the detail name are almost
      // always a local variation/category marker.  Keep descriptive colours
      // and style words so genuine names such as "Black Rose" remain distinct.
      if (IsNumeric(normalized) || IsNumberedVariantToken(normalized) ||
          normalized.size() <= 2 || normalized == "alt" ||
          normalized == "alternative") {
        continue;
      }
    }
    kept.push_back(token);
  }
  if (kept.empty() && !tokens.empty()) {
    kept.push_back(tokens.front());
  }
  std::string result;
  for (const auto &token : kept) {
    if (!result.empty()) {
      result.push_back(' ');
    }
    result.append(token);
  }
  return TrimDisplayName(result.empty() ? a_name : result);
}

std::string PluginFileKey(const std::string_view a_pluginName) {
  auto key = LowerAscii(std::string(a_pluginName));
  const auto separator = key.find_last_of("\\\\/");
  if (separator != std::string::npos) {
    key.erase(0, separator + 1);
  }
  return TrimSpaces(std::move(key));
}

bool HasMeaningfulCatalogRoot(const std::string_view a_root) {
  return std::ranges::any_of(Tokenize(std::string(a_root)),
                             [](const auto &token) {
                               const auto normalized = NormalizeKey(token);
                               return !normalized.empty() &&
                                      !IsNumeric(normalized) &&
                                      !IsStructuralComponentPrefix(normalized) &&
                                      !IsComponentPartToken(normalized) &&
                                      !kVariantTokens.contains(normalized) &&
                                      !kColorAndTypeTokens.contains(normalized);
                             });
}

bool IsAuthoritativeAddPlugin(const std::string_view a_pluginKey) {
  return a_pluginKey.size() == 10 && a_pluginKey.starts_with("add ") &&
         std::isdigit(static_cast<unsigned char>(a_pluginKey[4])) != 0 &&
         std::isdigit(static_cast<unsigned char>(a_pluginKey[5])) != 0 &&
         a_pluginKey.substr(6) == ".esp";
}

// ADD 01~12 are curated as whole packs, but their editor IDs commonly omit
// punctuation and generic words from their bundled Screenshot/list title
// (for example X-Fighter -> 0XFighterBody).  This relaxed comparison is
// deliberately limited to those ESPs; every other plugin keeps exact matching.
std::string CompactCatalogIdentity(const std::string_view a_value) {
  static const std::unordered_set<std::string> ignored{
      "armor", "armour", "outfit", "set", "clothes", "clothing",
      "collection"};
  std::string result;
  for (const auto &token : Tokenize(std::string(a_value))) {
    const auto normalized = NormalizeKey(token);
    if (normalized.empty() || ignored.contains(normalized)) {
      continue;
    }
    result.append(normalized);
  }
  // A leading zero is an ADD editor-ID convention, not part of a set name.
  if (result.size() > 1 && result.front() == '0' &&
      std::isalpha(static_cast<unsigned char>(result[1])) != 0) {
    result.erase(result.begin());
  }
  return result;
}

std::optional<std::size_t>
FindCatalogRootTokenStart(const std::vector<std::string> &a_nameTokens,
                          const std::vector<std::string> &a_rootTokens) {
  if (a_nameTokens.empty() || a_rootTokens.empty()) {
    return std::nullopt;
  }
  const auto match = std::ranges::search(a_nameTokens, a_rootTokens);
  if (match.begin() == a_nameTokens.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(
      std::ranges::distance(a_nameTokens.begin(), match.begin()));
}

bool ContainsHangul(const std::string_view a_text) {
  // UTF-8 leading-byte range for the Hangul syllable block.  We only use this
  // to retain a localized display prefix; matching itself remains normalized.
  return std::ranges::any_of(a_text, [](const unsigned char ch) {
    return ch >= 0xEA && ch <= 0xED;
  });
}

std::string JoinDisplayTokens(const std::vector<std::string> &a_tokens,
                              const std::size_t a_count) {
  std::string result;
  for (std::size_t index = 0; index < a_count && index < a_tokens.size();
       ++index) {
    if (!result.empty()) {
      result.push_back(' ');
    }
    result.append(a_tokens[index]);
  }
  return TrimDisplayName(std::move(result));
}

// The supplied catalogue is an explicit answer key for its own ESPs.  Match
// its longest named root before applying generic word/part heuristics.  This
// keeps arbitrary authored tails in a verified outfit family without leaking
// the same root to another plugin.
std::optional<std::string>
FindKnownCatalogFamilyRoot(const ArmorRecord &a_item) {
  const auto pluginKey = PluginFileKey(a_item.pluginName);
  if (pluginKey.empty()) {
    return std::nullopt;
  }
  const auto displayName = a_item.DisplayName();
  const auto displayTokens = Tokenize(std::string(displayName));
  std::vector<std::string> normalizedDisplayTokens;
  normalizedDisplayTokens.reserve(displayTokens.size());
  for (const auto &token : displayTokens) {
    normalizedDisplayTokens.push_back(NormalizeKey(token));
  }
  std::optional<std::string> bestRoot;
  std::optional<std::size_t> bestMatchStart;
  auto bestScore = std::size_t{0};
  const auto addPlugin = IsAuthoritativeAddPlugin(pluginKey);
  // ADD lists/screenshots name the set, while their ESP often stores that
  // identity only in the EditorID (for example X-Fighter -> 0XFighterBody).
  // Keep the exact display-name path for localized output, and use this
  // combined identity solely for ADD's curated relaxed lookup.
  const auto compactIdentity = CompactCatalogIdentity(
      std::string(displayName) + " " + a_item.editorID);
  for (const auto &family : kKnownCatalogFamilies) {
    if (PluginFileKey(family.plugin) != pluginKey ||
        !HasMeaningfulCatalogRoot(family.root)) {
      continue;
    }
    const auto rootTokens = Tokenize(NormalizeKey(std::string(family.root)));
    const auto matchStart =
        FindCatalogRootTokenStart(normalizedDisplayTokens, rootTokens);
    if (matchStart) {
      const auto score = rootTokens.size() * 100 +
                         CompactCatalogIdentity(family.root).size();
      if (score > bestScore) {
        bestScore = score;
        bestRoot = std::string(family.root);
        bestMatchStart = matchStart;
      }
      continue;
    }
    if (!addPlugin) {
      continue;
    }
    const auto compactRoot = CompactCatalogIdentity(family.root);
    if (compactRoot.size() < 4 ||
        compactIdentity.find(compactRoot) == std::string::npos) {
      continue;
    }
    const auto score = compactRoot.size();
    if (score > bestScore) {
      bestScore = score;
      bestRoot = std::string(family.root);
      bestMatchStart.reset();
    }
  }
  if (!bestRoot) {
    return std::nullopt;
  }
  if (!bestMatchStart) {
    return TrimDisplayName(std::move(*bestRoot));
  }
  const auto rootTokens = Tokenize(NormalizeKey(*bestRoot));
  const auto rootEnd = *bestMatchStart + rootTokens.size();
  const auto hasLocalizedPrefix =
      std::ranges::any_of(displayTokens | std::views::take(rootEnd),
                          [](const auto &token) { return ContainsHangul(token); });
  if (hasLocalizedPrefix) {
    return JoinDisplayTokens(displayTokens, rootEnd);
  }
  return TrimDisplayName(std::move(*bestRoot));
}

std::string ChooseKitDisplayName(const std::string &a_originalName,
                                 const std::string &a_normalizedName) {
  const auto normalized = TrimDisplayName(a_normalizedName);
  const auto cleanedOriginal =
      TrimDisplayName(RemoveBracketedText(a_originalName));
  if (cleanedOriginal.empty()) {
    return normalized;
  }

  const auto normalizedTokens = Tokenize(normalized);
  const auto originalTokens = Tokenize(cleanedOriginal);
  if (originalTokens.size() <= normalizedTokens.size()) {
    return normalized;
  }

  const auto extraCount = originalTokens.size() - normalizedTokens.size();
  if (extraCount > 0 && extraCount <= 2 &&
      std::ranges::all_of(
          originalTokens | std::views::drop(normalizedTokens.size()),
          IsDisplayFriendlyTailToken)) {
    return cleanedOriginal;
  }
  return normalized;
}

int GetKitDisplayScore(const std::string &a_displayName,
                       const std::string &a_normalizedName) {
  if (NormalizeKey(a_displayName) == NormalizeKey(a_normalizedName)) {
    return 1000;
  }

  const auto normalizedTokenCount = Tokenize(a_normalizedName).size();
  const auto displayTokens = Tokenize(a_displayName);
  if (displayTokens.size() > normalizedTokenCount) {
    const auto extraCount = displayTokens.size() - normalizedTokenCount;
    if (extraCount <= 2 &&
        std::ranges::all_of(
            displayTokens | std::views::drop(normalizedTokenCount),
            IsDisplayFriendlyTailToken)) {
      return 500 + static_cast<int>(displayTokens.size());
    }
  }
  return 100;
}

std::vector<OutfitGroup>
MergeGroups(const std::vector<OutfitGroup> &a_groups,
            const std::stop_token &a_stopToken = {}) {
  struct Bucket {
    std::string key;
    std::string name;
    int displayScore;
    std::vector<ArmorRecord> items;
    std::unordered_set<std::string> identifiers;
  };
  std::vector<Bucket> buckets;
  for (const auto &group : a_groups) {
    ThrowIfScanCancelled(a_stopToken);
    auto name = NormalizeKitName(group.name);
    auto key = NormalizeKey(name);
    if (key.empty()) {
      name = group.name;
      key = NormalizeKey(group.name);
    }
    const auto displayName = ChooseKitDisplayName(group.name, name);
    const auto displayScore = GetKitDisplayScore(displayName, name);
    auto bucket = std::ranges::find(buckets, key, &Bucket::key);
    if (bucket == buckets.end()) {
      buckets.push_back({key, displayName, displayScore, {}, {}});
      bucket = std::prev(buckets.end());
    } else if (displayScore > bucket->displayScore) {
      bucket->name = displayName;
      bucket->displayScore = displayScore;
    }
    for (const auto &item : group.items) {
      ThrowIfScanCancelled(a_stopToken);
      if (bucket->identifiers.insert(item.Identifier()).second) {
        bucket->items.push_back(item);
      }
    }
  }
  std::vector<OutfitGroup> result;
  for (auto &bucket : buckets) {
    ThrowIfScanCancelled(a_stopToken);
    if (bucket.items.size() >= kMinimumGroupSize) {
      result.push_back({std::move(bucket.name), std::move(bucket.items)});
    }
  }
  return result;
}

bool IsTokenPrefix(const std::vector<std::string> &a_prefix,
                   const std::vector<std::string> &a_value) {
  return a_prefix.size() < a_value.size() &&
         std::ranges::equal(a_prefix, a_value | std::views::take(a_prefix.size()),
                            [](const auto &left, const auto &right) {
                              return NormalizeKey(left) == NormalizeKey(right);
                            });
}

bool HasBodySourceSlot(const OutfitGroup &a_group) {
  return std::ranges::any_of(a_group.items, [](const auto &item) {
    return (item.sourceSlotMask & SlotMask(32)) != 0;
  });
}

bool ShouldMergeRelatedGroups(const OutfitGroup &a_shorter,
                              const OutfitGroup &a_longer) {
  const auto shorterTokens = Tokenize(a_shorter.name);
  const auto longerTokens = Tokenize(a_longer.name);
  if (!IsTokenPrefix(shorterTokens, longerTokens)) {
    return false;
  }
  const auto suffix = longerTokens | std::views::drop(shorterTokens.size());
  const auto suffixCount = longerTokens.size() - shorterTokens.size();
  const auto knownVariantSuffix = std::ranges::all_of(suffix, [](const auto &token) {
    const auto key = NormalizeKey(token);
    return kPartTokens.contains(key) || IsVariantMetadataToken(key);
  });
  if (knownVariantSuffix) {
    return true;
  }
  // Many outfit ESPs use one short, bare set label for a few records and
  // arbitrary author labels for the rest: "Mystic" / "Mystic Lace",
  // "Hepsy" / "Hepsy Long Sleeve T-Shirt", or "Starfish" /
  // "Starfish Arsenic Remix".  Once that explicit root record exists in the
  // same plugin it is a stronger family signal than an English/Korean part
  // dictionary. Keep the tail bounded and require compatible visual coverage
  // so an incidental text prefix cannot collapse an unrelated catalogue.
  const auto rootAnchoredFamily = [&]() {
    if (shorterTokens.empty() || shorterTokens.size() > 2 ||
        suffixCount > 4) {
      return false;
    }
    const auto root = NormalizeKey(shorterTokens.front());
    if ((root == "fox" || root == "폭스") && !suffix.empty() &&
        IsNumeric(NormalizeKey(*suffix.begin()))) {
      return false;
    }
    std::uint32_t shorterSlots = 0;
    std::uint32_t longerSlots = 0;
    for (const auto &item : a_shorter.items) {
      shorterSlots |= item.visualSlotMask;
    }
    for (const auto &item : a_longer.items) {
      longerSlots |= item.visualSlotMask;
    }
    if (shorterSlots == 0 || longerSlots == 0) {
      return false;
    }
    const auto sharedSlots = std::popcount(shorterSlots & longerSlots);
    const auto smallerCoverage =
        (std::min)(std::popcount(shorterSlots), std::popcount(longerSlots));
    return !HasBodySourceSlot(a_longer) || !HasBodySourceSlot(a_shorter) ||
           (smallerCoverage > 0 && sharedSlots * 2 >= smallerCoverage);
  };
  if (rootAnchoredFamily()) {
    return true;
  }
  if (shorterTokens.size() < 2 || suffixCount > 2) {
    return false;
  }

  std::uint32_t shorterSlots = 0;
  std::uint32_t longerSlots = 0;
  for (const auto &item : a_shorter.items) {
    shorterSlots |= item.visualSlotMask;
  }
  for (const auto &item : a_longer.items) {
    longerSlots |= item.visualSlotMask;
  }
  const auto sharedSlots = std::popcount(shorterSlots & longerSlots);
  const auto smallerCoverage =
      (std::min)(std::popcount(shorterSlots), std::popcount(longerSlots));
  return !HasBodySourceSlot(a_longer) || !HasBodySourceSlot(a_shorter) ||
         (smallerCoverage > 0 && sharedSlots * 2 >= smallerCoverage);
}

bool IsRootAnchoredFamilyMerge(const OutfitGroup &a_shorter,
                               const OutfitGroup &a_longer) {
  const auto shorterTokens = Tokenize(a_shorter.name);
  const auto longerTokens = Tokenize(a_longer.name);
  if (!IsTokenPrefix(shorterTokens, longerTokens) ||
      shorterTokens.empty() || shorterTokens.size() > 2 ||
      longerTokens.size() - shorterTokens.size() > 4) {
    return false;
  }
  const auto root = NormalizeKey(shorterTokens.front());
  if ((root == "fox" || root == "폭스") &&
      IsNumeric(NormalizeKey(longerTokens[shorterTokens.size()]))) {
    return false;
  }
  std::uint32_t shorterSlots = 0;
  std::uint32_t longerSlots = 0;
  for (const auto &item : a_shorter.items) {
    shorterSlots |= item.visualSlotMask;
  }
  for (const auto &item : a_longer.items) {
    longerSlots |= item.visualSlotMask;
  }
  if (shorterSlots == 0 || longerSlots == 0) {
    return false;
  }
  const auto sharedSlots = std::popcount(shorterSlots & longerSlots);
  const auto smallerCoverage =
      (std::min)(std::popcount(shorterSlots), std::popcount(longerSlots));
  return !HasBodySourceSlot(a_longer) || !HasBodySourceSlot(a_shorter) ||
         (smallerCoverage > 0 && sharedSlots * 2 >= smallerCoverage);
}

std::size_t CommonGroupTokenPrefix(const std::vector<std::string> &a_left,
                                   const std::vector<std::string> &a_right) {
  const auto limit = (std::min)(a_left.size(), a_right.size());
  auto prefix = std::size_t{0};
  while (prefix < limit &&
         NormalizeKey(a_left[prefix]) == NormalizeKey(a_right[prefix])) {
    ++prefix;
  }
  return prefix;
}

std::uint32_t OutfitGroupVisualCoverage(const OutfitGroup &a_group) {
  std::uint32_t coverage = 0;
  for (const auto &item : a_group.items) {
    coverage |= item.visualSlotMask;
  }
  return coverage;
}

bool HasDistinctNumberedFamilyTail(const std::vector<std::string> &a_left,
                                   const std::vector<std::string> &a_right,
                                   const std::size_t a_prefix) {
  if (a_prefix == 0 || a_prefix >= a_left.size() ||
      a_prefix >= a_right.size()) {
    return false;
  }
  const auto leftTail = NormalizeKey(a_left[a_prefix]);
  const auto rightTail = NormalizeKey(a_right[a_prefix]);
  if (!IsNumeric(leftTail) || !IsNumeric(rightTail) ||
      leftTail == rightTail) {
    return false;
  }
  const auto family = NormalizeKey(a_left[a_prefix - 1]);
  const auto root = NormalizeKey(a_left.front());
  return kNumberedFamilyTokens.contains(family) || root == "fox" ||
         root == "폭스";
}

bool IsSiblingVariantTail(const std::vector<std::string> &a_tokens,
                          const std::size_t a_start) {
  if (a_start >= a_tokens.size() || a_tokens.size() - a_start > 2) {
    return false;
  }
  return std::ranges::all_of(a_tokens | std::views::drop(a_start),
                             [](const auto &token) {
                               return IsVariantMetadataToken(token);
                             });
}

// Some authors encode an outfit's variant before its component, e.g.
// "MuSae Black Dress" / "MuSae White Boots" or "Cavaro Warrior Main" /
// "Cavaro Ranger Hands".  After ordinary normalization these become sibling
// groups rather than prefix groups.  Join only a substantial shared identity
// and metadata-only tails, keeping genuinely different named sets separate.
bool ShouldMergeSiblingVariantGroups(const OutfitGroup &a_left,
                                     const OutfitGroup &a_right) {
  const auto leftTokens = Tokenize(a_left.name);
  const auto rightTokens = Tokenize(a_right.name);
  const auto prefix = CommonGroupTokenPrefix(leftTokens, rightTokens);
  if (prefix < 2 || HasDistinctNumberedFamilyTail(leftTokens, rightTokens,
                                                   prefix) ||
      !IsSiblingVariantTail(leftTokens, prefix) ||
      !IsSiblingVariantTail(rightTokens, prefix)) {
    return false;
  }
  return true;
}

bool ShouldMergeComplementaryRootGroups(const OutfitGroup &a_left,
                                        const OutfitGroup &a_right) {
  const auto leftTokens = Tokenize(a_left.name);
  const auto rightTokens = Tokenize(a_right.name);
  const auto prefix = CommonGroupTokenPrefix(leftTokens, rightTokens);
  if (prefix == 0 ||
      (prefix == leftTokens.size() && prefix == rightTokens.size()) ||
      leftTokens.size() - prefix > 2 || rightTokens.size() - prefix > 2 ||
      HasDistinctNumberedFamilyTail(leftTokens, rightTokens, prefix)) {
    return false;
  }

  const auto leftCoverage = OutfitGroupVisualCoverage(a_left);
  const auto rightCoverage = OutfitGroupVisualCoverage(a_right);
  if (leftCoverage == 0 || rightCoverage == 0) {
    return false;
  }

  // Unknown component words are still safe to join when their actual outfit
  // slots complement each other. Overlapping color/number variants stay as
  // separate groups unless the ordinary semantic-token rules recognized them.
  return (leftCoverage & rightCoverage) == 0;
}

std::string CommonGroupDisplayRoot(const std::string &a_left,
                                   const std::string &a_right) {
  const auto leftTokens = Tokenize(a_left);
  const auto rightTokens = Tokenize(a_right);
  const auto prefix = CommonGroupTokenPrefix(leftTokens, rightTokens);
  std::string root;
  for (const auto &token : leftTokens | std::views::take(prefix)) {
    if (!root.empty()) {
      root.push_back(' ');
    }
    root.append(token);
  }
  return root.empty() ? a_left : root;
}

void AppendUniqueGroupItems(OutfitGroup &a_target,
                            const OutfitGroup &a_source,
                            const std::stop_token &a_stopToken) {
  std::unordered_set<std::string> identifiers;
  for (const auto &item : a_target.items) {
    ThrowIfScanCancelled(a_stopToken);
    identifiers.insert(item.Identifier());
  }
  for (const auto &item : a_source.items) {
    ThrowIfScanCancelled(a_stopToken);
    if (identifiers.insert(item.Identifier()).second) {
      a_target.items.push_back(item);
    }
  }
  std::ranges::sort(a_target.items, {}, &ArmorRecord::sourceOrder);
}

std::vector<OutfitGroup>
MergeRelatedSiblingGroups(
    std::vector<OutfitGroup> a_groups,
    const std::stop_token &a_stopToken = {}) {
  for (std::size_t shorterIndex = 0; shorterIndex < a_groups.size();
       ++shorterIndex) {
    ThrowIfScanCancelled(a_stopToken);
    for (std::size_t longerIndex = 0; longerIndex < a_groups.size();) {
      ThrowIfScanCancelled(a_stopToken);
      if (shorterIndex == longerIndex ||
          !ShouldMergeRelatedGroups(a_groups[shorterIndex],
                                    a_groups[longerIndex])) {
        ++longerIndex;
        continue;
      }

      auto &target = a_groups[shorterIndex];
      target.rootFamilyExpanded =
          target.rootFamilyExpanded || a_groups[longerIndex].rootFamilyExpanded ||
          IsRootAnchoredFamilyMerge(target, a_groups[longerIndex]);
      AppendUniqueGroupItems(target, a_groups[longerIndex], a_stopToken);
      a_groups.erase(a_groups.begin() + static_cast<std::ptrdiff_t>(longerIndex));
      if (longerIndex < shorterIndex) {
        --shorterIndex;
      }
    }
  }

  bool mergedSiblingVariants = true;
  while (mergedSiblingVariants) {
    mergedSiblingVariants = false;
    for (std::size_t leftIndex = 0; leftIndex < a_groups.size() &&
                                  !mergedSiblingVariants;
         ++leftIndex) {
      ThrowIfScanCancelled(a_stopToken);
      for (std::size_t rightIndex = leftIndex + 1;
           rightIndex < a_groups.size(); ++rightIndex) {
        ThrowIfScanCancelled(a_stopToken);
        if (!ShouldMergeSiblingVariantGroups(a_groups[leftIndex],
                                             a_groups[rightIndex])) {
          continue;
        }
        a_groups[leftIndex].name = CommonGroupDisplayRoot(
            a_groups[leftIndex].name, a_groups[rightIndex].name);
        a_groups[leftIndex].rootFamilyExpanded =
            a_groups[leftIndex].rootFamilyExpanded ||
            a_groups[rightIndex].rootFamilyExpanded;
        AppendUniqueGroupItems(a_groups[leftIndex], a_groups[rightIndex],
                               a_stopToken);
        a_groups.erase(a_groups.begin() +
                       static_cast<std::ptrdiff_t>(rightIndex));
        mergedSiblingVariants = true;
        break;
      }
    }
  }

  bool mergedComplementaryRoot = true;
  while (mergedComplementaryRoot) {
    mergedComplementaryRoot = false;
    for (std::size_t leftIndex = 0; leftIndex < a_groups.size() &&
                                  !mergedComplementaryRoot;
         ++leftIndex) {
      ThrowIfScanCancelled(a_stopToken);
      for (std::size_t rightIndex = leftIndex + 1;
           rightIndex < a_groups.size(); ++rightIndex) {
        ThrowIfScanCancelled(a_stopToken);
        if (!ShouldMergeComplementaryRootGroups(a_groups[leftIndex],
                                                a_groups[rightIndex])) {
          continue;
        }
        a_groups[leftIndex].name = CommonGroupDisplayRoot(
            a_groups[leftIndex].name, a_groups[rightIndex].name);
        a_groups[leftIndex].rootFamilyExpanded =
            a_groups[leftIndex].rootFamilyExpanded ||
            a_groups[rightIndex].rootFamilyExpanded;
        AppendUniqueGroupItems(a_groups[leftIndex], a_groups[rightIndex],
                               a_stopToken);
        a_groups.erase(a_groups.begin() +
                       static_cast<std::ptrdiff_t>(rightIndex));
        mergedComplementaryRoot = true;
        break;
      }
    }
  }
  std::erase_if(a_groups, IsNonHumanoidOutfitGroup);
  return a_groups;
}

bool IsReliableOutfitIdentity(const std::string_view a_name) {
  const auto tokens = Tokenize(std::string(a_name));
  return std::ranges::any_of(tokens, [](const auto &token) {
    const auto normalized = NormalizeKey(token);
    return !normalized.empty() && !IsNumeric(normalized) &&
           !IsStructuralComponentPrefix(normalized) &&
           !IsComponentPartToken(normalized) &&
           !kVariantTokens.contains(normalized) &&
           !kColorAndTypeTokens.contains(normalized);
  });
}

// FormIDs in a plugin are frequently assigned while pieces are being edited,
// not as one contiguous outfit block.  Build stable named buckets first so a
// cloak or accessory placed later in the file still joins the same outfit.
// The older token-tree parser remains as a conservative fallback for records
// without a reliable shared identity.
std::vector<OutfitGroup>
BuildStableOutfitGroups(const std::vector<ArmorRecord> &a_items,
                        const std::stop_token &a_stopToken = {}) {
  struct Bucket {
    std::string key;
    std::string name;
    std::vector<ArmorRecord> items;
  };

  std::vector<Bucket> buckets;
  for (const auto &item : a_items) {
    ThrowIfScanCancelled(a_stopToken);
    auto name = FindKnownCatalogFamilyRoot(item)
                    .value_or(NormalizeKitName(std::string(item.DisplayName())));
    auto key = NormalizeKey(name);
    if (key.empty() || !IsReliableOutfitIdentity(name)) {
      continue;
    }
    auto bucket = std::ranges::find(buckets, key, &Bucket::key);
    if (bucket == buckets.end()) {
      buckets.push_back({std::move(key), std::move(name), {}});
      bucket = std::prev(buckets.end());
    }
    bucket->items.push_back(item);
  }

  std::unordered_set<std::string> groupedIdentifiers;
  std::vector<OutfitGroup> result;
  for (auto &bucket : buckets) {
    ThrowIfScanCancelled(a_stopToken);
    if (bucket.items.size() < kMinimumGroupSize) {
      continue;
    }
    for (const auto &item : bucket.items) {
      groupedIdentifiers.insert(item.Identifier());
    }
    result.push_back({std::move(bucket.name), std::move(bucket.items)});
  }

  std::vector<ArmorRecord> residual;
  residual.reserve(a_items.size());
  for (const auto &item : a_items) {
    ThrowIfScanCancelled(a_stopToken);
    if (!groupedIdentifiers.contains(item.Identifier())) {
      residual.push_back(item);
    }
  }
  auto fallback = BuildGroupsAtDepth(residual, 0, a_stopToken);
  result.insert(result.end(), std::make_move_iterator(fallback.begin()),
                std::make_move_iterator(fallback.end()));
  return result;
}

std::vector<KitCandidate> BuildCandidates(
    const OutfitGroup &a_group,
    const std::function<void(float, std::string)> &a_progress,
    const std::stop_token &a_stopToken,
    std::size_t a_profileWorkerBudget);

bool HasDominantHighCandidateGroup(
    const std::vector<OutfitGroup> &a_groups,
    const std::size_t a_usableItemCount,
    const std::stop_token &a_stopToken) {
  constexpr std::size_t kMaximumAutomaticCandidateCount = 30;
  for (const auto &group : a_groups) {
    if (group.items.size() * 2 < a_usableItemCount) {
      continue;
    }
    const auto candidates =
        BuildCandidates(group, {}, a_stopToken, std::size_t{1});
    if (candidates.size() > kMaximumAutomaticCandidateCount) {
      return true;
    }
  }
  return false;
}

bool IsOriginalGroupingSuitable(const std::vector<ArmorRecord> &a_items,
                                const std::stop_token &a_stopToken) {
  std::size_t excludedAdult = 0;
  const auto classified =
      PreferBaseVariants(a_items, excludedAdult, true, a_stopToken);
  const auto usable =
      DeduplicateAppearanceRecords(classified, a_stopToken);
  if (usable.size() < kMinimumGroupSize) {
    return false;
  }

  auto groups = BuildStableOutfitGroups(usable, a_stopToken);
  if (groups.empty()) {
    groups.push_back({BuildGroupName(usable), usable});
  }
  groups = MergeGroups(groups, a_stopToken);
  groups = MergeRelatedSiblingGroups(std::move(groups), a_stopToken);
  return !groups.empty() &&
         !HasDominantHighCandidateGroup(groups, usable.size(), a_stopToken);
}

std::optional<std::string>
ExtractAttachedNumberVariant(const std::string_view a_token) {
  if (a_token.empty()) {
    return std::nullopt;
  }

  auto suffixStart = a_token.size();
  auto letterCount = std::size_t{0};
  while (suffixStart > 0 && letterCount < 2 &&
         static_cast<unsigned char>(a_token[suffixStart - 1]) < 128 &&
         std::isalpha(static_cast<unsigned char>(a_token[suffixStart - 1]))) {
    --suffixStart;
    ++letterCount;
  }
  auto digitEnd = suffixStart;
  while (suffixStart > 0 &&
         static_cast<unsigned char>(a_token[suffixStart - 1]) < 128 &&
         std::isdigit(static_cast<unsigned char>(a_token[suffixStart - 1]))) {
    --suffixStart;
  }
  if (suffixStart == digitEnd) {
    suffixStart = a_token.size();
    while (suffixStart > 0 &&
           static_cast<unsigned char>(a_token[suffixStart - 1]) < 128 &&
           std::isdigit(static_cast<unsigned char>(a_token[suffixStart - 1]))) {
      --suffixStart;
    }
    digitEnd = a_token.size();
  }
  if (suffixStart == 0 || suffixStart == digitEnd) {
    return std::nullopt;
  }
  return LowerAscii(std::string(a_token.substr(suffixStart)));
}

std::optional<std::string>
ExtractAttachedAlphabetVariant(const std::string_view a_token) {
  if (a_token.size() < 2) {
    return std::nullopt;
  }
  auto suffixStart = a_token.size();
  while (suffixStart > 0 && a_token.size() - suffixStart < 2) {
    const auto ch = static_cast<unsigned char>(a_token[suffixStart - 1]);
    if (ch >= 128 || !std::isupper(ch)) {
      break;
    }
    --suffixStart;
  }
  if (suffixStart == a_token.size() || suffixStart == 0) {
    return std::nullopt;
  }
  if (static_cast<unsigned char>(a_token[suffixStart - 1]) < 128 &&
      std::isupper(
          static_cast<unsigned char>(a_token[suffixStart - 1]))) {
    return std::nullopt;
  }
  const auto prefix = a_token.substr(0, suffixStart);
  const auto hasDistinctPrefix = std::ranges::any_of(
      prefix, [](const unsigned char ch) { return ch >= 128 || std::islower(ch); });
  if (!hasDistinctPrefix) {
    return std::nullopt;
  }
  return LowerAscii(std::string(a_token.substr(suffixStart)));
}

void AppendAttachedVariantTokens(const std::string_view a_token,
                                 std::vector<std::string> &a_profiles) {
  const auto normalized = NormalizeKey(a_token);
  if (normalized == "alt" || normalized == "alternative" ||
      (normalized.starts_with("alt") && normalized.size() > 3 &&
       IsNumberedVariantToken(normalized.substr(3)))) {
    a_profiles.push_back("alt");
  }
  if (const auto number = ExtractAttachedNumberVariant(a_token)) {
    a_profiles.push_back(*number);
  }
  if (const auto alphabet = ExtractAttachedAlphabetVariant(a_token)) {
    a_profiles.push_back(*alphabet);
  }

  const auto appendKnownSuffix = [&](const auto &tokens) {
    for (const auto &variant : tokens) {
      if (variant.size() < 3 || normalized.size() <= variant.size() ||
          !normalized.ends_with(variant)) {
        continue;
      }
      a_profiles.push_back(variant);
    }
  };
  appendKnownSuffix(kVariantTokens);
  appendKnownSuffix(kColorAndTypeTokens);
}

std::string StripAttachedComponentVariants(std::string_view a_token);

std::string ExtractProfile(const std::string &a_groupName,
                           const ArmorRecord &a_item,
                           const bool a_rootFamilyExpanded = false) {
  std::vector<std::string> profiles;
  const auto display = std::string(a_item.DisplayName());
  if (NormalizeKey(display).find("one piece") != std::string::npos) {
    profiles.push_back("onepiece");
  }
  if (std::ranges::any_of(Tokenize(a_item.editorID), [](const auto &token) {
        return NormalizeKey(token) == "smp";
      })) {
    profiles.push_back("smp");
  }
  std::string withoutBrackets;
  for (std::size_t index = 0; index < display.size();) {
    if (display[index] == '[') {
      const auto end = display.find(']', index + 1);
      if (end != std::string::npos) {
        for (const auto &token :
             Tokenize(display.substr(index + 1, end - index - 1))) {
          const auto normalized = NormalizeKey(token);
          if (!normalized.empty() && !kPartTokens.contains(normalized)) {
            profiles.push_back(normalized);
          }
        }
        index = end + 1;
        continue;
      }
    }
    withoutBrackets.push_back(display[index++]);
  }
  const auto itemTokens = Tokenize(withoutBrackets);
  const auto groupTokens = Tokenize(a_groupName);
  std::size_t prefix = 0;
  while (prefix < itemTokens.size() && prefix < groupTokens.size() &&
         NormalizeKey(itemTokens[prefix]) == NormalizeKey(groupTokens[prefix])) {
    ++prefix;
  }
  const auto hasExactGroupPrefix = prefix == groupTokens.size();
  for (std::size_t index = prefix; index < itemTokens.size(); ++index) {
    const auto token = NormalizeKey(itemTokens[index]);
    AppendAttachedVariantTokens(itemTokens[index], profiles);
    if (token.empty() || kPartTokens.contains(token)) {
      continue;
    }
    if (IsVariantMetadataToken(token) || token.size() <= 2) {
      profiles.push_back(token);
      continue;
    }
    // The expanded bare-family path deliberately joined labels that a general
    // part dictionary cannot know (e.g. Latex, Arsenic Remix, or a translated
    // author style name). Preserve such exact-name tails as candidate profile
    // data. This prevents a body from one named variant being paired with an
    // accessory from another while ordinary groups keep their existing local
    // choice behavior.
    if (a_rootFamilyExpanded && hasExactGroupPrefix) {
      const auto component = StripAttachedComponentVariants(itemTokens[index]);
      if (!component.empty() && !IsComponentPartToken(component)) {
        profiles.push_back(component);
      }
    }
  }
  std::vector<std::string> unique;
  for (const auto &profile : profiles) {
    if (std::ranges::find(unique, profile) == unique.end()) {
      unique.push_back(profile);
    }
  }
  std::string result;
  for (const auto &profile : unique) {
    if (!result.empty()) {
      result.push_back(' ');
    }
    result.append(profile);
  }
  return result;
}

bool IsAltVariantToken(const std::string_view a_token) {
  const auto normalized = NormalizeKey(a_token);
  return normalized == "alt" || normalized == "alternative" ||
         (normalized.starts_with("alt") && normalized.size() > 3 &&
          IsNumberedVariantToken(normalized.substr(3)));
}

bool IsStrongOutfitProfileToken(const std::string_view a_token) {
  const auto normalized = NormalizeKey(a_token);
  return kGemColorTokens.contains(normalized) ||
         (kColorAndTypeTokens.contains(normalized) &&
          !kLocalProfileTokens.contains(normalized));
}

std::string StripAttachedComponentVariants(const std::string_view a_token) {
  auto result = NormalizeKey(a_token);
  std::vector<std::string> attached;
  AppendAttachedVariantTokens(a_token, attached);
  for (auto iterator = attached.rbegin(); iterator != attached.rend();
       ++iterator) {
    const auto &variant = *iterator;
    if (result.size() > variant.size() && result.ends_with(variant)) {
      result.erase(result.size() - variant.size());
    }
  }
  return result;
}

std::string ExtractComponentKey(const std::string &a_groupName,
                                const ArmorRecord &a_item) {
  const auto itemTokens = Tokenize(RemoveBracketedText(a_item.DisplayName()));
  const auto groupTokens = Tokenize(a_groupName);
  std::size_t prefix = 0;
  while (prefix < itemTokens.size() && prefix < groupTokens.size() &&
         NormalizeKey(itemTokens[prefix]) == NormalizeKey(groupTokens[prefix])) {
    ++prefix;
  }

  std::string result;
  for (std::size_t index = prefix; index < itemTokens.size(); ++index) {
    const auto token = NormalizeKey(itemTokens[index]);
    if (token.empty() || IsAltVariantToken(token) ||
        IsVariantMetadataToken(token)) {
      continue;
    }
    const auto componentToken =
        StripAttachedComponentVariants(itemTokens[index]);
    if (componentToken.empty()) {
      continue;
    }
    if (!result.empty()) {
      result.push_back(' ');
    }
    result.append(componentToken);
  }
  return result.empty() ? NormalizeKey(a_item.DisplayName()) : result;
}

std::unordered_set<std::string> ProfileTokens(const std::string &a_profile) {
  std::unordered_set<std::string> result;
  for (const auto &token : Tokenize(a_profile)) {
    result.insert(NormalizeKey(token));
  }
  return result;
}

bool ProfilesCompatible(const std::string &a_left, const std::string &a_right) {
  if (a_left.empty() || a_right.empty()) {
    return a_left.empty() && a_right.empty();
  }
  const auto left = ProfileTokens(a_left);
  const auto right = ProfileTokens(a_right);
  const auto subset = [](const auto &a_first, const auto &a_second) {
    return std::ranges::all_of(a_first, [&](const auto &token) {
      return a_second.contains(token);
    });
  };
  return subset(left, right) || subset(right, left);
}

std::unordered_set<std::string>
HardProfileTokens(const std::string &a_profile) {
  auto tokens = ProfileTokens(a_profile);
  tokens.erase("smp");
  return tokens;
}

bool CandidateMatchesProfile(const std::string &a_candidateProfile,
                             const std::string &a_targetProfile) {
  const auto candidateAllTokens = ProfileTokens(a_candidateProfile);
  const auto targetAllTokens = ProfileTokens(a_targetProfile);
  const auto hasStandaloneToken = [](const auto &a_tokens) {
    return std::ranges::any_of(a_tokens, [](const auto &token) {
      return kStandaloneProfileTokens.contains(token);
    });
  };
  if (hasStandaloneToken(candidateAllTokens) !=
      hasStandaloneToken(targetAllTokens)) {
    return false;
  }
  if (a_targetProfile.empty()) {
    return a_candidateProfile.empty();
  }
  if (a_candidateProfile.empty()) {
    return true;
  }
  const auto candidateTokens = HardProfileTokens(a_candidateProfile);
  const auto targetTokens = HardProfileTokens(a_targetProfile);
  return std::ranges::all_of(candidateTokens, [&](const auto &token) {
    return targetTokens.contains(token);
  });
}

struct SlotCandidate {
  ArmorRecord item;
  std::string profile;
  std::string componentKey;
};

int ProfileTokenRank(const std::string_view a_token) {
  if (a_token == "alt") {
    return 0;
  }
  if (IsStrongOutfitProfileToken(a_token)) {
    return 1;
  }
  if (IsNumberedVariantToken(a_token) || a_token.size() <= 2) {
    return 2;
  }
  return 3;
}

std::string CanonicalProfile(std::vector<std::string> a_tokens) {
  for (auto &token : a_tokens) {
    token = NormalizeKey(token);
    if (token == "alternative") {
      token = "alt";
    }
  }
  std::erase_if(a_tokens,
                [](const auto &token) { return token.empty(); });
  std::ranges::sort(a_tokens, [](const auto &left, const auto &right) {
    return std::tuple{ProfileTokenRank(left), left} <
           std::tuple{ProfileTokenRank(right), right};
  });
  a_tokens.erase(std::unique(a_tokens.begin(), a_tokens.end()),
                 a_tokens.end());

  std::string result;
  for (const auto &token : a_tokens) {
    if (!result.empty()) {
      result.push_back(' ');
    }
    result.append(token);
  }
  return result;
}

void NormalizeOutfitWideProfiles(std::vector<SlotCandidate> &a_candidates) {
  std::uint32_t allSlots = 0;
  std::unordered_map<std::string, std::uint32_t> tokenSlots;
  for (const auto &candidate : a_candidates) {
    allSlots |= candidate.item.visualSlotMask;
    for (auto token : ProfileTokens(candidate.profile)) {
      if (token == "alternative") {
        token = "alt";
      }
      tokenSlots[token] |= candidate.item.visualSlotMask;
    }
  }

  const auto totalSlotCount = std::popcount(allSlots);
  for (auto &candidate : a_candidates) {
    std::vector<std::string> profileTokens;
    for (auto token : ProfileTokens(candidate.profile)) {
      if (token == "alternative") {
        token = "alt";
      }
      const auto coveredSlotCount = std::popcount(tokenSlots[token]);
      const auto repeatsAcrossOutfit =
          totalSlotCount <= 2 ||
          (coveredSlotCount >= 2 &&
           coveredSlotCount * 5 >= totalSlotCount * 2);
      if (token == "smp" || kStandaloneProfileTokens.contains(token) ||
          token == "alt" || IsStrongOutfitProfileToken(token) ||
          repeatsAcrossOutfit) {
        profileTokens.push_back(std::move(token));
      }
    }
    candidate.profile = CanonicalProfile(std::move(profileTokens));
  }
}

bool IsLocalChoiceProfileToken(const std::string_view a_token) {
  return a_token.starts_with("localchoice") ||
         a_token.starts_with("localzip");
}

std::string PublicCandidateProfile(const std::string &a_profile) {
  auto tokens = Tokenize(a_profile);
  std::erase_if(tokens, [](const auto &token) {
    return IsLocalChoiceProfileToken(NormalizeKey(token));
  });
  auto result = CanonicalProfile(std::move(tokens));
  return result.empty() ? "base" : result;
}

std::string CandidateDifferenceLabel(
    const std::string &a_groupName, const std::string &a_publicProfile,
    const ArmorRecord &a_item) {
  const auto buildLabel = [&](const std::string_view a_text) {
    const auto itemTokens = Tokenize(RemoveBracketedText(a_text));
    const auto groupTokens = Tokenize(RemoveBracketedText(a_groupName));
    std::size_t prefix = 0;
    while (prefix < itemTokens.size() && prefix < groupTokens.size() &&
           NormalizeKey(itemTokens[prefix]) ==
               NormalizeKey(groupTokens[prefix])) {
      ++prefix;
    }

    const auto profileTokens = ProfileTokens(a_publicProfile);
    std::vector<std::string> kept;
    for (std::size_t index = prefix; index < itemTokens.size(); ++index) {
      const auto normalized = NormalizeKey(itemTokens[index]);
      if (normalized.empty() || profileTokens.contains(normalized) ||
          IsLocalChoiceProfileToken(normalized)) {
        continue;
      }
      kept.push_back(std::move(normalized));
    }
    std::string label;
    for (const auto &token : kept) {
      if (!label.empty() && !IsNumberedVariantToken(token)) {
        label.push_back(' ');
      }
      label.append(token);
    }
    return label;
  };

  auto label = buildLabel(a_item.DisplayName());
  if (label.empty()) {
    label = buildLabel(a_item.editorID);
  }
  return label;
}

void DisambiguateCandidateProfiles(const std::string &a_groupName,
                                   std::vector<KitCandidate> &a_candidates) {
  std::unordered_map<std::string, std::vector<std::size_t>> profileGroups;
  for (std::size_t index = 0; index < a_candidates.size(); ++index) {
    profileGroups[NormalizeKey(a_candidates[index].profile)].push_back(index);
  }

  for (const auto &[_, indices] : profileGroups) {
    if (indices.size() <= 1) {
      continue;
    }

    std::unordered_map<std::string, std::size_t> identifierCounts;
    for (const auto index : indices) {
      std::unordered_set<std::string> candidateIdentifiers;
      for (const auto &item : a_candidates[index].items) {
        candidateIdentifiers.insert(item.Identifier());
      }
      for (const auto &identifier : candidateIdentifiers) {
        ++identifierCounts[identifier];
      }
    }

    std::vector<std::vector<std::string>> differenceLabels(indices.size());
    for (std::size_t groupIndex = 0; groupIndex < indices.size();
         ++groupIndex) {
      const auto candidateIndex = indices[groupIndex];
      auto appendLabels = [&](const bool a_onlyCandidateSpecific) {
        for (const auto &item : a_candidates[candidateIndex].items) {
          if (a_onlyCandidateSpecific &&
              identifierCounts[item.Identifier()] == indices.size()) {
            continue;
          }
          auto label = CandidateDifferenceLabel(
              a_groupName, a_candidates[candidateIndex].profile, item);
          if (!label.empty() &&
              std::ranges::find(differenceLabels[groupIndex], label) ==
                  differenceLabels[groupIndex].end()) {
            differenceLabels[groupIndex].push_back(std::move(label));
          }
        }
      };
      appendLabels(true);
      if (differenceLabels[groupIndex].empty()) {
        appendLabels(false);
      }
    }

    std::vector<std::string> displayProfiles;
    displayProfiles.reserve(indices.size());
    for (std::size_t groupIndex = 0; groupIndex < indices.size();
         ++groupIndex) {
      auto profile = a_candidates[indices[groupIndex]].profile;
      if (!differenceLabels[groupIndex].empty()) {
        profile.push_back(' ');
        profile.append(differenceLabels[groupIndex].front());
      }
      displayProfiles.push_back(std::move(profile));
    }

    // A synchronized local choice can differ in more than one component. Add
    // one more concise component label before falling back to a stable ordinal.
    std::unordered_map<std::string, std::vector<std::size_t>> stillDuplicated;
    for (std::size_t groupIndex = 0; groupIndex < indices.size();
         ++groupIndex) {
      stillDuplicated[NormalizeKey(displayProfiles[groupIndex])].push_back(
          groupIndex);
    }
    for (const auto &[__, duplicateIndices] : stillDuplicated) {
      if (duplicateIndices.size() <= 1) {
        continue;
      }
      for (const auto groupIndex : duplicateIndices) {
        if (differenceLabels[groupIndex].size() > 1) {
          displayProfiles[groupIndex].push_back(' ');
          displayProfiles[groupIndex].append(
              differenceLabels[groupIndex][1]);
        }
      }
    }

    stillDuplicated.clear();
    for (std::size_t groupIndex = 0; groupIndex < indices.size();
         ++groupIndex) {
      stillDuplicated[NormalizeKey(displayProfiles[groupIndex])].push_back(
          groupIndex);
    }
    for (const auto &[__, duplicateIndices] : stillDuplicated) {
      for (std::size_t ordinal = 1; ordinal < duplicateIndices.size();
           ++ordinal) {
        displayProfiles[duplicateIndices[ordinal]].append(
            std::format("{:02}", ordinal + 1));
      }
    }

    for (std::size_t groupIndex = 0; groupIndex < indices.size();
         ++groupIndex) {
      a_candidates[indices[groupIndex]].profile =
          std::move(displayProfiles[groupIndex]);
    }
  }
}

void AppendProfileToken(std::string &a_profile, std::string a_token) {
  auto tokens = Tokenize(a_profile);
  tokens.push_back(std::move(a_token));
  a_profile = CanonicalProfile(std::move(tokens));
}

using LocalChoiceDimension = std::vector<std::string>;

std::vector<LocalChoiceDimension>
AssignLocalChoiceProfiles(std::vector<SlotCandidate> &a_candidates) {
  struct Axis {
    std::uint32_t slotMask{0};
    std::size_t optionCount{0};
    std::vector<std::vector<std::size_t>> buckets;
  };

  std::map<std::uint32_t, std::map<std::string, std::vector<std::size_t>>>
      bySlotAndProfile;
  for (std::size_t index = 0; index < a_candidates.size(); ++index) {
    const auto mask = a_candidates[index].item.sourceSlotMask;
    if (mask != 0) {
      bySlotAndProfile[mask][a_candidates[index].profile].push_back(index);
    }
  }

  std::vector<Axis> axes;
  for (auto &[slotMask, profileBuckets] : bySlotAndProfile) {
    std::size_t optionCount = 0;
    std::vector<std::vector<std::size_t>> repeatedBuckets;
    for (auto &[_, indices] : profileBuckets) {
      if (indices.size() <= 1) {
        continue;
      }
      if (optionCount == 0) {
        optionCount = indices.size();
      }
      if (indices.size() != optionCount || optionCount > 4) {
        optionCount = 0;
        repeatedBuckets.clear();
        break;
      }
      std::ranges::sort(indices, [&](const auto left, const auto right) {
        return a_candidates[left].item.sourceOrder <
               a_candidates[right].item.sourceOrder;
      });
      repeatedBuckets.push_back(indices);
    }
    if (optionCount > 1 && !repeatedBuckets.empty()) {
      axes.push_back({slotMask, optionCount, std::move(repeatedBuckets)});
    }
  }

  std::vector<LocalChoiceDimension> dimensions;
  if (axes.empty()) {
    return dimensions;
  }

  const auto zippedOptionCount = axes.front().optionCount;
  const auto zipAxes = axes.size() >= 3 &&
                       std::ranges::all_of(axes, [&](const auto &axis) {
                         return axis.optionCount == zippedOptionCount;
                       });
  if (zipAxes) {
    LocalChoiceDimension dimension;
    for (std::size_t option = 0; option < zippedOptionCount; ++option) {
      dimension.push_back(std::format("localzip{}", option + 1));
    }
    for (const auto &axis : axes) {
      for (const auto &bucket : axis.buckets) {
        for (std::size_t option = 0; option < bucket.size(); ++option) {
          AppendProfileToken(a_candidates[bucket[option]].profile,
                             dimension[option]);
        }
      }
    }
    dimensions.push_back(std::move(dimension));
    return dimensions;
  }

  for (std::size_t axisIndex = 0; axisIndex < axes.size(); ++axisIndex) {
    auto &axis = axes[axisIndex];
    LocalChoiceDimension dimension;
    for (std::size_t option = 0; option < axis.optionCount; ++option) {
      dimension.push_back(std::format("localchoice{}v{}", axisIndex + 1,
                                      option + 1));
    }
    for (const auto &bucket : axis.buckets) {
      for (std::size_t option = 0; option < bucket.size(); ++option) {
        AppendProfileToken(a_candidates[bucket[option]].profile,
                           dimension[option]);
      }
    }
    dimensions.push_back(std::move(dimension));
  }
  return dimensions;
}

bool CandidateMatchesTarget(
    const SlotCandidate &a_candidate, const std::string &a_targetProfile,
    const std::vector<SlotCandidate> &a_candidates) {
  if (!CandidateMatchesProfile(a_candidate.profile, a_targetProfile)) {
    return false;
  }

  const auto candidateAllTokens = ProfileTokens(a_candidate.profile);
  const auto targetAllTokens = ProfileTokens(a_targetProfile);
  const auto candidateSmp = candidateAllTokens.contains("smp");
  const auto targetSmp = targetAllTokens.contains("smp");
  if (candidateSmp && !targetSmp) {
    return false;
  }
  if (targetSmp && !candidateSmp) {
    return !std::ranges::any_of(a_candidates, [&](const auto &other) {
      return other.componentKey == a_candidate.componentKey &&
             ProfileTokens(other.profile).contains("smp") &&
             CandidateMatchesProfile(other.profile, a_targetProfile);
    });
  }

  const auto targetTokens = HardProfileTokens(a_targetProfile);
  if (!targetTokens.contains("alt")) {
    return true;
  }
  if (HardProfileTokens(a_candidate.profile).contains("alt")) {
    return true;
  }

  return !std::ranges::any_of(a_candidates, [&](const auto &other) {
    return other.componentKey == a_candidate.componentKey &&
           HardProfileTokens(other.profile).contains("alt") &&
           CandidateMatchesProfile(other.profile, a_targetProfile);
  });
}

bool IsSmpVariant(const SlotCandidate &a_candidate) {
  if (ProfileTokens(a_candidate.profile).contains("smp")) {
    return true;
  }
  return std::ranges::any_of(
      Tokenize(a_candidate.item.DisplayName()), [](const auto &token) {
        return NormalizeKey(token) == "smp";
      });
}

bool HasSmpItem(const KitCandidate &a_candidate) {
  return std::ranges::any_of(a_candidate.items, [](const auto &item) {
    const auto text = std::string(item.DisplayName()) + " " + item.editorID;
    return std::ranges::any_of(Tokenize(text),
                               [](const auto &token) {
      return NormalizeKey(token) == "smp";
    });
  });
}

int ExposurePriority(const ArmorRecord &a_item) {
  const auto text = std::string(a_item.DisplayName()) + " " + a_item.editorID;
  const auto normalized = NormalizeKey(text);
  const auto tokens = ProfileTokens(normalized);
  auto score = (a_item.nsfw || IsAdultMarker(text, false)) ? 1000 : 0;
  const auto has = [&](const std::string_view token) {
    return tokens.contains(std::string(token));
  };
  if (has("open") || has("up") || has("skimpy") || has("slutty")) {
    score += 300;
  }
  if (has("short")) {
    score += 150;
  }
  if (has("closed") || has("long") || has("modest")) {
    score -= 250;
  }
  return score;
}

int CandidateExposurePriority(const KitCandidate &a_candidate) {
  return std::accumulate(
      a_candidate.items.begin(), a_candidate.items.end(), 0,
      [](const auto total, const auto &item) {
        return total + ExposurePriority(item);
      });
}

std::uint32_t CandidateLayoutCoverage(const KitCandidate &a_candidate) {
  std::uint32_t coverage = 0;
  for (const auto &item : a_candidate.items) {
    coverage |= item.layoutSlotMask != 0 ? item.layoutSlotMask
                                         : item.visualSlotMask;
  }
  return coverage;
}

int CandidatePositiveExposure(const KitCandidate &a_candidate) {
  return std::accumulate(
      a_candidate.items.begin(), a_candidate.items.end(), 0,
      [](const auto total, const auto &item) {
        return total + (std::max)(0, ExposurePriority(item));
      });
}

int CandidateBasePriority(const KitCandidate &a_candidate) {
  const auto tokens = ProfileTokens(a_candidate.profile);
  if (tokens.empty() || tokens.contains("base")) {
    return 2;
  }
  if (tokens.contains("default") || tokens.contains("original") ||
      tokens.contains("기본") || tokens.contains("오리지널") ||
      tokens.contains("默认") || tokens.contains("原版") ||
      tokens.contains("原始")) {
    return 1;
  }
  return 0;
}

std::size_t ChooseInitialCandidateIndex(
    const std::vector<KitCandidate> &a_candidates) {
  if (a_candidates.empty()) {
    return 0;
  }

  const auto rank = [](const KitCandidate &candidate) {
    const auto exposure = CandidatePositiveExposure(candidate);
    return std::tuple{exposure == 0,
                      std::popcount(CandidateLayoutCoverage(candidate)),
                      HasSmpItem(candidate),
                      CandidateBasePriority(candidate),
                      candidate.items.size(), -exposure, candidate.score};
  };
  auto bestIndex = std::size_t{0};
  auto bestRank = rank(a_candidates.front());
  for (std::size_t index = 1; index < a_candidates.size(); ++index) {
    const auto candidateRank = rank(a_candidates[index]);
    if (candidateRank > bestRank) {
      bestRank = candidateRank;
      bestIndex = index;
    }
  }
  return bestIndex;
}

void SelectInitialCandidate(GeneratedKit &a_kit) {
  const auto selected = ChooseInitialCandidateIndex(a_kit.candidates);
  a_kit.selectedCandidate = selected;
  a_kit.draftCandidate = selected;
}

std::string CanonicalCrossPluginKitKey(const std::string_view a_name) {
  static const std::unordered_map<std::string, std::string> aliases{
      {"파이터", "fighter"}};
  const auto normalizedName = NormalizeKitName(std::string(a_name));
  std::string key;
  for (auto token : Tokenize(normalizedName)) {
    token = NormalizeKey(token);
    if (const auto alias = aliases.find(token); alias != aliases.end()) {
      token = alias->second;
    }
    if (!token.empty()) {
      if (!key.empty()) {
        key.push_back(' ');
      }
      key.append(token);
    }
  }
  return key;
}

std::unordered_set<std::string> GeneratedKitPluginKeys(
    const GeneratedKit &a_kit) {
  std::unordered_set<std::string> plugins;
  const auto append = [&](const ArmorRecord &item) {
    if (!item.pluginName.empty()) {
      plugins.insert(LowerAscii(item.pluginName));
    }
  };
  if (!a_kit.sourceItems.empty()) {
    for (const auto &item : a_kit.sourceItems) {
      append(item);
    }
  } else {
    for (const auto &candidate : a_kit.candidates) {
      for (const auto &item : candidate.items) {
        append(item);
      }
    }
  }
  return plugins;
}

std::unordered_set<std::string> GeneratedKitModelPaths(
    const GeneratedKit &a_kit) {
  std::unordered_set<std::string> paths;
  const auto append = [&](const ArmorRecord &item) {
    paths.insert(item.armorModelPaths.begin(), item.armorModelPaths.end());
  };
  if (!a_kit.sourceItems.empty()) {
    for (const auto &item : a_kit.sourceItems) {
      append(item);
    }
  } else {
    for (const auto &candidate : a_kit.candidates) {
      for (const auto &item : candidate.items) {
        append(item);
      }
    }
  }
  return paths;
}

std::uint32_t MaximumCandidateCoverage(const GeneratedKit &a_kit) {
  std::uint32_t best = 0;
  for (const auto &candidate : a_kit.candidates) {
    const auto coverage = CandidateLayoutCoverage(candidate);
    if (std::popcount(coverage) > std::popcount(best)) {
      best = coverage;
    }
  }
  return best;
}

const KitCandidate *BestStructuredCandidate(const GeneratedKit &a_kit) {
  if (a_kit.candidates.empty()) {
    return nullptr;
  }
  return std::addressof(*std::ranges::max_element(
      a_kit.candidates, [](const auto &left, const auto &right) {
        return std::tuple{std::popcount(CandidateLayoutCoverage(left)),
                          left.items.size()} <
               std::tuple{std::popcount(CandidateLayoutCoverage(right)),
                          right.items.size()};
      }));
}

bool HasConsistentPieceStructure(const GeneratedKit &a_left,
                                 const GeneratedKit &a_right) {
  const auto *left = BestStructuredCandidate(a_left);
  const auto *right = BestStructuredCandidate(a_right);
  if (left == nullptr || right == nullptr || left->items.empty() ||
      right->items.empty()) {
    return false;
  }
  std::vector<std::uint32_t> unmatchedRightMasks;
  unmatchedRightMasks.reserve(right->items.size());
  for (const auto &item : right->items) {
    const auto mask = item.layoutSlotMask != 0 ? item.layoutSlotMask
                                                : item.visualSlotMask;
    if (mask != 0) {
      unmatchedRightMasks.push_back(mask);
    }
  }
  std::size_t comparableLeftCount = 0;
  std::size_t matchedCount = 0;
  for (const auto &item : left->items) {
    const auto leftMask = item.layoutSlotMask != 0 ? item.layoutSlotMask
                                                    : item.visualSlotMask;
    if (leftMask == 0) {
      continue;
    }
    ++comparableLeftCount;
    const auto match = std::ranges::find_if(
        unmatchedRightMasks, [&](const auto rightMask) {
          return (leftMask & rightMask) != 0;
        });
    if (match != unmatchedRightMasks.end()) {
      unmatchedRightMasks.erase(match);
      ++matchedCount;
    }
  }
  const auto smallerItemCount =
      (std::min)(comparableLeftCount, right->items.size());
  return smallerItemCount >= 3 && matchedCount * 4 >= smallerItemCount * 3;
}

bool HasSamePieceIdentity(const ArmorRecord &a_left,
                          const ArmorRecord &a_right) {
  const auto leftMask = a_left.layoutSlotMask != 0 ? a_left.layoutSlotMask
                                                    : a_left.visualSlotMask;
  const auto rightMask = a_right.layoutSlotMask != 0 ? a_right.layoutSlotMask
                                                      : a_right.visualSlotMask;
  if (leftMask == 0 || rightMask == 0 || (leftMask & rightMask) == 0) {
    return false;
  }

  return std::ranges::any_of(a_left.armorModelPaths, [&](const auto &leftPath) {
    const auto normalizedLeftPath = NormalizeKey(leftPath);
    return !normalizedLeftPath.empty() &&
           std::ranges::any_of(a_right.armorModelPaths,
                               [&](const auto &rightPath) {
                                 return normalizedLeftPath ==
                                        NormalizeKey(rightPath);
                               });
  });
}

// Differently named copies have no reliable display-name evidence.  Require
// their actual candidate pieces (same slot plus the same model path) to agree
// at the same threshold used for the rest of duplicate
// detection.  This prevents unrelated outfits with a coincidentally similar
// slot layout from being collapsed.
bool HasConsistentPieceContent(const GeneratedKit &a_left,
                               const GeneratedKit &a_right) {
  const auto *left = BestStructuredCandidate(a_left);
  const auto *right = BestStructuredCandidate(a_right);
  if (left == nullptr || right == nullptr) {
    return false;
  }

  std::vector<bool> matchedRight(right->items.size());
  std::size_t comparableLeftCount = 0;
  std::size_t comparableRightCount = 0;
  std::size_t matchedCount = 0;
  for (const auto &item : right->items) {
    const auto mask = item.layoutSlotMask != 0 ? item.layoutSlotMask
                                                : item.visualSlotMask;
    if (mask != 0 && !item.armorModelPaths.empty()) {
      ++comparableRightCount;
    }
  }
  for (const auto &leftItem : left->items) {
    const auto mask = leftItem.layoutSlotMask != 0 ? leftItem.layoutSlotMask
                                                    : leftItem.visualSlotMask;
    if (mask == 0 || leftItem.armorModelPaths.empty()) {
      continue;
    }
    ++comparableLeftCount;
    for (std::size_t rightIndex = 0; rightIndex < right->items.size();
         ++rightIndex) {
      if (!matchedRight[rightIndex] &&
          HasSamePieceIdentity(leftItem, right->items[rightIndex])) {
        matchedRight[rightIndex] = true;
        ++matchedCount;
        break;
      }
    }
  }
  const auto smallerItemCount =
      (std::min)(comparableLeftCount, comparableRightCount);
  return smallerItemCount >= 3 && matchedCount * 4 >= smallerItemCount * 3;
}

auto CrossPluginDuplicatePreference(const GeneratedKit &a_kit) {
  auto maximumSlots = 0;
  auto maximumItems = std::size_t{0};
  for (const auto &candidate : a_kit.candidates) {
    maximumSlots = (std::max)(
        maximumSlots, std::popcount(CandidateLayoutCoverage(candidate)));
    maximumItems = (std::max)(maximumItems, candidate.items.size());
  }
  return std::tuple{maximumSlots, maximumItems, a_kit.sourceItems.size(),
                    a_kit.candidates.size()};
}

bool AreCrossPluginDuplicateKits(const GeneratedKit &a_left,
                                 const GeneratedKit &a_right) {
  const auto leftKey = CanonicalCrossPluginKitKey(a_left.name);
  const auto rightKey = CanonicalCrossPluginKitKey(a_right.name);
  const auto matchingName = !leftKey.empty() && leftKey == rightKey;
  const auto matchingPieceContent =
      HasConsistentPieceContent(a_left, a_right);
  if (!matchingName && !matchingPieceContent) {
    return false;
  }

  const auto leftPlugins = GeneratedKitPluginKeys(a_left);
  const auto rightPlugins = GeneratedKitPluginKeys(a_right);
  if (leftPlugins.empty() || rightPlugins.empty() ||
      std::ranges::any_of(leftPlugins, [&](const auto &plugin) {
        return rightPlugins.contains(plugin);
      })) {
    return false;
  }

  const auto leftCoverage = MaximumCandidateCoverage(a_left);
  const auto rightCoverage = MaximumCandidateCoverage(a_right);
  const auto smallerCoverage =
      (std::min)(std::popcount(leftCoverage), std::popcount(rightCoverage));
  const auto sharedCoverage = std::popcount(leftCoverage & rightCoverage);
  return smallerCoverage >= 3 && sharedCoverage * 4 >= smallerCoverage * 3 &&
         HasConsistentPieceStructure(a_left, a_right) &&
         (matchingName || matchingPieceContent);
}

std::size_t CollapseCrossPluginDuplicateKits(
    std::vector<GeneratedKit> &a_kits) {
  auto removed = std::size_t{0};
  std::vector<GeneratedKit> unique;
  unique.reserve(a_kits.size());
  for (auto &kit : a_kits) {
    const auto duplicate =
        std::ranges::find_if(unique, [&](const auto &existing) {
          return AreCrossPluginDuplicateKits(existing, kit);
        });
    if (duplicate == unique.end()) {
      unique.push_back(std::move(kit));
      continue;
    }
    if (CrossPluginDuplicatePreference(kit) >
        CrossPluginDuplicatePreference(*duplicate)) {
      *duplicate = std::move(kit);
    }
    ++removed;
  }
  a_kits = std::move(unique);
  return removed;
}

struct SlotSelection {
  std::int64_t score{0};
  std::vector<ArmorRecord> items;
};

using CandidateProgress = std::function<void(float, std::string)>;

std::size_t CountSlotConflicts(const std::vector<ArmorRecord> &a_items) {
  std::size_t conflicts = 0;
  for (const auto slot : kVisualSlots) {
    const auto mask = SlotMask(slot);
    const auto count = std::ranges::count_if(a_items, [&](const auto &item) {
      return (item.sourceSlotMask & mask) != 0;
    });
    if (count > 1) {
      conflicts += count - 1;
    }
  }
  return conflicts;
}

std::int64_t ScoreCandidate(const SlotCandidate &a_candidate,
                            const std::string &a_profile) {
  auto score = static_cast<std::int64_t>(std::popcount(a_candidate.item.visualSlotMask)) *
               1'000'000;
  if (a_candidate.profile.empty()) {
    score += a_profile.empty() ? 80'000 : 45'000;
  } else if (a_candidate.profile == a_profile) {
    score += 100'000;
  } else if (ProfilesCompatible(a_candidate.profile, a_profile)) {
    score += 70'000;
  } else {
    score += 5'000;
  }
  if (IsSmpVariant(a_candidate)) {
    score += 60'000;
  }
  score += static_cast<std::int64_t>(
      10'000 - (std::min)(a_candidate.item.sourceOrder, std::size_t{10'000}));
  score -= static_cast<std::int64_t>(
      (std::min)(std::size_t{2'000}, Tokenize(a_candidate.profile).size() * 50));
  return score;
}

SlotSelection SelectSingleSlotCandidates(
    const std::vector<SlotCandidate> &a_candidates,
    const std::string &a_profile, const CandidateProgress &a_progress,
    const std::string_view a_groupName, const std::size_t a_profileIndex,
    const std::size_t a_profileCount,
    const std::stop_token &a_stopToken) {
  constexpr auto kNoCandidate =
      (std::numeric_limits<std::size_t>::max)();
  std::array<std::size_t, 32> bestCandidateBySlot;
  bestCandidateBySlot.fill(kNoCandidate);
  std::array<std::int64_t, 32> bestScoreBySlot{};

  for (std::size_t candidateIndex = 0;
       candidateIndex < a_candidates.size(); ++candidateIndex) {
    ThrowIfScanCancelled(a_stopToken);
    const auto &candidate = a_candidates[candidateIndex];
    if (!CandidateMatchesTarget(candidate, a_profile, a_candidates)) {
      if (a_progress) {
        const auto selectedSlotCount = static_cast<std::size_t>(
            std::ranges::count_if(bestCandidateBySlot,
                                  [&](const auto index) {
                                    return index != kNoCandidate;
                                  }));
        a_progress(
            static_cast<float>(candidateIndex + 1) /
                static_cast<float>(a_candidates.size()),
            Localization::Get().Format(
                "progress.slot_search_state", a_groupName, a_profileIndex,
                a_profileCount, candidateIndex + 1, a_candidates.size(),
                selectedSlotCount));
      }
      continue;
    }
    const auto slotIndex =
        static_cast<std::size_t>(std::countr_zero(candidate.item.sourceSlotMask));
    const auto score = ScoreCandidate(candidate, a_profile);
    if (bestCandidateBySlot[slotIndex] == kNoCandidate ||
        score > bestScoreBySlot[slotIndex]) {
      bestCandidateBySlot[slotIndex] = candidateIndex;
      bestScoreBySlot[slotIndex] = score;
    }

    if (a_progress) {
      const auto selectedSlotCount = static_cast<std::size_t>(
          std::ranges::count_if(bestCandidateBySlot, [&](const auto index) {
            return index != kNoCandidate;
          }));
      a_progress(
          static_cast<float>(candidateIndex + 1) /
              static_cast<float>(a_candidates.size()),
          Localization::Get().Format(
              "progress.slot_search_state", a_groupName, a_profileIndex,
              a_profileCount, candidateIndex + 1, a_candidates.size(),
              selectedSlotCount));
    }
  }

  std::vector<std::size_t> selectedIndices;
  selectedIndices.reserve(bestCandidateBySlot.size());
  for (const auto candidateIndex : bestCandidateBySlot) {
    if (candidateIndex != kNoCandidate) {
      selectedIndices.push_back(candidateIndex);
    }
  }
  std::ranges::sort(selectedIndices);

  SlotSelection result;
  result.items.reserve(selectedIndices.size());
  for (const auto candidateIndex : selectedIndices) {
    result.score += ScoreCandidate(a_candidates[candidateIndex], a_profile);
    result.items.push_back(a_candidates[candidateIndex].item);
  }
  return result;
}

SlotSelection SelectSlots(const std::vector<SlotCandidate> &a_candidates,
                          const std::string &a_profile,
                          const CandidateProgress &a_progress,
                          const std::string_view a_groupName,
                          const std::size_t a_profileIndex,
                          const std::size_t a_profileCount,
                          const std::stop_token &a_stopToken,
                          const std::size_t a_stateWorkerBudget) {
  if (std::ranges::all_of(a_candidates, [](const auto &candidate) {
        return std::has_single_bit(candidate.item.sourceSlotMask);
      })) {
    return SelectSingleSlotCandidates(
        a_candidates, a_profile, a_progress, a_groupName, a_profileIndex,
        a_profileCount, a_stopToken);
  }

  // Candidates with the same occupied-slot mask are interchangeable for the
  // exact packing problem. Keeping only the highest-scoring one preserves the
  // optimum while preventing color/number variants from multiplying the DP
  // state work.
  std::unordered_map<std::uint32_t, std::size_t> bestCandidateByMask;
  for (std::size_t candidateIndex = 0;
       candidateIndex < a_candidates.size(); ++candidateIndex) {
    ThrowIfScanCancelled(a_stopToken);
    const auto &candidate = a_candidates[candidateIndex];
    if (!CandidateMatchesTarget(candidate, a_profile, a_candidates)) {
      continue;
    }
    const auto existing =
        bestCandidateByMask.find(candidate.item.sourceSlotMask);
    if (existing == bestCandidateByMask.end()) {
      bestCandidateByMask.emplace(candidate.item.sourceSlotMask,
                                  candidateIndex);
      continue;
    }
    const auto existingScore =
        ScoreCandidate(a_candidates[existing->second], a_profile);
    const auto candidateScore = ScoreCandidate(candidate, a_profile);
    if (candidateScore > existingScore ||
        (candidateScore == existingScore &&
         std::tuple{candidate.item.sourceOrder, candidate.item.runtimeFormID} <
             std::tuple{a_candidates[existing->second].item.sourceOrder,
                        a_candidates[existing->second].item.runtimeFormID})) {
      existing->second = candidateIndex;
    }
  }

  std::vector<std::size_t> eligibleCandidateIndices;
  eligibleCandidateIndices.reserve(bestCandidateByMask.size());
  for (const auto candidateIndex : bestCandidateByMask | std::views::values) {
    eligibleCandidateIndices.push_back(candidateIndex);
  }
  std::ranges::sort(eligibleCandidateIndices, [&](const auto left,
                                                  const auto right) {
    return std::tuple{a_candidates[left].item.sourceOrder,
                      a_candidates[left].item.runtimeFormID} <
           std::tuple{a_candidates[right].item.sourceOrder,
                      a_candidates[right].item.runtimeFormID};
  });

  // A single multi-slot piece used to push every otherwise independent slot
  // into one global DP.  Large packs such as ADD 03 Dark Knight then expanded
  // twenty distinct masks toward 2^20 states even though only slots 54/58
  // actually conflicted.  Masks that overlap no other eligible mask are an
  // independent positive-score choice, so select them directly and reserve
  // the exact DP for the genuinely interacting component.
  SlotSelection independentSelection;
  std::vector<std::size_t> interactingCandidateIndices;
  interactingCandidateIndices.reserve(eligibleCandidateIndices.size());
  for (std::size_t left = 0; left < eligibleCandidateIndices.size(); ++left) {
    ThrowIfScanCancelled(a_stopToken);
    const auto candidateIndex = eligibleCandidateIndices[left];
    const auto mask = a_candidates[candidateIndex].item.sourceSlotMask;
    bool overlapsAnotherMask = false;
    for (std::size_t right = 0; right < eligibleCandidateIndices.size();
         ++right) {
      if (left == right) {
        continue;
      }
      const auto otherIndex = eligibleCandidateIndices[right];
      if ((mask & a_candidates[otherIndex].item.sourceSlotMask) != 0) {
        overlapsAnotherMask = true;
        break;
      }
    }
    if (overlapsAnotherMask) {
      interactingCandidateIndices.push_back(candidateIndex);
    } else {
      independentSelection.score += ScoreCandidate(
          a_candidates[candidateIndex], a_profile);
      independentSelection.items.push_back(a_candidates[candidateIndex].item);
    }
  }

  if (interactingCandidateIndices.empty()) {
    std::ranges::sort(independentSelection.items, {},
                      &ArmorRecord::sourceOrder);
    return independentSelection;
  }

  std::unordered_map<std::uint32_t, SlotSelection> states;
  states.emplace(0, SlotSelection{});
  for (std::size_t eligibleIndex = 0;
       eligibleIndex < interactingCandidateIndices.size(); ++eligibleIndex) {
    ThrowIfScanCancelled(a_stopToken);
    const auto candidateIndex = interactingCandidateIndices[eligibleIndex];
    const auto &candidate = a_candidates[candidateIndex];
    std::vector<std::pair<std::uint32_t, SlotSelection>> snapshot;
    snapshot.reserve(states.size());
    std::size_t copiedStateCount = 0;
    for (const auto &state : states) {
      if ((copiedStateCount++ & 127U) == 0) {
        ThrowIfScanCancelled(a_stopToken);
      }
      snapshot.push_back(state);
    }
    struct StateTransition {
      std::uint32_t mask{0};
      SlotSelection selection;
    };
    constexpr std::size_t minimumStatesPerWorker = 2048;
    const auto usefulWorkers =
        snapshot.empty()
            ? std::size_t{1}
            : (snapshot.size() + minimumStatesPerWorker - 1) /
                  minimumStatesPerWorker;
    const auto stateWorkerCount =
        (std::max)(std::size_t{1},
                   (std::min)(a_stateWorkerBudget, usefulWorkers));
    std::vector<std::vector<StateTransition>> transitions(stateWorkerCount);
    std::atomic_size_t processedStates{0};
    std::atomic_bool transitionFailed{false};
    std::mutex transitionFailureMutex;
    std::exception_ptr transitionFailure;
    const auto score = ScoreCandidate(candidate, a_profile);

    const auto processStateRange = [&](const std::size_t workerIndex,
                                       const std::size_t begin,
                                       const std::size_t end) {
      try {
        auto &workerTransitions = transitions[workerIndex];
        workerTransitions.reserve(end - begin);
        for (std::size_t stateIndex = begin; stateIndex < end; ++stateIndex) {
          if ((stateIndex & 511U) == 0) {
            if (transitionFailed.load(std::memory_order_acquire)) {
              return;
            }
            ThrowIfScanCancelled(a_stopToken);
          }
          const auto &[mask, state] = snapshot[stateIndex];
          if ((mask & candidate.item.sourceSlotMask) == 0) {
            auto next = state;
            next.score += score;
            next.items.push_back(candidate.item);
            workerTransitions.push_back(
                {mask | candidate.item.sourceSlotMask, std::move(next)});
          }
          const auto processed =
              processedStates.fetch_add(1, std::memory_order_relaxed) + 1;
          if (a_progress && stateWorkerCount > 1 &&
              (processed & 2047U) == 0) {
            const auto innerRatio =
                static_cast<float>(processed) /
                static_cast<float>((std::max)(std::size_t{1},
                                              snapshot.size()));
            const auto current = static_cast<float>(eligibleIndex) + innerRatio;
            const auto ratio =
                interactingCandidateIndices.empty()
                    ? 1.0F
                    : current /
                          static_cast<float>(interactingCandidateIndices.size());
            a_progress(
                ratio,
                Localization::Get().Format(
                    "progress.slot_search_state", a_groupName, a_profileIndex,
                    a_profileCount, eligibleIndex + 1,
                    interactingCandidateIndices.size(), states.size()));
          }
        }
      } catch (...) {
        {
          const std::scoped_lock lock(transitionFailureMutex);
          if (!transitionFailure) {
            transitionFailure = std::current_exception();
          }
        }
        transitionFailed.store(true, std::memory_order_release);
      }
    };

    if (stateWorkerCount == 1) {
      processStateRange(0, 0, snapshot.size());
    } else {
      std::vector<std::jthread> stateWorkers;
      stateWorkers.reserve(stateWorkerCount);
      for (std::size_t workerIndex = 0; workerIndex < stateWorkerCount;
           ++workerIndex) {
        const auto begin = snapshot.size() * workerIndex / stateWorkerCount;
        const auto end =
            snapshot.size() * (workerIndex + 1) / stateWorkerCount;
        stateWorkers.emplace_back([&, workerIndex, begin, end](
                                      const std::stop_token) {
          ::SetThreadPriority(::GetCurrentThread(),
                              THREAD_PRIORITY_BELOW_NORMAL);
          processStateRange(workerIndex, begin, end);
        });
      }
      for (auto &worker : stateWorkers) {
        worker.join();
      }
    }
    if (transitionFailure) {
      std::rethrow_exception(transitionFailure);
    }
    ThrowIfScanCancelled(a_stopToken);

    const auto transitionCount = std::accumulate(
        transitions.begin(), transitions.end(), std::size_t{},
        [](const auto total, const auto &workerTransitions) {
          return total + workerTransitions.size();
        });
    states.reserve(states.size() + transitionCount);
    for (auto &workerTransitions : transitions) {
      for (auto &transition : workerTransitions) {
        const auto existing = states.find(transition.mask);
        if (existing == states.end() ||
            transition.selection.score > existing->second.score) {
          states[transition.mask] = std::move(transition.selection);
        }
      }
    }
    if (a_progress) {
      a_progress(
          static_cast<float>(eligibleIndex + 1) /
              static_cast<float>((std::max)(
                  std::size_t{1}, interactingCandidateIndices.size())),
          Localization::Get().Format(
              "progress.slot_search_state", a_groupName, a_profileIndex,
              a_profileCount, eligibleIndex + 1,
              interactingCandidateIndices.size(), states.size()));
    }
  }
  SlotSelection best;
  std::tuple<int, std::int64_t, std::size_t> bestKey{};
  bool hasBest = false;
  std::size_t checkedStateCount = 0;
  for (const auto &state : states | std::views::values) {
    if ((checkedStateCount++ & 127U) == 0) {
      ThrowIfScanCancelled(a_stopToken);
    }
    std::uint32_t mask = 0;
    for (const auto &item : state.items) {
      mask |= item.sourceSlotMask;
    }
    const auto firstOrder =
        state.items.empty()
            ? (std::numeric_limits<std::size_t>::max)()
            : std::ranges::min_element(state.items, {},
                                       &ArmorRecord::sourceOrder)
                  ->sourceOrder;
    const auto key = std::tuple{
        std::popcount(mask), state.score,
        (std::numeric_limits<std::size_t>::max)() - firstOrder};
    if (!hasBest || bestKey < key) {
      best = state;
      bestKey = key;
      hasBest = true;
    }
  }
  best.score += independentSelection.score;
  best.items.insert(best.items.end(), independentSelection.items.begin(),
                    independentSelection.items.end());
  std::ranges::sort(best.items, {}, &ArmorRecord::sourceOrder);
  return best;
}

std::vector<std::string>
ChooseProfiles(const std::vector<SlotCandidate> &a_candidates,
               const std::vector<LocalChoiceDimension> &a_localDimensions,
               const std::stop_token &a_stopToken) {
  std::vector<std::string> structuralOptions{std::string{}};
  std::vector<std::string> colorOptions{std::string{}};
  bool hasSmp = false;
  for (const auto &candidate : a_candidates) {
    ThrowIfScanCancelled(a_stopToken);
    std::vector<std::string> structuralTokens;
    std::vector<std::string> colorTokens;
    for (const auto &token : ProfileTokens(candidate.profile)) {
      if (token == "smp") {
        hasSmp = true;
      } else if (IsLocalChoiceProfileToken(token)) {
        continue;
      } else if (IsStrongOutfitProfileToken(token)) {
        colorTokens.push_back(token);
      } else {
        structuralTokens.push_back(token);
      }
    }
    auto structural = CanonicalProfile(std::move(structuralTokens));
    auto color = CanonicalProfile(std::move(colorTokens));
    if (!structural.empty() &&
        std::ranges::find(structuralOptions, structural) ==
            structuralOptions.end()) {
      structuralOptions.push_back(std::move(structural));
    }
    if (!color.empty() &&
        std::ranges::find(colorOptions, color) == colorOptions.end()) {
      colorOptions.push_back(std::move(color));
    }
  }

  std::vector<std::vector<std::string>> localCombinations{{}};
  for (const auto &dimension : a_localDimensions) {
    std::vector<std::vector<std::string>> next;
    for (const auto &combination : localCombinations) {
      for (const auto &option : dimension) {
        auto expanded = combination;
        expanded.push_back(option);
        next.push_back(std::move(expanded));
      }
    }
    localCombinations = std::move(next);
    if (localCombinations.size() >= kMaximumGeneratedCandidateProfiles) {
      localCombinations.resize(kMaximumGeneratedCandidateProfiles);
      break;
    }
  }

  std::vector<std::string> result;
  std::unordered_set<std::string> seen;
  const auto append = [&](std::vector<std::string> a_tokens) {
    if (result.size() >= kMaximumGeneratedCandidateProfiles) {
      return;
    }
    auto profile = CanonicalProfile(std::move(a_tokens));
    if (seen.insert(profile).second) {
      result.push_back(std::move(profile));
    }
  };
  for (const auto &structural : structuralOptions) {
    const auto structuralTokens = Tokenize(structural);
    const auto standalone = std::ranges::any_of(
        structuralTokens, [](const auto &token) {
          return kStandaloneProfileTokens.contains(NormalizeKey(token));
        });
    const std::vector<std::vector<std::string>> noLocal{{}};
    const auto &combinations = standalone ? noLocal : localCombinations;
    for (const auto &color : colorOptions) {
      for (const auto &local : combinations) {
        for (int smp = 0; smp <= (hasSmp ? 1 : 0); ++smp) {
          std::vector<std::string> tokens = structuralTokens;
          const auto colorTokens = Tokenize(color);
          tokens.insert(tokens.end(), colorTokens.begin(), colorTokens.end());
          tokens.insert(tokens.end(), local.begin(), local.end());
          if (smp != 0) {
            tokens.push_back("smp");
          }
          append(std::move(tokens));
        }
      }
    }
  }
  if (result.empty()) {
    result.push_back({});
  }
  return result;
}

std::vector<KitCandidate> BuildCandidates(
    const OutfitGroup &a_group, const CandidateProgress &a_progress,
    const std::stop_token &a_stopToken,
    const std::size_t a_profileWorkerBudget) {
  ThrowIfScanCancelled(a_stopToken);
  if (a_progress) {
    a_progress(0.0F, Localization::Get().Format(
                         "progress.preparing_slot_resolve", a_group.name));
  }
  std::vector<SlotCandidate> candidates;
  for (const auto &item : a_group.items) {
    ThrowIfScanCancelled(a_stopToken);
    if (item.visualSlotMask != 0) {
      candidates.push_back({item, ExtractProfile(a_group.name, item,
                                                  a_group.rootFamilyExpanded),
                            ExtractComponentKey(a_group.name, item)});
    }
  }
  if (candidates.empty()) {
    return {};
  }
  NormalizeOutfitWideProfiles(candidates);
  const auto localChoiceDimensions = AssignLocalChoiceProfiles(candidates);
  bool conflicts = false;
  for (const auto slot : kVisualSlots) {
    ThrowIfScanCancelled(a_stopToken);
    const auto count = std::ranges::count_if(candidates, [&](const auto &item) {
      return (item.item.sourceSlotMask & SlotMask(slot)) != 0;
    });
    conflicts |= count > 1;
  }
  if (!conflicts) {
    std::vector<ArmorRecord> items;
    for (const auto &candidate : candidates) {
      items.push_back(candidate.item);
    }
    if (a_progress) {
      a_progress(1.0F, Localization::Get().Format(
                           "progress.slot_resolve_ok", a_group.name));
    }
    return {{"base", std::move(items), 0}};
  }

  std::vector<KitCandidate> result;
  std::set<std::vector<std::uint32_t>> seen;
  const auto profiles =
      ChooseProfiles(candidates, localChoiceDimensions, a_stopToken);
  std::vector<std::size_t> profileStateWorkerBudgets(profiles.size(), 1);
  if (!profiles.empty() && profiles.size() < a_profileWorkerBudget) {
    const auto baseBudget = a_profileWorkerBudget / profiles.size();
    const auto extraBudget = a_profileWorkerBudget % profiles.size();
    for (std::size_t index = 0; index < profiles.size(); ++index) {
      profileStateWorkerBudgets[index] =
          baseBudget + (index < extraBudget ? 1 : 0);
    }
  }
  constexpr float searchStart = 0.125F;
  std::vector<SlotSelection> selections(profiles.size());
  std::vector<float> profileProgress(profiles.size(), 0.0F);
  float profileProgressSum = 0.0F;
  std::mutex profileProgressMutex;

  const auto resolveProfile = [&](const std::size_t profileIndex) {
    ThrowIfScanCancelled(a_stopToken);
    const auto &profile = profiles[profileIndex];
    const auto profileLabel = profile.empty() ? std::string("base") : profile;
    const auto reportProfileProgress =
        [&](const float a_ratio, std::string a_detail) {
          if (!a_progress) {
            return;
          }
          float combined = 0.0F;
          {
            const std::scoped_lock lock(profileProgressMutex);
            const auto previous = profileProgress[profileIndex];
            const auto updated =
                (std::max)(previous, std::clamp(a_ratio, 0.0F, 1.0F));
            profileProgress[profileIndex] = updated;
            profileProgressSum += updated - previous;
            combined =
                profileProgressSum / static_cast<float>(profileProgress.size());
          }
          a_progress(searchStart + (1.0F - searchStart) * combined,
                     std::move(a_detail));
        };
    reportProfileProgress(
        0.0F, Localization::Get().Format(
                  "progress.slot_search_profile", a_group.name,
                  profileIndex + 1, profiles.size(), profileLabel));
    selections[profileIndex] = SelectSlots(
        candidates, profile,
        [&](const float a_ratio, std::string a_detail) {
          reportProfileProgress(a_ratio, std::move(a_detail));
        },
        a_group.name, profileIndex + 1, profiles.size(), a_stopToken,
        profileStateWorkerBudgets[profileIndex]);
    reportProfileProgress(
        1.0F, Localization::Get().Format(
                  "progress.slot_search_profile", a_group.name,
                  profileIndex + 1, profiles.size(), profileLabel));
  };

  const auto profileWorkerCount =
      (std::max)(std::size_t{1},
                 (std::min)(a_profileWorkerBudget, profiles.size()));
  if (profileWorkerCount == 1) {
    for (std::size_t profileIndex = 0; profileIndex < profiles.size();
         ++profileIndex) {
      resolveProfile(profileIndex);
    }
  } else {
    std::atomic_size_t nextProfileIndex{0};
    std::atomic_bool profileFailed{false};
    std::mutex profileFailureMutex;
    std::exception_ptr profileFailure;
    const auto profileWorker = [&]() {
      ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      while (!a_stopToken.stop_requested() &&
             !profileFailed.load(std::memory_order_acquire)) {
        const auto profileIndex =
            nextProfileIndex.fetch_add(1, std::memory_order_relaxed);
        if (profileIndex >= profiles.size()) {
          return;
        }
        try {
          resolveProfile(profileIndex);
        } catch (...) {
          {
            const std::scoped_lock lock(profileFailureMutex);
            if (!profileFailure) {
              profileFailure = std::current_exception();
            }
          }
          profileFailed.store(true, std::memory_order_release);
          return;
        }
      }
    };
    std::vector<std::jthread> profileWorkers;
    profileWorkers.reserve(profileWorkerCount);
    for (std::size_t index = 0; index < profileWorkerCount; ++index) {
      profileWorkers.emplace_back(
          [&](const std::stop_token) { profileWorker(); });
    }
    for (auto &worker : profileWorkers) {
      worker.join();
    }
    if (profileFailure) {
      std::rethrow_exception(profileFailure);
    }
  }

  for (std::size_t profileIndex = 0; profileIndex < profiles.size();
       ++profileIndex) {
    const auto &profile = profiles[profileIndex];
    auto &selection = selections[profileIndex];
    std::ranges::sort(selection.items, {}, &ArmorRecord::sourceOrder);
    std::vector<std::uint32_t> signature;
    for (const auto &item : selection.items) {
      signature.push_back(item.runtimeFormID);
    }
    std::ranges::sort(signature);
    const auto groupHasBody = std::ranges::any_of(
        a_group.items, [](const auto &item) {
          return (item.sourceSlotMask & SlotMask(32)) != 0;
        });
    const auto selectionHasBody = std::ranges::any_of(
        selection.items, [](const auto &item) {
          return (item.sourceSlotMask & SlotMask(32)) != 0;
        });
    if (!selection.items.empty() && (!groupHasBody || selectionHasBody) &&
        seen.insert(std::move(signature)).second) {
      result.push_back(
          {PublicCandidateProfile(profile), std::move(selection.items),
           selection.score});
    }
  }
  std::erase_if(result,
                [](const auto &candidate) { return candidate.items.empty(); });
  std::ranges::stable_sort(result, [](const auto &left, const auto &right) {
    return std::tuple{HasSmpItem(left), CandidateExposurePriority(left),
                      left.score} >
           std::tuple{HasSmpItem(right), CandidateExposurePriority(right),
                      right.score};
  });
  DisambiguateCandidateProfiles(a_group.name, result);
  if (a_progress) {
    a_progress(1.0F,
               Localization::Get().Format("progress.slot_resolve_complete",
                                          a_group.name));
  }
  return result;
}

std::string BuildConsoleProgressLine(
    const float, const std::size_t, const std::size_t,
    const std::string_view a_pluginName,
    const std::string_view a_detail,
    const std::chrono::steady_clock::duration a_elapsed) {
  const auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(a_elapsed).count();
  return std::format("  {} - {} ({:02}:{:02})", a_pluginName, a_detail,
                     seconds / 60, seconds % 60);
}

struct ScanWorkerPlan {
  std::size_t workerCount{1};
  std::size_t logicalProcessors{1};
  std::uint64_t availableMemoryMiB{0};
};

ScanWorkerPlan DetermineScanWorkerPlan(const std::size_t a_jobCount) {
  ScanWorkerPlan plan;
  plan.logicalProcessors =
      (std::max)(std::size_t{1},
                 static_cast<std::size_t>(std::thread::hardware_concurrency()));
  const auto cpuWorkers =
      plan.logicalProcessors > 1 ? plan.logicalProcessors - 1 : std::size_t{1};

  MEMORYSTATUSEX memoryStatus{.dwLength = sizeof(MEMORYSTATUSEX)};
  std::size_t memoryWorkers = cpuWorkers;
  if (::GlobalMemoryStatusEx(&memoryStatus)) {
    constexpr std::uint64_t bytesPerMiB = 1024ULL * 1024ULL;
    constexpr std::uint64_t gameReserveMiB = 2048;
    constexpr std::uint64_t workerBudgetMiB = 512;
    plan.availableMemoryMiB = memoryStatus.ullAvailPhys / bytesPerMiB;
    const auto usableMemoryMiB =
        plan.availableMemoryMiB > gameReserveMiB
            ? plan.availableMemoryMiB - gameReserveMiB
            : std::uint64_t{0};
    memoryWorkers = static_cast<std::size_t>(
        (std::max)(std::uint64_t{1}, usableMemoryMiB / workerBudgetMiB));
  }

  plan.workerCount =
      (std::max)(std::size_t{1},
                 (std::min)({a_jobCount, cpuWorkers, memoryWorkers}));
  return plan;
}

std::vector<std::size_t>
DistributeWorkerBudgets(const std::size_t a_totalWorkerBudget,
                        const std::size_t a_workerCount) {
  if (a_workerCount == 0) {
    return {};
  }
  const auto totalBudget = (std::max)(a_totalWorkerBudget, a_workerCount);
  std::vector<std::size_t> budgets(
      a_workerCount, totalBudget / a_workerCount);
  for (std::size_t index = 0; index < totalBudget % a_workerCount; ++index) {
    ++budgets[index];
  }
  return budgets;
}

float UpdateMonotonicPluginProgress(std::vector<float> &a_progress,
                                    const std::size_t a_pluginIndex,
                                    const float a_newProgress,
                                    float &a_progressSum) {
  if (a_pluginIndex >= a_progress.size() || a_progress.empty()) {
    return 1.0F;
  }
  const auto previous = a_progress[a_pluginIndex];
  const auto updated =
      (std::max)(previous, std::clamp(a_newProgress, 0.0F, 1.0F));
  a_progress[a_pluginIndex] = updated;
  a_progressSum += updated - previous;
  return std::clamp(a_progressSum / static_cast<float>(a_progress.size()),
                    0.0F, 1.0F);
}

std::string SafeFilename(std::string a_name) {
  for (auto &ch : a_name) {
    if (static_cast<unsigned char>(ch) < 32 || ch == '<' || ch == '>' ||
        ch == ':' || ch == '"' || ch == '/' || ch == '\\' || ch == '|' ||
        ch == '?' || ch == '*') {
      ch = '_';
    }
  }
  a_name = TrimSpaces(a_name);
  while (!a_name.empty() && (a_name.back() == '.' || a_name.back() == ' ')) {
    a_name.pop_back();
  }
  return a_name.empty() ? "Kit" : a_name;
}

std::filesystem::path PathFromUtf8(const std::string_view a_value) {
  const auto *begin = reinterpret_cast<const char8_t *>(a_value.data());
  return std::filesystem::path(
      std::u8string(begin, begin + a_value.size()));
}

std::filesystem::path UniqueOutputPath(const std::filesystem::path &a_root,
                                       const std::string &a_displayName) {
  const auto stem = PathFromUtf8(SafeFilename(a_displayName));
  auto candidate = a_root / stem;
  candidate += L".json";
  if (!std::filesystem::exists(candidate)) {
    return candidate;
  }
  for (std::size_t index = 2;; ++index) {
    candidate = a_root / PathFromUtf8(
                             std::format("{} {}", SafeFilename(a_displayName),
                                         index));
    candidate += L".json";
    if (!std::filesystem::exists(candidate)) {
      return candidate;
    }
  }
}

nlohmann::json BuildKitJson(const std::string &a_name,
                            const KitCandidate &a_candidate) {
  nlohmann::json items = nlohmann::json::object();
  std::unordered_map<std::string, std::size_t> editorCounts;
  for (const auto &item : a_candidate.items) {
    if (!item.editorID.empty()) {
      ++editorCounts[LowerAscii(item.editorID)];
    }
  }
  for (const auto &item : a_candidate.items) {
    auto key = item.editorID;
    if (key.empty() || editorCounts[LowerAscii(key)] != 1) {
      key = std::format("Form{:08X}", item.localFormID);
    }
    items[key] = {{"Plugin", item.pluginName},
                  {"Name", std::string(item.DisplayName())},
                  {"Amount", 1},
                  {"Equipped", true}};
  }

  std::map<std::uint32_t, std::vector<std::string>> rows;
  for (const auto &item : a_candidate.items) {
    if (item.layoutSlotMask != 0 &&
        std::has_single_bit(item.layoutSlotMask)) {
      rows[item.layoutSlotMask].push_back(item.Identifier());
    }
  }
  nlohmann::json layoutRows = nlohmann::json::array();
  for (auto &[mask, identifiers] : rows) {
    std::ranges::sort(identifiers);
    identifiers.erase(std::unique(identifiers.begin(), identifiers.end()),
                      identifiers.end());
    layoutRows.push_back({{"targetKind", "slot"},
                          {"targetSlotMask", mask},
                          {"overrideIdentifiers", identifiers},
                          {"hideEquipped", false}});
  }

  return {{a_name,
           {{"Collection", "Generated"},
            {"Description", "Created by Skyrim Fitting System."},
            {"Items", std::move(items)},
            {"SkyrimFittingSystem", {{"layoutRows", std::move(layoutRows)}}}}}};
}
} // namespace

namespace sfs::kit_generator {
std::string_view ArmorRecord::DisplayName() const {
  return !name.empty() ? std::string_view(name) : std::string_view(editorID);
}

std::string ArmorRecord::Identifier() const {
  return std::format("{}|{:08X}", pluginName, localFormID);
}

bool GeneratedKit::IsSelectedCandidateNsfw() const {
  if (safetyPrefixOverride.has_value()) {
    return *safetyPrefixOverride;
  }
  if (candidates.empty()) {
    return false;
  }
  const auto selected = (std::min)(selectedCandidate, candidates.size() - 1);
  return std::ranges::any_of(candidates[selected].items,
                             [](const auto &item) { return item.nsfw; });
}

void GeneratedKit::FreezeSafetyPrefixFromSelectedCandidate() {
  safetyPrefixOverride = IsSelectedCandidateNsfw();
}

void GeneratedKit::ToggleSafetyPrefix() {
  safetyPrefixOverride = !IsSelectedCandidateNsfw();
}

Generator &Generator::Get() {
  static Generator instance;
  return instance;
}

Generator::~Generator() {
  if (assessmentWorker_.joinable()) {
    assessmentWorker_.request_stop();
    assessmentWorker_.join();
  }
  if (worker_.joinable()) {
    worker_.request_stop();
    worker_.join();
  }
}

void Generator::SnapshotLoadedArmorForms() {
  if (state_.load(std::memory_order_acquire) == ScanState::Scanning) {
    return;
  }
  if (assessmentWorker_.joinable()) {
    assessmentWorker_.request_stop();
    assessmentWorker_.join();
  }
  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    logger::error("Could not snapshot armor forms: TESDataHandler unavailable");
    return;
  }

  std::map<std::string, PluginSource> sources;
  std::unordered_map<std::string, std::size_t> orders;
  for (auto *armor : dataHandler->GetFormArray<RE::TESObjectARMO>()) {
    if (!armor || armor->IsDeleted() || armor->IsIgnored()) {
      continue;
    }
    const auto *file = armor->GetFile(0);
    if (!file || file->GetFilename().empty()) {
      continue;
    }
    const auto pluginName = std::string(file->GetFilename());
    if (kBaseGamePlugins.contains(LowerAscii(pluginName))) {
      continue;
    }

    std::uint32_t armorMask = armor->GetSlotMask().underlying();
    std::uint32_t addonMask = 0;
    std::vector<std::uint32_t> armorAddonFormIDs;
    std::vector<std::string> armorModelPaths;
    for (const auto *addon : armor->armorAddons) {
      if (addon) {
        addonMask |= addon->GetSlotMask().underlying();
        armorAddonFormIDs.push_back(addon->GetFormID());
        for (const auto &model : addon->bipedModels) {
          if (const auto *path = model.GetModel(); path != nullptr &&
                                                  path[0] != '\0') {
            auto normalizedPath = LowerAscii(path);
            std::ranges::replace(normalizedPath, '\\', '/');
            armorModelPaths.push_back(std::move(normalizedPath));
          }
        }
      }
    }
    std::ranges::sort(armorAddonFormIDs);
    armorAddonFormIDs.erase(
        std::unique(armorAddonFormIDs.begin(), armorAddonFormIDs.end()),
        armorAddonFormIDs.end());
    std::ranges::sort(armorModelPaths);
    armorModelPaths.erase(
        std::unique(armorModelPaths.begin(), armorModelPaths.end()),
        armorModelPaths.end());
    const auto sourceMask = armorMask != 0 ? armorMask : addonMask;
    if (sourceMask == 0) {
      continue;
    }

    const auto copyText = [](const char *text) {
      return text && text[0] != '\0' ? std::string(text) : std::string{};
    };
    ArmorRecord record;
    record.pluginName = pluginName;
    record.runtimeFormID = armor->GetFormID();
    record.localFormID = armor->GetLocalFormID();
    record.editorID = copyText(armor->GetFormEditorID());
    record.name = copyText(armor->GetName());
    record.sourceSlotMask = sourceMask;
    record.visualSlotMask =
        InferVisualSlot(record.editorID + " " + record.name, sourceMask);
    auto workbenchMask = sourceMask;
    if ((workbenchMask & SlotMask(31)) != 0 &&
        (workbenchMask & SlotMask(42)) != 0) {
      workbenchMask &= ~SlotMask(31);
    }
    record.layoutSlotMask = SelectPrimaryLayoutSlot(workbenchMask);
    record.armorAddonFormIDs = std::move(armorAddonFormIDs);
    record.armorModelPaths = std::move(armorModelPaths);
    record.sourceOrder = orders[pluginName]++;
    record.enchanted = armor->formEnchanting != nullptr;
    if (record.DisplayName().empty() || record.visualSlotMask == 0 ||
        record.layoutSlotMask == 0) {
      continue;
    }

    auto [sourceIt, _] = sources.try_emplace(
        pluginName, PluginSource{.name = pluginName, .selected = false});
    sourceIt->second.armors.push_back(std::move(record));
  }

  pluginSources_.clear();
  pluginSources_.reserve(sources.size());
  for (auto &[_, source] : sources) {
    if (!source.armors.empty()) {
      pluginSources_.push_back(std::move(source));
    }
  }
  std::ranges::sort(pluginSources_, [](const auto &left, const auto &right) {
    return LowerAscii(left.name) < LowerAscii(right.name);
  });
  state_.store(ScanState::Ready, std::memory_order_release);
  logger::info("SFS Kit Generator snapshot: {} outfit plugins, {} armor forms",
               pluginSources_.size(),
               std::accumulate(pluginSources_.begin(), pluginSources_.end(),
                               std::size_t{}, [](const auto total, const auto &source) {
                                 return total + source.armors.size();
                               }));

  assessmentWorker_ = std::jthread(
      [this](const std::stop_token token) {
        ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        try {
          for (auto &source : pluginSources_) {
            ThrowIfScanCancelled(token);
            const auto suitable =
                IsOriginalGroupingSuitable(source.armors, token);
            ThrowIfScanCancelled(token);
            source.groupingAssessment->store(
                suitable ? OriginalGroupingAssessment::Suitable
                         : OriginalGroupingAssessment::Unsuitable,
                std::memory_order_release);
          }
        } catch (const ScanCancelled &) {
          return;
        }
      });
}

std::vector<PluginSource> &Generator::PluginSources() { return pluginSources_; }
const std::vector<PluginSource> &Generator::PluginSources() const {
  return pluginSources_;
}
const std::vector<GeneratedKit> &Generator::GeneratedKits() const {
  return generatedKits_;
}
std::vector<GeneratedKit> &Generator::GeneratedKits() { return generatedKits_; }
bool Generator::IncludeSafetyPrefix() const { return includeSafetyPrefix_; }

ProgressSnapshot Generator::GetProgressSnapshot() const {
  ProgressSnapshot snapshot;
  snapshot.state = state_.load(std::memory_order_acquire);
  snapshot.overall = overallProgress_.load(std::memory_order_relaxed);
  snapshot.currentPlugin = pluginProgress_.load(std::memory_order_relaxed);
  snapshot.parallel = parallelScan_.load(std::memory_order_relaxed);
  const std::scoped_lock lock(progressMutex_);
  snapshot.pluginIndex = progressPluginIndex_;
  snapshot.pluginCount = progressPluginCount_;
  snapshot.currentPluginName = progressPluginName_;
  snapshot.detail = progressDetail_;
  snapshot.logLines = logLines_;
  return snapshot;
}

bool Generator::StartScan(const bool a_includeSafetyPrefix) {
  const auto currentState = state_.load(std::memory_order_acquire);
  if (currentState == ScanState::Scanning ||
      currentState == ScanState::Cancelling) {
    return false;
  }
  std::vector<PluginSource> selected;
  for (const auto &source : pluginSources_) {
    if (source.selected) {
      selected.push_back(source);
    }
  }
  if (selected.empty()) {
    return false;
  }
  std::jthread stoppedAssessmentWorker;
  if (assessmentWorker_.joinable()) {
    assessmentWorker_.request_stop();
    stoppedAssessmentWorker = std::move(assessmentWorker_);
  }
  if (worker_.joinable()) {
    worker_.join();
  }
  includeSafetyPrefix_ = a_includeSafetyPrefix;
  generatedKits_.clear();
  {
    const std::scoped_lock lock(progressMutex_);
    logLines_.clear();
    progressPluginIndex_ = 0;
    progressPluginCount_ = selected.size();
    progressPluginName_.clear();
    progressDetail_ = Localization::Get().Text("progress.starting");
  }
  overallProgress_.store(0.0F, std::memory_order_relaxed);
  pluginProgress_.store(0.0F, std::memory_order_relaxed);
  parallelScan_.store(false, std::memory_order_relaxed);
  Localization::Get().SyncWithHost();
  state_.store(ScanState::Scanning, std::memory_order_release);
  worker_ = std::jthread([this, sources = std::move(selected),
                          assessment = std::move(stoppedAssessmentWorker)](
                             const std::stop_token token) mutable {
    if (assessment.joinable()) {
      assessment.join();
    }
    RunScan(token, std::move(sources));
  });
  return true;
}

void Generator::CancelScan() {
  auto expected = ScanState::Scanning;
  if (!state_.compare_exchange_strong(expected, ScanState::Cancelling,
                                      std::memory_order_acq_rel)) {
    return;
  }
  {
    const std::scoped_lock lock(progressMutex_);
    progressDetail_ = Localization::Get().Text("progress.cancelling");
  }
  worker_.request_stop();
}

void Generator::DiscardScanResults() {
  if (state_.load(std::memory_order_acquire) != ScanState::Complete) {
    return;
  }
  if (worker_.joinable()) {
    worker_.join();
  }
  generatedKits_.clear();
  overallProgress_.store(0.0F, std::memory_order_relaxed);
  pluginProgress_.store(0.0F, std::memory_order_relaxed);
  parallelScan_.store(false, std::memory_order_relaxed);
  {
    const std::scoped_lock lock(progressMutex_);
    progressPluginIndex_ = 0;
    progressPluginCount_ = 0;
    progressPluginName_.clear();
    progressDetail_.clear();
    logLines_.clear();
  }
  state_.store(ScanState::Ready, std::memory_order_release);
}

void Generator::SetProgress(const std::size_t a_completedPluginCount,
                            const std::size_t a_pluginCount,
                            const float a_overallProgress,
                            const float a_activeProgress,
                            const bool a_parallel,
                            std::string a_pluginName, std::string a_detail) {
  overallProgress_.store(std::clamp(a_overallProgress, 0.0F, 1.0F),
                         std::memory_order_relaxed);
  pluginProgress_.store(std::clamp(a_activeProgress, 0.0F, 1.0F),
                        std::memory_order_relaxed);
  parallelScan_.store(a_parallel, std::memory_order_relaxed);
  const std::scoped_lock lock(progressMutex_);
  progressPluginIndex_ =
      (std::min)(a_completedPluginCount, a_pluginCount);
  progressPluginCount_ = a_pluginCount;
  progressPluginName_ = std::move(a_pluginName);
  progressDetail_ = std::move(a_detail);
}

void Generator::AppendLog(std::string a_line) {
  const std::scoped_lock lock(progressMutex_);
  logLines_.push_back(std::move(a_line));
}

void Generator::RunScan(const std::stop_token a_stopToken,
                        std::vector<PluginSource> a_sources) {
  const auto resetAfterCancellation = [&]() {
    generatedKits_.clear();
    overallProgress_.store(0.0F, std::memory_order_relaxed);
    pluginProgress_.store(0.0F, std::memory_order_relaxed);
    parallelScan_.store(false, std::memory_order_relaxed);
    {
      const std::scoped_lock lock(progressMutex_);
      progressPluginIndex_ = 0;
      progressPluginCount_ = 0;
      progressPluginName_.clear();
      progressDetail_.clear();
      logLines_.clear();
    }
    state_.store(ScanState::Ready, std::memory_order_release);
  };

  try {
    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    auto &localization = Localization::Get();
    AppendLog(localization.Format("log.input", a_sources.size()));
    AppendLog(localization.Format(
        "log.output",
        "Data/Interface/SkyrimFittingSystem/user/kits"));
    AppendLog(localization.Text(includeSafetyPrefix_ ? "log.mode_prefix"
                                                     : "log.mode_normal"));
    const auto workerPlan =
        DetermineScanWorkerPlan((std::numeric_limits<std::size_t>::max)());
    AppendLog(localization.Format(
        "log.parallel_workers", workerPlan.workerCount,
        workerPlan.logicalProcessors, workerPlan.availableMemoryMiB));

    const auto pluginWorkerCount =
        (std::max)(std::size_t{1},
                   (std::min)(a_sources.size(), workerPlan.workerCount));
    const auto pluginWorkerBudgets =
        DistributeWorkerBudgets(workerPlan.workerCount, pluginWorkerCount);
    const bool parallelPlugins = pluginWorkerCount > 1;
    parallelScan_.store(parallelPlugins, std::memory_order_relaxed);
    std::string workerBudgetText;
    for (const auto budget : pluginWorkerBudgets) {
      if (!workerBudgetText.empty()) {
        workerBudgetText.append(", ");
      }
      workerBudgetText.append(std::to_string(budget));
    }
    AppendLog(localization.Format("log.parallel_plugins", pluginWorkerCount,
                                  workerBudgetText));

    std::vector<std::vector<GeneratedKit>> generatedByPlugin(a_sources.size());
    std::vector<float> perPluginProgress(a_sources.size(), 0.0F);
    float perPluginProgressSum = 0.0F;
    std::size_t completedPluginCount = 0;
    std::mutex aggregateProgressMutex;
    std::atomic_bool pluginFailed{false};

    const auto publishProgress =
        [&](const std::size_t a_pluginIndex, const float a_progress,
            const std::string &a_pluginName, const std::string &a_detail) {
          const std::scoped_lock lock(aggregateProgressMutex);
          const bool wasComplete =
              perPluginProgress[a_pluginIndex] >= 1.0F;
          const auto overall = UpdateMonotonicPluginProgress(
              perPluginProgress, a_pluginIndex, a_progress,
              perPluginProgressSum);
          const auto current = perPluginProgress[a_pluginIndex];
          if (!wasComplete && current >= 1.0F) {
            ++completedPluginCount;
          }
          SetProgress(completedPluginCount, a_sources.size(), overall,
                      parallelPlugins ? overall : current, parallelPlugins,
                      a_pluginName, a_detail);
        };

    const auto processPlugin = [&](const std::size_t pluginIndex,
                                   const std::size_t pluginWorkerBudget) {
      ThrowIfScanCancelled(a_stopToken);
      if (pluginFailed.load(std::memory_order_acquire)) {
        throw ScanCancelled{};
      }
      const auto &source = a_sources[pluginIndex];
      std::vector<GeneratedKit> pluginGenerated;
      const auto pluginStarted = std::chrono::steady_clock::now();
      auto lastProgressLogged = pluginStarted - std::chrono::seconds(1);
      int lastLoggedPercent = -1;
      std::mutex reportProgressMutex;
      const auto reportProgress =
          [&](const float a_progress, std::string a_detail,
              const bool a_force = false) {
            ThrowIfScanCancelled(a_stopToken);
            if (pluginFailed.load(std::memory_order_acquire)) {
              throw ScanCancelled{};
            }
            const std::scoped_lock reportLock(reportProgressMutex);
            publishProgress(pluginIndex, a_progress, source.name, a_detail);
            const auto now = std::chrono::steady_clock::now();
            const auto rounded = static_cast<int>(
                std::floor(std::clamp(a_progress, 0.0F, 1.0F) * 100.0F));
            if (!a_force && rounded == lastLoggedPercent &&
                now - lastProgressLogged < std::chrono::milliseconds(150)) {
              return;
            }
            lastLoggedPercent = rounded;
            lastProgressLogged = now;
            AppendLog(BuildConsoleProgressLine(
                a_progress, pluginIndex + 1, a_sources.size(), source.name,
                a_detail, now - pluginStarted));
          };
      AppendLog(localization.Format("log.reading", pluginIndex + 1,
                                    a_sources.size(), source.name));
      reportProgress(0.0F, localization.Text("progress.reading_esp"), true);

      reportProgress(0.35F, localization.Text("progress.filtering"), true);
      std::size_t excludedAdult = 0;
      const auto classified = PreferBaseVariants(
          source.armors, excludedAdult, true, a_stopToken);
      const auto usable =
          DeduplicateAppearanceRecords(classified, a_stopToken);
      ThrowIfScanCancelled(a_stopToken);
      AppendLog(localization.Format("log.records", source.armors.size(),
                                    usable.size(), excludedAdult));

      reportProgress(0.42F, localization.Text("progress.building_tree"), true);
      auto groups = BuildStableOutfitGroups(usable, a_stopToken);
      ThrowIfScanCancelled(a_stopToken);
      if (groups.empty() && usable.size() >= kMinimumGroupSize) {
        groups.push_back({BuildGroupName(usable), usable});
      }
      const auto treeGroupCount = groups.size();

      reportProgress(0.55F, localization.Text("progress.merging"), true);
      groups = MergeGroups(groups, a_stopToken);
      groups =
          MergeRelatedSiblingGroups(std::move(groups), a_stopToken);
      ThrowIfScanCancelled(a_stopToken);
      AppendLog(localization.Format("log.merge", treeGroupCount,
                                    groups.size()));

      const auto before = std::accumulate(
          groups.begin(), groups.end(), std::size_t{},
          [](const auto total, const auto &group) {
            return total + group.items.size();
          });
      const auto conflictsBefore = std::accumulate(
          groups.begin(), groups.end(), std::size_t{},
          [](const auto total, const auto &group) {
            return total + CountSlotConflicts(group.items);
          });

      std::vector<std::vector<KitCandidate>> candidatesByGroup(groups.size());
      if (!groups.empty()) {
        std::vector<float> groupProgress(groups.size(), 0.0F);
        float groupProgressSum = 0.0F;
        std::mutex groupProgressMutex;
        std::atomic_size_t completedGroups{0};
        std::atomic_size_t nextGroupIndex{0};
        std::atomic_bool groupFailed{false};
        std::mutex groupFailureMutex;
        std::exception_ptr groupFailure;

        const auto reportGroupProgress =
            [&](const std::size_t a_groupIndex, const float a_ratio,
                std::string a_detail, const bool a_force = false) {
              ThrowIfScanCancelled(a_stopToken);
              if (groupFailed.load(std::memory_order_acquire)) {
                throw ScanCancelled{};
              }
              float combined = 0.0F;
              {
                const std::scoped_lock lock(groupProgressMutex);
                const auto previous = groupProgress[a_groupIndex];
                const auto updated =
                    (std::max)(previous, std::clamp(a_ratio, 0.0F, 1.0F));
                groupProgress[a_groupIndex] = updated;
                groupProgressSum += updated - previous;
                combined = groupProgressSum /
                           static_cast<float>(groupProgress.size());
              }
              reportProgress(
                  0.65F + combined * 0.20F,
                  localization.Format(
                      "progress.parallel_groups",
                      completedGroups.load(std::memory_order_relaxed),
                      groups.size(), a_detail),
                  a_force);
            };

        const auto groupWorkerCount =
            (std::min)(groups.size(), pluginWorkerBudget);
        const auto profilesPerGroup =
            groups.size() <= pluginWorkerBudget
                ? pluginWorkerBudget / groups.size()
                : std::size_t{1};
        const auto extraProfileWorkers =
            groups.size() <= pluginWorkerBudget
                ? pluginWorkerBudget % groups.size()
                : std::size_t{0};

        const auto groupWorker = [&]() {
          ::SetThreadPriority(::GetCurrentThread(),
                              THREAD_PRIORITY_BELOW_NORMAL);
          while (!a_stopToken.stop_requested() &&
                 !groupFailed.load(std::memory_order_acquire)) {
            const auto groupIndex =
                nextGroupIndex.fetch_add(1, std::memory_order_relaxed);
            if (groupIndex >= groups.size()) {
              return;
            }
            try {
              reportGroupProgress(
                  groupIndex, 0.0F,
                  localization.Format("progress.resolving_slots",
                                      groupIndex + 1, groups.size(),
                                      groups[groupIndex].name),
                  true);
              const auto profileWorkerBudget =
                  profilesPerGroup +
                  (groupIndex < extraProfileWorkers ? 1 : 0);
              candidatesByGroup[groupIndex] = BuildCandidates(
                  groups[groupIndex],
                  [&](const float a_ratio, std::string a_detail) {
                    reportGroupProgress(groupIndex, a_ratio,
                                        std::move(a_detail));
                  },
                  a_stopToken, profileWorkerBudget);
              completedGroups.fetch_add(1, std::memory_order_relaxed);
              reportGroupProgress(
                  groupIndex, 1.0F,
                  localization.Format("progress.slot_resolve_complete",
                                      groups[groupIndex].name),
                  true);
            } catch (...) {
              {
                const std::scoped_lock lock(groupFailureMutex);
                if (!groupFailure) {
                  groupFailure = std::current_exception();
                }
              }
              groupFailed.store(true, std::memory_order_release);
              return;
            }
          }
        };

        std::vector<std::jthread> groupWorkers;
        groupWorkers.reserve(groupWorkerCount);
        for (std::size_t index = 0; index < groupWorkerCount; ++index) {
          groupWorkers.emplace_back(
              [&](const std::stop_token) { groupWorker(); });
        }
        for (auto &worker : groupWorkers) {
          worker.join();
        }
        if (groupFailure) {
          std::rethrow_exception(groupFailure);
        }
      }

      std::size_t after = 0;
      std::size_t conflictsAfter = 0;
      for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        ThrowIfScanCancelled(a_stopToken);
        auto &candidates = candidatesByGroup[groupIndex];
        if (!candidates.empty()) {
          after += candidates.front().items.size();
          conflictsAfter += CountSlotConflicts(candidates.front().items);
          GeneratedKit generatedKit{groups[groupIndex].name,
                                    std::move(candidates),
                                    groups[groupIndex].items};
          SelectInitialCandidate(generatedKit);
          pluginGenerated.push_back(std::move(generatedKit));
        }
      }
      AppendLog(localization.Format("log.slot_resolve", before, after,
                                    conflictsBefore, conflictsAfter));
      reportProgress(1.0F,
                     localization.Format("progress.plugin_done", groups.size()),
                     true);
      generatedByPlugin[pluginIndex] = std::move(pluginGenerated);
    };

    if (pluginWorkerCount == 1) {
      for (std::size_t pluginIndex = 0; pluginIndex < a_sources.size();
           ++pluginIndex) {
        processPlugin(pluginIndex, pluginWorkerBudgets.front());
      }
    } else {
      std::atomic_size_t nextPluginIndex{0};
      std::mutex pluginFailureMutex;
      std::exception_ptr pluginFailure;
      const auto pluginWorker = [&](const std::size_t a_workerIndex) {
        ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        while (!a_stopToken.stop_requested() &&
               !pluginFailed.load(std::memory_order_acquire)) {
          const auto pluginIndex =
              nextPluginIndex.fetch_add(1, std::memory_order_relaxed);
          if (pluginIndex >= a_sources.size()) {
            return;
          }
          try {
            processPlugin(pluginIndex, pluginWorkerBudgets[a_workerIndex]);
          } catch (...) {
            {
              const std::scoped_lock lock(pluginFailureMutex);
              if (!pluginFailure) {
                pluginFailure = std::current_exception();
              }
            }
            pluginFailed.store(true, std::memory_order_release);
            return;
          }
        }
      };

      std::vector<std::jthread> pluginWorkers;
      pluginWorkers.reserve(pluginWorkerCount);
      for (std::size_t workerIndex = 0; workerIndex < pluginWorkerCount;
           ++workerIndex) {
        pluginWorkers.emplace_back([&, workerIndex](const std::stop_token) {
          pluginWorker(workerIndex);
        });
      }
      for (auto &worker : pluginWorkers) {
        worker.join();
      }
      if (pluginFailure) {
        std::rethrow_exception(pluginFailure);
      }
    }

    if (a_stopToken.stop_requested() ||
        state_.load(std::memory_order_acquire) == ScanState::Cancelling) {
      resetAfterCancellation();
      return;
    }

    std::vector<GeneratedKit> generated;
    for (auto &pluginGenerated : generatedByPlugin) {
      for (auto &kit : pluginGenerated) {
        generated.push_back(std::move(kit));
      }
    }

    const auto removedCrossPluginDuplicates =
        CollapseCrossPluginDuplicateKits(generated);
    if (removedCrossPluginDuplicates > 0) {
      logger::info("SFS Kit Generator removed {} duplicate kits copied across "
                   "ESP files",
                   removedCrossPluginDuplicates);
    }

    std::ranges::stable_sort(generated, [](const auto &left, const auto &right) {
      return LowerAscii(left.name) < LowerAscii(right.name);
    });
#if defined(SFS_PERSONAL_KIT_COMPLETION)
    const auto personalStats =
        sfs::personal_kit_completion::CompleteGeneratedKits(generated,
                                                             pluginSources_);
    AppendLog(std::format(
        "Personal Kit Completion: {} candidates, {} feet, {} underwear",
        personalStats.visitedCandidates, personalStats.addedFeet,
        personalStats.addedUnderwear));
#endif
    ThrowIfScanCancelled(a_stopToken);
    generatedKits_ = std::move(generated);
    AppendLog(localization.Format("log.done", generatedKits_.size()));
    overallProgress_.store(1.0F, std::memory_order_relaxed);
    pluginProgress_.store(1.0F, std::memory_order_relaxed);
    auto expected = ScanState::Scanning;
    if (!state_.compare_exchange_strong(expected, ScanState::Complete,
                                        std::memory_order_acq_rel)) {
      resetAfterCancellation();
    }
  } catch (const ScanCancelled &) {
    resetAfterCancellation();
  } catch (const std::exception &exception) {
    if (a_stopToken.stop_requested() ||
        state_.load(std::memory_order_acquire) == ScanState::Cancelling) {
      resetAfterCancellation();
      return;
    }
    AppendLog(Localization::Get().Format("log.error", exception.what()));
    state_.store(ScanState::Failed, std::memory_order_release);
  } catch (...) {
    if (a_stopToken.stop_requested() ||
        state_.load(std::memory_order_acquire) == ScanState::Cancelling) {
      resetAfterCancellation();
      return;
    }
    AppendLog(Localization::Get().Text("log.error_unknown"));
    state_.store(ScanState::Failed, std::memory_order_release);
  }
}

bool Generator::MergeGeneratedKits(
    const std::vector<std::size_t> &a_indices,
                                   std::string &a_error) {
  a_error.clear();
  if (state_.load(std::memory_order_acquire) != ScanState::Complete) {
    a_error = Localization::Get().Text("error.scan_incomplete");
    return false;
  }
  std::set<std::size_t> selectedIndices;
  for (const auto index : a_indices) {
    if (index >= generatedKits_.size()) {
      a_error = Localization::Get().Text("error.merge_selection");
      return false;
    }
    selectedIndices.insert(index);
  }
  if (selectedIndices.size() < 2) {
    a_error = Localization::Get().Text("error.merge_selection");
    return false;
  }

  const auto firstIndex = *selectedIndices.begin();
  const auto collectPluginNames = [](const GeneratedKit &a_kit) {
    std::unordered_set<std::string> pluginNames;
    const auto appendItem = [&](const ArmorRecord &a_item) {
      if (!a_item.pluginName.empty()) {
        pluginNames.insert(LowerAscii(a_item.pluginName));
      }
    };
    if (!a_kit.sourceItems.empty()) {
      for (const auto &item : a_kit.sourceItems) {
        appendItem(item);
      }
    } else {
      for (const auto &candidate : a_kit.candidates) {
        for (const auto &item : candidate.items) {
          appendItem(item);
        }
      }
    }
    return pluginNames;
  };
  std::optional<std::string> selectedPluginName;
  for (const auto index : selectedIndices) {
    const auto pluginNames = collectPluginNames(generatedKits_[index]);
    if (pluginNames.size() != 1) {
      a_error = Localization::Get().Text("error.merge_cross_plugin");
      return false;
    }
    const auto &pluginName = *pluginNames.begin();
    if (selectedPluginName.has_value() && *selectedPluginName != pluginName) {
      a_error = Localization::Get().Text("error.merge_cross_plugin");
      return false;
    }
    selectedPluginName = pluginName;
  }

  std::vector<ArmorRecord> mergedItems;
  std::unordered_set<std::uint32_t> seenFormIDs;
  const auto appendKitItems = [&](const GeneratedKit &a_kit) {
    const auto appendItem = [&](const ArmorRecord &a_item) {
      if (seenFormIDs.insert(a_item.runtimeFormID).second) {
        mergedItems.push_back(a_item);
      }
    };
    if (!a_kit.sourceItems.empty()) {
      for (const auto &item : a_kit.sourceItems) {
        appendItem(item);
      }
      return;
    }
    for (const auto &candidate : a_kit.candidates) {
      for (const auto &item : candidate.items) {
        appendItem(item);
      }
    }
  };
  for (const auto index : selectedIndices) {
    appendKitItems(generatedKits_[index]);
  }
  if (mergedItems.empty()) {
    a_error = Localization::Get().Text("error.merge_no_candidates");
    return false;
  }

  try {
    std::ranges::stable_sort(mergedItems, {}, &ArmorRecord::sourceOrder);
    const auto rawGroupName = BuildGroupName(mergedItems);
    const auto normalizedGroupName = NormalizeKitName(rawGroupName);
    auto representativeName =
        ChooseKitDisplayName(rawGroupName, normalizedGroupName);
    if (representativeName.empty()) {
      representativeName = rawGroupName;
    }
    auto mergedGroup = OutfitGroup{representativeName, mergedItems};
    const auto workerPlan = DetermineScanWorkerPlan(
        (std::numeric_limits<std::size_t>::max)());
    auto candidates = BuildCandidates(mergedGroup, {}, std::stop_token{},
                                      workerPlan.workerCount);
    if (candidates.empty()) {
      a_error = Localization::Get().Text("error.merge_no_candidates");
      return false;
    }

    auto mergedKit =
        GeneratedKit{std::move(mergedGroup.name), std::move(candidates),
                     std::move(mergedItems)};
#if defined(SFS_PERSONAL_KIT_COMPLETION)
    const auto personalStats =
        sfs::personal_kit_completion::CompleteGeneratedKit(mergedKit,
                                                            pluginSources_);
    AppendLog(std::format(
        "Personal Kit Completion after kit merge: {} candidates, {} feet, "
        "{} underwear",
        personalStats.visitedCandidates, personalStats.addedFeet,
        personalStats.addedUnderwear));
#endif
    SelectInitialCandidate(mergedKit);
    generatedKits_[firstIndex] = std::move(mergedKit);
    for (auto index = selectedIndices.rbegin();
         index != selectedIndices.rend(); ++index) {
      if (*index == firstIndex) {
        continue;
      }
      generatedKits_.erase(generatedKits_.begin() +
                           static_cast<std::ptrdiff_t>(*index));
    }
    std::ranges::stable_sort(generatedKits_, [](const auto &left,
                                                const auto &right) {
      return LowerAscii(left.name) < LowerAscii(right.name);
    });
    return true;
  } catch (const std::exception &exception) {
    a_error = Localization::Get().Format("error.merge_failed",
                                         exception.what());
    return false;
  }
}

bool Generator::MergeKitCandidates(
    const std::size_t a_kitIndex,
    const std::vector<std::size_t> &a_candidateIndices,
    std::string &a_error) {
  a_error.clear();
  if (state_.load(std::memory_order_acquire) != ScanState::Complete) {
    a_error = Localization::Get().Text("error.scan_incomplete");
    return false;
  }
  if (a_kitIndex >= generatedKits_.size()) {
    a_error = Localization::Get().Text("error.merge_candidate_selection");
    return false;
  }

  auto &kit = generatedKits_[a_kitIndex];
  std::set<std::size_t> selectedIndices;
  for (const auto index : a_candidateIndices) {
    if (index >= kit.candidates.size()) {
      a_error = Localization::Get().Text("error.merge_candidate_selection");
      return false;
    }
    selectedIndices.insert(index);
  }
  if (selectedIndices.size() < 2) {
    a_error = Localization::Get().Text("error.merge_candidate_selection");
    return false;
  }

  const auto firstIndex = *selectedIndices.begin();
  const auto primaryIndex = firstIndex;
  const auto previousSelectedCandidate = kit.selectedCandidate;
  const auto previousDraftCandidate = kit.draftCandidate;

  KitCandidate merged;
  const auto &primary = kit.candidates[primaryIndex];
  merged.score = primary.score;
  std::vector<std::string> mergedProfileTokens;
  const auto appendProfile = [&](const std::string &a_profile) {
    for (auto token : Tokenize(a_profile)) {
      token = NormalizeKey(token);
      if (!token.empty() && token != "base") {
        mergedProfileTokens.push_back(std::move(token));
      }
    }
  };
  appendProfile(primary.profile);
  std::unordered_set<std::uint32_t> seenFormIDs;
  std::uint32_t occupiedSlots = 0;
  const auto appendItems = [&](const KitCandidate &a_candidate) {
    for (const auto &item : a_candidate.items) {
      if (!seenFormIDs.insert(item.runtimeFormID).second ||
          item.sourceSlotMask == 0 ||
          (occupiedSlots & item.sourceSlotMask) != 0) {
        continue;
      }
      occupiedSlots |= item.sourceSlotMask;
      merged.items.push_back(item);
    }
  };
#if defined(SFS_PERSONAL_KIT_COMPLETION)
  const auto appendMergeItems = [&](const KitCandidate &a_candidate) {
    auto prepared = a_candidate;
    sfs::personal_kit_completion::RemoveAutomaticCompletionItems(kit,
                                                                  prepared);
    appendItems(prepared);
  };
#else
  const auto appendMergeItems = [&](const KitCandidate &a_candidate) {
    appendItems(a_candidate);
  };
#endif
  appendMergeItems(primary);
  for (const auto index : selectedIndices) {
    if (index == primaryIndex) {
      continue;
    }
    const auto &candidate = kit.candidates[index];
    appendProfile(candidate.profile);
    merged.score += candidate.score;
    appendMergeItems(candidate);
  }
  if (merged.items.empty()) {
    a_error = Localization::Get().Text("error.merge_candidate_empty");
    return false;
  }
  std::ranges::sort(merged.items, {}, &ArmorRecord::sourceOrder);
  merged.profile = CanonicalProfile(std::move(mergedProfileTokens));
  if (merged.profile.empty()) {
    merged.profile = "base";
  }
#if defined(SFS_PERSONAL_KIT_COMPLETION)
  const auto personalStats =
      sfs::personal_kit_completion::CompleteGeneratedCandidate(
          kit, merged, pluginSources_);
  AppendLog(std::format(
      "Personal Kit Completion after candidate merge: {} feet, {} underwear",
      personalStats.addedFeet, personalStats.addedUnderwear));
#endif

  kit.candidates[firstIndex] = std::move(merged);
  for (auto index = selectedIndices.rbegin();
       index != selectedIndices.rend(); ++index) {
    if (*index == firstIndex) {
      continue;
    }
    kit.candidates.erase(kit.candidates.begin() +
                         static_cast<std::ptrdiff_t>(*index));
  }
  const auto remapCandidateIndex = [&](const std::size_t a_previousIndex) {
    if (selectedIndices.contains(a_previousIndex)) {
      return firstIndex;
    }
    const auto removedBefore = static_cast<std::size_t>(std::ranges::count_if(
        selectedIndices, [&](const auto index) {
          return index != firstIndex && index < a_previousIndex;
        }));
    return a_previousIndex - removedBefore;
  };
  kit.selectedCandidate = remapCandidateIndex(previousSelectedCandidate);
  kit.draftCandidate = remapCandidateIndex(previousDraftCandidate);
  return true;
}

std::size_t Generator::DeleteGeneratedKits(
    const std::vector<std::size_t> &a_indices) {
  if (state_.load(std::memory_order_acquire) != ScanState::Complete ||
      a_indices.empty()) {
    return 0;
  }
  return EraseGeneratedKitsAtIndices(generatedKits_, a_indices);
}

std::size_t Generator::CreateKitFiles(std::string &a_error) {
  a_error.clear();
  if (state_.load(std::memory_order_acquire) != ScanState::Complete) {
    a_error = Localization::Get().Text("error.scan_incomplete");
    return 0;
  }
  const auto outputRoot = std::filesystem::path("Data") / "Interface" /
                          "SkyrimFittingSystem" / "user" / "kits";
  std::error_code error;
  std::filesystem::create_directories(outputRoot, error);
  if (error) {
    a_error = Localization::Get().Format("error.create_folder",
                                         error.message());
    return 0;
  }

  std::size_t written = 0;
  try {
    for (const auto &kit : generatedKits_) {
      if (kit.candidates.empty()) {
        continue;
      }
      const auto selected =
          (std::min)(kit.selectedCandidate, kit.candidates.size() - 1);
      const auto &candidate = kit.candidates[selected];
#if defined(SFS_PERSONAL_KIT_COMPLETION)
      const auto displayName =
          sfs::personal_kit_completion::BuildPersonalOutputName(kit, candidate);
#else
      const auto nsfw = kit.IsSelectedCandidateNsfw();
      const auto displayName = includeSafetyPrefix_
                                   ? std::format("[{}] {}", nsfw ? "NSFW" : "SFW",
                                                 kit.name)
                                   : kit.name;
#endif
      const auto outputPath = UniqueOutputPath(outputRoot, displayName);
      std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);
      if (!stream.is_open()) {
        throw std::runtime_error(
            Localization::Get().Format("error.open_output",
                                       outputPath.string()));
      }
      stream << BuildKitJson(displayName, candidate).dump(
          2, ' ', false, nlohmann::json::error_handler_t::replace);
      stream << '\n';
      if (!stream.good()) {
        throw std::runtime_error(
            Localization::Get().Format("error.write_output",
                                       outputPath.string()));
      }
      ++written;
    }
  } catch (const std::exception &exception) {
    a_error = exception.what();
  }
  return written;
}
} // namespace sfs::kit_generator
