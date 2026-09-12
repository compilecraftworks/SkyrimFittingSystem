#include "native/ArmorSkinning.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "TngGenitalCoverRules.h"
#include "conditions/Status.h"
#include "native/ActiveAppearanceSlotLookup.h"
#include "native/ArmorRefreshRules.h"
#include "native/DaveIntegration.h"
#include "native/ExternalEquipmentTransactions.h"
#include "native/FittingDye.h"
#include "native/FittingSlotState.h"
#include "native/FinalRenderedOutfitRules.h"
#include "native/GenitalCompatibility.h"
#include "native/GenitalArmorResolver.h"
#include "native/HelmetToggle2Integration.h"
#include "native/IedVisitorRoutingRules.h"
#include "native/RaceMenuBodyMorph.h"
#include "native/RegisteredAppearanceMorphRules.h"
#include "features/devious_devices/DeviousDevicesIntegration.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include "ui/Menu.h"
#include "workbench/AppearanceSlotProtection.h"
#include "workbench/AutomaticEquipmentVisibility.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RE {
class ActorWeightModel;
InventoryChanges::IItemChangeVisitor::~IItemChangeVisitor() = default;
} // namespace RE

namespace {
[[nodiscard]] std::uint32_t
GetProtectedActualSlotMask(const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return 0;
  }
  const auto armorSlotMask =
      static_cast<std::uint32_t>(sfs::armor::GetArmorDisplaySlotMask(a_armor));
  return armorSlotMask &
         static_cast<std::uint32_t>(
             sfs::workbench::GetEffectiveAppearanceProtectedSlotMask());
}

struct DisplaySet {
  bool active{false};
  bool trackRegisteredAppearanceMorphNodes{false};
  bool applyInitialNativeMorphs{true};
  std::vector<const RE::TESObjectARMO *> armors;
  std::unordered_set<RE::FormID> hiddenArmorFormIDs;
  std::unordered_set<RE::FormID> forceVisibleArmorFormIDs;
  std::uint32_t slotMask{0};
  std::uint32_t hiddenSlotMask{0};
  bool genitalArmorEquipped{false};
  bool genitalCompatibilityAvailable{false};
  bool genitalCorrectionActive{false};
  bool concealGenitals{false};

  [[nodiscard]] bool Contains(const RE::TESObjectARMO *a_armor) const {
    return std::ranges::find(armors, a_armor) != armors.end();
  }
};

struct DavFallbackRefreshSignature {
  bool active{false};
  std::uint32_t slotMask{0};
  std::uint32_t hiddenSlotMask{0};
  std::uint32_t releasedActualHairSlotMask{0};
  std::vector<RE::FormID> displayArmorFormIDs;
  std::vector<RE::FormID> hiddenArmorFormIDs;
  std::vector<RE::FormID> hiddenWornArmorFormIDs;

  [[nodiscard]] bool
  operator==(const DavFallbackRefreshSignature &) const = default;
};

struct EmptyEquipmentDisplaySignature {
  bool active{false};
  std::uint32_t slotMask{0};
  std::vector<RE::FormID> displayArmorFormIDs;

  [[nodiscard]] bool HasDisplayArmors() const {
    return active && !displayArmorFormIDs.empty();
  }

  [[nodiscard]] bool
  operator==(const EmptyEquipmentDisplaySignature &) const = default;
};

std::atomic_uint64_t g_armorRefreshGeneration{0};
thread_local std::uint32_t g_buildDisplaySetDepth{0};

struct DisplaySetBuildScope {
  DisplaySetBuildScope() { ++g_buildDisplaySetDepth; }
  ~DisplaySetBuildScope() { --g_buildDisplaySetDepth; }

  DisplaySetBuildScope(const DisplaySetBuildScope &) = delete;
  DisplaySetBuildScope &operator=(const DisplaySetBuildScope &) = delete;
};

std::mutex g_queuedArmorRefreshMutex;
std::unordered_map<RE::FormID, std::uint64_t> g_queuedArmorRefreshGeneration;
std::atomic<std::uintptr_t> g_iedVisitWornItemsChainTarget{0};
std::atomic<std::uintptr_t> g_passthroughVisitWornItemsChainTarget{0};
std::mutex g_queuedIedEvaluationMutex;
std::unordered_map<RE::FormID, std::uint64_t> g_queuedIedEvaluations;
std::uint64_t g_nextIedEvaluationToken{0};
std::atomic_bool g_iedEvaluateDispatchWarningLogged{false};
std::mutex g_davFallbackRefreshSignatureMutex;
std::unordered_map<RE::FormID, DavFallbackRefreshSignature>
    g_davFallbackRefreshSignatures;
std::mutex g_emptyEquipmentDisplaySignatureMutex;
std::unordered_map<RE::FormID, EmptyEquipmentDisplaySignature>
    g_emptyEquipmentDisplaySignatures;

enum class SOSUserArmorPolicy : std::uint8_t { kNeutral, kReveal, kConceal };

using ArmorGenitalKeywordDisposition =
    sfs::native::ArmorGenitalKeywordDisposition;
using ArmorGenitalKeywordOverride = sfs::native::ArmorGenitalKeywordOverride;

std::mutex g_sosUserArmorMutex;
std::unordered_set<RE::FormID> g_sosUserRevealingArmors;
std::unordered_set<RE::FormID> g_sosUserConcealingArmors;
std::unordered_set<RE::FormID> g_pendingSOSUserRevealingArmors;
std::unordered_set<RE::FormID> g_pendingSOSUserConcealingArmors;
bool g_sosUserArmorSyncActive{false};
std::mutex g_runtimeKeywordMutex;
std::unordered_set<std::uint64_t> g_sfsOwnedRuntimeKeywords;
constexpr std::uint32_t kArmorClassificationMigrationSerializationType = 'AKMG';
constexpr std::uint32_t kArmorClassificationMigrationSerializationVersion = 1;
constexpr std::uint32_t kCurrentArmorClassificationMigrationVersion = 2;
std::mutex g_armorClassificationMigrationMutex;
std::unordered_set<std::uint64_t> g_baselineArmorClassificationRuntimeKeywords;
bool g_armorClassificationKeywordBaselineCaptured{false};
std::atomic_uint32_t g_completedArmorClassificationMigrationVersion{
    kCurrentArmorClassificationMigrationVersion};
std::atomic_bool g_armorClassificationMigrationPending{false};
std::atomic_uint64_t g_armorClassificationMigrationScheduleGeneration{0};

[[nodiscard]] constexpr bool
ShouldRemoveLegacyRuntimeKeyword(const bool a_hasKeyword,
                                 const bool a_wasInBaseline,
                                 const bool a_isProtectedByUserPolicy) {
  return a_hasKeyword && !a_wasInBaseline && !a_isProtectedByUserPolicy;
}

static_assert(ShouldRemoveLegacyRuntimeKeyword(true, false, false));
static_assert(!ShouldRemoveLegacyRuntimeKeyword(true, true, false));
static_assert(!ShouldRemoveLegacyRuntimeKeyword(true, false, true));
static_assert(!ShouldRemoveLegacyRuntimeKeyword(false, false, false));

[[nodiscard]] std::uint64_t
QueueActorArmorRefreshGeneration(const RE::FormID a_actorFormID) {
  std::lock_guard lock(g_queuedArmorRefreshMutex);
  return ++g_queuedArmorRefreshGeneration[a_actorFormID];
}

[[nodiscard]] bool
IsLatestActorArmorRefreshGeneration(const RE::FormID a_actorFormID,
                                    const std::uint64_t a_generation) {
  std::lock_guard lock(g_queuedArmorRefreshMutex);
  const auto generationIt = g_queuedArmorRefreshGeneration.find(a_actorFormID);
  return generationIt != g_queuedArmorRefreshGeneration.end() &&
         generationIt->second == a_generation;
}

void ClearQueuedActorArmorRefreshes() {
  std::lock_guard lock(g_queuedArmorRefreshMutex);
  g_queuedArmorRefreshGeneration.clear();
}

void ClearQueuedIedEvaluations() {
  std::lock_guard lock(g_queuedIedEvaluationMutex);
  g_queuedIedEvaluations.clear();
}

[[nodiscard]] bool IsActorRefreshable(RE::Actor *a_actor) {
  return a_actor != nullptr && !a_actor->IsDeleted() &&
         !a_actor->IsDisabled() && a_actor->Is3DLoaded();
}

class IedEvaluateCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
  void operator()(RE::BSScript::Variable) override {}

  void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object> &) override {}
};

void QueueIedEvaluate(RE::Actor *a_actor) {
  if (!a_actor) {
    return;
  }

  const auto actorFormID = a_actor->GetFormID();
  auto *taskInterface = SKSE::GetTaskInterface();
  if (actorFormID == 0 || !taskInterface) {
    return;
  }

  std::uint64_t token = 0;
  {
    std::lock_guard lock(g_queuedIedEvaluationMutex);
    if (g_queuedIedEvaluations.contains(actorFormID)) {
      return;
    }
    token = ++g_nextIedEvaluationToken;
    g_queuedIedEvaluations.emplace(actorFormID, token);
  }

  // Run after the engine's original visitor completes. IED is then refreshed
  // through its public actor-level API rather than being re-entered with the
  // SFS filtering visitor.
  taskInterface->AddTask([actorFormID, token]() {
    {
      std::lock_guard lock(g_queuedIedEvaluationMutex);
      const auto queuedIt = g_queuedIedEvaluations.find(actorFormID);
      if (queuedIt == g_queuedIedEvaluations.end() ||
          queuedIt->second != token) {
        return;
      }
      g_queuedIedEvaluations.erase(queuedIt);
    }

    auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
    if (!IsActorRefreshable(actor)) {
      return;
    }

    auto *vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
      return;
    }

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
        new IedEvaluateCallback());
    if (!vm->DispatchStaticCall(
            "IED", "Evaluate",
            RE::MakeFunctionArguments(static_cast<RE::Actor *>(actor)),
            callback) &&
        !g_iedEvaluateDispatchWarningLogged.exchange(true)) {
      logger::warn(
          "SFS IED compatibility: IED.Evaluate could not be dispatched; hidden real equipment remains safe, but IED may refresh on its next normal update");
    }
  });
}

// A paused SFS menu stops the normal actor-animation tick. A replacement kit
// preview can therefore attach new skinned geometry after the last pose was
// propagated to the skeleton, leaving that geometry in its bind (T) pose until
// the menu closes. Keep this deliberately narrower than a general refresh:
// only the player, only an active row-replacement preview, and only while the
// game is already paused are eligible.
[[nodiscard]] bool IsPausedReplacementPreviewForPlayer(RE::Actor *a_actor) {
  auto *player = RE::PlayerCharacter::GetSingleton();
  if (!a_actor || a_actor != player) {
    return false;
  }

  auto *ui = RE::UI::GetSingleton();
  auto *menu = sfs::Menu::GetSingleton();
  return ui && ui->GameIsPaused() && menu && menu->IsGameDataLoaded() &&
         menu->GetWorkbench().GetNativePreviewRowsForActor(
             a_actor->GetFormID()) != nullptr &&
         menu->GetWorkbench().IsNativePreviewReplacingRowsForActor(
             a_actor->GetFormID());
}

void QueuePausedReplacementPreviewPoseSync(RE::Actor *a_actor) {
  if (!IsPausedReplacementPreviewForPlayer(a_actor)) {
    return;
  }

  const auto actorFormID = a_actor->GetFormID();
  auto *taskInterface = SKSE::GetTaskInterface();
  if (!taskInterface || actorFormID == 0) {
    return;
  }

  // Queue behind the backend refresh. DAVE may schedule its own actor rebuild
  // task, so issuing the pose synchronization inline can still precede the new
  // attachment. A zero-delta update evaluates the current pose without
  // advancing gameplay time or the player's animation timeline.
  taskInterface->AddTask([actorFormID]() {
    auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
    if (!IsPausedReplacementPreviewForPlayer(actor)) {
      return;
    }

    actor->UpdateAnimation(0.0f);
    actor->Update3DPosition(true);
  });
}

void ClearDavFallbackRefreshSignatures() {
  std::lock_guard lock(g_davFallbackRefreshSignatureMutex);
  g_davFallbackRefreshSignatures.clear();
}

void ClearEmptyEquipmentDisplaySignatures() {
  std::lock_guard lock(g_emptyEquipmentDisplaySignatureMutex);
  g_emptyEquipmentDisplaySignatures.clear();
}

class WornArmorVisitor final : public RE::InventoryChanges::IItemChangeVisitor {
public:
  RE::BSContainer::ForEachResult
  Visit(RE::InventoryEntryData *a_entryData) override {
    if (a_entryData && a_entryData->object) {
      if (auto *armor = a_entryData->object->As<RE::TESObjectARMO>()) {
        armors.insert(armor);
      }
    }

    return RE::BSContainer::ForEachResult::kContinue;
  }

  std::unordered_set<const RE::TESObjectARMO *> armors;
};

[[nodiscard]] std::unordered_set<const RE::TESObjectARMO *>
CollectEquippedArmors(RE::TESObjectREFR *a_target);

