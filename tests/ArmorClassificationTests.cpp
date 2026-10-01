// Production classifier, keyword ownership, token projection and environment
// code run against host form fakes. This does not execute Skyrim or SOS Papyrus.
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "native/GenitalCompatibilityRules.h"

namespace logger { template <class... T> void info(T&&...) {} }
namespace RE {
using FormID = std::uint32_t;
struct TESForm {
  FormID id{};
  std::string editorID;
  virtual ~TESForm() = default;
  FormID GetFormID() const { return id; }
  inline static std::unordered_map<std::string, TESForm*> byEditorID;
  inline static std::unordered_map<FormID, TESForm*> byID;
  inline static unsigned editorLookups{};
  template <class T> static T* LookupByEditorID(const char* key) {
    ++editorLookups;
    const auto it = byEditorID.find(key);
    return it == byEditorID.end() ? nullptr : dynamic_cast<T*>(it->second);
  }
  template <class T = TESForm> static T* LookupByID(FormID key) {
    const auto it = byID.find(key);
    return it == byID.end() ? nullptr : dynamic_cast<T*>(it->second);
  }
};
struct BGSKeyword : TESForm {};
struct TESQuest : TESForm {};
struct TESObjectARMO : TESForm {
  std::string name;
  std::uint32_t slots{};
  bool internal{};
  unsigned adds{}, removes{};
  std::vector<BGSKeyword*> keywords;
  const auto& GetKeywords() const { return keywords; }
  bool HasKeyword(const BGSKeyword* key) const {
    return std::ranges::find(keywords, key) != keywords.end();
  }
  void AddKeyword(BGSKeyword* key) { ++adds; keywords.push_back(key); }
  void RemoveKeyword(BGSKeyword* key) { ++removes; std::erase(keywords, key); }
};
struct BGSBipedObjectForm {
  enum class BipedObjectSlot : std::uint32_t {
    kBody = 1u << 2, kModPelvisPrimary = 1u << 19
  };
};
struct TESDataHandler {
  inline static bool available = true;
  inline static TESForm* sosAPI{};
  inline static unsigned lookups{};
  static TESDataHandler* GetSingleton() {
    static TESDataHandler value;
    return available ? &value : nullptr;
  }
  TESForm* LookupForm(FormID id, const char* plugin) {
    ++lookups;
    return id == 0x1EDA4 && std::string_view(plugin) == "Schlongs of Skyrim.esp"
        ? sosAPI : nullptr;
  }
};
}
namespace sfs::armor {
std::string GetEditorID(const RE::TESForm* form) { return form ? form->editorID : ""; }
std::string GetDisplayName(const RE::TESObjectARMO* armor) { return armor->name; }
std::uint32_t GetArmorDisplaySlotMask(const RE::TESObjectARMO* armor) { return armor->slots; }
bool IsSosTngInternalArmor(const RE::TESObjectARMO* armor) { return armor->internal; }
bool IsTngGenitalCoverArmor(const RE::TESObjectARMO* armor) { return armor->internal; }
}
#include "GenitalEnvironment.production.inc"
namespace sfs::native {
#include "ClassificationTypes.production.inc"
}
namespace {
using sfs::native::ArmorGenitalKeywordDisposition;
using sfs::native::ArmorGenitalKeywordOverride;
std::mutex g_sosUserArmorMutex, g_runtimeKeywordMutex;
std::unordered_set<RE::FormID> g_sosUserRevealingArmors, g_sosUserConcealingArmors;
std::unordered_set<std::uint64_t> g_sfsOwnedRuntimeKeywords;
#include "Classification.production.inc"
}
namespace sfs::native {
#include "ClassificationPublic.production.inc"
}
namespace {
#include "ClassificationTokens.production.inc"
int errors = 0;
unsigned checks = 0;
void Check(bool pass, std::string_view message) {
  ++checks;
  if (!pass) { ++errors; std::cerr << "FAIL: " << message << '\n'; }
}
void Register(RE::TESForm& form, RE::FormID id, std::string_view editorID) {
  form.id = id; form.editorID = editorID;
  RE::TESForm::byEditorID[form.editorID] = &form;
  RE::TESForm::byID[id] = &form;
}
bool Contains(const std::vector<RE::BGSKeyword*>& keys, const RE::BGSKeyword* key) {
  return std::ranges::find(keys, key) != keys.end();
}
}
int main() {
  using namespace sfs::native;
  using D = ArmorGenitalKeywordDisposition;
  constexpr auto body = 1u << 2, pelvis = 1u << 19;
  std::array<RE::BGSKeyword, 6> runtime;
  constexpr std::array names{"SOS_Revealing", "SOS_Concealing", "SOS_Underwear",
                             "TNG_Revealing", "TNG_Covering", "TNG_Underwear"};
  for (unsigned i=0; i<runtime.size(); ++i) Register(runtime[i], 10+i, names[i]);
  RE::TESQuest sos;
  Register(sos, 100, "SOS_Misc");
  RE::TESObjectARMO tng;
  Register(tng, 101, "TNG_GenitalCover"); tng.internal = true;
  RE::TESObjectARMO armor;
  Register(armor, 200, "SteelArmor"); armor.name="Steel Armor"; armor.slots=body;
  RE::BGSKeyword ocf;
  Register(ocf, 300, "OCF_BodyTypeUnderwearF_Top");
  RE::TESDataHandler::sosAPI = &sos;
  genital_compatibility::InitializeEnvironment();

  // The reported case: category metadata is not evidence of exposed geometry.
  armor.keywords = {&ocf};
  Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kConceal,
        "OCF Top must not reveal ordinary slot-32 armor");
  SynchronizeArmorClassificationKeywords(&armor);
  Check(!armor.HasKeyword(&runtime[0]) && !armor.HasKeyword(&runtime[3]),
        "OCF must not inject SOS/TNG revealing keywords");
  const auto ordinaryToken = BuildEffectiveSourceKeywords(&armor);
  Check(!Contains(ordinaryToken, &runtime[0]) && !Contains(ordinaryToken, &runtime[3]) &&
        Contains(ordinaryToken, &runtime[1]) && Contains(ordinaryToken, &ocf),
        "virtual token shares corrected decision and preserves unrelated keywords");

  for (const auto keyword : {"OCF_BodyTypeUnderwearF_Top", "OCF_BodyTypeUpper",
         "OtherCategory_BikiniTop", "OtherCategory_Shirt", "OtherCategory_Bra",
         "ArmorMaterialSteel", "ClothingBody", "VendorItemArmor"}) {
    ocf.editorID = keyword;
    Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kConceal, keyword);
  }
  // An unrelated negative category must not suppress a genuine named upper.
  armor.name = "Bikini Top"; ocf.editorID = "Category_Bottoms";
  Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kReveal,
        "category keywords must not veto an actual upper garment either");
  armor.keywords.clear();

  for (const auto ambiguous : {"SteelArmor_Top", "SteelArmor_Upper", "SteelArmor_Chest",
         "SteelArmor_Crop", "SteelArmor_Tube", "SteelArmor_Breast", "SteelArmor_Halter",
         "UppercutArmor", "Chestplate", "ShirtlessArmor", "Vestments", "BikiniTopology"}) {
    armor.editorID = ambiguous; armor.name = "Steel Armor";
    Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kConceal, ambiguous);
    armor.name = ambiguous; armor.editorID = "SteelArmor";
    Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kConceal, ambiguous);
  }
  armor.editorID = "Example_Bikini"; armor.name = "Top Quality Armor";
  Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kConceal,
        "do not synthesize bikini top across EditorID/display-name boundary");

  for (const auto upper : {"Bikini Top", "BikiniUpper", "Dress Top", "Upper Swimsuit",
         "Crop Top", "Tube Top", "Tank Top", "Halter Top", "Croptop", "Breast Wrap",
         "Chest Wrap", "Linen Blouse", "Lace Bra", "Leather Vest", "White Shirt",
         "Silk Camisole", "Bodice", "Bolero", "Bralette", "Brassiere", "Bustier",
         "Hoodie", "Jacket", "Sweater", "Bikini Tops", "TankTops", "Lace Bras",
         "Cotton Shirts", "Leather Vests", "비키니 상의", "브라", "比基尼上衣", "胸罩"}) {
    armor.editorID = "ExampleArmor"; armor.name = upper;
    Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kReveal, upper);
    armor.editorID = upper; armor.name = "Example Armor";
    Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kReveal, upper);
  }
  for (const auto covered : {"Full Dress", "Steel Cuirass", "Torso", "Corset", "Bodysuit",
         "Catsuit", "Jumpsuit", "Leotard", "Unitard", "Shirt and Pants", "Vest and Skirt",
         "원피스", "连衣裙"}) {
    armor.editorID = "ExampleArmor"; armor.name = covered;
    Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kConceal, covered);
  }
  armor.editorID = "ExampleArmor"; armor.name = "Bikini Top"; armor.slots = 1u << 3;
  Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kInherit,
        "upper classification remains limited to body slot 32");
  armor.slots = pelvis; armor.name = "Leather Belt";
  Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kInherit,
        "unclassified pelvis accessory preserves ESP/KID rules");
  for (const auto slots : {pelvis, body|pelvis}) {
    armor.slots = slots; armor.name = "Bikini Bottom";
    const auto lower = GetArmorGenitalKeywordOverride(&armor);
    Check(lower.disposition == D::kConceal && lower.underwear && lower.materializeCoveringKeywords,
          "slot-49 lower garments still conceal, including combined 32+49");
  }
  armor.slots = pelvis; armor.name = "Example Armor"; ocf.editorID = "Category_Panties";
  armor.keywords = {&ocf};
  Check(GetArmorGenitalKeywordOverride(&armor).underwear,
        "existing slot-49 lower keyword classification is not changed by upper fix");
  armor.keywords.clear(); armor.slots = body; armor.name = "Steel Armor";

  g_sosUserRevealingArmors.insert(armor.id);
  Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kReveal, "SOS MCM reveal wins");
  auto manualToken = BuildEffectiveSourceKeywords(&armor);
  Check(armor.HasKeyword(&runtime[0]) && Contains(manualToken, &runtime[0]),
        "manual reveal reaches both physical and virtual-token keywords");
  g_sosUserConcealingArmors.insert(armor.id);
  Check(GetArmorGenitalKeywordOverride(&armor).disposition == D::kConceal, "SOS MCM conceal wins conflicts");
  manualToken = BuildEffectiveSourceKeywords(&armor);
  Check(!armor.HasKeyword(&runtime[0]) && armor.HasKeyword(&runtime[1]) &&
        !Contains(manualToken, &runtime[0]) && Contains(manualToken, &runtime[1]),
        "manual conceal replaces only SFS-owned revealing state");
  g_sosUserRevealingArmors.clear(); g_sosUserConcealingArmors.clear();
  (void)BuildEffectiveSourceKeywords(&armor);
  Check(g_sfsOwnedRuntimeKeywords.empty(), "clearing manual policy releases redundant owned keywords");
  armor.internal = true;
  Check(!GetArmorGenitalKeywordOverride(&armor).IsActive(), "internal SOS/TNG armor excluded");
  armor.internal = false;
  Check(!GetArmorGenitalKeywordOverride(nullptr).IsActive() && BuildEffectiveSourceKeywords(nullptr).empty(),
        "null boundaries remain safe");

  // Four environments, repeated reveal/cover/lower transitions: ownership
  // drains after each transition and idle queries make no new keyword writes.
  for (const bool hasSos : {false, true}) for (const bool hasTng : {false, true}) {
    RE::TESDataHandler::sosAPI = hasSos ? &sos : nullptr;
    RE::TESForm::byEditorID.erase("SOS_Misc");
    RE::TESForm::byEditorID.erase("TNG_GenitalCover");
    if (hasTng) RE::TESForm::byEditorID["TNG_GenitalCover"] = &tng;
    genital_compatibility::InitializeEnvironment();
    const auto env = genital_compatibility::GetEnvironment();
    Check(env.sosInstalled == hasSos && env.tngInstalled == hasTng,
          "SOS and TNG environment detection remains independent");
    Check(genital_compatibility::GetSosApiForm() == (hasSos ? &sos : nullptr),
          "resolver API availability exactly follows detected SOS environment");
    armor.keywords = {&ocf}; ocf.editorID = "OCF_BodyTypeUnderwearF_Top";
    g_sfsOwnedRuntimeKeywords.clear();
    for (unsigned round = 0; round < 128; ++round) {
      armor.slots = body; armor.name = "Lace Bra";
      auto token = BuildEffectiveSourceKeywords(&armor);
      Check(armor.HasKeyword(&runtime[0]) == hasSos && armor.HasKeyword(&runtime[3]) == hasTng &&
            Contains(token, &runtime[0]) == hasSos && Contains(token, &runtime[3]) == hasTng,
            "genuine upper retains runtime and virtual-token support");
      const auto adds = armor.adds, removes = armor.removes;
      for (unsigned idle=0; idle<8; ++idle) (void)BuildEffectiveSourceKeywords(&armor);
      Check(armor.adds == adds && armor.removes == removes, "idle queries never repeat keyword mutations");
      armor.name = "Steel Armor";
      token = BuildEffectiveSourceKeywords(&armor);
      Check(!armor.HasKeyword(&runtime[0]) && !armor.HasKeyword(&runtime[3]) &&
            !Contains(token, &runtime[0]) && !Contains(token, &runtime[3]) &&
            g_sfsOwnedRuntimeKeywords.empty() && armor.HasKeyword(&ocf),
            "obsolete SFS revealing removed without residual ownership or OCF damage");
      armor.slots = pelvis; armor.name = "Bikini Bottom";
      token = BuildEffectiveSourceKeywords(&armor);
      Check(armor.HasKeyword(&runtime[1]) == hasSos && armor.HasKeyword(&runtime[2]) == hasSos &&
            armor.HasKeyword(&runtime[4]) == hasTng && armor.HasKeyword(&runtime[5]) == hasTng,
            "lower covering and underwear materialization survives");
      armor.slots = body; armor.name = "Steel Armor";
      (void)BuildEffectiveSourceKeywords(&armor);
      Check(g_sfsOwnedRuntimeKeywords.empty() && armor.keywords.size() == 1,
            "transition cleanup leaves only original classification keyword");
    }
    // Source ESP/KID keywords are never deleted/adopted by SFS. Contextual
    // virtual tokens still apply the established higher-priority 32/49 policy.
    armor.keywords = {&ocf, &runtime[0], &runtime[3]};
    const auto token = BuildEffectiveSourceKeywords(&armor);
    Check(armor.HasKeyword(&runtime[0]) && armor.HasKeyword(&runtime[3]) &&
          g_sfsOwnedRuntimeKeywords.empty() && !Contains(token, &runtime[0]) && !Contains(token, &runtime[3]),
          "preserve other mods' physical keywords, apply existing logical overlay only to token view");
  }
  // Missing standard plugin still supports SOS_Misc fallback (e.g. renamed ESP).
  RE::TESDataHandler::sosAPI = nullptr;
  RE::TESForm::byEditorID["SOS_Misc"] = &sos;
  RE::TESDataHandler::available = false;
  genital_compatibility::InitializeEnvironment();
  Check(genital_compatibility::IsSosInstalled(), "SOS_Misc fallback detection retained");
  Check(genital_compatibility::GetSosApiForm() == &sos, "resolver uses detected fallback form");
  const auto editorLookups = RE::TESForm::editorLookups;
  const auto pluginLookups = RE::TESDataHandler::lookups;
  RE::TESQuest impostor;
  Register(impostor, 102, "SOS_Misc");
  RE::TESDataHandler::sosAPI = &impostor;
  for (unsigned i=0; i<128; ++i)
    Check(genital_compatibility::GetSosApiForm() == &sos, "resolver retains detected API identity");
  Check(RE::TESForm::editorLookups == editorLookups && RE::TESDataHandler::lookups == pluginLookups,
        "API requests do not repeat plugin/EditorID detection");
  RE::TESForm::byID.erase(sos.id);
  Check(genital_compatibility::GetSosApiForm() == nullptr, "unresolvable API form never returns a retained pointer");
  RE::TESForm::byID[sos.id] = &sos;
  RE::TESDataHandler::available = true;
  genital_compatibility::InitializeEnvironment();
  Check(genital_compatibility::GetSosApiForm() == &impostor, "explicit environment initialization refreshes identity");
  RE::TESDataHandler::sosAPI = nullptr;
  RE::TESForm::byEditorID.erase("SOS_Misc");
  genital_compatibility::InitializeEnvironment();
  Check(!genital_compatibility::IsSosInstalled() && genital_compatibility::GetSosApiForm() == nullptr,
        "absent SOS clears previous detected identity");

  armor.slots = 1u << 3; armor.keywords = {&ocf, &runtime[0]};
  Check(BuildEffectiveSourceKeywords(&armor) == armor.keywords,
        "outside 32/49 the source keyword view remains inherited even without SOS/TNG");
  RE::TESDataHandler::sosAPI = &sos;
  genital_compatibility::InitializeEnvironment();
  for (auto& key : runtime) RE::TESForm::byEditorID.erase(key.editorID);
  armor.slots = body; armor.name = "Lace Bra"; armor.keywords = {&ocf};
  Check(BuildEffectiveSourceKeywords(&armor) == armor.keywords && g_sfsOwnedRuntimeKeywords.empty(),
        "missing compatibility keyword forms require no synthetic allocation");

  std::cout << "ArmorClassificationTests: " << (errors ? "FAILED" : "PASSED") << " (" << checks
            << " checks; 4 environments x 128 transitions; production bodies, host form fakes)\n";
  return errors ? 1 : 0;
}
