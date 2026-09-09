#include "native/OpenAnimationReplacerIntegration.h"

#include "ArmorUtils.h"
#include "Plugin.h"
#include "native/ArmorSkinning.h"
#include "third_party/oar/OARConditionsAPI.h"

#include <cmath>
#include <mutex>
#include <string>

namespace {
using OARResult = OAR_API::Conditions::APIResult;

class IsShownArmorEquipped final : public Conditions::CustomCondition {
public:
  static constexpr std::string_view CONDITION_NAME{"SFS_IsShownArmorEquipped"};

  IsShownArmorEquipped() {
    armor_ = static_cast<Conditions::IFormConditionComponent *>(
        AddBaseComponent(Conditions::ConditionComponentType::kForm, "Armor",
                         "Armor that must be present in the final rendered outfit."));
  }

  [[nodiscard]] RE::BSString GetName() const override {
    return CONDITION_NAME.data();
  }
  [[nodiscard]] RE::BSString GetDescription() const override {
    return "Checks final shown armor: visible real equipment plus active SFS appearances.";
  }
  [[nodiscard]] REL::Version GetRequiredVersion() const override {
    return {1, 4, 4};
  }

protected:
  bool EvaluateImpl(RE::TESObjectREFR *a_refr, RE::hkbClipGenerator *,
                    void *) const override {
    const auto *armor = armor_ && armor_->GetTESFormValue()
                            ? armor_->GetTESFormValue()->As<RE::TESObjectARMO>()
                            : nullptr;
    return sfs::native::IsArmorShownForActor(a_refr ? a_refr->As<RE::Actor>()
                                                    : nullptr,
                                               armor);
  }

private:
  Conditions::IFormConditionComponent *armor_{nullptr};
};

class ShownArmorHasKeyword final : public Conditions::CustomCondition {
public:
  static constexpr std::string_view CONDITION_NAME{"SFS_ShownArmorHasKeyword"};

  ShownArmorHasKeyword() {
    keyword_ = static_cast<Conditions::IKeywordConditionComponent *>(
        AddBaseComponent(Conditions::ConditionComponentType::kKeyword, "Keyword",
                         "Keyword required on final shown armor."));
  }

  [[nodiscard]] RE::BSString GetName() const override {
    return CONDITION_NAME.data();
  }
  [[nodiscard]] RE::BSString GetDescription() const override {
    return "Checks a keyword on final shown armor, not only technically worn armor.";
  }
  [[nodiscard]] REL::Version GetRequiredVersion() const override {
    return {1, 4, 4};
  }

protected:
  bool EvaluateImpl(RE::TESObjectREFR *a_refr, RE::hkbClipGenerator *,
                    void *) const override {
    if (!keyword_) {
      return false;
    }
    return sfs::native::AnyShownArmorForActor(
        a_refr ? a_refr->As<RE::Actor>() : nullptr,
        [](const RE::TESObjectARMO *a_armor, void *a_context) {
          const auto *keyword =
              static_cast<const Conditions::IKeywordConditionComponent *>(
                  a_context);
          return a_armor && keyword && keyword->HasKeyword(a_armor);
        },
        keyword_);
  }

private:
  Conditions::IKeywordConditionComponent *keyword_{nullptr};
};

// OAR's numeric BipedObjectSlot values are zero-based: 0 is Head (30),
// 2 is Body (32), and 31 is FX (61).  Keep the conversion local to this
// condition so other SFS APIs continue to use Skyrim's 30..61 slot numbers.
class ShownArmorInSlotHasKeyword final : public Conditions::CustomCondition {
public:
  static constexpr std::string_view CONDITION_NAME{
      "SFS_IsShownArmorInSlotHasKeyword"};

  ShownArmorInSlotHasKeyword() {
    slot_ = static_cast<Conditions::INumericConditionComponent *>(
        AddBaseComponent(Conditions::ConditionComponentType::kNumeric, "Slot",
                         "Zero-based biped slot index (0=Head/30, 2=Body/32)."));
    keyword_ = static_cast<Conditions::IKeywordConditionComponent *>(
        AddBaseComponent(Conditions::ConditionComponentType::kKeyword, "Keyword",
                         "Keyword required on final shown armor in Slot."));
  }