[[nodiscard]] std::string ToLower(std::string a_value) {
  std::ranges::transform(a_value, a_value.begin(), [](const unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return a_value;
}

[[nodiscard]] bool IsAsciiAlpha(const unsigned char a_ch) {
  return (a_ch >= 'A' && a_ch <= 'Z') || (a_ch >= 'a' && a_ch <= 'z');
}

[[nodiscard]] bool IsAsciiUpper(const unsigned char a_ch) {
  return a_ch >= 'A' && a_ch <= 'Z';
}

[[nodiscard]] bool IsAsciiLower(const unsigned char a_ch) {
  return a_ch >= 'a' && a_ch <= 'z';
}

[[nodiscard]] bool IsAsciiDigit(const unsigned char a_ch) {
  return a_ch >= '0' && a_ch <= '9';
}

[[nodiscard]] bool IsAsciiAlphaNumeric(const unsigned char a_ch) {
  return IsAsciiAlpha(a_ch) || IsAsciiDigit(a_ch);
}

[[nodiscard]] std::string BuildSearchTextPart(const std::string_view a_value) {
  std::string output;
  output.reserve(a_value.size() * 2);

  unsigned char previous = 0;
  for (std::size_t index = 0; index < a_value.size(); ++index) {
    const auto ch = static_cast<unsigned char>(a_value[index]);
    const auto next = index + 1 < a_value.size()
                          ? static_cast<unsigned char>(a_value[index + 1])
                          : static_cast<unsigned char>(0);

    const bool splitCamelCase =
        IsAsciiUpper(ch) &&
        ((IsAsciiLower(previous) || IsAsciiDigit(previous)) ||
         (IsAsciiUpper(previous) && IsAsciiLower(next)));
    const bool splitAlphaDigit = IsAsciiAlphaNumeric(ch) &&
                                 IsAsciiAlphaNumeric(previous) &&
                                 (IsAsciiDigit(ch) != IsAsciiDigit(previous));
    if (!output.empty() && (splitCamelCase || splitAlphaDigit)) {
      output.push_back(' ');
    }

    if (IsAsciiAlphaNumeric(ch)) {
      output.push_back(static_cast<char>(std::tolower(ch)));
    } else if (ch >= 0x80) {
      output.push_back(static_cast<char>(ch));
    } else if (!output.empty() && output.back() != ' ') {
      output.push_back(' ');
    }
    previous = ch;
  }

  if (!output.empty() && output.back() == ' ') {
    output.pop_back();
  }
  return output;
}

void AppendSearchText(std::string &a_output, const std::string &a_value) {
  const auto text = BuildSearchTextPart(a_value);
  if (text.empty()) {
    return;
  }
  if (!a_output.empty()) {
    a_output.push_back(' ');
  }
  a_output.append(text);
}

[[nodiscard]] bool ContainsSearchTerm(const std::string &a_text,
                                      const std::string_view a_term) {
  if (a_text.empty() || a_term.empty()) {
    return false;
  }

  const auto containsNonAscii = [](const std::string_view a_value) {
    return std::ranges::any_of(
        a_value, [](const unsigned char ch) { return ch >= 0x80; });
  };

  if (containsNonAscii(a_term)) {
    return a_text.find(a_term) != std::string::npos;
  }

  std::string paddedText;
  paddedText.reserve(a_text.size() + 2);
  paddedText.push_back(' ');
  paddedText.append(a_text);
  paddedText.push_back(' ');

  std::string paddedTerm;
  paddedTerm.reserve(a_term.size() + 2);
  paddedTerm.push_back(' ');
  paddedTerm.append(a_term);
  paddedTerm.push_back(' ');
  if (paddedText.find(paddedTerm) != std::string::npos) {
    return true;
  }

  if (a_term.size() < 4) {
    return false;
  }

  std::string prefixTerm;
  prefixTerm.reserve(a_term.size() + 1);
  prefixTerm.push_back(' ');
  prefixTerm.append(a_term);
  if (paddedText.find(prefixTerm) != std::string::npos) {
    return true;
  }

  static constexpr std::array kEmbeddedTerms{
      std::string_view{"bikini"},   std::string_view{"lingerie"},
      std::string_view{"monokini"}, std::string_view{"pantie"},
      std::string_view{"panties"},  std::string_view{"pantsu"},
      std::string_view{"panty"},    std::string_view{"panrty"},
      std::string_view{"swimsuit"}, std::string_view{"swimwear"},
      std::string_view{"tankini"},  std::string_view{"thong"},
      std::string_view{"thongs"}};
  if (std::ranges::find(kEmbeddedTerms, a_term) != kEmbeddedTerms.end()) {
    return a_text.find(a_term) != std::string::npos;
  }

  return false;
}

template <std::size_t N>
[[nodiscard]] bool
ContainsAnySearchTerm(const std::string &a_text,
                      const std::array<std::string_view, N> &a_terms) {
  return std::ranges::any_of(a_terms, [&](const auto term) {
    return ContainsSearchTerm(a_text, term);
  });
}

[[nodiscard]] std::string
BuildArmorClassificationText(
    const RE::TESObjectARMO *a_armor,
    const bool a_ignoreGenericVanillaArmorKeywords = false) {
  std::string text;
  if (!a_armor) {
    return text;
  }

  AppendSearchText(text, sfs::armor::GetEditorID(a_armor));
  AppendSearchText(text, sfs::armor::GetDisplayName(a_armor));
  for (const auto *keyword : a_armor->GetKeywords()) {
    const auto keywordEditorID = sfs::armor::GetEditorID(keyword);
    if (a_ignoreGenericVanillaArmorKeywords) {
      static constexpr std::array kGenericVanillaArmorKeywords{
          std::string_view{"ArmorBoots"},
          std::string_view{"ArmorCuirass"},
          std::string_view{"ArmorGauntlets"},
          std::string_view{"ArmorHeavy"},
          std::string_view{"ArmorHelmet"},
          std::string_view{"ArmorLight"},
          std::string_view{"ArmorShield"},
          std::string_view{"ClothingBody"},
          std::string_view{"ClothingFeet"},
          std::string_view{"ClothingHands"},
          std::string_view{"ClothingHead"},
          std::string_view{"ClothingNecklace"},
          std::string_view{"ClothingRing"},
          std::string_view{"VendorItemArmor"},
          std::string_view{"VendorItemClothing"},
          std::string_view{"VendorItemJewelry"}};
      const bool genericKeyword =
          std::ranges::find(kGenericVanillaArmorKeywords, keywordEditorID) !=
              kGenericVanillaArmorKeywords.end() ||
          keywordEditorID.starts_with("ArmorMaterial");
      if (genericKeyword) {
        continue;
      }
    }
    AppendSearchText(text, keywordEditorID);
  }
  return text;
}

[[nodiscard]] bool IsPlayerActor(const RE::Actor *a_actor) {
  const auto *player = RE::PlayerCharacter::GetSingleton();
  return a_actor && player && a_actor->GetFormID() == player->GetFormID();
}

[[nodiscard]] bool IsActorFemale(RE::Actor *a_actor) {
  const auto *actorBase = a_actor ? a_actor->GetActorBase() : nullptr;
  return actorBase && actorBase->IsFemale();
}

[[nodiscard]] std::uint32_t BodySlotMask() {
  return static_cast<std::uint32_t>(
      std::to_underlying(RE::BGSBipedObjectForm::BipedObjectSlot::kBody));
}

[[nodiscard]] std::uint32_t GenitalSlotMask() {
  return static_cast<std::uint32_t>(std::to_underlying(
      RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary));
}

[[nodiscard]] bool ActorSkinUsesGenitalSlot(RE::Actor *a_actor) {
  const auto *skin = a_actor ? a_actor->GetSkin() : nullptr;
  return skin != nullptr &&
         (sfs::armor::GetArmorDisplaySlotMask(skin) & GenitalSlotMask()) != 0;
}

[[nodiscard]] std::uint32_t PelvisPrimarySlotMask() {
  return static_cast<std::uint32_t>(std::to_underlying(
      RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisPrimary));
}

[[nodiscard]] std::uint32_t
GetDisplaySlotMask(const RE::TESObjectARMO *a_armor) {
  return static_cast<std::uint32_t>(
      sfs::armor::GetArmorDisplaySlotMask(a_armor));
}

[[nodiscard]] std::uint32_t
GetArmorConflictSlotMask(const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return 0;
  }

  return GetDisplaySlotMask(a_armor);
}

[[nodiscard]] bool HasKeywordEditorID(const RE::TESObjectARMO *a_armor,
                                      const std::string_view a_editorID) {
  if (!a_armor || a_editorID.empty()) {
    return false;
  }

  for (const auto *keyword : a_armor->GetKeywords()) {
    if (keyword && sfs::armor::GetEditorID(keyword) == a_editorID) {
      return true;
    }
  }
  return false;
}

struct RuntimeKeywords {
  RE::BGSKeyword *sosRevealing{nullptr};
  RE::BGSKeyword *sosConcealing{nullptr};
  RE::BGSKeyword *sosUnderwear{nullptr};
  RE::BGSKeyword *tngRevealing{nullptr};
  RE::BGSKeyword *tngCovering{nullptr};
  RE::BGSKeyword *tngUnderwear{nullptr};

  [[nodiscard]] bool HasAny() const {
    return sosRevealing || sosConcealing || sosUnderwear || tngRevealing ||
           tngCovering || tngUnderwear;
  }

  [[nodiscard]] bool HasSos() const {
    return sosRevealing || sosConcealing || sosUnderwear;
  }

  [[nodiscard]] bool HasTng() const {
    return tngRevealing || tngCovering || tngUnderwear;
  }
};

[[nodiscard]] std::array<RE::BGSKeyword *, 6>
BuildRuntimeKeywordArray(const RuntimeKeywords &a_keywords) {
  return {a_keywords.sosRevealing, a_keywords.sosConcealing,
          a_keywords.sosUnderwear, a_keywords.tngRevealing,
          a_keywords.tngCovering,  a_keywords.tngUnderwear};
}

[[nodiscard]] bool IsRevealingRuntimeKeyword(const RuntimeKeywords &a_keywords,
                                             const RE::BGSKeyword *a_keyword) {
  return a_keyword != nullptr && (a_keyword == a_keywords.sosRevealing ||
                                  a_keyword == a_keywords.tngRevealing);
}

[[nodiscard]] bool IsConcealingRuntimeKeyword(const RuntimeKeywords &a_keywords,
                                              const RE::BGSKeyword *a_keyword) {
  return a_keyword != nullptr && (a_keyword == a_keywords.sosConcealing ||
                                  a_keyword == a_keywords.sosUnderwear ||
                                  a_keyword == a_keywords.tngCovering ||
                                  a_keyword == a_keywords.tngUnderwear);
}

[[nodiscard]] RuntimeKeywords LookupRuntimeKeywords() {
  return {.sosRevealing =
              RE::TESForm::LookupByEditorID<RE::BGSKeyword>("SOS_Revealing"),
          .sosConcealing =
              RE::TESForm::LookupByEditorID<RE::BGSKeyword>("SOS_Concealing"),
          .sosUnderwear =
              RE::TESForm::LookupByEditorID<RE::BGSKeyword>("SOS_Underwear"),
          .tngRevealing =
              RE::TESForm::LookupByEditorID<RE::BGSKeyword>("TNG_Revealing"),
          .tngCovering =
              RE::TESForm::LookupByEditorID<RE::BGSKeyword>("TNG_Covering"),
          .tngUnderwear =
              RE::TESForm::LookupByEditorID<RE::BGSKeyword>("TNG_Underwear")};
}

[[nodiscard]] std::uint64_t
RuntimeKeywordOwnershipKey(const RE::TESObjectARMO *a_armor,
                           const RE::BGSKeyword *a_keyword) {
  return a_armor && a_keyword
             ? (static_cast<std::uint64_t>(a_armor->GetFormID()) << 32u) |
                   a_keyword->GetFormID()
             : 0;
}

void SynchronizeSFSOwnedRuntimeKeyword(RE::TESObjectARMO *a_armor,
                                       RE::BGSKeyword *a_keyword,
                                       const bool a_shouldHave) {
  const auto key = RuntimeKeywordOwnershipKey(a_armor, a_keyword);
  if (key == 0) {
    return;
  }

  std::lock_guard lock(g_runtimeKeywordMutex);
  const bool owned = g_sfsOwnedRuntimeKeywords.contains(key);
  if (a_shouldHave) {
    if (!a_armor->HasKeyword(a_keyword)) {
      a_armor->AddKeyword(a_keyword);
      g_sfsOwnedRuntimeKeywords.insert(key);
    }
    return;
  }

  if (owned) {
    a_armor->RemoveKeyword(a_keyword);
    g_sfsOwnedRuntimeKeywords.erase(key);
  }
}

[[nodiscard]] SOSUserArmorPolicy
GetSOSUserArmorPolicy(const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return SOSUserArmorPolicy::kNeutral;
  }

  std::lock_guard lock(g_sosUserArmorMutex);
  const auto formID = a_armor->GetFormID();
  if (g_sosUserConcealingArmors.contains(formID)) {
    return SOSUserArmorPolicy::kConceal;
  }
  if (g_sosUserRevealingArmors.contains(formID)) {
    return SOSUserArmorPolicy::kReveal;
  }
  return SOSUserArmorPolicy::kNeutral;
}

[[nodiscard]] ArmorGenitalKeywordOverride
ResolveArmorGenitalKeywordOverride(const RE::TESObjectARMO *a_armor);

[[nodiscard]] bool IsGenitalRevealingArmor(RE::Actor *a_actor,
                                           const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return false;
  }

  const auto sfsOverride = ResolveArmorGenitalKeywordOverride(a_armor);
  if (sfsOverride.IsActive()) {
    return sfsOverride.disposition ==
           ArmorGenitalKeywordDisposition::kReveal;
  }

  const auto environment =
      sfs::native::genital_compatibility::GetEnvironment();
  const bool isFemale = IsActorFemale(a_actor);
  return (environment.sosInstalled &&
          HasKeywordEditorID(a_armor, "SOS_Revealing")) ||
         (environment.tngInstalled &&
          (HasKeywordEditorID(a_armor, "TNG_Revealing") ||
           (isFemale &&
            HasKeywordEditorID(a_armor, "TNG_RevealingOnlyWomen")) ||
           (!isFemale &&
            HasKeywordEditorID(a_armor, "TNG_RevealingOnlyMen"))));
}

[[nodiscard]] bool
IsExplicitGenitalCoveringArmor(const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return false;
  }

  const auto sfsOverride = ResolveArmorGenitalKeywordOverride(a_armor);
  if (sfsOverride.IsActive()) {
    return sfsOverride.disposition ==
           ArmorGenitalKeywordDisposition::kConceal;
  }

  const auto environment =
      sfs::native::genital_compatibility::GetEnvironment();
  return (environment.sosInstalled &&
          (HasKeywordEditorID(a_armor, "SOS_Concealing") ||
           HasKeywordEditorID(a_armor, "SOS_Underwear"))) ||
         (environment.tngInstalled &&
          (HasKeywordEditorID(a_armor, "TNG_Covering") ||
           HasKeywordEditorID(a_armor, "TNG_Underwear")));
}

[[nodiscard]] bool IsGenitalCoveringArmor(RE::Actor *a_actor,
                                          const RE::TESObjectARMO *a_armor) {
  const auto sfsOverride = ResolveArmorGenitalKeywordOverride(a_armor);
  if (sfsOverride.IsActive()) {
    return sfsOverride.disposition ==
           ArmorGenitalKeywordDisposition::kConceal;
  }
  if (IsExplicitGenitalCoveringArmor(a_armor)) {
    return true;
  }

  const auto environment =
      sfs::native::genital_compatibility::GetEnvironment();
  if (!environment.tngInstalled) {
    return false;
  }
  const bool isFemale = IsActorFemale(a_actor);
  return a_armor &&
         ((isFemale && HasKeywordEditorID(a_armor, "TNG_RevealingOnlyMen")) ||
          (!isFemale && HasKeywordEditorID(a_armor, "TNG_RevealingOnlyWomen")));
}

[[nodiscard]] bool
IsLikelyGenitalConcealingPelvisArmor(const RE::TESObjectARMO *a_armor) {
  if (!a_armor ||
      (GetDisplaySlotMask(a_armor) & PelvisPrimarySlotMask()) == 0) {
    return false;
  }

  static constexpr std::array kLowerBodyTerms{
      std::string_view{"bikini"},
      std::string_view{"bot"},
      std::string_view{"bottom"},
      std::string_view{"bottoms"},
      std::string_view{"bathing"},
      std::string_view{"brief"},
      std::string_view{"briefs"},
      std::string_view{"bustle"},
      std::string_view{"culotte"},
      std::string_view{"culottes"},
      std::string_view{"dress"},
      std::string_view{"boxer"},
      std::string_view{"boxers"},
      std::string_view{"falda"},
      std::string_view{"fundoshi"},
      std::string_view{"hotpants"},
      std::string_view{"inner"},
      std::string_view{"jean"},
      std::string_view{"jeans"},
      std::string_view{"knicker"},
      std::string_view{"knickers"},
      std::string_view{"legging"},
      std::string_view{"leggings"},
      std::string_view{"lingerie"},
      std::string_view{"loincloth"},
      std::string_view{"low"},
      std::string_view{"lower"},
      std::string_view{"maillot"},
      std::string_view{"monokini"},
      std::string_view{"one piece"},
      std::string_view{"pantie"},
      std::string_view{"panties"},
      std::string_view{"pants"},
      std::string_view{"pantsmp"},
      std::string_view{"pantsu"},
      std::string_view{"panty"},
      std::string_view{"panrty"},
      std::string_view{"pussy"},
      std::string_view{"short"},
      std::string_view{"shorts"},
      std::string_view{"skirt"},
      std::string_view{"string"},
      std::string_view{"strings"},
      std::string_view{"swim"},
      std::string_view{"swimsuit"},
      std::string_view{"swimwear"},
      std::string_view{"tankini"},
      std::string_view{"thong"},
      std::string_view{"thongs"},
      std::string_view{"trouser"},
      std::string_view{"trousers"},
      std::string_view{"underpant"},
      std::string_view{"underpants"},
      std::string_view{"underwear"},
      std::string_view{"undies"},
      std::string_view{"uw"},
      std::string_view{"body suit"},
      std::string_view{"bodysuit"},
      std::string_view{"cat suit"},
      std::string_view{"catsuit"},
      std::string_view{"jump suit"},
      std::string_view{"jumpsuit"},
      std::string_view{"leotard"},
      std::string_view{"unitard"},
      std::string_view{"yoga"},
      std::string_view{"\xEB\xA0\x88\xEC\x98\xA4\xED\x83\x80\xEB\x93\x9C"},
      std::string_view{"\xEB\xA0\x88\xEA\xB9\x85\xEC\x8A\xA4"},
      std::string_view{"\xEB\x9E\x80\xEC\xA0\x9C\xEB\xA6\xAC"},
      std::string_view{"\xEB\xB0\x94\xEB\x94\x94\xEC\x88\x98\xED\x8A\xB8"},
      std::string_view{"\xEC\x9C\xA0\xEB\x8B\x88\xED\x83\x80\xEB\x93\x9C"},
      std::string_view{"\xEC\xBA\xA3\xEC\x88\x98\xED\x8A\xB8"},
      std::string_view{"\xEB\xB0\x94\xEC\xA7\x80"},
      std::string_view{"\xEB\xB9\x84\xED\x82\xA4\xEB\x8B\x88"},
      std::string_view{"\xEB\xB3\xB4\xEC\xA7\x80"},
      std::string_view{"\xEB\xB7\xB0\xEC\xA7\x80"},
      std::string_view{"\xEB\x93\x9C\xEB\xA0\x88\xEC\x8A\xA4"},
      std::string_view{"\xEB\xAA\xA8\xEB\x85\xB8\xED\x82\xA4\xEB\x8B\x88"},
      std::string_view{"\xEC\x88\x98\xEC\x98\x81\xEB\xB3\xB5"},
      std::string_view{"\xEC\x86\x8D\xEB\xB0\x94\xEC\xA7\x80"},
      std::string_view{"\xEC\x86\x8D\xEC\x98\xB7"},
      std::string_view{"\xEC\x8A\xA4\xEC\xBB\xA4\xED\x8A\xB8"},
      std::string_view{"\xEC\x87\xBC\xEC\xB8\xA0"},
      std::string_view{"\xEC\x88\x8F\xED\x8C\xAC\xEC\xB8\xA0"},
      std::string_view{"\xEC\xA0\x90\xED\x94\x84\xEC\x88\x98\xED\x8A\xB8"},
      std::string_view{"\xEC\x8A\xA4\xED\x8A\xB8\xEB\xA7\x81"},
      std::string_view{"\xEC\x8A\xA4\xED\x8A\xB8\xEB\xA7\x81\xEC\x8A\xA4"},
      std::string_view{"\xEC\x88\x8F"},
      std::string_view{"\xEC\x9A\x94\xEA\xB0\x80"},
      std::string_view{"\xEC\x9B\x90\xED\x94\xBC\xEC\x8A\xA4"},
      std::string_view{"\xEC\xB9\x98\xEB\xA7\x88"},
      std::string_view{"\xED\x8C\xAC\xEC\xB8\xA0"},
      std::string_view{"\xED\x8C\xAC\xED\x8B\xB0"},
      std::string_view{"\xED\x95\x98\xEC\x9D\x98"},
      std::string_view{"\xE4\xB8\x8B\xE8\xA3\x85"},
      std::string_view{"\xE4\xB8\x8B\xE8\xA1\xA3"},
      std::string_view{"\xE5\x86\x85\xE8\xA3\xA4"},
      std::string_view{"\xE5\xBA\x95\xE8\xA3\xA4"},
      std::string_view{"\xE5\x86\x85\xE8\xA1\xA3\xE4\xB8\x8B"},
      std::string_view{"\xE5\x86\x85\xE8\xA1\xA3 \xE4\xB8\x8B"},
      std::string_view{"\xE8\xBF\x9E\xE4\xBD\x93\xE8\xA1\xA3"},
      std::string_view{
          "\xE8\xBF\x9E\xE4\xBD\x93\xE7\xB4\xA7\xE8\xBA\xAB\xE8\xA1\xA3"},
      std::string_view{
          "\xE7\xB4\xA7\xE8\xBA\xAB\xE8\xBF\x9E\xE4\xBD\x93\xE8\xA1\xA3"},
      std::string_view{
          "\xE5\x85\xA8\xE8\xBA\xAB\xE7\xB4\xA7\xE8\xBA\xAB\xE8\xA1\xA3"},
      std::string_view{"\xE8\xBF\x9E\xE4\xBD\x93\xE8\xA3\xA4"},
      std::string_view{"\xE6\xB3\xB3\xE8\xA3\x85"},
      std::string_view{"\xE6\xB3\xB3\xE8\xA3\xA4"},
      std::string_view{"\xE8\xA3\xA4"},
      std::string_view{"\xE8\xA3\xA4\xE5\xAD\x90"},
      std::string_view{"\xE7\x9F\xAD\xE8\xA3\xA4"},
      std::string_view{"\xE7\x83\xAD\xE8\xA3\xA4"},
      std::string_view{"\xE7\x89\x9B\xE4\xBB\x94\xE8\xA3\xA4"},
      std::string_view{"\xE6\x89\x93\xE5\xBA\x95\xE8\xA3\xA4"},
      std::string_view{"\xE7\xB4\xA7\xE8\xBA\xAB\xE8\xA3\xA4"},
      std::string_view{"\xE8\xA3\x99"},
      std::string_view{"\xE8\xA3\x99\xE5\xAD\x90"},
      std::string_view{"\xE7\x9F\xAD\xE8\xA3\x99"},
      std::string_view{"\xE8\xBF\xB7\xE4\xBD\xA0\xE8\xA3\x99"},
      std::string_view{"\xE5\x8D\x8A\xE8\xBA\xAB\xE8\xA3\x99"},
      std::string_view{"\xE8\xBF\x9E\xE8\xA1\xA3\xE8\xA3\x99"},
      std::string_view{"\xE6\x97\x97\xE8\xA2\x8D"},
      std::string_view{
          "\xE6\xAF\x94\xE5\x9F\xBA\xE5\xB0\xBC\xE4\xB8\x8B\xE8\xA3\x85"},
      std::string_view{
          "\xE6\xAF\x94\xE5\x9F\xBA\xE5\xB0\xBC\xE4\xB8\x8B\xE8\xA1\xA3"}};

  return ContainsAnySearchTerm(BuildArmorClassificationText(a_armor),
                               kLowerBodyTerms);
}

