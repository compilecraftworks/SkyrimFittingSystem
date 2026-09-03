#include "catalog/BodyFamily.h"

#include "ArmorUtils.h"
#include "native/FittingDye.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sfs::body_family {
namespace {
struct ArmorEvidence {
  Mask explicitFamilies{0};
  std::array<bool, 2> hasThirdPersonModel{false, false};
};

struct ActorSignature {
  RE::FormID skinFormID{0};
  RE::FormID raceFormID{0};
  std::uintptr_t thirdPersonRoot{0};
  Sex sex{Sex::Male};

  [[nodiscard]] bool operator==(const ActorSignature &) const = default;
};

struct ActorCacheEntry {
  ActorSignature signature;
  Mask family{0};
};

std::mutex g_cacheMutex;
std::unordered_map<RE::FormID, ActorCacheEntry> g_actorCache;
std::array<std::optional<Mask>, 2> g_installedDefaultCache;

constexpr std::size_t SexIndex(const Sex a_sex) {
  return a_sex == Sex::Female ? 1U : 0U;
}

Sex ActorSex(const RE::Actor *a_actor) {
  const auto *base = a_actor ? a_actor->GetActorBase() : nullptr;
  return base && base->GetSex() == RE::SEX::kFemale ? Sex::Female : Sex::Male;
}

void AppendSignal(std::string &a_target, const std::string_view a_value) {
  if (a_value.empty()) {
    return;
  }
  if (!a_target.empty()) {
    a_target.push_back(' ');
  }
  a_target.append(a_value);
}

void AppendFormSignals(std::string &a_target, const RE::TESForm *a_form) {
  if (!a_form) {
    return;
  }
  AppendSignal(a_target, armor::GetPluginName(a_form));
  AppendSignal(a_target, armor::GetEditorID(a_form));
  if (const auto *name = a_form->GetName(); name && name[0] != '\0') {
    AppendSignal(a_target, name);
  }
}

ArmorEvidence CollectArmorEvidence(const RE::TESObjectARMO *a_armor) {
  ArmorEvidence evidence;
  if (!a_armor) {
    return evidence;
  }

  std::string commonSignals;
  AppendFormSignals(commonSignals, a_armor);
  const auto *keywordForm =
      static_cast<const RE::BGSKeywordForm *>(a_armor);
  for (std::uint32_t index = 0; index < keywordForm->GetNumKeywords(); ++index) {
    if (const auto keyword = keywordForm->GetKeywordAt(index);
        keyword && keyword.value()) {
      AppendFormSignals(commonSignals, keyword.value());
    }
  }

  std::array<std::string, 2> sexSignals;
  for (const auto *addon : a_armor->armorAddons) {
    if (!addon) {
      continue;
    }
    AppendFormSignals(commonSignals, addon);

    for (const auto sex : {Sex::Male, Sex::Female}) {
      const auto index = SexIndex(sex);
      const auto appendModel = [&](const RE::TESModelTextureSwap &a_model,
                                   const bool a_countsAsThirdPerson) {
        if (const auto *path = a_model.GetModel(); path && path[0] != '\0') {
          AppendSignal(sexSignals[index], path);
          evidence.hasThirdPersonModel[index] |= a_countsAsThirdPerson;
        }
      };
      appendModel(addon->bipedModels[index], true);
      appendModel(addon->bipedModel1stPersons[index], false);
    }
  }

  for (const auto sex : {Sex::Male, Sex::Female}) {
    const auto index = SexIndex(sex);
    std::string signals = commonSignals;
    AppendSignal(signals, sexSignals[index]);
    evidence.explicitFamilies |= DetectText(signals, sex);
  }
  return evidence;
}

Mask SelectSingleExplicitFamily(const Mask a_detected, const Sex a_sex) {
  const auto candidates = a_detected & NonVanillaFamilies(a_sex);
  return std::popcount(candidates) == 1 ? candidates : 0;
}

Mask DetectLoadedSkinFamily(RE::Actor *a_actor, RE::TESObjectARMO *a_skin,
                            const Sex a_sex) {
  if (!a_actor || !a_skin || !a_actor->Is3DLoaded()) {
    return 0;
  }

  std::vector<std::string> skinTokens;
  skinTokens.push_back(armor::FormatFormID(a_skin->GetFormID()));
  for (const auto *addon : a_skin->armorAddons) {
    if (addon) {
      skinTokens.push_back(armor::FormatFormID(addon->GetFormID()));
    }
  }

  Mask detected = 0;
  for (const auto &shape : native::dye::ScanLoadedActorShapes(a_actor)) {
    if (shape.firstPerson ||
        !std::ranges::any_of(skinTokens, [&](const std::string &a_token) {
          return shape.scenePath.find(a_token) != std::string::npos;
        })) {
      continue;
    }

    std::string signals = shape.shapeName;
    AppendSignal(signals, shape.diffuseTexture);
    AppendSignal(signals, shape.scenePath);
    detected |= DetectText(signals, a_sex);
  }
  return SelectSingleExplicitFamily(detected, a_sex);
}

std::string LowerAscii(std::string a_value) {
  std::ranges::transform(a_value, a_value.begin(),
                         [](const unsigned char a_character) {
                           return static_cast<char>(std::tolower(a_character));
                         });
  return a_value;
}

Mask FrameworkPluginFamily(const std::string_view a_filename,
                           const Sex a_sex) {
  const auto name = LowerAscii(std::string(a_filename));
  if (a_sex == Sex::Female) {
    if (name == "cbbe.esp" || name == "3ba.esp" ||
        name.find("racemenumorphscbbe") != std::string::npos ||
        name.find("cbbe3ba") != std::string::npos ||
        name.find("cbbe 3ba") != std::string::npos) {
      return Bit(Family::Cbbe);
    }
    if (name == "bhunp.esp" || name == "bhunp3bbb.esp" ||
        name.find("racemenumorphsbhunp") != std::string::npos ||
        name.find("racemenumorphsuunp") != std::string::npos) {
      return Bit(Family::Unp);
    }
    if (name == "ube.esp" || name.starts_with("ube_") ||
        name.find("ultimatebodyenhancer") != std::string::npos) {
      return Bit(Family::Ube);
    }
    return 0;
  }

  if (name == "himbo.esp" ||
      name.find("racemenumorphshimbo") != std::string::npos) {
    return Bit(Family::Himbo);
  }
  if (name == "sam.esp" || name == "sam.esm" || name == "samlight.esp" ||
      name == "sam_light.esp" || name.starts_with("sam_light_") ||
      name.find("racemenumorphssam") != std::string::npos) {
    return Bit(Family::Sam);
  }
  return 0;
}

Mask DetectInstalledDefault(const Sex a_sex) {
  const auto cacheIndex = SexIndex(a_sex);
  {
    std::lock_guard lock(g_cacheMutex);
    if (g_installedDefaultCache[cacheIndex].has_value()) {
      return *g_installedDefaultCache[cacheIndex];
    }
  }

  Mask detected = 0;
  if (const auto *dataHandler = RE::TESDataHandler::GetSingleton()) {
    const auto collect = [&](const RE::TESFile *const *a_files,
                             const std::size_t a_count) {
      if (!a_files) {
        return;
      }
      for (std::size_t index = 0; index < a_count; ++index) {
        const auto *file = a_files[index];
        if (file && !file->GetFilename().empty()) {
          detected |= FrameworkPluginFamily(file->GetFilename(), a_sex);
        }
      }
    };
    collect(dataHandler->GetLoadedMods(), dataHandler->GetLoadedModCount());
    collect(dataHandler->GetLoadedLightMods(),
            dataHandler->GetLoadedLightModCount());
  }

  detected = SelectSingleExplicitFamily(detected, a_sex);
  {
    std::lock_guard lock(g_cacheMutex);
    g_installedDefaultCache[cacheIndex] = detected;
  }
  return detected;
}
} // namespace