  [[nodiscard]] RE::BSString GetName() const override {
    return CONDITION_NAME.data();
  }
  [[nodiscard]] RE::BSString GetDescription() const override {
    return "Checks a keyword on final shown armor in one biped slot, not only technically worn armor.";
  }
  [[nodiscard]] REL::Version GetRequiredVersion() const override {
    return {1, 4, 4};
  }

protected:
  bool EvaluateImpl(RE::TESObjectREFR *a_refr, RE::hkbClipGenerator *,
                    void *) const override {
    if (!slot_ || !keyword_) {
      return false;
    }

    const auto slotValue = slot_->GetNumericValue(a_refr);
    constexpr float kFirstBipedIndex = 0.0F;
    constexpr float kOnePastLastBipedIndex = 32.0F;
    if (!std::isfinite(slotValue) || std::trunc(slotValue) != slotValue ||
        slotValue < kFirstBipedIndex || slotValue >= kOnePastLastBipedIndex) {
      return false;
    }

    const auto armorSlotNumber =
        static_cast<std::uint32_t>(slotValue) + 30U;
    const auto armorSlotMask = static_cast<std::uint32_t>(
        sfs::armor::GetArmorSlotMask(armorSlotNumber));
    if (armorSlotMask == 0) {
      return false;
    }

    struct Context {
      std::uint32_t slotMask;
      const Conditions::IKeywordConditionComponent *keyword;
    };
    Context context{armorSlotMask, keyword_};
    return sfs::native::AnyShownArmorForActor(
        a_refr ? a_refr->As<RE::Actor>() : nullptr,
        [](const RE::TESObjectARMO *a_armor, void *a_context) {
          const auto *context = static_cast<const Context *>(a_context);
          return a_armor && context && context->keyword &&
                 (static_cast<std::uint32_t>(
                      sfs::armor::GetArmorDisplaySlotMask(a_armor)) &
                  context->slotMask) != 0 &&
                 context->keyword->HasKeyword(a_armor);
        },
        &context);
  }

private:
  Conditions::INumericConditionComponent *slot_{nullptr};
  Conditions::IKeywordConditionComponent *keyword_{nullptr};
};

class IsShownBodyNaked final : public Conditions::CustomCondition {
public:
  static constexpr std::string_view CONDITION_NAME{"SFS_IsShownBodyNaked"};

  [[nodiscard]] RE::BSString GetName() const override {
    return CONDITION_NAME.data();
  }
  [[nodiscard]] RE::BSString GetDescription() const override {
    return "Checks whether the final rendered outfit has no armor covering Body slot 32.";
  }
  [[nodiscard]] REL::Version GetRequiredVersion() const override {
    return {1, 4, 4};
  }

protected:
  bool EvaluateImpl(RE::TESObjectREFR *a_refr, RE::hkbClipGenerator *,
                    void *) const override {
    const auto kBodySlotMask = static_cast<std::uint32_t>(
        sfs::armor::GetArmorSlotMask(32));
    return sfs::native::IsActorVisuallyNakedForSlots(
        a_refr ? a_refr->As<RE::Actor>() : nullptr, kBodySlotMask);
  }
};

template <class T> bool RegisterCondition() {
  // The outer startup mutex serializes this per-condition success latch.
  // A failed attempt must not make any condition permanently unavailable.
  static bool registered = false;
  if (registered) { return true; }
  const auto result = OAR_API::Conditions::GetAPI()
                          ->AddCustomCondition(SKSE::GetPluginHandle(),
                                               Plugin::NAME.data(), Plugin::VERSION,
                                               T::CONDITION_NAME.data(),
                                               Conditions::CustomCondition::GetFactory<T>());
  switch (result) {
  case OARResult::OK:
    registered = true;
    logger::info("Registered Open Animation Replacer condition {}",
                 T::CONDITION_NAME);
    break;
  case OARResult::AlreadyRegistered:
    registered = true;
    logger::info("Open Animation Replacer condition {} is already registered",
                 T::CONDITION_NAME);
    break;
  default:
    logger::warn("Could not register Open Animation Replacer condition {} (result={})",
                 T::CONDITION_NAME, static_cast<int>(result));
    break;
  }
  return registered;
}

bool RegisterConditionsImpl() {
  if (!OAR_API::Conditions::GetAPI()) {
    logger::info("Open Animation Replacer Conditions API is not available yet; SFS will retry at later startup fences");
    return false;
  }
  // Evaluate all four even when one fails. Completed registrations stay put.
  const bool armor = RegisterCondition<IsShownArmorEquipped>();
  const bool keyword = RegisterCondition<ShownArmorHasKeyword>();
  const bool slot = RegisterCondition<ShownArmorInSlotHasKeyword>();
  const bool naked = RegisterCondition<IsShownBodyNaked>();
  return armor && keyword && slot && naked;
}
} // namespace

namespace sfs::native::oar {
void RegisterConditions() {
  static std::mutex mutex;
  static bool complete = false;
  const std::scoped_lock lock(mutex);
  if (!complete) { complete = RegisterConditionsImpl(); }
}
} // namespace sfs::native::oar