[[nodiscard]] bool
IsLikelyUpperOnlyBodyArmor(const RE::TESObjectARMO *a_armor) {
  if (!a_armor || (GetDisplaySlotMask(a_armor) & BodySlotMask()) == 0) {
    return false;
  }

  // Generic vanilla equipment taxonomy such as ArmorCuirass, ArmorHeavy,
  // ArmorMaterial*, ClothingBody, and VendorItemArmor must not turn an
  // ordinary cuirass into an upper-only garment. Item names, EditorIDs, and
  // semantic mod/KID keywords remain available to the classifier.
  const auto text = BuildArmorClassificationText(a_armor, true);
  static constexpr std::array kExplicitUpperOnlyTerms{
      std::string_view{"bikini top"},
      std::string_view{"bikini upper"},
      std::string_view{"dress top"},
      std::string_view{"dress upper"},
      std::string_view{"swim top"},
      std::string_view{"swim upper"},
      std::string_view{"swimsuit top"},
      std::string_view{"swimsuit upper"},
      std::string_view{"upper bikini"},
      std::string_view{"upper dress"},
      std::string_view{"upper swim"},
      std::string_view{"upper swimsuit"},
      std::string_view{
          "\xEB\xB9\x84\xED\x82\xA4\xEB\x8B\x88 \xEC\x83\x81\xEC\x9D\x98"},
      std::string_view{
          "\xEB\x93\x9C\xEB\xA0\x88\xEC\x8A\xA4 \xEC\x83\x81\xEC\x9D\x98"},
      std::string_view{
          "\xEC\x88\x98\xEC\x98\x81\xEB\xB3\xB5 \xEC\x83\x81\xEC\x9D\x98"},
      std::string_view{
          "\xEC\x83\x81\xEC\x9D\x98 \xEB\xB9\x84\xED\x82\xA4\xEB\x8B\x88"},
      std::string_view{
          "\xEC\x83\x81\xEC\x9D\x98 \xEB\x93\x9C\xEB\xA0\x88\xEC\x8A\xA4"},
      std::string_view{
          "\xEC\x83\x81\xEC\x9D\x98 \xEC\x88\x98\xEC\x98\x81\xEB\xB3\xB5"},
      std::string_view{"\xE5\x86\x85\xE8\xA1\xA3\xE4\xB8\x8A"},
      std::string_view{"\xE5\x86\x85\xE8\xA1\xA3 \xE4\xB8\x8A"},
      std::string_view{
          "\xE6\xAF\x94\xE5\x9F\xBA\xE5\xB0\xBC\xE4\xB8\x8A\xE8\xA1\xA3"},
      std::string_view{"\xE6\xB3\xB3\xE8\xA3\x85\xE4\xB8\x8A\xE8\xA1\xA3"},
      std::string_view{"\xE6\xB3\xB3\xE8\xA1\xA3\xE4\xB8\x8A\xE8\xA1\xA3"}};
  if (ContainsAnySearchTerm(text, kExplicitUpperOnlyTerms)) {
    return true;
  }

  static constexpr std::array kLowerBodyBlockers{
      std::string_view{"bottom"},
      std::string_view{"bottoms"},
      std::string_view{"boxer"},
      std::string_view{"boxers"},
      std::string_view{"brief"},
      std::string_view{"briefs"},
      std::string_view{"culotte"},
      std::string_view{"culottes"},
      std::string_view{"dress"},
      std::string_view{"falda"},
      std::string_view{"fundoshi"},
      std::string_view{"hotpants"},
      std::string_view{"jean"},
      std::string_view{"jeans"},
      std::string_view{"knicker"},
      std::string_view{"knickers"},
      std::string_view{"legging"},
      std::string_view{"leggings"},
      std::string_view{"loincloth"},
      std::string_view{"low"},
      std::string_view{"lower"},
      std::string_view{"maillot"},
      std::string_view{"monokini"},
      std::string_view{"one piece"},
      std::string_view{"onepiece"},
      std::string_view{"pantie"},
      std::string_view{"panties"},
      std::string_view{"pants"},
      std::string_view{"pantsmp"},
      std::string_view{"pantsu"},
      std::string_view{"panty"},
      std::string_view{"panrty"},
      std::string_view{"short"},
      std::string_view{"shorts"},
      std::string_view{"skirt"},
      std::string_view{"thong"},
      std::string_view{"thongs"},
      std::string_view{"trouser"},
      std::string_view{"trousers"},
      std::string_view{"underpant"},
      std::string_view{"underpants"},
      std::string_view{"undies"},
      std::string_view{"\xEB\xA0\x88\xEA\xB9\x85\xEC\x8A\xA4"},
      std::string_view{"\xEB\xB0\x94\xEC\xA7\x80"},
      std::string_view{"\xEB\x93\x9C\xEB\xA0\x88\xEC\x8A\xA4"},
      std::string_view{"\xEC\x86\x8D\xEB\xB0\x94\xEC\xA7\x80"},
      std::string_view{"\xEC\x8A\xA4\xEC\xBB\xA4\xED\x8A\xB8"},
      std::string_view{"\xEC\x87\xBC\xEC\xB8\xA0"},
      std::string_view{"\xEC\x88\x8F\xED\x8C\xAC\xEC\xB8\xA0"},
      std::string_view{"\xEC\x9B\x90\xED\x94\xBC\xEC\x8A\xA4"},
      std::string_view{"\xEC\xB9\x98\xEB\xA7\x88"},
      std::string_view{"\xED\x8C\xAC\xEC\xB8\xA0"},
      std::string_view{"\xED\x8C\xAC\xED\x8B\xB0"},
      std::string_view{"\xED\x95\x98\xEC\x9D\x98"},
      std::string_view{"\xED\x95\x98\xEC\xB2\xB4"},
      std::string_view{"\xE4\xB8\x8B\xE8\xA3\x85"},
      std::string_view{"\xE4\xB8\x8B\xE8\xA1\xA3"},
      std::string_view{"\xE5\x86\x85\xE8\xA3\xA4"},
      std::string_view{"\xE5\xBA\x95\xE8\xA3\xA4"},
      std::string_view{"\xE5\x86\x85\xE8\xA1\xA3\xE4\xB8\x8B"},
      std::string_view{"\xE5\x86\x85\xE8\xA1\xA3 \xE4\xB8\x8B"},
      std::string_view{"\xE6\xB3\xB3\xE8\xA3\xA4"},
      std::string_view{"\xE8\xA3\xA4"},
      std::string_view{"\xE8\xA3\xA4\xE5\xAD\x90"},
      std::string_view{"\xE7\x9F\xAD\xE8\xA3\xA4"},
      std::string_view{"\xE7\x83\xAD\xE8\xA3\xA4"},
      std::string_view{"\xE7\x89\x9B\xE4\xBB\x94\xE8\xA3\xA4"},
      std::string_view{"\xE6\x89\x93\xE5\xBA\x95\xE8\xA3\xA4"},
      std::string_view{"\xE7\xB4\xA7\xE8\xBA\xAB\xE8\xA3\xA4"},
      std::string_view{"\xE8\xA3\x99"},
      std::string_view{"\xE8\xA3\x99\xE5\xAD\x90"},
      std::string_view{"\xE7\x9F\xAD\xE8\xA3\x99"},
      std::string_view{"\xE8\xBF\xB7\xE4\xBD\xA0\xE8\xA3\x99"},
      std::string_view{"\xE5\x8D\x8A\xE8\xBA\xAB\xE8\xA3\x99"},
      std::string_view{"\xE8\xBF\x9E\xE8\xA1\xA3\xE8\xA3\x99"},
      std::string_view{"\xE6\x97\x97\xE8\xA2\x8D"},
      std::string_view{"\xE8\xBF\x9E\xE4\xBD\x93\xE8\xA1\xA3"},
      std::string_view{
          "\xE8\xBF\x9E\xE4\xBD\x93\xE7\xB4\xA7\xE8\xBA\xAB\xE8\xA1\xA3"},
      std::string_view{"\xE7\xB4\xA7\xE8\xBA\xAB\xE8\xA1\xA3"},
      std::string_view{"\xE7\xA4\xBC\xE6\x9C\x8D"}};
  if (ContainsAnySearchTerm(text, kLowerBodyBlockers)) {
    return false;
  }

  static constexpr std::array kUpperBodyTerms{
      std::string_view{"blouse"},
      std::string_view{"bodice"},
      std::string_view{"bolero"},
      std::string_view{"bra"},
      std::string_view{"bralette"},
      std::string_view{"brassiere"},
      std::string_view{"breast"},
      std::string_view{"bustier"},
      std::string_view{"camisole"},
      std::string_view{"chest"},
      std::string_view{"crop"},
      std::string_view{"croptop"},
      std::string_view{"halter"},
      std::string_view{"hoodie"},
      std::string_view{"jacket"},
      std::string_view{"shirt"},
      std::string_view{"sweater"},
      std::string_view{"top"},
      std::string_view{"tube"},
      std::string_view{"upper"},
      std::string_view{"vest"},
      std::string_view{"\xEA\xB0\x80\xEC\x8A\xB4"},
      std::string_view{"\xEB\xB8\x8C\xEB\x9D\xBC"},
      std::string_view{"\xEB\xB8\x8C\xEB\x9E\x98\xEC\xA7\x80\xEC\x96\xB4"},
      std::string_view{"\xEB\xB8\x94\xEB\x9D\xBC\xEC\x9A\xB0\xEC\x8A\xA4"},
      std::string_view{"\xEC\x83\x81\xEC\x9D\x98"},
      std::string_view{"\xEC\x85\x94\xEC\xB8\xA0"},
      std::string_view{"\xEC\x9E\x90\xEC\xBC\x93"},
      std::string_view{"\xEC\x9E\xAC\xED\x82\xB7"},
      std::string_view{"\xEC\xA1\xB0\xEB\x81\xBC"},
      std::string_view{"\xED\x83\x91"},
      std::string_view{"\xED\x81\xAC\xEB\xA1\xAD"},
      std::string_view{"\xE4\xB8\x8A\xE8\xA3\x85"},
      std::string_view{"\xE4\xB8\x8A\xE8\xA1\xA3"},
      std::string_view{"\xE5\x86\x85\xE8\xA1\xA3\xE4\xB8\x8A"},
      std::string_view{"\xE5\x86\x85\xE8\xA1\xA3 \xE4\xB8\x8A"},
      std::string_view{"\xE5\xA4\xB9\xE5\x85\x8B"},
      std::string_view{"\xE5\xA4\x96\xE5\xA5\x97"},
      std::string_view{"\xE6\x96\x87\xE8\x83\xB8"},
      std::string_view{"\xE8\x83\xB8\xE7\xBD\xA9"},
      std::string_view{"\xE4\xB9\xB3\xE7\xBD\xA9"},
      std::string_view{"\xE6\x8A\xB9\xE8\x83\xB8"},
      std::string_view{"\xE8\x83\x8C\xE5\xBF\x83"},
      std::string_view{"\xE8\xA1\xAC\xE8\xA1\xAB"},
      std::string_view{"\xE5\x90\x8A\xE5\xB8\xA6"}};
  return ContainsAnySearchTerm(text, kUpperBodyTerms);
}

[[nodiscard]] ArmorGenitalKeywordOverride
ResolveArmorGenitalKeywordOverride(const RE::TESObjectARMO *a_armor) {
  if (!a_armor || sfs::armor::IsSosTngInternalArmor(a_armor)) {
    return {};
  }

  // Explicit SOS MCM choices remain the highest-priority SFS input.
  switch (GetSOSUserArmorPolicy(a_armor)) {
  case SOSUserArmorPolicy::kReveal:
    return {.disposition = ArmorGenitalKeywordDisposition::kReveal};
  case SOSUserArmorPolicy::kConceal:
    return {.disposition = ArmorGenitalKeywordDisposition::kConceal};
  case SOSUserArmorPolicy::kNeutral: break;
  }

  // A classified slot-49 lower garment conceals. Slot-49 accessories do not
  // gain an override merely from their slot, and keep ordinary ESP/KID rules.
  // For combined slot-32/49 garments the lower-body decision wins. SOS/TNG
  // do not consistently infer slot 49, so materialize both covering and
  // underwear keywords for every positively classified lower garment.
  if (IsLikelyGenitalConcealingPelvisArmor(a_armor)) {
    return {.disposition = ArmorGenitalKeywordDisposition::kConceal,
            .underwear = true};
  }

  // Slot 32 reveals only when the established multilingual name/EditorID
  // classifier identifies an upper-only garment. General and full-body slot
  // 32 armor already conceals under ordinary SOS/TNG slot behavior, so keep
  // the SFS display override but do not attach redundant covering keywords.
  if ((GetDisplaySlotMask(a_armor) & BodySlotMask()) != 0) {
    if (IsLikelyUpperOnlyBodyArmor(a_armor)) {
      return {.disposition = ArmorGenitalKeywordDisposition::kReveal};
    }
    return {.disposition = ArmorGenitalKeywordDisposition::kConceal,
            .materializeCoveringKeywords = false};
  }

  // Outside the 32/49 policy, preserve normal ESP/KID keyword behavior.
  return {};
}

void SynchronizeArmorClassificationKeywordsImpl(RE::TESObjectARMO *a_armor) {
  // This is the global SFS 32/49 classification overlay.  It is deliberately
  // independent of the external strip-link mode: the same runtime SOS/TNG
  // classification is needed for normal equipment, vanilla-slot matching,
  // and the contextual virtual-token view.  Only the token transaction and
  // expanded-slot suppression lifecycle are gated by ModSettingsSlots.
  if (!a_armor || sfs::armor::IsSosTngInternalArmor(a_armor)) {
    return;
  }

  const auto keywords = LookupRuntimeKeywords();
  const auto environment =
      sfs::native::genital_compatibility::GetEnvironment();
  if ((!environment.sosInstalled || !keywords.HasSos()) &&
      (!environment.tngInstalled || !keywords.HasTng())) {
    return;
  }

  const auto sfsOverride = ResolveArmorGenitalKeywordOverride(a_armor);
  const bool shouldReveal =
      sfsOverride.disposition == ArmorGenitalKeywordDisposition::kReveal;
  const bool shouldConceal =
      sfsOverride.disposition == ArmorGenitalKeywordDisposition::kConceal;
  const bool shouldMaterializeCovering =
      shouldConceal && sfsOverride.materializeCoveringKeywords;
  const bool shouldMarkUnderwear =
      shouldMaterializeCovering && sfsOverride.underwear;

  SynchronizeSFSOwnedRuntimeKeyword(
      a_armor, keywords.sosRevealing,
      environment.sosInstalled && shouldReveal);
  SynchronizeSFSOwnedRuntimeKeyword(
      a_armor, keywords.tngRevealing,
      environment.tngInstalled && shouldReveal);
  SynchronizeSFSOwnedRuntimeKeyword(
      a_armor, keywords.sosConcealing,
      environment.sosInstalled && shouldMaterializeCovering);
  SynchronizeSFSOwnedRuntimeKeyword(
      a_armor, keywords.tngCovering,
      environment.tngInstalled && shouldMaterializeCovering);
  SynchronizeSFSOwnedRuntimeKeyword(
      a_armor, keywords.sosUnderwear,
      environment.sosInstalled && shouldMarkUnderwear);
  SynchronizeSFSOwnedRuntimeKeyword(
      a_armor, keywords.tngUnderwear,
      environment.tngInstalled && shouldMarkUnderwear);
}

void QueueArmorClassificationMigrationRefreshes() {
  std::unordered_set<RE::FormID> actorFormIDs;
  if (const auto *player = RE::PlayerCharacter::GetSingleton()) {
    actorFormIDs.insert(player->GetFormID());
  }

  auto *menu = sfs::Menu::GetSingleton();
  if (menu && menu->IsGameDataLoaded()) {
    auto workbenchStateLock = menu->GetWorkbench().AcquireStateLock();
    for (const auto &row : menu->GetWorkbench().GetRows()) {
      if (row.ownerActorFormID != 0 && row.HasOverridesOrHideState()) {
        actorFormIDs.insert(row.ownerActorFormID);
      }
    }
  }

  for (const auto actorFormID : actorFormIDs) {
    if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID)) {
      sfs::native::QueueArmorRefreshFor(actor);
    }
  }
}