Mask DetectArmor(const RE::TESObjectARMO *a_armor) {
  return CollectArmorEvidence(a_armor).explicitFamilies;
}

Mask ClassifyCatalogArmor(const RE::TESObjectARMO *a_armor) {
  const auto evidence = CollectArmorEvidence(a_armor);
  Mask classified = 0;
  const bool hasAnyThirdPersonModel =
      evidence.hasThirdPersonModel[0] || evidence.hasThirdPersonModel[1];

  for (const auto sex : {Sex::Male, Sex::Female}) {
    const auto index = SexIndex(sex);
    if (hasAnyThirdPersonModel && !evidence.hasThirdPersonModel[index]) {
      continue;
    }
    const auto explicitFamilies = evidence.explicitFamilies & SexFamilies(sex);
    classified |= explicitFamilies != 0 ? explicitFamilies : VanillaFamily(sex);
  }
  return classified;
}

Mask ResolveActor(RE::Actor *a_actor) {
  if (!a_actor) {
    return 0;
  }

  const auto sex = ActorSex(a_actor);
  auto *skin = a_actor->GetSkin();
  const ActorSignature signature{
      .skinFormID = skin ? skin->GetFormID() : 0,
      .raceFormID = a_actor->GetRace() ? a_actor->GetRace()->GetFormID() : 0,
      .thirdPersonRoot =
          reinterpret_cast<std::uintptr_t>(a_actor->Get3D(false)),
      .sex = sex,
  };

  {
    std::lock_guard lock(g_cacheMutex);
    if (const auto cached = g_actorCache.find(a_actor->GetFormID());
        cached != g_actorCache.end() && cached->second.signature == signature) {
      return cached->second.family;
    }
  }

  // Form metadata is the cheapest and remains available while the body is
  // covered. Only scan this one actor's loaded Skin branch when metadata is
  // generic, then use a single installed-framework default as the last hint.
  Mask family = skin ? SelectSingleExplicitFamily(DetectArmor(skin), sex) : 0;
  if (family == 0) {
    family = DetectLoadedSkinFamily(a_actor, skin, sex);
  }
  if (family == 0) {
    family = DetectInstalledDefault(sex);
  }
  // Do not guess Vanilla when every actor-local and installed-framework hint
  // is ambiguous. A zero result deliberately makes the catalog filter fail
  // open, which is safer for custom followers and mixed UBE/3BA load orders.

  {
    std::lock_guard lock(g_cacheMutex);
    g_actorCache.insert_or_assign(a_actor->GetFormID(),
                                  ActorCacheEntry{signature, family});
  }
  logger::debug("SFS catalog body family actor={:08X} skin={:08X} family={}",
                a_actor->GetFormID(), signature.skinFormID, Name(family));
  return family;
}

void ResetRuntimeCaches() {
  std::lock_guard lock(g_cacheMutex);
  g_actorCache.clear();
  g_installedDefaultCache = {};
}
} // namespace sfs::body_family