[[nodiscard]] bool RunLegacyArmorClassificationKeywordMigration() {
  std::lock_guard migrationLock(g_armorClassificationMigrationMutex);
  if (!g_armorClassificationMigrationPending.load() ||
      !g_armorClassificationKeywordBaselineCaptured) {
    return false;
  }

  const auto keywords = LookupRuntimeKeywords();
  if (!keywords.HasAny()) {
    g_completedArmorClassificationMigrationVersion.store(
        kCurrentArmorClassificationMigrationVersion);
    g_armorClassificationMigrationPending.store(false);
    logger::info("Completed one-time armor classification keyword migration: "
                 "no SOS/TNG runtime keywords are loaded");
    return true;
  }

  std::unordered_set<RE::FormID> userRevealingArmors;
  std::unordered_set<RE::FormID> userConcealingArmors;
  {
    std::lock_guard userListLock(g_sosUserArmorMutex);
    userRevealingArmors = g_sosUserRevealingArmors;
    userConcealingArmors = g_sosUserConcealingArmors;
  }

  std::size_t removedKeywordCount = 0;
  std::size_t affectedArmorCount = 0;
  if (auto *dataHandler = RE::TESDataHandler::GetSingleton()) {
    for (auto *armor : dataHandler->GetFormArray<RE::TESObjectARMO>()) {
      if (!armor) {
        continue;
      }

      const auto armorFormID = armor->GetFormID();
      const bool userReveal = userRevealingArmors.contains(armorFormID);
      const bool userConceal = userConcealingArmors.contains(armorFormID);
      bool armorChanged = false;

      for (auto *keyword : BuildRuntimeKeywordArray(keywords)) {
        if (!keyword) {
          continue;
        }

        const auto ownershipKey = RuntimeKeywordOwnershipKey(armor, keyword);
        const bool protectedByUserPolicy =
            (userReveal && IsRevealingRuntimeKeyword(keywords, keyword)) ||
            (userConceal && IsConcealingRuntimeKeyword(keywords, keyword));
        if (!ShouldRemoveLegacyRuntimeKeyword(
                armor->HasKeyword(keyword),
                g_baselineArmorClassificationRuntimeKeywords.contains(
                    ownershipKey),
                protectedByUserPolicy)) {
          continue;
        }

        armor->RemoveKeyword(keyword);
        armorChanged = true;
        ++removedKeywordCount;
      }

      if (armorChanged) {
        ++affectedArmorCount;
      }
    }
  }

  {
    std::lock_guard ownershipLock(g_runtimeKeywordMutex);
    g_sfsOwnedRuntimeKeywords.clear();
  }
  ClearDavFallbackRefreshSignatures();
  ClearEmptyEquipmentDisplaySignatures();
  sfs::native::ClearResolvedGenitalArmors();
  g_completedArmorClassificationMigrationVersion.store(
      kCurrentArmorClassificationMigrationVersion);
  g_armorClassificationMigrationPending.store(false);

  logger::info("Completed one-time armor classification keyword migration: "
               "removed {} stale keyword(s) from {} armor form(s); plugin/KID "
               "baseline and SOS StorageUtil user choices were preserved",
               removedKeywordCount, affectedArmorCount);
  QueueArmorClassificationMigrationRefreshes();
  return true;
}

[[nodiscard]] bool IsGenerallyRevealingArmor(const RE::TESObjectARMO *a_armor) {
  const auto sfsOverride = ResolveArmorGenitalKeywordOverride(a_armor);
  if (sfsOverride.IsActive()) {
    return sfsOverride.disposition ==
           ArmorGenitalKeywordDisposition::kReveal;
  }
  const auto environment =
      sfs::native::genital_compatibility::GetEnvironment();
  return (environment.sosInstalled &&
          HasKeywordEditorID(a_armor, "SOS_Revealing")) ||
         (environment.tngInstalled &&
          HasKeywordEditorID(a_armor, "TNG_Revealing"));
}

[[nodiscard]] bool
IsDefaultGenitalConcealingBodyArmor(RE::Actor *a_actor,
                                    const RE::TESObjectARMO *a_armor) {
  return a_armor != nullptr &&
         (GetDisplaySlotMask(a_armor) & BodySlotMask()) != 0 &&
         !IsGenitalRevealingArmor(a_actor, a_armor);
}

[[nodiscard]] bool IsGenitalArmor(const RE::TESObjectARMO *a_armor) {
  return sfs::armor::IsSosTngGenitalArmor(a_armor);
}

[[nodiscard]] std::uint32_t
GetSkinningSlotMask(const RE::TESObjectARMO *a_armor,
                    const bool a_applyGenitalCompatibility) {
  sfs::native::SynchronizeArmorClassificationKeywords(
      const_cast<RE::TESObjectARMO *>(a_armor));
  auto slotMask = GetDisplaySlotMask(a_armor);
  if (!a_armor || slotMask == 0) {
    return slotMask;
  }

  if (!a_applyGenitalCompatibility || IsGenitalArmor(a_armor)) {
    return slotMask;
  }

  if ((slotMask & BodySlotMask()) != 0 && (slotMask & GenitalSlotMask()) != 0 &&
      IsGenerallyRevealingArmor(a_armor)) {
    slotMask &= ~GenitalSlotMask();
  }

  return slotMask;
}

[[nodiscard]] bool ShouldRevealGenitals(RE::Actor *a_actor,
                                        const RE::TESObjectARMO *a_armor) {
  sfs::native::SynchronizeArmorClassificationKeywords(
      const_cast<RE::TESObjectARMO *>(a_armor));
  if (!a_armor || sfs::armor::IsSosTngInternalArmor(a_armor) ||
      IsGenitalCoveringArmor(a_actor, a_armor)) {
    return false;
  }

  return IsGenitalRevealingArmor(a_actor, a_armor);
}

[[nodiscard]] bool ShouldConcealGenitals(RE::Actor *a_actor,
                                         const RE::TESObjectARMO *a_armor) {
  sfs::native::SynchronizeArmorClassificationKeywords(
      const_cast<RE::TESObjectARMO *>(a_armor));
  if (!a_armor || sfs::armor::IsSosTngInternalArmor(a_armor) ||
      IsGenitalRevealingArmor(a_actor, a_armor)) {
    return false;
  }

  const auto slotMask = GetArmorConflictSlotMask(a_armor);
  return (slotMask & GenitalSlotMask()) != 0 ||
         IsGenitalCoveringArmor(a_actor, a_armor);
}

[[nodiscard]] bool
ShouldVisibleArmorConcealGenitals(RE::Actor *a_actor,
                                  const RE::TESObjectARMO *a_armor) {
  if (!a_armor || sfs::armor::IsSosTngInternalArmor(a_armor) ||
      IsGenitalRevealingArmor(a_actor, a_armor)) {
    return false;
  }

  return ShouldConcealGenitals(a_actor, a_armor) ||
         IsDefaultGenitalConcealingBodyArmor(a_actor, a_armor);
}

[[nodiscard]] bool
IsConditionActiveForActor(const std::optional<std::string> &a_conditionId,
                          RE::Actor *a_actor) {
  if (!a_conditionId.has_value() || !a_actor) {
    return false;
  }

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return false;
  }

  auto conditionStateLock = menu->AcquireConditionStateLock();
  auto &conditions = menu->GetConditions();
  const auto *definition =
      sfs::conditions::FindDefinitionById(conditions, *a_conditionId);
  if (!definition || !sfs::conditions::IsWorkbenchSelectable(*definition)) {
    return false;
  }

  if (!sfs::conditions::EvaluateDefinitionStatus(*definition, conditions)
           .IsActive()) {
    return false;
  }

  auto materialized =
      sfs::conditions::MaterializeConditionById(*a_conditionId, conditions);
  if (!materialized || !materialized->condition) {
    return false;
  }

  return materialized->condition->IsTrue(a_actor, a_actor);
}

[[nodiscard]] bool
IsRowActiveForActor(const sfs::workbench::VariantWorkbenchRow &a_row,
                    RE::Actor *a_actor) {
  if (!a_row.IsOwnedByActor(a_actor)) {
    return false;
  }
  if (a_row.conditionId.has_value()) {
    return IsConditionActiveForActor(a_row.conditionId, a_actor);
  }

  return a_row.HasOwnerActor() || IsPlayerActor(a_actor);
}

struct ConditionalVisibilityDecisions {
  std::unordered_map<RE::FormID, bool> actual;
  std::unordered_map<RE::FormID, bool> fitting;
};

[[nodiscard]] ConditionalVisibilityDecisions
BuildConditionalVisibilityDecisions(
    const sfs::workbench::VariantWorkbench &a_workbench, RE::Actor *a_actor) {
  ConditionalVisibilityDecisions decisions;
  for (const auto &rule : a_workbench.GetConditionalVisibilityRules()) {
    const auto *targetArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(rule.target.formID);
    const auto targetDisplaySlotMask =
        targetArmor != nullptr
            ? sfs::armor::GetArmorDisplaySlotMask(targetArmor)
            : rule.target.slotMask;
    if (!rule.IsOwnedByActor(a_actor) || rule.conditionId.empty() ||
        rule.target.formID == 0 ||
        sfs::workbench::IsAppearanceRegistrationProtectedSlotMask(
            targetDisplaySlotMask) ||
        !IsConditionActiveForActor(rule.conditionId, a_actor)) {
      continue;
    }
    auto &targetMap =
        rule.targetKind ==
                sfs::workbench::ConditionalVisibilityTargetKind::Actual
            ? decisions.actual
            : decisions.fitting;
    static_cast<void>(
        targetMap.emplace(rule.target.formID, rule.visibleWhenTrue));
  }
  return decisions;
}

[[nodiscard]] bool
IsRealArmorVisibleInDisplaySet([[maybe_unused]] RE::Actor *a_actor,
                               const DisplaySet &a_displaySet,
                               const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return false;
  }

  if (GetProtectedActualSlotMask(a_armor) != 0) {
    return true;
  }

  if (a_displaySet.forceVisibleArmorFormIDs.contains(a_armor->GetFormID())) {
    return true;
  }

  if (a_displaySet.hiddenArmorFormIDs.contains(a_armor->GetFormID())) {
    return false;
  }

  const auto slotMask =
      GetSkinningSlotMask(a_armor, a_displaySet.genitalCompatibilityAvailable);
  return slotMask == 0 || (slotMask & a_displaySet.hiddenSlotMask) == 0;
}

[[nodiscard]] std::vector<const RE::TESObjectARMO *> CollectVisibleRealArmors(
    RE::Actor *a_actor, const DisplaySet &a_displaySet,
    const std::unordered_set<const RE::TESObjectARMO *> &a_equippedArmors) {
  std::vector<const RE::TESObjectARMO *> visibleArmors;
  visibleArmors.reserve(a_equippedArmors.size());
  for (const auto *armor : a_equippedArmors) {
    // TNG_GenitalCover is renderer state. It must remain available to the worn
    // mask path below, but must never vote that the actor's outfit conceals.
    if (!sfs::armor::IsTngGenitalCoverArmor(armor) &&
        IsRealArmorVisibleInDisplaySet(a_actor, a_displaySet, armor)) {
      visibleArmors.push_back(armor);
    }
  }
  return visibleArmors;
}

[[nodiscard]] const RE::TESObjectARMO *FindEquippedArmorByFormID(
    const std::unordered_set<const RE::TESObjectARMO *> &a_equippedArmors,
    const RE::FormID a_formID) {
  if (a_formID == 0) {
    return nullptr;
  }

  for (const auto *armor : a_equippedArmors) {
    if (armor && armor->GetFormID() == a_formID) {
      return armor;
    }
  }

  return nullptr;
}

[[nodiscard]] std::uint32_t GetActorEquippedRowSlotMask(
    const sfs::workbench::VariantWorkbenchRow &a_row,
    const std::unordered_set<const RE::TESObjectARMO *> &a_equippedArmors,
    const bool a_applyGenitalCompatibility) {
  if (a_row.equipped.IsSlot()) {
    return 0;
  }

  if (const auto *armor =
          FindEquippedArmorByFormID(a_equippedArmors, a_row.equipped.formID);
      armor != nullptr) {
    return GetSkinningSlotMask(armor, a_applyGenitalCompatibility);
  }

  if (a_row.isEquipped) {
    return static_cast<std::uint32_t>(a_row.equipped.slotMask);
  }

  return 0;
}

[[nodiscard]] bool AnyVisibleArmorRevealsGenitals(
    RE::Actor *a_actor,
    const std::vector<const RE::TESObjectARMO *> &a_visibleArmors) {
  return std::ranges::any_of(a_visibleArmors,
                             [a_actor](const RE::TESObjectARMO *a_armor) {
                               return ShouldRevealGenitals(a_actor, a_armor);
                             });
}

[[nodiscard]] bool AnyVisibleArmorConcealsGenitals(
    RE::Actor *a_actor,
    const std::vector<const RE::TESObjectARMO *> &a_visibleArmors) {
  return std::ranges::any_of(
      a_visibleArmors, [a_actor](const RE::TESObjectARMO *a_armor) {
        return ShouldVisibleArmorConcealGenitals(a_actor, a_armor);
      });
}

[[nodiscard]] bool ShouldManageActorDisplay(RE::Actor *a_actor,
                                            sfs::Menu &a_menu) {
  if (!a_actor) {
    return false;
  }
  if (IsPlayerActor(a_actor)) {
    return true;
  }

  const auto actorFormID = a_actor->GetFormID();
  if (a_menu.GetWorkbench().GetNativePreviewRowsForActor(actorFormID)) {
    return true;
  }

  if (std::ranges::any_of(a_menu.GetWorkbench().GetRows(),
                          [actorFormID](const auto &a_row) {
                            return a_row.ownerActorFormID == actorFormID &&
                                   a_row.HasOverridesOrHideState();
                          })) {
    return true;
  }
  return std::ranges::any_of(
      a_menu.GetWorkbench().GetConditionalVisibilityRules(),
      [actorFormID](const auto &a_rule) {
        return a_rule.ownerActorFormID == actorFormID;
      });
}

[[nodiscard]] DisplaySet
BuildDisplaySet(RE::Actor *a_actor,
                const bool a_applyTemporarySuppression = true) {
  DisplaySet displaySet;
  if (!a_actor) {
    return displaySet;
  }
  DisplaySetBuildScope buildScope;

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return displaySet;
  }
  auto workbenchStateLock = menu->GetWorkbench().AcquireStateLock();
  if (!ShouldManageActorDisplay(a_actor, *menu)) {
    return displaySet;
  }

  const bool hideFittingOverrides = menu->HideFittingOverridesForActor(a_actor);
  const auto conditionalVisibility =
      BuildConditionalVisibilityDecisions(menu->GetWorkbench(), a_actor);
  for (const auto &[formID, visible] : conditionalVisibility.actual) {
    if (visible) {
      displaySet.forceVisibleArmorFormIDs.insert(formID);
    }
  }

  const auto equippedArmors = CollectEquippedArmors(a_actor);
  const auto genitalEnvironment =
      sfs::native::genital_compatibility::GetEnvironment();
  const auto *resolvedGenitalArmor =
      sfs::native::GetResolvedGenitalArmor(a_actor);
  if (genitalEnvironment.sosInstalled && !resolvedGenitalArmor) {
    sfs::native::RequestGenitalArmorResolution(a_actor);
  }
  displaySet.genitalArmorEquipped =
      std::ranges::any_of(equippedArmors, [](const auto *a_armor) {
        return IsGenitalArmor(a_armor);
      });
  const bool tngCoverEquipped =
      std::ranges::any_of(equippedArmors, [](const auto *a_armor) {
        return sfs::armor::IsTngGenitalCoverArmor(a_armor);
      });
  const bool actorSkinUsesGenitalSlot = ActorSkinUsesGenitalSlot(a_actor);
  const bool playerSosKeywordFallback =
      genitalEnvironment.sosInstalled && IsPlayerActor(a_actor) &&
      LookupRuntimeKeywords().HasSos();
  const bool sosCompatibilityAvailable =
      sfs::native::genital_compatibility::rules::
          IsSosCompatibilityAvailable(
              genitalEnvironment, resolvedGenitalArmor != nullptr,
              displaySet.genitalArmorEquipped,
              playerSosKeywordFallback);
  const bool tngCompatibilityAvailable =
      sfs::native::genital_compatibility::rules::
          IsTngCompatibilityAvailable(genitalEnvironment,
                                      actorSkinUsesGenitalSlot,
                                      tngCoverEquipped);
  displaySet.genitalCompatibilityAvailable =
      sosCompatibilityAvailable || tngCompatibilityAvailable;

  std::uint32_t occupiedDisplaySlots = 0;
  std::uint32_t forceVisibleRealSlotMask = 0;
  auto suppressedFittingSlots =
      a_applyTemporarySuppression &&
              sfs::workbench::IsModSettingsStripLinkPolicyActive()
          ? sfs::native::GetVirtualTokenSuppressedFittingSlotMask(a_actor)
          : 0;
  if (a_applyTemporarySuppression) {
    // Helmet Toggle is a display-only compatibility state, not an external
    // strip transaction.  It therefore applies to every strip-link policy
    // (mod settings, vanilla slots, and direct mappings) without changing
    // their token/anchor decisions.
    suppressedFittingSlots |=
        sfs::native::GetHeadgearToggleSuppressedFittingSlotMask(a_actor);
    for (const auto *armor : equippedArmors) {
      if (const auto forceVisibleSlotMask = GetProtectedActualSlotMask(armor);
          forceVisibleSlotMask != 0) {
        suppressedFittingSlots |= forceVisibleSlotMask;
        forceVisibleRealSlotMask |= forceVisibleSlotMask;
      }
    }
  }
  const auto actorFormID = a_actor->GetFormID();
  const bool allowRestrictedGeneratedPreview =
      menu->GetWorkbench().IsNativePreviewSelectionForActor(
          actorFormID, "kit-generator:");
  const auto appendRows =
      [&](const std::vector<sfs::workbench::VariantWorkbenchRow> &a_rows,
          const bool a_ignoreConditions,
          const bool a_hideEquippedWhenOverrideDisplayed) {
        const bool allowRestrictedPreview =
            a_ignoreConditions && allowRestrictedGeneratedPreview;
        std::vector<const sfs::workbench::VariantWorkbenchRow *> orderedRows;
        orderedRows.reserve(a_rows.size());
        for (const auto &row : a_rows)
          orderedRows.push_back(&row);
        std::stable_partition(
            orderedRows.begin(), orderedRows.end(),
            [](const auto *row) { return row->HasCondition(); });
        std::uint32_t activeLockedSlotMask = 0;
        for (const auto *rowPtr : orderedRows) {
          const auto &row = *rowPtr;
          if (!row.IsOwnedByActor(a_actor) ||
              (!a_ignoreConditions && !IsRowActiveForActor(row, a_actor))) {
            continue;
          }
          for (const auto &item : row.overrides) {
            if (!item.locked) {
              continue;
            }
            const auto visualSlotMask = static_cast<std::uint32_t>(
                row.GetOverrideVisualSlotMask(item));
            const auto conditionalFitting =
                conditionalVisibility.fitting.find(item.formID);
            const bool userHidden =
                !allowRestrictedPreview &&
                (row.IsProtectedAppearance(item) ||
                 (row.HasCondition()
                      ? item.hidden
                      : conditionalFitting != conditionalVisibility.fitting.end()
                            ? !conditionalFitting->second
                            : (hideFittingOverrides || item.hidden)));
            if (!userHidden) {
              activeLockedSlotMask |= visualSlotMask;
            }
          }
        }
        for (const auto *rowPtr : orderedRows) {
          const auto &row = *rowPtr;
          if (!row.IsOwnedByActor(a_actor) ||
              (!a_ignoreConditions && !IsRowActiveForActor(row, a_actor))) {
            continue;
          }

          const auto rowEquippedSlotMask = GetActorEquippedRowSlotMask(
              row, equippedArmors, displaySet.genitalCompatibilityAvailable);
          const auto conditionalActual =
              conditionalVisibility.actual.find(row.equipped.formID);
          const bool ddOrdinaryVisible =
              sfs::devious_devices::IsDeviousDevicesRenderedDeviceOrdinaryVisible(
                  a_actor->GetFormID(), row.equipped.formID);
          const bool hideEquipped =
              conditionalActual != conditionalVisibility.actual.end()
                  ? !conditionalActual->second
                  : ddOrdinaryVisible
                      ? false
                  : menu->GetWorkbench().ResolveEquippedHiddenForActor(a_actor,
                                                                       row);
          if (hideEquipped && rowEquippedSlotMask != 0) {
            if (row.equipped.formID != 0) {
              displaySet.hiddenArmorFormIDs.insert(row.equipped.formID);
            }
            displaySet.hiddenSlotMask |= rowEquippedSlotMask;
            displaySet.active = true;
          }

          if (row.overrides.empty()) {
            continue;
          }

          bool displayedRowOverride = false;
          std::uint32_t displayedRowSlotMask = 0;
          for (const auto &overrideItem : row.overrides) {
            const auto overrideVisualSlotMask = static_cast<std::uint32_t>(
                row.GetOverrideVisualSlotMask(overrideItem));
            const bool protectedAppearance =
                !allowRestrictedPreview &&
                row.IsProtectedAppearance(overrideItem);
            const auto conditionalFitting =
                conditionalVisibility.fitting.find(overrideItem.formID);
            if (!overrideItem.locked &&
                (overrideVisualSlotMask & activeLockedSlotMask) != 0) {
              continue;
            }
            const bool overrideHidden =
                !allowRestrictedPreview &&
                (protectedAppearance ||
                 row.IsOverrideAutomaticallySuppressed(overrideItem) ||
                 (row.HasCondition()
                      ? overrideItem.hidden
                      : conditionalFitting != conditionalVisibility.fitting.end()
                            ? !conditionalFitting->second
                            : (hideFittingOverrides || overrideItem.hidden)));
            if (overrideHidden) {
              if (!protectedAppearance && row.HasCondition() &&
                  overrideVisualSlotMask != 0) {
                occupiedDisplaySlots |= overrideVisualSlotMask;
                displaySet.active = true;
              }
              continue;
            }

            const auto *armor =
                RE::TESForm::LookupByID<RE::TESObjectARMO>(overrideItem.formID);
            if (!armor ||
                (!allowRestrictedPreview && IsGenitalArmor(armor))) {
              continue;
            }

            const bool appearanceTicketSuppressed =
                !overrideItem.locked && !allowRestrictedPreview &&
                a_applyTemporarySuppression &&
                (overrideVisualSlotMask & suppressedFittingSlots) == 0 &&
                sfs::virtual_tokens::IsVirtualWornTokenAppearanceSuppressed(
                    a_actor->GetFormID(), armor->GetFormID(),
                    overrideVisualSlotMask);

            const auto skinningSlotMask = GetSkinningSlotMask(
                armor, displaySet.genitalCompatibilityAvailable);
            auto slotMask = overrideVisualSlotMask != 0 ? overrideVisualSlotMask
                                                        : skinningSlotMask;
            if (slotMask == 0 || appearanceTicketSuppressed ||
                (!overrideItem.locked && !allowRestrictedPreview &&
                 (slotMask & suppressedFittingSlots) != 0) ||
                (slotMask & occupiedDisplaySlots) != 0) {
              continue;
            }

            occupiedDisplaySlots |= slotMask;
            displaySet.slotMask |= slotMask;
            displaySet.armors.push_back(armor);
            displayedRowOverride = true;
            displayedRowSlotMask |= slotMask;
          }

          if (displayedRowOverride) {
            if (a_hideEquippedWhenOverrideDisplayed) {
              if (rowEquippedSlotMask != 0) {
                if (row.equipped.formID != 0) {
                  displaySet.hiddenArmorFormIDs.insert(row.equipped.formID);
                }
                displaySet.hiddenSlotMask |= rowEquippedSlotMask;
              } else if (displayedRowSlotMask != 0) {
                for (const auto *equippedArmor : equippedArmors) {
                  if (!equippedArmor ||
                      (!allowRestrictedPreview &&
                       IsGenitalArmor(equippedArmor))) {
                    continue;
                  }

                  const auto equippedArmorSlotMask = GetSkinningSlotMask(
                      equippedArmor, displaySet.genitalCompatibilityAvailable);
                  if ((equippedArmorSlotMask & displayedRowSlotMask) == 0) {
                    continue;
                  }

                  displaySet.hiddenArmorFormIDs.insert(
                      equippedArmor->GetFormID());
                  displaySet.hiddenSlotMask |= equippedArmorSlotMask;
                }
              }
            }
            displaySet.active = true;
          }
        }
      };

  const auto *previewRows =
      menu->GetWorkbench().GetNativePreviewRowsForActor(actorFormID);
  const bool previewReplacesRows =
      previewRows != nullptr &&
      menu->GetWorkbench().IsNativePreviewReplacingRowsForActor(actorFormID);
  if (a_applyTemporarySuppression) {
    suppressedFittingSlots |=
        sfs::devious_devices::CalculateDeviousDevicesHiderSuppressedFittingSlotMask(
            a_actor, previewRows, previewReplacesRows,
            suppressedFittingSlots);
  }
  if (previewReplacesRows) {
    for (const auto &row : menu->GetWorkbench().GetRows()) {
      if (!row.IsOwnedByActor(a_actor) || !row.isEquipped || row.IsSlotRow()) {
        continue;
      }

      const auto rowEquippedSlotMask = GetActorEquippedRowSlotMask(
          row, equippedArmors, displaySet.genitalCompatibilityAvailable);
      const auto conditionalActual =
          conditionalVisibility.actual.find(row.equipped.formID);
      const bool ddOrdinaryVisible =
          sfs::devious_devices::IsDeviousDevicesRenderedDeviceOrdinaryVisible(
              a_actor->GetFormID(), row.equipped.formID);
      const bool hideEquipped =
          conditionalActual != conditionalVisibility.actual.end()
              ? !conditionalActual->second
              : ddOrdinaryVisible
                  ? false
              : menu->GetWorkbench().ResolveEquippedHiddenForActor(a_actor,
                                                                   row);
      if (hideEquipped && rowEquippedSlotMask != 0) {
        if (row.equipped.formID != 0) {
          displaySet.hiddenArmorFormIDs.insert(row.equipped.formID);
        }
        displaySet.hiddenSlotMask |= rowEquippedSlotMask;
        displaySet.active = true;
      }
    }
  }
  if (previewRows != nullptr) {
    // A kit preview replaces only the registered-appearance projection. Its
    // actual-equipment visibility must still come from the saved workbench
    // rows and their active conditions above. Do not add a second, temporary
    // hide merely because the preview happens to use the same display slot.
    // Keep the established overlay behavior for catalog previews, which do
    // not replace the saved rows.
    appendRows(*previewRows, true, !previewReplacesRows);
  }
  if (!previewReplacesRows) {
    appendRows(menu->GetWorkbench().GetRows(), false, false);
  }

  // RaceMenu attachment callbacks can run synchronously from the skinning
  // call below, including during the first post-load 3D construction before
  // RefreshArmorFor has executed. Carry the decision made under the same
  // actor-local workbench lock. Preview roots also need subsequent live updates;
  // only their initial attach-time morph application belongs to RaceMenu alone.
  displaySet.trackRegisteredAppearanceMorphNodes =
      sfs::native::racemenu::rules::ShouldTrackRegisteredAppearanceNodes(
          displaySet.active, displaySet.armors.size());
  displaySet.applyInitialNativeMorphs =
      sfs::native::racemenu::rules::ShouldApplyInitialNativeMorphs(
          previewReplacesRows);

  const auto visibleRealArmors =
      CollectVisibleRealArmors(a_actor, displaySet, equippedArmors);
  const auto displayedFittingsConcealGenitals =
      displaySet.genitalCompatibilityAvailable &&
      AnyVisibleArmorConcealsGenitals(a_actor, displaySet.armors);
  const auto displayedFittingsRevealGenitals =
      displaySet.genitalCompatibilityAvailable &&
      AnyVisibleArmorRevealsGenitals(a_actor, displaySet.armors);
  const auto visibleRealArmorsRevealGenitals =
      displaySet.genitalCompatibilityAvailable &&
      AnyVisibleArmorRevealsGenitals(a_actor, visibleRealArmors);
  const auto visibleRealArmorsConcealGenitals =
      displaySet.genitalCompatibilityAvailable &&
      AnyVisibleArmorConcealsGenitals(a_actor, visibleRealArmors);
  const auto hiddenRealArmorRequiresGenitalCorrection =
      displaySet.genitalCompatibilityAvailable &&
      std::ranges::any_of(equippedArmors, [&](const auto *armor) {
        return armor && !sfs::armor::IsSosTngInternalArmor(armor) &&
               !IsRealArmorVisibleInDisplaySet(a_actor, displaySet, armor) &&
               (ShouldRevealGenitals(a_actor, armor) ||
                ShouldVisibleArmorConcealGenitals(a_actor, armor));
      });

  displaySet.concealGenitals =
      displayedFittingsConcealGenitals || visibleRealArmorsConcealGenitals;
  displaySet.genitalCorrectionActive =
      displayedFittingsRevealGenitals || displayedFittingsConcealGenitals ||
      visibleRealArmorsRevealGenitals || visibleRealArmorsConcealGenitals ||
      hiddenRealArmorRequiresGenitalCorrection;

  const auto tngCoverProjection =
      sfs::armor::rules::ResolveGenitalCoverProjection(
          displaySet.genitalCorrectionActive &&
              sfs::armor::rules::ShouldApplyGenitalCoverProjection(
                  genitalEnvironment.tngInstalled,
                  actorSkinUsesGenitalSlot, tngCoverEquipped),
          displaySet.concealGenitals);
  if (displaySet.concealGenitals) {
    for (const auto *armor : equippedArmors) {
      if (IsGenitalArmor(armor)) {
        displaySet.hiddenArmorFormIDs.insert(armor->GetFormID());
      }
    }
    // TNG places its genital geometry in the actor skin and normally equips a
    // blank slot-52 armor based on actual equipment. Registered appearances
    // need the same partition blocker even when the actual outfit is revealing.
    if (tngCoverProjection.occupyGenitalSlot) {
      displaySet.slotMask |= GenitalSlotMask();
    }
  } else if (displaySet.genitalCorrectionActive) {
    // Conversely, an equipped TNG blocker reflects actual gear and must not
    // keep the skin hidden when the effective SFS appearance is revealing.
    for (const auto *armor : equippedArmors) {
      if (tngCoverProjection.hideEquippedCover &&
          sfs::armor::IsTngGenitalCoverArmor(armor)) {
        displaySet.hiddenArmorFormIDs.insert(armor->GetFormID());
      }
    }
    if (!displaySet.genitalArmorEquipped && resolvedGenitalArmor &&
        !displaySet.Contains(resolvedGenitalArmor)) {
      displaySet.armors.push_back(resolvedGenitalArmor);
      displaySet.slotMask |= GetSkinningSlotMask(resolvedGenitalArmor, true);
    }
  }

  displaySet.active = displaySet.active || displaySet.genitalCorrectionActive ||
                      forceVisibleRealSlotMask != 0;
  if (displaySet.active) {
    displaySet.hiddenSlotMask |= forceVisibleRealSlotMask;
  }

  return displaySet;
}

[[nodiscard]] std::unordered_set<const RE::TESObjectARMO *>
CollectEquippedArmors(RE::TESObjectREFR *a_target) {
  std::unordered_set<const RE::TESObjectARMO *> equipped;
  auto *inventory = a_target ? a_target->GetInventoryChanges() : nullptr;
  if (!inventory) {
    return equipped;
  }

  WornArmorVisitor visitor;
  inventory->VisitWornItems(visitor);
  return std::move(visitor.armors);
}

[[nodiscard]] bool ShouldHideRealArmor([[maybe_unused]] RE::Actor *a_actor,
                                       const DisplaySet &a_displaySet,
                                       const RE::TESObjectARMO *a_armor) {
  if (!a_armor || a_displaySet.Contains(a_armor)) {
    return false;
  }

  if (GetProtectedActualSlotMask(a_armor) != 0) {
    return false;
  }

  if (a_displaySet.forceVisibleArmorFormIDs.contains(a_armor->GetFormID())) {
    return false;
  }

  if (IsGenitalArmor(a_armor) && !a_displaySet.concealGenitals) {
    return false;
  }

  if (a_displaySet.hiddenArmorFormIDs.contains(a_armor->GetFormID())) {
    return true;
  }

  const auto slotMask =
      GetSkinningSlotMask(a_armor, a_displaySet.genitalCompatibilityAvailable);
  return slotMask != 0 && (slotMask & a_displaySet.hiddenSlotMask) != 0;
}

[[nodiscard]] RE::TESObjectARMO *
GetEntryArmor(RE::InventoryEntryData *a_entryData) {
  if (!a_entryData || !a_entryData->object) {
    return nullptr;
  }

  return a_entryData->object->As<RE::TESObjectARMO>();
}

// Intercept the known engine InitWornVisitor's three base-interface callbacks,
// not the opaque provider's CALL or its concrete object. The vptr, RTTI and all
// concrete fields remain unchanged. Install once, before gameplay; ordinary
// visits always chain unchanged. Only the exact visitor in a synchronous SFS
// invocation on this thread is filtered. No inventory/extra-data mutation.
class ScopedHiddenWornVisitorFilter final {
public:
  using Visitor = RE::InventoryChanges::IItemChangeVisitor;
  using Entry = RE::InventoryEntryData;
  using Result = RE::BSContainer::ForEachResult;

  ScopedHiddenWornVisitorFilter(Visitor *a_visitor, RE::Actor *a_actor,
                               const DisplaySet &a_displaySet,
                               bool a_filterRequired)
      : previous_(current_), visitor_(a_visitor), actor_(a_actor),
        displaySet_(a_displaySet), filterRequired_(a_filterRequired) {
    current_ = this;
  }
  ~ScopedHiddenWornVisitorFilter() { current_ = previous_; }
  ScopedHiddenWornVisitorFilter(const ScopedHiddenWornVisitorFilter &) = delete;
  ScopedHiddenWornVisitorFilter &operator=(const ScopedHiddenWornVisitorFilter &) = delete;

  static bool Install(std::uintptr_t a_vtable) {
    if (installed_) { return true; }
    if (!a_vtable) { return false; }
    auto *slots = reinterpret_cast<const std::uintptr_t *>(a_vtable) + 1;
    const std::array<std::uintptr_t, 3> original{slots[0], slots[1], slots[2]};
    if (std::ranges::any_of(original, [](auto address) { return address == 0; })) {
      return false;
    }
    originalVisit_ = reinterpret_cast<VisitFn>(original[0]);
    originalShouldVisit_ = reinterpret_cast<ShouldVisitFn>(original[1]);
    originalUnk03_ = reinterpret_cast<Unk03Fn>(original[2]);
    const std::array<std::uintptr_t, 3> replacement{
        reinterpret_cast<std::uintptr_t>(&Visit),
        reinterpret_cast<std::uintptr_t>(&ShouldVisit),
        reinterpret_cast<std::uintptr_t>(&Unk03)};
    // One verified contiguous write; never leave a partially installed set
    // after an expected-bytes mismatch. Destructor slot and COL are untouched.
    installed_ = REL::safe_write(reinterpret_cast<std::uintptr_t>(slots),
        replacement.data(), sizeof(replacement), original.data(), sizeof(original));
    return installed_;
  }

private:
  static bool ShouldSkip(Visitor *a_visitor, Entry *a_entry) {
    for (auto *scope = current_; scope; scope = scope->previous_) {
      if (scope->visitor_ == a_visitor) {
        return scope->filterRequired_ && ShouldHideRealArmor(
            scope->actor_, scope->displaySet_, GetEntryArmor(a_entry));
      }
    }
    return false;
  }
  static Result Visit(Visitor *a_visitor, Entry *a_entry) {
    return ShouldSkip(a_visitor, a_entry) ? Result::kContinue
                                        : originalVisit_(a_visitor, a_entry);
  }
  static bool ShouldVisit(Visitor *a_visitor, Entry *a_entry,
                          RE::TESBoundObject *a_object) {
    return !ShouldSkip(a_visitor, a_entry) &&
           originalShouldVisit_(a_visitor, a_entry, a_object);
  }
  static Result Unk03(Visitor *a_visitor, Entry *a_entry,
                      void *a_arg2, bool *a_arg3) {
    if (ShouldSkip(a_visitor, a_entry)) {
      if (a_arg3) { *a_arg3 = true; }
      return Result::kContinue;
    }
    return originalUnk03_(a_visitor, a_entry, a_arg2, a_arg3);
  }
  using VisitFn = Result (*)(Visitor *, Entry *);
  using ShouldVisitFn = bool (*)(Visitor *, Entry *, RE::TESBoundObject *);
  using Unk03Fn = Result (*)(Visitor *, Entry *, void *, bool *);
  inline static VisitFn originalVisit_{};
  inline static ShouldVisitFn originalShouldVisit_{};
  inline static Unk03Fn originalUnk03_{};
  inline static bool installed_{};
  inline static thread_local ScopedHiddenWornVisitorFilter *current_{};
  ScopedHiddenWornVisitorFilter *previous_;
  Visitor *visitor_;
  RE::Actor *actor_;
  const DisplaySet &displaySet_;
  bool filterRequired_;
};

class HiddenRealEquipmentFilterVisitor final
    : public RE::InventoryChanges::IItemChangeVisitor {
public:
  HiddenRealEquipmentFilterVisitor(
      RE::Actor *a_actor, const DisplaySet &a_displaySet,
      RE::InventoryChanges::IItemChangeVisitor &a_visitor)
      : actor_(a_actor), displaySet_(a_displaySet), visitor_(a_visitor) {}

  RE::BSContainer::ForEachResult
  Visit(RE::InventoryEntryData *a_entryData) override {
    if (ShouldSkip(a_entryData)) {
      return RE::BSContainer::ForEachResult::kContinue;
    }

    return visitor_.Visit(a_entryData);
  }

  bool ShouldVisit(RE::InventoryEntryData *a_entryData,
                   RE::TESBoundObject *a_object) override {
    if (ShouldSkip(a_entryData)) {
      return false;
    }

    return visitor_.ShouldVisit(a_entryData, a_object);
  }

  RE::BSContainer::ForEachResult Unk_03(RE::InventoryEntryData *a_entryData,
                                        void *a_arg2, bool *a_arg3) override {
    if (ShouldSkip(a_entryData)) {
      if (a_arg3) {
        *a_arg3 = true;
      }
      return RE::BSContainer::ForEachResult::kContinue;
    }

    return visitor_.Unk_03(a_entryData, a_arg2, a_arg3);
  }

private:
  [[nodiscard]] bool ShouldSkip(RE::InventoryEntryData *a_entryData) {
    auto *armor = GetEntryArmor(a_entryData);
    if (!ShouldHideRealArmor(actor_, displaySet_, armor)) {
      return false;
    }

    return true;
  }

  RE::Actor *actor_{nullptr};
  const DisplaySet &displaySet_;
  RE::InventoryChanges::IItemChangeVisitor &visitor_;
};

[[nodiscard]] std::uint32_t
CollectVisibleWornSlotMask(RE::TESObjectREFR *a_target,
                           const DisplaySet &a_displaySet) {
  std::uint32_t slotMask = 0;
  for (const auto *armor : CollectEquippedArmors(a_target)) {
    auto *actor = a_target ? a_target->As<RE::Actor>() : nullptr;
    if (ShouldHideRealArmor(actor, a_displaySet, armor)) {
      continue;
    }
    slotMask |=
        GetSkinningSlotMask(armor, a_displaySet.genitalCompatibilityAvailable);
  }
  return slotMask;
}

[[nodiscard]] std::uint32_t
CollectHiddenWornSlotMask(RE::TESObjectREFR *a_target,
                          const DisplaySet &a_displaySet) {
  std::uint32_t slotMask = 0;
  for (const auto *armor : CollectEquippedArmors(a_target)) {
    auto *actor = a_target ? a_target->As<RE::Actor>() : nullptr;
    if (ShouldHideRealArmor(actor, a_displaySet, armor)) {
      slotMask |= GetSkinningSlotMask(
          armor, a_displaySet.genitalCompatibilityAvailable);
    }
  }
  return slotMask;
}

[[nodiscard]] std::uint32_t PreserveUnmanagedHeadgearWornMask(
    RE::Actor *a_actor, const DisplaySet &a_displaySet,
    const std::uint32_t a_baseWornMask, const std::uint32_t a_result) {
  if (!a_actor) {
    return a_result;
  }

  const auto *player = RE::PlayerCharacter::GetSingleton();
  if (player && a_actor->GetFormID() == player->GetFormID()) {
    return a_result;
  }

  const auto headSlot = static_cast<std::uint32_t>(
      std::to_underlying(RE::BGSBipedObjectForm::BipedObjectSlot::kHead));
  const auto hairSlot = static_cast<std::uint32_t>(
      std::to_underlying(RE::BGSBipedObjectForm::BipedObjectSlot::kHair));
  const auto circletSlot = static_cast<std::uint32_t>(
      std::to_underlying(RE::BGSBipedObjectForm::BipedObjectSlot::kCirclet));
  const auto headgearSlotMask = headSlot | hairSlot | circletSlot;

  const bool hasManagedHeadgearFitting =
      (a_displaySet.slotMask & headgearSlotMask) != 0;
  if (hasManagedHeadgearFitting) {
    return a_result;
  }

  return (a_result & ~headgearSlotMask) | (a_baseWornMask & headgearSlotMask);
}

[[nodiscard]] std::vector<RE::FormID>
SortUniqueFormIDs(std::vector<RE::FormID> a_formIDs) {
  std::erase(a_formIDs, RE::FormID{0});
  std::ranges::sort(a_formIDs);
  a_formIDs.erase(std::ranges::unique(a_formIDs).begin(), a_formIDs.end());
  return a_formIDs;
}

[[nodiscard]] std::vector<RE::FormID>
CollectDisplayArmorFormIDs(const DisplaySet &a_displaySet) {
  std::vector<RE::FormID> formIDs;
  formIDs.reserve(a_displaySet.armors.size());
  for (const auto *armor : a_displaySet.armors) {
    formIDs.push_back(armor ? armor->GetFormID() : 0);
  }
  return SortUniqueFormIDs(std::move(formIDs));
}

[[nodiscard]] std::vector<RE::FormID>
CollectHiddenArmorFormIDs(const DisplaySet &a_displaySet) {
  std::vector<RE::FormID> formIDs(a_displaySet.hiddenArmorFormIDs.begin(),
                                  a_displaySet.hiddenArmorFormIDs.end());
  return SortUniqueFormIDs(std::move(formIDs));
}

[[nodiscard]] std::vector<RE::FormID>
CollectHiddenWornArmorFormIDs(RE::Actor *a_actor,
                              const DisplaySet &a_displaySet) {
  std::vector<RE::FormID> formIDs;
  for (const auto *armor : CollectEquippedArmors(a_actor)) {
    if (ShouldHideRealArmor(a_actor, a_displaySet, armor)) {
      formIDs.push_back(armor->GetFormID());
    }
  }
  return SortUniqueFormIDs(std::move(formIDs));
}

[[nodiscard]] DavFallbackRefreshSignature
BuildDavFallbackRefreshSignature(RE::Actor *a_actor,
                                 const DisplaySet &a_displaySet) {
  return {.active = a_displaySet.active,
          .slotMask = a_displaySet.slotMask,
          .hiddenSlotMask = a_displaySet.hiddenSlotMask,
          .releasedActualHairSlotMask =
              sfs::native::helmet_toggle::GetActualHairSlotReleaseMask(
                  a_actor, a_displaySet.slotMask),
          .displayArmorFormIDs = CollectDisplayArmorFormIDs(a_displaySet),
          .hiddenArmorFormIDs = CollectHiddenArmorFormIDs(a_displaySet),
          .hiddenWornArmorFormIDs =
              CollectHiddenWornArmorFormIDs(a_actor, a_displaySet)};
}

[[nodiscard]] EmptyEquipmentDisplaySignature
BuildEmptyEquipmentDisplaySignature(const DisplaySet &a_displaySet) {
  return {.active = a_displaySet.active,
          .slotMask = a_displaySet.slotMask,
          .displayArmorFormIDs = CollectDisplayArmorFormIDs(a_displaySet)};
}

[[nodiscard]] constexpr bool ShouldRebuildEmptyEquipmentDisplay(
    const bool a_hadPreviousSignature, const bool a_signatureChanged,
    const bool a_previousHadDisplayArmors,
    const bool a_currentHasDisplayArmors, const bool a_hasEquippedArmor,
    const bool a_equipmentChangeRefresh) {
  if (a_hasEquippedArmor || a_equipmentChangeRefresh) {
    return false;
  }
  if (!a_hadPreviousSignature) {
    return a_currentHasDisplayArmors;
  }
  return a_signatureChanged &&
         (a_previousHadDisplayArmors || a_currentHasDisplayArmors);
}

static_assert(ShouldRebuildEmptyEquipmentDisplay(false, true, false, true,
                                                 false, false));
static_assert(ShouldRebuildEmptyEquipmentDisplay(true, true, true, false,
                                                 false, false));
static_assert(!ShouldRebuildEmptyEquipmentDisplay(true, true, true, true,
                                                  true, false));
static_assert(!ShouldRebuildEmptyEquipmentDisplay(true, true, true, true,
                                                  false, true));

[[nodiscard]] bool ShouldRunEmptyEquipment3DRefresh(
    const RE::FormID a_actorFormID,
    EmptyEquipmentDisplaySignature a_currentSignature,
    const bool a_hasEquippedArmor, const bool a_equipmentChangeRefresh) {
  if (a_actorFormID == 0) {
    return false;
  }

  std::lock_guard lock(g_emptyEquipmentDisplaySignatureMutex);
  const auto signatureIt =
      g_emptyEquipmentDisplaySignatures.find(a_actorFormID);
  const bool hadPreviousSignature =
      signatureIt != g_emptyEquipmentDisplaySignatures.end();
  const bool signatureChanged =
      !hadPreviousSignature || signatureIt->second != a_currentSignature;
  const bool previousHadDisplayArmors =
      hadPreviousSignature && signatureIt->second.HasDisplayArmors();
  const bool currentHasDisplayArmors = a_currentSignature.HasDisplayArmors();

  if (hadPreviousSignature) {
    signatureIt->second = std::move(a_currentSignature);
  } else {
    g_emptyEquipmentDisplaySignatures.emplace(
        a_actorFormID, std::move(a_currentSignature));
  }

  return ShouldRebuildEmptyEquipmentDisplay(
      hadPreviousSignature, signatureChanged, previousHadDisplayArmors,
      currentHasDisplayArmors, a_hasEquippedArmor,
      a_equipmentChangeRefresh);
}

[[nodiscard]] bool
ShouldRunDavFallback3DRefresh(const RE::FormID a_actorFormID,
                              DavFallbackRefreshSignature a_currentSignature,
                              const bool a_forceActiveRefresh) {
  if (a_actorFormID == 0) {
    return false;
  }

  std::lock_guard lock(g_davFallbackRefreshSignatureMutex);
  const auto signatureIt = g_davFallbackRefreshSignatures.find(a_actorFormID);
  if (signatureIt == g_davFallbackRefreshSignatures.end()) {
    const bool shouldRefresh =
        a_currentSignature.active || a_forceActiveRefresh;
    g_davFallbackRefreshSignatures.emplace(a_actorFormID,
                                           std::move(a_currentSignature));
    return shouldRefresh;
  }

  if (signatureIt->second == a_currentSignature) {
    return a_forceActiveRefresh && a_currentSignature.active;
  }

  const bool shouldRefresh =
      signatureIt->second.active || a_currentSignature.active;
  signatureIt->second = std::move(a_currentSignature);
  return shouldRefresh;
}

void SyncDaveHiddenRealEquipment(RE::Actor *a_actor,
                                 const DisplaySet &a_displaySet) {
  if (!a_actor || !sfs::native::dave::IsApiReady()) {
    return;
  }

  if (!a_displaySet.active) {
    sfs::native::dave::ClearHiddenRealEquipment(a_actor);
    return;
  }

  std::vector<std::string> hiddenSourceAddons;
  for (const auto *armor : CollectEquippedArmors(a_actor)) {
    if (!ShouldHideRealArmor(a_actor, a_displaySet, armor)) {
      continue;
    }

    for (const auto *armorAddon : armor->armorAddons) {
      const auto identifier = sfs::armor::GetFormIdentifier(armorAddon);
      if (!identifier.empty()) {
        hiddenSourceAddons.push_back(identifier);
      }
    }
  }

  sfs::native::dave::SyncHiddenRealEquipment(a_actor, hiddenSourceAddons);
}

namespace re {
enum class EquipFlag : std::uint8_t {
  kNone = 0,
  kNeedsUpdate = 1 << 0,
};

void SetEquipFlag(RE::AIProcess *a_process, EquipFlag a_flag) {
  using Func = void (*)(RE::AIProcess *, EquipFlag);
  static REL::Relocation<Func> func{RELOCATION_ID(38867, 39907)};
  return func(a_process, a_flag);
}

void UpdateEquipment(RE::AIProcess *a_process, RE::Actor *a_actor) {
  using Func = void (*)(RE::AIProcess *, RE::Actor *);
  static REL::Relocation<Func> func{RELOCATION_ID(38404, 39395)};
  return func(a_process, a_actor);
}

bool Update3D(RE::Actor *a_actor) {
  using Func = bool (*)(RE::Actor *);
  static REL::Relocation<Func> func{RELOCATION_ID(19316, 19743)};
  return func(a_actor);
}

bool ApplyArmorAddon(const RE::TESObjectARMO *a_armor, RE::TESRace *a_race,
                     RE::ActorWeightModel *a_model, bool a_isFemale) {
  using Func = bool (*)(const RE::TESObjectARMO *, RE::TESRace *,
                        RE::ActorWeightModel *, bool);
  static REL::Relocation<Func> func{RELOCATION_ID(17392, 17792)};
  return func(a_armor, a_race, a_model, a_isFemale);
}

} // namespace re
} // namespace

namespace sfs::native {
void SetIedVisitWornItemsChainTarget(const std::uintptr_t a_chainTarget) {
  g_iedVisitWornItemsChainTarget.store(a_chainTarget);
}

void SetPassthroughVisitWornItemsChainTarget(const std::uintptr_t a_chainTarget) {
  g_passthroughVisitWornItemsChainTarget.store(a_chainTarget);
}

bool InstallOriginalWornVisitorFilter() {
  // CommonLib's versioned SE/AE table for the engine concrete visitor. The
  // IItemChangeVisitor base slots 1/2/3 are identical on supported flat layouts.
  static REL::Relocation<std::uintptr_t> vtable{RE::VTABLE___InitWornVisitor[0]};
  return ScopedHiddenWornVisitorFilter::Install(vtable.address());
}

void SynchronizeArmorClassificationKeywords(RE::TESObjectARMO *a_armor) {
  SynchronizeArmorClassificationKeywordsImpl(a_armor);
}

ArmorGenitalKeywordOverride
GetArmorGenitalKeywordOverride(const RE::TESObjectARMO *a_armor) {
  return ResolveArmorGenitalKeywordOverride(a_armor);
}

void BeginSOSUserArmorListSync() {
  std::lock_guard lock(g_sosUserArmorMutex);
  g_pendingSOSUserRevealingArmors.clear();
  g_pendingSOSUserConcealingArmors.clear();
  g_sosUserArmorSyncActive = true;
}

void AddSOSUserRevealingArmor(RE::TESForm *a_form) {
  auto *armor = a_form ? a_form->As<RE::TESObjectARMO>() : nullptr;
  if (!armor) {
    return;
  }
  std::lock_guard lock(g_sosUserArmorMutex);
  if (g_sosUserArmorSyncActive) {
    g_pendingSOSUserRevealingArmors.insert(armor->GetFormID());
  }
}

void AddSOSUserConcealingArmor(RE::TESForm *a_form) {
  auto *armor = a_form ? a_form->As<RE::TESObjectARMO>() : nullptr;
  if (!armor) {
    return;
  }
  std::lock_guard lock(g_sosUserArmorMutex);
  if (g_sosUserArmorSyncActive) {
    g_pendingSOSUserConcealingArmors.insert(armor->GetFormID());
  }
}

bool EndSOSUserArmorListSync() {
  bool changed = false;
  {
    std::lock_guard lock(g_sosUserArmorMutex);
    if (!g_sosUserArmorSyncActive) {
      return false;
    }

    changed = g_sosUserRevealingArmors != g_pendingSOSUserRevealingArmors ||
              g_sosUserConcealingArmors != g_pendingSOSUserConcealingArmors;
    g_sosUserRevealingArmors.swap(g_pendingSOSUserRevealingArmors);
    g_sosUserConcealingArmors.swap(g_pendingSOSUserConcealingArmors);
    g_pendingSOSUserRevealingArmors.clear();
    g_pendingSOSUserConcealingArmors.clear();
    g_sosUserArmorSyncActive = false;
  }

  ScheduleLegacyArmorClassificationKeywordMigration();
  return changed;
}

void ClearSOSUserArmorLists() {
  std::lock_guard lock(g_sosUserArmorMutex);
  g_sosUserRevealingArmors.clear();
  g_sosUserConcealingArmors.clear();
  g_pendingSOSUserRevealingArmors.clear();
  g_pendingSOSUserConcealingArmors.clear();
  g_sosUserArmorSyncActive = false;
}

void ClearSFSOwnedRuntimeKeywords() {
  std::vector<std::uint64_t> ownedKeywords;
  {
    std::lock_guard lock(g_runtimeKeywordMutex);
    ownedKeywords.assign(g_sfsOwnedRuntimeKeywords.begin(),
                         g_sfsOwnedRuntimeKeywords.end());
    g_sfsOwnedRuntimeKeywords.clear();
  }

  for (const auto key : ownedKeywords) {
    const auto armorFormID = static_cast<RE::FormID>(key >> 32u);
    const auto keywordFormID = static_cast<RE::FormID>(key);
    auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorFormID);
    auto *keyword = RE::TESForm::LookupByID<RE::BGSKeyword>(keywordFormID);
    if (armor && keyword) {
      armor->RemoveKeyword(keyword);
    }
  }
}

void InvalidateQueuedArmorRefreshes() {
  ++g_armorRefreshGeneration;
  ++g_armorClassificationMigrationScheduleGeneration;
  ClearQueuedActorArmorRefreshes();
  ClearQueuedIedEvaluations();
  ClearDavFallbackRefreshSignatures();
  ClearEmptyEquipmentDisplaySignatures();
  sfs::native::ClearResolvedGenitalArmors();
  ClearSOSUserArmorLists();
  ClearSFSOwnedRuntimeKeywords();
}

void CaptureArmorClassificationKeywordBaseline() {
  std::lock_guard lock(g_armorClassificationMigrationMutex);
  if (g_armorClassificationKeywordBaselineCaptured) {
    return;
  }

  const auto keywords = LookupRuntimeKeywords();
  if (auto *dataHandler = RE::TESDataHandler::GetSingleton()) {
    for (auto *armor : dataHandler->GetFormArray<RE::TESObjectARMO>()) {
      if (!armor) {
        continue;
      }
      for (auto *keyword : BuildRuntimeKeywordArray(keywords)) {
        if (keyword && armor->HasKeyword(keyword)) {
          g_baselineArmorClassificationRuntimeKeywords.insert(
              RuntimeKeywordOwnershipKey(armor, keyword));
        }
      }
    }
  }

  g_armorClassificationKeywordBaselineCaptured = true;
  logger::info("Captured armor classification keyword baseline: {} plugin/KID "
               "keyword assignment(s)",
               g_baselineArmorClassificationRuntimeKeywords.size());
}

void ScheduleLegacyArmorClassificationKeywordMigration() {
  if (!g_armorClassificationMigrationPending.load()) {
    return;
  }

  const auto generation = ++g_armorClassificationMigrationScheduleGeneration;
  std::thread([generation]() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (g_armorClassificationMigrationScheduleGeneration.load() != generation) {
      return;
    }

    auto *taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
      logger::warn("Deferred one-time armor classification keyword migration: "
                   "SKSE task interface is unavailable");
      return;
    }
    taskInterface->AddTask([generation]() {
      if (g_armorClassificationMigrationScheduleGeneration.load() ==
          generation) {
        (void)RunLegacyArmorClassificationKeywordMigration();
      }
    });
  }).detach();
}

void SerializeArmorClassificationMigrationState(
    SKSE::SerializationInterface *a_skse) {
  if (!a_skse) {
    return;
  }

  const auto completedVersion =
      g_completedArmorClassificationMigrationVersion.load();
  a_skse->WriteRecord(kArmorClassificationMigrationSerializationType,
                      kArmorClassificationMigrationSerializationVersion,
                      &completedVersion,
                      static_cast<std::uint32_t>(sizeof(completedVersion)));
}

void DeserializeArmorClassificationMigrationState(
    SKSE::SerializationInterface *a_skse) {
  ++g_armorClassificationMigrationScheduleGeneration;
  g_completedArmorClassificationMigrationVersion.store(0);
  g_armorClassificationMigrationPending.store(true);
  if (!a_skse) {
    return;
  }

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    logger::info("Legacy save detected: one-time armor classification keyword "
                 "migration is pending");
    return;
  }

  if (type != kArmorClassificationMigrationSerializationType ||
      version != kArmorClassificationMigrationSerializationVersion ||
      length != sizeof(std::uint32_t)) {
    logger::warn("Ignoring unsupported armor classification migration record: "
                 "type={:X}, version={}, length={}",
                 type, version, length);
    return;
  }

  std::uint32_t completedVersion = 0;
  if (!a_skse->ReadRecordData(&completedVersion, sizeof(completedVersion))) {
    logger::error("Failed to read armor classification migration state");
    return;
  }

  const bool migrationPending =
      completedVersion < kCurrentArmorClassificationMigrationVersion;
  g_completedArmorClassificationMigrationVersion.store(completedVersion);
  g_armorClassificationMigrationPending.store(migrationPending);
  logger::info("Armor classification keyword migration state: completed={}, "
               "target={}, pending={}",
               completedVersion, kCurrentArmorClassificationMigrationVersion,
               migrationPending);
}

void RevertArmorClassificationMigrationState() {
  ++g_armorClassificationMigrationScheduleGeneration;
  g_completedArmorClassificationMigrationVersion.store(
      kCurrentArmorClassificationMigrationVersion);
  g_armorClassificationMigrationPending.store(false);
}

struct ActiveFittingArmor {
  const RE::TESObjectARMO *armor{nullptr};
  std::uint32_t slotMask{0};
  // Workbench rows can be rebuilt as soon as the state lock is released.
  // Keep owned identities so callers never observe dangling vector elements.
  std::string rowKey;
  std::string itemKey;
};

[[nodiscard]] std::vector<ActiveFittingArmor>
CollectActiveFittingArmors(
    RE::Actor *a_actor,
    const bool a_ignoreVirtualTokenSuppression = false) {
  std::vector<ActiveFittingArmor> activeArmors;
  if (!a_actor) {
    return activeArmors;
  }

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return activeArmors;
  }
  auto workbenchStateLock = menu->GetWorkbench().AcquireStateLock();

  const bool hideBaseFittingOverrides =
      menu->HideFittingOverridesForActor(a_actor);
  const auto conditionalVisibility =
      BuildConditionalVisibilityDecisions(menu->GetWorkbench(), a_actor);

  auto suppressedFittingSlots =
      sfs::workbench::IsModSettingsStripLinkPolicyActive()
          ? sfs::native::GetVirtualTokenSuppressedFittingSlotMask(a_actor)
          : 0;
  suppressedFittingSlots |=
      sfs::native::GetHeadgearToggleSuppressedFittingSlotMask(a_actor);
  const auto externalActualSuppressedSlots =
      sfs::native::external_equipment::GetSuppressedActualSlotMask(
          a_actor->GetFormID());
  const bool usesActualEquipmentLinks =
      sfs::workbench::IsActualEquipmentStripLinkPolicyActive();
  if (a_ignoreVirtualTokenSuppression) {
    suppressedFittingSlots &=
        ~sfs::native::GetVirtualTokenSuppressedFittingSlotMask(a_actor);
  }
  std::uint32_t occupiedDisplaySlots = 0;
  const auto actorFormID = a_actor->GetFormID();
  const bool allowRestrictedGeneratedPreview =
      menu->GetWorkbench().IsNativePreviewSelectionForActor(
          actorFormID, "kit-generator:");
  const auto appendRows =
      [&](const std::vector<sfs::workbench::VariantWorkbenchRow> &a_rows,
          const bool a_ignoreConditions) {
        const bool allowRestrictedPreview =
            a_ignoreConditions && allowRestrictedGeneratedPreview;
        std::vector<const sfs::workbench::VariantWorkbenchRow *> orderedRows;
        orderedRows.reserve(a_rows.size());
        for (const auto &row : a_rows)
          orderedRows.push_back(&row);
        std::stable_partition(
            orderedRows.begin(), orderedRows.end(),
            [](const auto *row) { return row->HasCondition(); });
        std::uint32_t activeLockedSlotMask = 0;
        for (const auto *rowPtr : orderedRows) {
          const auto &row = *rowPtr;
          if (!row.IsOwnedByActor(a_actor) ||
              (!a_ignoreConditions && !IsRowActiveForActor(row, a_actor))) {
            continue;
          }
          for (const auto &item : row.overrides) {
            if (!item.locked) {
              continue;
            }
            const auto visualSlotMask = static_cast<std::uint32_t>(
                row.GetOverrideVisualSlotMask(item));
            const auto conditionalFitting =
                conditionalVisibility.fitting.find(item.formID);
            const bool userHidden =
                !allowRestrictedPreview &&
                (row.IsProtectedAppearance(item) ||
                 (row.HasCondition()
                      ? item.hidden
                      : conditionalFitting != conditionalVisibility.fitting.end()
                            ? !conditionalFitting->second
                            : (hideBaseFittingOverrides || item.hidden)));
            if (!userHidden) {
              activeLockedSlotMask |= visualSlotMask;
            }
          }
        }
        for (const auto *rowPtr : orderedRows) {
          const auto &row = *rowPtr;
          if (!row.IsOwnedByActor(a_actor) ||
              (!a_ignoreConditions && !IsRowActiveForActor(row, a_actor))) {
            continue;
          }

          for (const auto &overrideItem : row.overrides) {
            const auto overrideVisualSlotMask = static_cast<std::uint32_t>(
                row.GetOverrideVisualSlotMask(overrideItem));
            const bool protectedAppearance =
                !allowRestrictedPreview &&
                row.IsProtectedAppearance(overrideItem);
            if (!overrideItem.locked &&
                (overrideVisualSlotMask & activeLockedSlotMask) != 0) {
              continue;
            }
            const bool automaticallySuppressed =
                !overrideItem.locked && !allowRestrictedPreview &&
                (usesActualEquipmentLinks
                    ? overrideItem.automaticEquipmentAnchorSlotMask != 0 &&
                          !overrideItem.automaticEquipmentUserVisible &&
                          (overrideItem.automaticEquipmentAnchorSlotMask &
                           externalActualSuppressedSlots) != 0
                    : row.IsOverrideAutomaticallySuppressed(overrideItem));
            const auto conditionalFitting =
                conditionalVisibility.fitting.find(overrideItem.formID);
            const bool overrideHidden =
                !allowRestrictedPreview &&
                (protectedAppearance || automaticallySuppressed ||
                 (row.HasCondition()
                      ? overrideItem.hidden
                      : conditionalFitting != conditionalVisibility.fitting.end()
                            ? !conditionalFitting->second
                            : (hideBaseFittingOverrides ||
                               overrideItem.hidden)));
            if (overrideHidden) {
              if (!protectedAppearance && row.HasCondition() &&
                  overrideVisualSlotMask != 0) {
                occupiedDisplaySlots |= overrideVisualSlotMask;
              }
              continue;
            }

            const auto *armor =
                RE::TESForm::LookupByID<RE::TESObjectARMO>(overrideItem.formID);
            if (!armor ||
                (!allowRestrictedPreview && IsGenitalArmor(armor))) {
              continue;
            }

            const auto slotMask = overrideVisualSlotMask;
            const bool appearanceTicketSuppressed =
                !overrideItem.locked && !allowRestrictedPreview &&
                !a_ignoreVirtualTokenSuppression &&
                (slotMask & suppressedFittingSlots) == 0 &&
                sfs::virtual_tokens::IsVirtualWornTokenAppearanceSuppressed(
                    a_actor->GetFormID(), armor->GetFormID(), slotMask);
            if (slotMask == 0 || appearanceTicketSuppressed ||
                (!overrideItem.locked && !allowRestrictedPreview &&
                 (slotMask & suppressedFittingSlots) != 0)) {
              continue;
            }

            if ((slotMask & occupiedDisplaySlots) != 0) {
              continue;
            }
            occupiedDisplaySlots |= slotMask;
            activeArmors.push_back({.armor = armor,
                                    .slotMask = slotMask,
                                    .rowKey = row.key,
                                    .itemKey = overrideItem.key});
          }
        }
      };

  const auto *previewRows =
      menu->GetWorkbench().GetNativePreviewRowsForActor(actorFormID);
  const bool previewReplacesRows =
      previewRows != nullptr &&
      menu->GetWorkbench().IsNativePreviewReplacingRowsForActor(actorFormID);
  if (!a_ignoreVirtualTokenSuppression) {
    suppressedFittingSlots |=
        sfs::devious_devices::CalculateDeviousDevicesHiderSuppressedFittingSlotMask(
            a_actor, previewRows, previewReplacesRows,
            suppressedFittingSlots);
  }
  if (previewRows != nullptr) {
    appendRows(*previewRows, true);
  }
  if (!previewReplacesRows) {
    appendRows(menu->GetWorkbench().GetRows(), false);
  }

  std::uint32_t activeSlotMask = 0;
  for (const auto &entry : activeArmors) {
    activeSlotMask |= entry.slotMask;
    logger::debug("SFS native: active fitting entry actor={:08X} armor={:08X} "
                  "slotMask={:08X} name='{}'",
                  actorFormID, entry.armor ? entry.armor->GetFormID() : 0,
                  entry.slotMask,
                  entry.armor ? sfs::armor::GetDisplayName(entry.armor) : "");
  }
  logger::debug("SFS native: active fitting summary actor={:08X} count={} "
                "activeMask={:08X} suppressed={:08X} previewRows={} "
                "previewReplaces={}",
                actorFormID, activeArmors.size(), activeSlotMask,
                suppressedFittingSlots, previewRows != nullptr,
                previewReplacesRows);

  return activeArmors;
}

[[nodiscard]] const RE::TESObjectARMO *
FindActiveFittingArmorForSlot(RE::Actor *a_actor,
                              const std::uint32_t a_slotMask,
                              std::uint32_t *a_armorSlotMask = nullptr,
                              const bool a_ignoreVirtualTokenSuppression =
                                  false) {
  if (!a_actor || a_slotMask == 0) {
    return nullptr;
  }

  for (const auto &entry : CollectActiveFittingArmors(
           a_actor, a_ignoreVirtualTokenSuppression)) {
    if (entry.slotMask == 0 || (entry.slotMask & a_slotMask) == 0) {
      continue;
    }

    if (a_armorSlotMask != nullptr) {
      *a_armorSlotMask = entry.slotMask;
    }
    return entry.armor;
  }

  return nullptr;
}

std::uint32_t GetActiveFittingSlotMask(RE::Actor *a_actor) {
  std::uint32_t slotMask = 0;
  for (const auto &entry : CollectActiveFittingArmors(a_actor)) {
    slotMask |= entry.slotMask;
  }
  return slotMask;
}

std::uint32_t GetDisplayedFittingSlotMask(RE::Actor *a_actor) {
  return GetFinalRenderedOutfitSnapshot(a_actor).additionalSlotMask;
}

[[nodiscard]] static FinalRenderedOutfitSnapshot BuildFinalRenderedOutfitSnapshot(
    RE::Actor *a_actor, const DisplaySet &a_displaySet) {
  FinalRenderedOutfitSnapshot snapshot;
  snapshot.managedBySfs = a_displaySet.active;
  snapshot.additionalSlotMask = a_displaySet.slotMask;
  snapshot.visibleAdditionalArmors = a_displaySet.armors;

  const auto equippedArmors = CollectEquippedArmors(a_actor);
  snapshot.visibleActualArmors =
      CollectVisibleRealArmors(a_actor, a_displaySet, equippedArmors);
  for (const auto *armor : snapshot.visibleActualArmors) {
    if (armor) {
      snapshot.visibleActualSlotMask |= static_cast<std::uint32_t>(
          sfs::armor::GetArmorDisplaySlotMask(armor));
    }
  }
  return snapshot;
}

FinalRenderedOutfitSnapshot GetFinalRenderedOutfitSnapshot(RE::Actor *a_actor) {
  if (!a_actor) {
    return {};
  }
  return BuildFinalRenderedOutfitSnapshot(a_actor, BuildDisplaySet(a_actor));
}

const RE::TESObjectARMO *GetDisplayedFittingArmorForSlot(
    RE::Actor *a_actor, const std::uint32_t a_slotMask) {
  if (!a_actor || a_slotMask == 0) {
    return nullptr;
  }

  const auto snapshot = GetFinalRenderedOutfitSnapshot(a_actor);
  const auto found = std::ranges::find_if(
      snapshot.visibleAdditionalArmors,
      [a_slotMask](const RE::TESObjectARMO *a_armor) {
        return a_armor &&
               (static_cast<std::uint32_t>(
                    sfs::armor::GetArmorDisplaySlotMask(a_armor)) &
                a_slotMask) != 0;
      });
  return found != snapshot.visibleAdditionalArmors.end() ? *found : nullptr;
}

std::optional<bool>
GetDisplayedBodyKeywordState(RE::Actor *a_actor,
                             const RE::BGSKeyword *a_keyword) {
  if (!a_actor || !a_keyword || g_buildDisplaySetDepth != 0) {
    return std::nullopt;
  }

  const auto keywordEditorID = sfs::armor::GetEditorID(a_keyword);
  const bool asksArmorCuirass = keywordEditorID == "ArmorCuirass";
  const bool asksClothingBody = keywordEditorID == "ClothingBody";
  if (!asksArmorCuirass && !asksClothingBody) {
    return std::nullopt;
  }

  // Vanilla already evaluated this keyword. Do not walk an unmanaged NPC's
  // worn inventory merely to return that same answer. Use the exact existing
  // display decision, including conditional real-equipment hides and previews.
  const auto displaySet = BuildDisplaySet(a_actor);
  if (!displaySet.active) {
    return std::nullopt;
  }
  const auto snapshot = BuildFinalRenderedOutfitSnapshot(a_actor, displaySet);

  if (std::ranges::any_of(snapshot.visibleActualArmors,
                          [a_keyword](const auto *a_armor) {
        return a_armor && a_armor->HasKeyword(a_keyword);
      })) {
    return true;
  }

  const auto bodyMask = static_cast<std::uint32_t>(
      sfs::armor::GetArmorSlotMask(32));
  return std::ranges::any_of(
      snapshot.visibleAdditionalArmors,
      [&](const RE::TESObjectARMO *a_armor) {
        if (!a_armor) {
          return false;
        }
        if (a_armor->HasKeyword(a_keyword)) {
          return true;
        }

        // Some appearance-only ARMO records correctly occupy Body but omit
        // the usual vanilla body keyword. Infer only this exact Body case
        // from the record's armor type; do not treat arbitrary accessories or
        // other mod slots as clothing merely because they are visible.
        const auto appearanceMask = static_cast<std::uint32_t>(
            sfs::armor::GetArmorDisplaySlotMask(a_armor));
        if ((appearanceMask & bodyMask) == 0) {
          return false;
        }
        return asksClothingBody ? a_armor->IsClothing()
                                : (a_armor->IsLightArmor() ||
                                   a_armor->IsHeavyArmor());
      });
}

bool IsSFSOwnedRuntimeKeyword(const RE::TESObjectARMO *a_armor,
                              const RE::BGSKeyword *a_keyword) {
  const auto key = RuntimeKeywordOwnershipKey(a_armor, a_keyword);
  if (key == 0) {
    return false;
  }
  std::lock_guard lock(g_runtimeKeywordMutex);
  return g_sfsOwnedRuntimeKeywords.contains(key);
}

std::array<std::optional<ActiveFittingAppearance>, 32>
GetActiveFittingAppearancesBySlot(
    RE::Actor *a_actor, const bool a_ignoreVirtualTokenSuppression) {
  std::array<std::optional<ActiveFittingAppearance>, 32> result;
  const auto entries =
      CollectActiveFittingArmors(a_actor, a_ignoreVirtualTokenSuppression);
  const auto winners = rules::BuildFirstAppearanceSlotLookup(entries);
  for (std::size_t bit = 0; bit < result.size(); ++bit) {
    if (winners[bit] == entries.size()) {
      continue;
    }
    const auto &entry = entries[winners[bit]];
    if (!entry.armor || entry.rowKey.empty() || entry.itemKey.empty()) {
      continue;
    }
    result[bit] = ActiveFittingAppearance{
        .armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(entry.armor->GetFormID()),
        .slotMask = entry.slotMask,
        .rowKey = entry.rowKey,
        .itemKey = entry.itemKey};
  }
  return result;
}

RE::TESObjectARMO *
GetActiveFittingArmorForSlot(RE::Actor *a_actor,
                             const std::uint32_t a_slotMask,
                             const bool a_ignoreVirtualTokenSuppression) {
  const auto *armor = FindActiveFittingArmorForSlot(
      a_actor, a_slotMask, nullptr,
      a_ignoreVirtualTokenSuppression);
  return armor != nullptr
             ? RE::TESForm::LookupByID<RE::TESObjectARMO>(armor->GetFormID())
             : nullptr;
}

std::uint32_t
GetActiveFittingArmorSlotMaskForSlot(RE::Actor *a_actor,
                                     const std::uint32_t a_slotMask,
                                     const bool a_ignoreVirtualTokenSuppression) {
  std::uint32_t armorSlotMask = 0;
  const auto *armor = FindActiveFittingArmorForSlot(
      a_actor, a_slotMask, &armorSlotMask,
      a_ignoreVirtualTokenSuppression);
  (void)armor;
  return armorSlotMask;
}

std::uint32_t GetHiddenRealEquipmentSlotMask(RE::Actor *a_actor) {
  const auto displaySet = BuildDisplaySet(a_actor);
  return CollectHiddenWornSlotMask(a_actor, displaySet);
}

bool IsRealEquipmentHiddenForActorSlots(RE::Actor *a_actor,
                                        const std::uint32_t a_slotMask) {
  if (!a_actor || a_slotMask == 0) {
    return false;
  }

  return (GetHiddenRealEquipmentSlotMask(a_actor) & a_slotMask) != 0;
}

bool ShouldOverrideSkinning(RE::TESObjectREFR *a_target) {
  auto *actor = a_target ? a_target->As<RE::Actor>() : nullptr;
  const auto displaySet = BuildDisplaySet(actor);
  return displaySet.active ||
         sfs::native::helmet_toggle::GetActualHairSlotReleaseMask(
             actor, displaySet.slotMask) != 0;
}

bool ShouldBlockVanillaArmor(RE::TESObjectARMO *a_armor,
                             RE::TESObjectREFR *a_target) {
  if (!a_armor) {
    return false;
  }

  auto *actor = a_target ? a_target->As<RE::Actor>() : nullptr;
  const auto displaySet = BuildDisplaySet(actor);
  return ShouldHideRealArmor(actor, displaySet, a_armor);
}

bool ShouldBlockDavInitWornArmor(RE::TESObjectARMO *a_armor,
                                 RE::TESObjectREFR *a_target) {
  auto *actor = a_target ? a_target->As<RE::Actor>() : nullptr;
  const auto displaySet = BuildDisplaySet(actor);
  const bool blocked = ShouldHideRealArmor(actor, displaySet, a_armor);
  return blocked;
}

std::uint32_t GetDisplayWornMask(RE::InventoryChanges *a_inventory,
                                 RE::TESObjectREFR *a_target,
                                 const std::uint32_t a_baseWornMask) {
  (void)a_inventory;
  auto *actor = a_target ? a_target->As<RE::Actor>() : nullptr;
  const auto displaySet = BuildDisplaySet(actor);
  const auto releasedActualHairSlotMask =
      sfs::native::helmet_toggle::GetActualHairSlotReleaseMask(
          actor, displaySet.slotMask);
  if (!displaySet.active) {
    return a_baseWornMask & ~releasedActualHairSlotMask;
  }

  if (sfs::native::dave::IsDynamicArmorVariantsLoaded() &&
      sfs::native::dave::IsApiReady()) {
    // The chained DAVE GetWornMask result is already resolved through all
    // active variants. In particular, HT2's overrideHead=showAll removes the
    // real helmet's Hair bit here. Rebuilding visible actual equipment from
    // raw ARMO masks during genital correction would put that bit back and
    // make the actor bald. SFS-owned genital/actual hiding is also represented
    // by the DAVE variant synchronized before RefreshActor, so only merge the
    // registered-appearance projection.
    const auto hiddenWornSlots =
        CollectHiddenWornSlotMask(a_target, displaySet);
    const auto visibleWornSlots =
        CollectVisibleWornSlotMask(a_target, displaySet);
    const auto sfsHiddenWornSlots =
        sfs::native::refresh_rules::ResolveSfsHiddenWornSlotMask(
            hiddenWornSlots, visibleWornSlots);
    const auto result =
        sfs::native::refresh_rules::MergeDaveResolvedWornMask(
            a_baseWornMask, displaySet.slotMask, sfsHiddenWornSlots);
    logger::debug("SFS DAVE native: worn-mask actor={:08X} base={:08X} "
                  "displaySlot={:08X} hiddenSlot={:08X} result={:08X} "
                  "sfsHiddenWorn={:08X} hairRelease={:08X} "
                  "genitalCorrection={} apiReady=true",
                  actor ? actor->GetFormID() : 0, a_baseWornMask,
                  displaySet.slotMask, displaySet.hiddenSlotMask, result,
                  sfsHiddenWornSlots, releasedActualHairSlotMask,
                  displaySet.genitalCorrectionActive);
    return result & ~releasedActualHairSlotMask;
  }

  if (sfs::native::dave::IsDynamicArmorVariantsLoaded()) {
    const auto hiddenWornSlots =
        CollectHiddenWornSlotMask(a_target, displaySet);
    const auto visibleWornSlots =
        CollectVisibleWornSlotMask(a_target, displaySet);
    const auto result = PreserveUnmanagedHeadgearWornMask(
        actor, displaySet, a_baseWornMask,
        (a_baseWornMask & ~hiddenWornSlots) | visibleWornSlots |
            displaySet.slotMask);
    return result & ~releasedActualHairSlotMask;
  }

  const auto result = PreserveUnmanagedHeadgearWornMask(
      actor, displaySet, a_baseWornMask,
      displaySet.slotMask |
          CollectVisibleWornSlotMask(a_target, displaySet));
  logger::debug(
      "SFS native: worn-mask actor={:08X} base={:08X} displaySlot={:08X} "
      "hiddenSlot={:08X} result={:08X} daveLoaded=false",
      actor ? actor->GetFormID() : 0, a_baseWornMask, displaySet.slotMask,
      displaySet.hiddenSlotMask, result);
  return result & ~releasedActualHairSlotMask;
}

void ApplyAdditionalDisplayArmors(RE::Actor *a_actor,
                                  RE::ActorWeightModel *a_actorWeightModel) {
  if (!a_actor || !a_actorWeightModel) {
    return;
  }

  const auto displaySet = BuildDisplaySet(a_actor);
  sfs::native::racemenu::SetRegisteredAppearanceDisplayActive(
      a_actor, displaySet.trackRegisteredAppearanceMorphNodes,
      displaySet.applyInitialNativeMorphs);
  if (!displaySet.active || displaySet.armors.empty()) {
    return;
  }

  auto *actorBase = a_actor->GetActorBase();
  if (!actorBase) {
    return;
  }

  auto *race = actorBase->GetRace();
  if (!race) {
    race = a_actor->GetRace();
  }
  if (!race) {
    return;
  }

  const auto equippedArmors = CollectEquippedArmors(a_actor);
  const bool isFemale = actorBase->IsFemale();
  for (const auto *armor : displaySet.armors) {
    if (!armor || !sfs::armor::HasArmorAddons(armor)) {
      continue;
    }

    if (equippedArmors.contains(armor)) {
      continue;
    }

    const auto attachmentScene =
        sfs::native::racemenu::CaptureAttachmentScene(a_actor);
    re::ApplyArmorAddon(armor, race, a_actorWeightModel, isFemale);
    sfs::native::racemenu::MorphNewRegisteredAppearanceNodes(
        a_actor, attachmentScene, armor->GetFormID(),
        displaySet.applyInitialNativeMorphs);
  }
  sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(a_actor);
}

void VisitWornItemsWithHiddenRealEquipmentFilter(
    RE::InventoryChanges *a_inventory,
    RE::InventoryChanges::IItemChangeVisitor *a_visitor,
    RE::TESObjectREFR *a_target, const std::uintptr_t a_visitWornItems) {
  using VisitWornItems = void (*)(RE::InventoryChanges *,
                                  RE::InventoryChanges::IItemChangeVisitor *);
  auto *visitWornItems = reinterpret_cast<VisitWornItems>(a_visitWornItems);
  if (!visitWornItems || !a_inventory || !a_visitor) {
    return;
  }

  auto *target = a_target ? a_target : a_inventory->owner;
  auto *actor = target ? target->As<RE::Actor>() : nullptr;
  const auto displaySet = BuildDisplaySet(actor);
  const auto filterRequired =
      displaySet.active &&
      CollectHiddenWornSlotMask(target, displaySet) != 0;
  const auto iedChainTarget = g_iedVisitWornItemsChainTarget.load();
  const auto route = sfs::native::ied::rules::ResolveVisitorRoute(
      filterRequired, a_visitWornItems, iedChainTarget,
      g_passthroughVisitWornItemsChainTarget.load());
  // Also shadow an enclosing opaque scope on an explicit nested dispatch that
  // reuses the same visitor but selects another actor/route.
  ScopedHiddenWornVisitorFilter scope{
      a_visitor, actor, displaySet,
      filterRequired && route == sfs::native::ied::rules::VisitorRoute::
                                     CurrentTargetWithOriginalVisitorFilter};
  if (route == sfs::native::ied::rules::VisitorRoute::
                   CurrentTargetWithOriginalVisitorFilter) {
    visitWornItems(a_inventory, a_visitor);
    return;
  }
  if (route == sfs::native::ied::rules::VisitorRoute::
                   CurrentTargetUnfiltered) {
    visitWornItems(a_inventory, a_visitor);
    return;
  }

  HiddenRealEquipmentFilterVisitor visitor{actor, displaySet, *a_visitor};
  if (route == sfs::native::ied::rules::VisitorRoute::
                   OriginalEngineWithSfsFilterThenIedEvaluate) {
    // IED's hook accepts a concrete InitWornVisitor reference. Passing SFS's
    // generic filtering visitor through that hook violates its ABI and can
    // crash during an equipment rebuild. Filter through the original engine
    // function, then queue IED's own public actor refresh.
    static REL::Relocation<std::uintptr_t> originalVisitWornItemsRelocation{
        RELOCATION_ID(15856, 16096)};
    auto *originalVisitWornItems = reinterpret_cast<VisitWornItems>(
        originalVisitWornItemsRelocation.address());
    if (originalVisitWornItems) {
      originalVisitWornItems(a_inventory, &visitor);
      QueueIedEvaluate(actor);
      return;
    }

    logger::error(
        "SFS IED compatibility: original VisitWornItems relocation is unavailable; skipped filtered custom-skin visit to avoid an unsafe IED visitor call");
    return;
  }

  visitWornItems(a_inventory, &visitor);
}

bool IsDisplayedFittingArmor(RE::Actor *a_actor,
                             const RE::TESObjectARMO *a_armor) {
  if (!a_actor || !a_armor) {
    return false;
  }
  const auto snapshot = GetFinalRenderedOutfitSnapshot(a_actor);
  return std::ranges::find(snapshot.visibleAdditionalArmors, a_armor) !=
         snapshot.visibleAdditionalArmors.end();
}

bool IsArmorShownForActor(RE::Actor *a_actor,
                          const RE::TESObjectARMO *a_armor) {
  if (!a_actor || !a_armor) {
    return false;
  }

  const auto snapshot = GetFinalRenderedOutfitSnapshot(a_actor);
  return std::ranges::find(snapshot.visibleAdditionalArmors, a_armor) !=
             snapshot.visibleAdditionalArmors.end() ||
         std::ranges::find(snapshot.visibleActualArmors, a_armor) !=
             snapshot.visibleActualArmors.end();
}

bool AnyShownArmorForActor(RE::Actor *a_actor,
                           const ShownArmorPredicate a_predicate,
                           void *a_context) {
  if (!a_actor || !a_predicate) {
    return false;
  }

  const auto snapshot = GetFinalRenderedOutfitSnapshot(a_actor);
  for (const auto *armor : snapshot.visibleActualArmors) {
    if (a_predicate(armor, a_context)) {
      return true;
    }
  }
  return std::ranges::any_of(
      snapshot.visibleAdditionalArmors,
      [a_predicate, a_context](const auto *armor) {
        return a_predicate(armor, a_context);
      });
}

bool IsActorVisuallyNakedForSlots(RE::Actor *a_actor,
                                  const std::uint32_t a_slotMask) {
  if (!a_actor || a_slotMask == 0) {
    return false;
  }

  const auto snapshot = GetFinalRenderedOutfitSnapshot(a_actor);
  return sfs::native::final_outfit::rules::IsVisuallyNakedForSlots(
      snapshot.visibleActualSlotMask, snapshot.additionalSlotMask, a_slotMask);
}

void RefreshArmorFor(RE::Actor *a_actor, const ArmorRefreshReason a_reason) {
  if (!IsActorRefreshable(a_actor)) {
    return;
  }

  auto *menu = sfs::Menu::GetSingleton();
  if (!menu || !menu->IsGameDataLoaded()) {
    return;
  }

  // Reconcile only this already-refreshing actor. This restores an NPC or
  // follower's actor-local HT2 state after its 3D is rebuilt without polling
  // the world and without recursively scheduling another backend refresh.
  sfs::native::helmet_toggle::SynchronizeActor(a_actor, false);

  const auto displaySet = BuildDisplaySet(a_actor);
  // Publish actor-local live-update eligibility before every backend dispatch.
  // Initial preview morphing is separated at the native capture call site.
  sfs::native::racemenu::SetRegisteredAppearanceDisplayActive(
      a_actor, displaySet.trackRegisteredAppearanceMorphNodes,
      displaySet.applyInitialNativeMorphs);

  // Do not discard remembered BodyMorph nodes before asking the backend to
  // refresh. DAVE may keep unchanged attachments and emit no replacement
  // OnAttach event. RaceMenuBodyMorph prunes nodes only after they actually
  // leave the actor's scene or registered appearance set.
  SyncDaveHiddenRealEquipment(a_actor, displaySet);

  const bool equipmentChangeRefresh =
      a_reason == ArmorRefreshReason::kEquipmentChange;
  const auto equippedArmors = CollectEquippedArmors(a_actor);
  const bool emptyEquipment3DRefresh = ShouldRunEmptyEquipment3DRefresh(
      a_actor->GetFormID(), BuildEmptyEquipmentDisplaySignature(displaySet),
      !equippedArmors.empty(), equipmentChangeRefresh);

  const bool daveApiReady = sfs::native::dave::IsApiReady();
  const bool davLoaded = !daveApiReady &&
                         sfs::native::dave::IsDynamicArmorVariantsLoaded();
  bool davFallback3DRefresh = false;
  if (davLoaded) {
    const auto signature =
        BuildDavFallbackRefreshSignature(a_actor, displaySet);
    davFallback3DRefresh = ShouldRunDavFallback3DRefresh(
        a_actor->GetFormID(), signature, equipmentChangeRefresh);
  }
  auto *process = !daveApiReady && !davLoaded && !emptyEquipment3DRefresh
                      ? a_actor->GetActorRuntimeData().currentProcess
                      : nullptr;
  const auto plan = refresh_rules::BuildPlan(
      {.daveApiReady = daveApiReady,
       .davLoaded = davLoaded,
       .emptyEquipment3DRefresh = emptyEquipment3DRefresh,
       .davFallback3DRefresh = davFallback3DRefresh,
       .nativeProcessAvailable = process != nullptr});
  const auto queueFollowups = [&]() {
    if (plan.queuePoseSync) {
      QueuePausedReplacementPreviewPoseSync(a_actor);
    }
    if (plan.queueDyeRestore) {
      dye::QueueSavedWorldTintRestore(a_actor);
    }
    if (plan.queueHighHeelSync) {
      sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(a_actor);
    }
  };

  switch (plan.backend) {
  case refresh_rules::Backend::DaveEmptyEquipment3D:
    // DAVE's public refresh remains authoritative for ordinary SFS rebuilds.
    // With no worn ARMO, bootstrap only this actor's changed display signature.
    logger::debug(
        "Refreshing empty-equipment actor {:08X} with Actor::Update3D for DAVE registered-appearance bootstrap",
        a_actor->GetFormID());
    re::Update3D(a_actor);
    queueFollowups();
    return;
  case refresh_rules::Backend::DaveApi: {
    const auto reason =
        equipmentChangeRefresh ? "equipment change" : "display state";
    logger::debug("Refreshing actor {:08X} with DAVE API for {}",
                  a_actor->GetFormID(), reason);
    refresh_rules::RunDaveRefresh(
        [&] { return sfs::native::dave::RefreshActor(a_actor); }, [&] {
      logger::warn("DAVE {} refresh failed for actor {:08X}; rebuilding this actor's 3D through the existing DAVE engine hooks", reason,
                   a_actor->GetFormID());
      // A rejected API refresh must not strand display/dye/heel changes. This
      // is an actor-local engine rebuild, not a native backend ownership switch;
      // DAVE still owns its installed armor hooks and active variants.
      re::Update3D(a_actor);
    }, queueFollowups);
    return;
  }
  case refresh_rules::Backend::DavFallback3D:
    logger::debug("Refreshing actor {:08X} with Actor::Update3D for DAV "
                  "native fallback reason={}",
                  a_actor->GetFormID(),
                  equipmentChangeRefresh ? "equipment-change"
                                         : "display-state");
    re::Update3D(a_actor);
    queueFollowups();
    return;
  case refresh_rules::Backend::NativeEmptyEquipment3D:
    logger::debug(
        "Refreshing empty-equipment actor {:08X} with Actor::Update3D for native registered-appearance bootstrap",
        a_actor->GetFormID());
    re::Update3D(a_actor);
    queueFollowups();
    return;
  case refresh_rules::Backend::NativeEquipment:
    re::SetEquipFlag(process, re::EquipFlag::kNeedsUpdate);
    re::UpdateEquipment(process, a_actor);
    queueFollowups();
    return;
  case refresh_rules::Backend::None:
  default:
    return;
  }
}

void QueueArmorRefreshFor(RE::Actor *a_actor,
                          const ArmorRefreshReason a_reason) {
  if (!a_actor) {
    return;
  }

  const auto actorFormID = a_actor->GetFormID();
  if (actorFormID == 0) {
    return;
  }
  const auto generation = g_armorRefreshGeneration.load();
  const auto actorRefreshGeneration =
      QueueActorArmorRefreshGeneration(actorFormID);

  auto *taskInterface = SKSE::GetTaskInterface();
  if (!taskInterface) {
    logger::warn(
        "Skipped armor refresh for {:08X}: SKSE task interface is unavailable",
        actorFormID);
    return;
  }

  taskInterface->AddTask(
      [actorFormID, generation, actorRefreshGeneration, a_reason]() {
    if (g_armorRefreshGeneration.load() != generation) {
      return;
    }
    if (!IsLatestActorArmorRefreshGeneration(actorFormID,
                                             actorRefreshGeneration)) {
      return;
    }
    auto *menu = sfs::Menu::GetSingleton();
    if (!menu || !menu->IsGameDataLoaded()) {
      return;
    }
    auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
        RefreshArmorFor(actor, a_reason);
      });
}
void QueuePlayerArmorRefresh() {
  QueueArmorRefreshFor(RE::PlayerCharacter::GetSingleton());
}
} // namespace sfs::native
