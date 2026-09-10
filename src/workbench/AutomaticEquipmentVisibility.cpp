#include "workbench/AutomaticEquipmentVisibility.h"

#include "ArmorUtils.h"
#include "native/ArmorSkinning.h"
#include "workbench/AppearanceSlotProtection.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <initializer_list>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sfs::workbench {
namespace {
std::atomic<ExternalModStripLinkMode> g_externalModStripLinkMode{
    ExternalModStripLinkMode::ModSettingsSlots};
std::atomic<ExternalModStripLinkMode> g_customExternalModStripLinkBaseMode{
    ExternalModStripLinkMode::ModSettingsSlots};
std::atomic<ExternalModStripLinkMode> g_customDirectAutomaticBaseMode{
    ExternalModStripLinkMode::ModSettingsSlots};
std::atomic_uint64_t g_customDisabledAppearanceSlotMask{0};
std::mutex g_customDirectMappingsMutex;
AutomaticEquipmentSlotMappings g_customDirectMappings{};
AutomaticEquipmentSlotOverrides g_customDirectOverrides{};
std::mutex g_actorSettingsMutex;
AutomaticEquipmentVisibilitySettingsByActor g_actorSettings;
std::mutex g_equipmentStateEventMutex;
std::unordered_map<RE::FormID, std::unordered_map<RE::FormID, bool>>
    g_equipmentStateEventsByActor;
constexpr std::uint32_t kActorSettingsSerializationType = 'AEVS';
constexpr std::uint32_t kActorSettingsSerializationVersion = 2;

[[nodiscard]] std::uint64_t Slot(const std::uint32_t a_slotNumber) {
  return armor::GetArmorSlotMask(a_slotNumber);
}

[[nodiscard]] ActorAutomaticEquipmentVisibilitySettings
SanitizeSettings(const ActorAutomaticEquipmentVisibilitySettings &a_settings) {
  auto result = a_settings;
  if (result.mode == AutomaticEquipmentVisibilityMode::CustomSlots) {
    // Custom remains an item-local serialized binding marker, never an actor
    // base mode.
    result.mode = AutomaticEquipmentVisibilityMode::VanillaSlots;
  }
  for (auto &slotNumber : result.customMappings) {
    if (slotNumber < kAutomaticEquipmentFirstSlot ||
        slotNumber > kAutomaticEquipmentLastSlot) {
      slotNumber = 0;
    }
  }
  return result;
}

[[nodiscard]] bool IsAsciiAlpha(const unsigned char a_character) {
  return (a_character >= 'A' && a_character <= 'Z') ||
         (a_character >= 'a' && a_character <= 'z');
}

[[nodiscard]] bool IsAsciiUpper(const unsigned char a_character) {
  return a_character >= 'A' && a_character <= 'Z';
}

[[nodiscard]] bool IsAsciiLower(const unsigned char a_character) {
  return a_character >= 'a' && a_character <= 'z';
}

[[nodiscard]] bool IsAsciiDigit(const unsigned char a_character) {
  return a_character >= '0' && a_character <= '9';
}

[[nodiscard]] bool IsAsciiAlphaNumeric(const unsigned char a_character) {
  return IsAsciiAlpha(a_character) || IsAsciiDigit(a_character);
}

[[nodiscard]] std::string BuildSearchTextPart(const std::string_view a_value) {
  std::string output;
  output.reserve(a_value.size() * 2);
  unsigned char previous = 0;
  for (std::size_t index = 0; index < a_value.size(); ++index) {
    const auto character = static_cast<unsigned char>(a_value[index]);
    const auto next = index + 1 < a_value.size()
                          ? static_cast<unsigned char>(a_value[index + 1])
                          : static_cast<unsigned char>(0);
    const bool splitCamelCase =
        IsAsciiUpper(character) &&
        ((IsAsciiLower(previous) || IsAsciiDigit(previous)) ||
         (IsAsciiUpper(previous) && IsAsciiLower(next)));
    const bool splitAlphaDigit =
        IsAsciiAlphaNumeric(character) && IsAsciiAlphaNumeric(previous) &&
        (IsAsciiDigit(character) != IsAsciiDigit(previous));
    if (!output.empty() && (splitCamelCase || splitAlphaDigit)) {
      output.push_back(' ');
    }
    if (IsAsciiAlphaNumeric(character)) {
      output.push_back(static_cast<char>(std::tolower(character)));
    } else if (character >= 0x80) {
      output.push_back(static_cast<char>(character));
    } else if (!output.empty() && output.back() != ' ') {
      output.push_back(' ');
    }
    previous = character;
  }
  if (!output.empty() && output.back() == ' ') {
    output.pop_back();
  }
  return output;
}

void AppendSearchText(std::string &a_text, const std::string_view a_value) {
  const auto part = BuildSearchTextPart(a_value);
  if (part.empty()) {
    return;
  }
  if (!a_text.empty()) {
    a_text.push_back(' ');
  }
  a_text.append(part);
}

[[nodiscard]] std::string
BuildClassificationIdentityText(const RE::TESObjectARMO *a_armor) {
  std::string text;
  if (!a_armor) {
    return text;
  }
  AppendSearchText(text, armor::GetEditorID(a_armor));
  AppendSearchText(text, armor::GetDisplayName(a_armor));
  return text;
}

void AppendClassificationKeywordText(std::string &a_text,
                                     const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return;
  }
  for (const auto *keyword : a_armor->GetKeywords()) {
    // SFS's own runtime SOS/TNG overlay is a rendering/classification result,
    // not an identity hint for automatic appearance-slot matching. Keep
    // original ESP/KID and other mod keywords in the classifier, but do not
    // let a keyword that SFS just added cause a second classification pass.
    if (sfs::native::IsSFSOwnedRuntimeKeyword(a_armor, keyword)) {
      continue;
    }
    AppendSearchText(a_text, armor::GetEditorID(keyword));
  }
}

[[nodiscard]] bool
ContainsAny(const std::string_view a_text,
            const std::initializer_list<std::string_view> a_terms,
            const bool a_allowEnglishPlurals = true) {
  std::string paddedText;
  paddedText.reserve(a_text.size() + 2);
  paddedText.push_back(' ');
  paddedText.append(a_text);
  paddedText.push_back(' ');

  const auto containsWholeTerm = [&](const std::string_view a_term) {
    std::string paddedTerm;
    paddedTerm.reserve(a_term.size() + 2);
    paddedTerm.push_back(' ');
    paddedTerm.append(a_term);
    paddedTerm.push_back(' ');
    return paddedText.find(paddedTerm) != std::string::npos;
  };

  return std::ranges::any_of(a_terms, [&](const auto term) {
    if (term.empty()) {
      return false;
    }
    const bool nonAscii = std::ranges::any_of(
        term, [](const unsigned char ch) { return ch >= 0x80; });
    if (nonAscii) {
      return a_text.find(term) != std::string_view::npos;
    }

    if (containsWholeTerm(term)) {
      return true;
    }
    if (!a_allowEnglishPlurals) {
      return false;
    }

    // Permit ordinary English plurals without treating every token which
    // merely begins with a term as a match (for example cap/cape or
    // hand/handle). Explicit irregular spellings remain in the category list.
    std::string plural(term);
    plural.push_back('s');
    if (containsWholeTerm(plural)) {
      return true;
    }
    plural.assign(term);
    plural.append("es");
    if (containsWholeTerm(plural)) {
      return true;
    }
    if (term.ends_with('y')) {
      plural.assign(term.substr(0, term.size() - 1));
      plural.append("ies");
      if (containsWholeTerm(plural)) {
        return true;
      }
    }
    return false;
  });
}

[[nodiscard]] bool ContainsAsciiTokenSuffix(
    const std::string_view a_text,
    const std::initializer_list<std::string_view> a_terms) {
  std::size_t tokenStart = 0;
  while (tokenStart < a_text.size()) {
    const auto tokenEnd = a_text.find(' ', tokenStart);
    const auto token = a_text.substr(
        tokenStart, tokenEnd == std::string_view::npos ? std::string_view::npos
                                                       : tokenEnd - tokenStart);
    if (std::ranges::any_of(a_terms, [&](const auto term) {
          return token.size() > term.size() && token.ends_with(term);
        })) {
      return true;
    }
    if (tokenEnd == std::string_view::npos) {
      break;
    }
    tokenStart = tokenEnd + 1;
  }
  return false;
}

[[nodiscard]] VanillaAnchorPriority
Priority(const std::initializer_list<std::uint32_t> a_slotNumbers) {
  VanillaAnchorPriority result;
  for (const auto slotNumber : a_slotNumbers) {
    if (result.count >= result.slotMasks.size()) {
      break;
    }
    result.slotMasks[result.count++] = Slot(slotNumber);
  }
  return result;
}
} // namespace

ExternalModStripLinkMode GetExternalModStripLinkMode() {
  return g_externalModStripLinkMode.load(std::memory_order_acquire);
}

void SetExternalModStripLinkMode(const ExternalModStripLinkMode a_mode) {
  // DirectSlots is an internal custom-editor base, never a global option.
  // Development builds briefly persisted it directly; consume that value as
  // Custom so existing settings continue to use their saved direct mappings.
  const auto sanitized = SanitizeExternalStripLinkMode(a_mode);
  g_externalModStripLinkMode.store(sanitized, std::memory_order_release);
}

ExternalModStripLinkMode GetCustomExternalModStripLinkBaseMode() {
  return g_customExternalModStripLinkBaseMode.load(std::memory_order_acquire);
}

void SetCustomExternalModStripLinkBaseMode(
    const ExternalModStripLinkMode a_mode) {
  const auto sanitized = SanitizeCustomStripLinkBaseMode(a_mode);
  g_customExternalModStripLinkBaseMode.store(sanitized,
                                              std::memory_order_release);
}

ExternalModStripLinkMode GetEffectiveExternalModStripLinkMode() {
  return ResolveEffectiveStripLinkMode(
      {.configuredMode = GetExternalModStripLinkMode(),
       .customBaseMode = GetCustomExternalModStripLinkBaseMode()});
}

bool IsModSettingsStripLinkPolicyActive() {
  return IsModSettingsPolicyActive(
      {.configuredMode = GetExternalModStripLinkMode(),
       .customBaseMode = GetCustomExternalModStripLinkBaseMode(),
       .directAutomaticBaseMode =
           GetCustomDirectStripLinkAutomaticBaseMode()});
}

bool IsActualEquipmentStripLinkPolicyActive() {
  return IsActualEquipmentPolicyActive(
      {.configuredMode = GetExternalModStripLinkMode(),
       .customBaseMode = GetCustomExternalModStripLinkBaseMode(),
       .directAutomaticBaseMode =
           GetCustomDirectStripLinkAutomaticBaseMode()});
}

ExternalModStripLinkMode GetCustomDirectStripLinkAutomaticBaseMode() {
  return g_customDirectAutomaticBaseMode.load(std::memory_order_acquire);
}

void SetCustomDirectStripLinkAutomaticBaseMode(
    const ExternalModStripLinkMode a_mode) {
  const auto sanitized = SanitizeDirectAutomaticBaseMode(a_mode);
  g_customDirectAutomaticBaseMode.store(sanitized,
                                        std::memory_order_release);
}

std::uint64_t GetCustomStripLinkDisabledAppearanceSlotMask() {
  return g_customDisabledAppearanceSlotMask.load(std::memory_order_acquire);
}

void SetCustomStripLinkDisabledAppearanceSlotMask(
    const std::uint64_t a_slotMask) {
  std::uint64_t supportedMask = 0;
  for (std::uint32_t slot = kAutomaticEquipmentFirstSlot;
       slot <= kAutomaticEquipmentLastSlot; ++slot) {
    supportedMask |= armor::GetArmorSlotMask(slot);
  }
  g_customDisabledAppearanceSlotMask.store(a_slotMask & supportedMask,
                                           std::memory_order_release);
}

bool IsExternalModStripLinkAppearanceEnabled(
    const std::uint64_t a_appearanceSlotMask) {
  // Protection is a runtime gate as well as a registration gate. Existing
  // saved appearances and direct mappings are deliberately retained so they
  // can return when the user removes protection, but no protected appearance
  // may participate in either the actual-equipment or virtual-token pipeline
  // while protection is active.
  return IsStripLinkedAppearanceEnabled(
      {.configuredMode = GetExternalModStripLinkMode(),
       .customBaseMode = GetCustomExternalModStripLinkBaseMode(),
       .disabledAppearanceSlotMask =
           GetCustomStripLinkDisabledAppearanceSlotMask()},
      a_appearanceSlotMask,
      IsAppearanceRegistrationProtectedSlotMask(a_appearanceSlotMask));
}

AutomaticEquipmentSlotMappings GetCustomDirectStripLinkMappings() {
  std::lock_guard lock(g_customDirectMappingsMutex);
  return g_customDirectMappings;
}

void SetCustomDirectStripLinkMappings(
    const AutomaticEquipmentSlotMappings &a_mappings) {
  auto sanitized = a_mappings;
  for (auto &slotNumber : sanitized) {
    if (slotNumber < kAutomaticEquipmentFirstSlot ||
        slotNumber > kAutomaticEquipmentLastSlot) {
      slotNumber = 0;
    }
  }
  std::lock_guard lock(g_customDirectMappingsMutex);
  g_customDirectMappings = sanitized;
}

AutomaticEquipmentSlotOverrides GetCustomDirectStripLinkOverrides() {
  std::lock_guard lock(g_customDirectMappingsMutex);
  return g_customDirectOverrides;
}

void SetCustomDirectStripLinkOverrides(
    const AutomaticEquipmentSlotOverrides &a_overrides) {
  std::lock_guard lock(g_customDirectMappingsMutex);
  g_customDirectOverrides = a_overrides;
}

std::optional<std::uint64_t> ResolveCustomDirectStripLinkAnchorSlotMask(
    const std::uint64_t a_appearanceSlotMask) {
  ExternalStripLinkPolicyState policy{
      .configuredMode = GetExternalModStripLinkMode(),
      .customBaseMode = GetCustomExternalModStripLinkBaseMode(),
      .directAutomaticBaseMode =
          GetCustomDirectStripLinkAutomaticBaseMode(),
      .protectedAppearanceSlotMask = GetEffectiveAppearanceProtectedSlotMask()};
  {
    std::lock_guard lock(g_customDirectMappingsMutex);
    policy.directMappings = g_customDirectMappings;
    policy.directOverrides = g_customDirectOverrides;
  }
  return ResolveDirectSlotMask(policy, a_appearanceSlotMask, false);
}

std::optional<std::uint64_t> ResolveCustomDirectStripLinkTokenSlotMask(
    const std::uint64_t a_appearanceSlotMask) {
  ExternalStripLinkPolicyState policy{
      .configuredMode = GetExternalModStripLinkMode(),
      .customBaseMode = GetCustomExternalModStripLinkBaseMode(),
      .directAutomaticBaseMode =
          GetCustomDirectStripLinkAutomaticBaseMode(),
      .protectedAppearanceSlotMask = GetEffectiveAppearanceProtectedSlotMask()};
  {
    std::lock_guard lock(g_customDirectMappingsMutex);
    policy.directMappings = g_customDirectMappings;
    policy.directOverrides = g_customDirectOverrides;
  }
  return ResolveDirectSlotMask(policy, a_appearanceSlotMask, true);
}

ActorAutomaticEquipmentVisibilitySettings
GetActorAutomaticEquipmentVisibilitySettings(const RE::FormID a_actorFormID) {
  if (a_actorFormID == 0) {
    return {};
  }
  std::lock_guard lock(g_actorSettingsMutex);
  const auto setting = g_actorSettings.find(a_actorFormID);
  return setting == g_actorSettings.end()
             ? ActorAutomaticEquipmentVisibilitySettings{}
             : setting->second;
}

void SetActorAutomaticEquipmentVisibilitySettings(
    const RE::FormID a_actorFormID,
    const ActorAutomaticEquipmentVisibilitySettings &a_settings) {
  if (a_actorFormID == 0) {
    return;
  }
  std::lock_guard lock(g_actorSettingsMutex);
  g_actorSettings.insert_or_assign(a_actorFormID, SanitizeSettings(a_settings));
}

AutomaticEquipmentVisibilitySettingsByActor
GetAllActorAutomaticEquipmentVisibilitySettings() {
  std::lock_guard lock(g_actorSettingsMutex);
  return g_actorSettings;
}

void ReplaceAllActorAutomaticEquipmentVisibilitySettings(
    const AutomaticEquipmentVisibilitySettingsByActor &a_settingsByActor) {
  AutomaticEquipmentVisibilitySettingsByActor sanitized;
  sanitized.reserve(a_settingsByActor.size());
  for (const auto &[actorFormID, settings] : a_settingsByActor) {
    if (actorFormID != 0) {
      sanitized.emplace(actorFormID, SanitizeSettings(settings));
    }
  }
  std::lock_guard lock(g_actorSettingsMutex);
  g_actorSettings = std::move(sanitized);
}

void SerializeActorAutomaticEquipmentVisibilitySettings(
    SKSE::SerializationInterface *a_skse) {
  if (!a_skse) {
    return;
  }

  nlohmann::json root;
  root["actors"] = nlohmann::json::array();
  const auto settingsByActor =
      GetAllActorAutomaticEquipmentVisibilitySettings();
  std::vector<RE::FormID> actorFormIDs;
  actorFormIDs.reserve(settingsByActor.size());
  for (const auto &[actorFormID, _] : settingsByActor) {
    if (actorFormID != 0) {
      actorFormIDs.push_back(actorFormID);
    }
  }
  std::ranges::sort(actorFormIDs);

  for (const auto actorFormID : actorFormIDs) {
    const auto &settings = settingsByActor.at(actorFormID);
    std::vector<std::uint32_t> customMappings(settings.customMappings.begin(),
                                              settings.customMappings.end());
    std::vector<std::uint32_t> customOverrideSlots;
    for (std::size_t index = 0; index < settings.customOverrides.size();
         ++index) {
      if (settings.customOverrides[index]) {
        customOverrideSlots.push_back(static_cast<std::uint32_t>(index) +
                                      kAutomaticEquipmentFirstSlot);
      }
    }
    root["actors"].push_back(
        {{"formID", actorFormID},
         {"mode", static_cast<std::uint8_t>(settings.mode)},
         {"customMappings", std::move(customMappings)},
         {"customOverrideSlots", std::move(customOverrideSlots)}});
  }

  const auto payload = root.dump();
  a_skse->WriteRecord(kActorSettingsSerializationType,
                      kActorSettingsSerializationVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

void DeserializeActorAutomaticEquipmentVisibilitySettings(
    SKSE::SerializationInterface *a_skse) {
  if (!a_skse) {
    return;
  }

  {
    // Never trust Revert ordering to clear the previous save. This guarantees
    // that an absent or damaged record cannot leak another save's actor map.
    std::lock_guard lock(g_actorSettingsMutex);
    g_actorSettings.clear();
  }

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    logger::info("No actor strip-link record: all actors start disabled");
    return;
  }

  if (type != kActorSettingsSerializationType) {
    logger::warn(
        "Skipping unexpected SFS actor strip-link record type {:X}", type);
    return;
  }

  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::error("Failed to read SFS actor strip-link settings payload");
    return;
  }

  if (version == 1) {
    // v1 was emitted only by pre-release development builds and could contain
    // a global settings.json migration. Discard it so the released feature's
    // documented default remains actor-local No Linking for every old save.
    logger::info("Discarded pre-release strip-link settings record; all "
                 "actors start disabled");
    return;
  }
  if (version != kActorSettingsSerializationVersion) {
    logger::warn("Skipping SFS actor strip-link settings from unsupported "
                 "version {}",
                 version);
    return;
  }

  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object() || !root["actors"].is_array()) {
    logger::error("Failed to parse SFS actor strip-link settings payload");
    return;
  }

  AutomaticEquipmentVisibilitySettingsByActor loaded;
  for (const auto &actorState : root["actors"]) {
    if (!actorState.is_object()) {
      continue;
    }
    const auto savedActorFormID = actorState.value("formID", RE::FormID{0});
    RE::FormID actorFormID = 0;
    if (savedActorFormID == 0 ||
        !a_skse->ResolveFormID(savedActorFormID, actorFormID) ||
        actorFormID == 0) {
      continue;
    }

    ActorAutomaticEquipmentVisibilitySettings settings{};
    const auto modeValue = actorState.value("mode", std::uint8_t{0});
    if (modeValue <= static_cast<std::uint8_t>(
                         AutomaticEquipmentVisibilityMode::ModdingSlots)) {
      settings.mode = static_cast<AutomaticEquipmentVisibilityMode>(modeValue);
    }
    if (const auto mappings = actorState.find("customMappings");
        mappings != actorState.end() && mappings->is_array()) {
      const auto count =
          (std::min)(mappings->size(), settings.customMappings.size());
      for (std::size_t index = 0; index < count; ++index) {
        if (const auto value = (*mappings)[index]; value.is_number_integer()) {
          settings.customMappings[index] = value.get<std::uint8_t>();
        }
      }
    }
    if (const auto overrides = actorState.find("customOverrideSlots");
        overrides != actorState.end() && overrides->is_array()) {
      for (const auto &value : *overrides) {
        if (!value.is_number_integer()) {
          continue;
        }
        const auto slotNumber = value.get<std::uint32_t>();
        if (slotNumber >= kAutomaticEquipmentFirstSlot &&
            slotNumber <= kAutomaticEquipmentLastSlot) {
          settings.customOverrides[slotNumber - kAutomaticEquipmentFirstSlot] =
              true;
        }
      }
    }
    loaded.insert_or_assign(actorFormID, SanitizeSettings(settings));
  }

  {
    std::lock_guard lock(g_actorSettingsMutex);
    g_actorSettings = std::move(loaded);
  }
}

void RevertActorAutomaticEquipmentVisibilitySettings() {
  {
    std::lock_guard lock(g_actorSettingsMutex);
    g_actorSettings.clear();
  }
  ClearAutomaticEquipmentStateEvents();
}

void RecordAutomaticEquipmentStateEvent(const RE::FormID a_actorFormID,
                                        const RE::FormID a_armorFormID,
                                        const bool a_equipped) {
  if (a_actorFormID == 0 || a_armorFormID == 0) {
    return;
  }
  std::lock_guard lock(g_equipmentStateEventMutex);
  g_equipmentStateEventsByActor[a_actorFormID].insert_or_assign(a_armorFormID,
                                                                a_equipped);
}

std::vector<AutomaticEquipmentStateOverride>
ReconcileAutomaticEquipmentStateEvents(
    const RE::FormID a_actorFormID,
    const std::span<const RE::FormID> a_observedWornArmorFormIDs) {
  std::vector<AutomaticEquipmentStateOverride> unresolved;
  if (a_actorFormID == 0) {
    return unresolved;
  }

  std::lock_guard lock(g_equipmentStateEventMutex);
  const auto actorIt = g_equipmentStateEventsByActor.find(a_actorFormID);
  if (actorIt == g_equipmentStateEventsByActor.end()) {
    return unresolved;
  }

  auto &events = actorIt->second;
  for (auto eventIt = events.begin(); eventIt != events.end();) {
    const bool observedEquipped =
        std::ranges::find(a_observedWornArmorFormIDs, eventIt->first) !=
        a_observedWornArmorFormIDs.end();
    if (observedEquipped == eventIt->second) {
      eventIt = events.erase(eventIt);
      continue;
    }
    unresolved.push_back(
        {.armorFormID = eventIt->first, .equipped = eventIt->second});
    ++eventIt;
  }
  if (events.empty()) {
    g_equipmentStateEventsByActor.erase(actorIt);
  }
  return unresolved;
}

void ClearAutomaticEquipmentStateEvents() {
  std::lock_guard lock(g_equipmentStateEventMutex);
  g_equipmentStateEventsByActor.clear();
}

std::uint64_t GetAutomaticEquipmentControlSlotMask(
    const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return 0;
  }

  auto slotMask = armor::GetArmorDisplaySlotMask(a_armor) |
                  armor::GetArmorAddonSlotMask(a_armor);
  const auto hairSlot = Slot(31);
  const auto circletSlot = Slot(42);
  if ((slotMask & (hairSlot | circletSlot)) ==
      (hairSlot | circletSlot)) {
    // Vanilla helmets commonly reserve Hair only to hide the actor's hair.
    // Their actual-equipment strip control is Circlet; a standalone slot-31
    // wig/hair item remains an independent Hair control.
    slotMask &= ~hairSlot;
  }
  const auto bodySlot = Slot(32);
  const auto genitalSlot = Slot(49);
  if (!armor::IsSosTngInternalArmor(a_armor) &&
      (slotMask & (bodySlot | genitalSlot)) == (bodySlot | genitalSlot)) {
    slotMask &= ~genitalSlot;
  }
  return slotMask;
}

VanillaAnchorPriority
GetVanillaAnchorPriority(const RE::TESObjectARMO *a_appearance) {
  if (!a_appearance) {
    return {};
  }

  const auto identityText = BuildClassificationIdentityText(a_appearance);
  auto text = identityText;
  AppendClassificationKeywordText(text, a_appearance);
  const auto displayMask = armor::GetArmorDisplaySlotMask(a_appearance);

  // Preserve distinct vanilla appearance slots before semantic classification
  // can fold them into a neighboring primary equipment category. Multi-slot
  // body, hand, foot, and head items continue through the ordinary rules below
  // and select their representative primary slot.
  if ((displayMask & Slot(31)) != 0 &&
      (displayMask & (Slot(30) | Slot(42))) == 0) {
    return Priority({31});
  }
  // Vanilla Long Hair is a hair-visibility partition rather than an ordinary
  // headgear anchor. Treat a slot-41 appearance like replacement Hair (31),
  // unless the same item explicitly occupies an ordinary headgear slot.
  if ((displayMask & Slot(41)) != 0 &&
      (displayMask & (Slot(30) | Slot(31) | Slot(42))) == 0) {
    return Priority({31});
  }
  if ((displayMask & Slot(34)) != 0 &&
      (displayMask & (Slot(32) | Slot(33))) == 0) {
    return Priority({34});
  }
  if ((displayMask & Slot(38)) != 0 &&
      (displayMask & (Slot(32) | Slot(37))) == 0) {
    return Priority({38});
  }

  // The vocabulary mirrors the 19,132-record outfit-backup analysis. Keep the
  // English, Korean, Simplified Chinese, and Traditional Chinese terms beside
  // one another so a category cannot silently lose one of the supported
  // languages. First match wins; specific accessories therefore precede broad
  // body and miscellaneous categories.

  // Hair ornaments which are not slot-31 replacement hair prefer circlet
  // (42). Wigs placed in an ordinary head slot remain head equipment here.
  const bool hairAccessoryAppearance = ContainsAny(
      text, {"hair ornament", "hair accessory", "hairpin",   "hair pin",
             "headband",      "head band",      "헤어 장식", "헤어장식",
             "헤어 액세서리", "머리 장식",      "머리장식",  "머리핀",
             "머리띠",        "发饰",           "髮飾",      "发夹",
             "髮夾",          "发簪",           "髮簪",      "发箍",
             "髮箍",          "头饰",           "頭飾"});
  if (!hairAccessoryAppearance &&
      ContainsAny(text, {"wig",        "hairpiece", "hair piece", "hairdo",
                         "ponytail",   "pigtail",   "hair",       "가발",
                         "헤어피스",   "헤어 피스", "포니테일",   "트윈테일",
                         "양갈래머리", "헤어",      "머리카락",   "假发",
                         "假髮",       "发片",      "髮片",       "马尾",
                         "馬尾",       "双马尾",    "雙馬尾",     "头发",
                         "頭髮",       "发型",      "髮型"})) {
    return Priority({30, 42});
  }

  if (ContainsAny(text, {"shield", "buckler", "방패", "버클러", "盾", "盾牌",
                         "圆盾", "圓盾", "小圆盾", "小圓盾"})) {
    return Priority({39, 32});
  }
  if (ContainsAny(text, {"circlet",
                         "tiara",
                         "crown",
                         "glasses",
                         "eyeglass",
                         "goggle",
                         "eyepatch",
                         "eye patch",
                         "eyewear",
                         "monocle",
                         "bandage eyes",
                         "mask",
                         "gag",
                         "blindfold",
                         "face cloth",
                         "facecloth",
                         "facemask",
                         "face mask",
                         "muzzle",
                         "mouth cover",
                         "veil",
                         "earring",
                         "ear ring",
                         "earpiece",
                         "ear piece",
                         "ear",
                         "eye",
                         "eyes",
                         "mouth",
                         "face",
                         "face jewel",
                         "facejewel",
                         "ear piercing",
                         "nose piercing",
                         "lip piercing",
                         "face piercing",
                         "hair ornament",
                         "hair accessory",
                         "hairpin",
                         "hair pin",
                         "headband",
                         "head band",
                         "halo",
                         "angel circle",
                         "horn",
                         "서클릿",
                         "티아라",
                         "왕관",
                         "안경",
                         "고글",
                         "안대",
                         "눈가리개",
                         "모노클",
                         "마스크",
                         "가면",
                         "재갈",
                         "면사포",
                         "입가리개",
                         "얼굴가리개",
                         "베일",
                         "귀걸이",
                         "귀 장식",
                         "귀장식",
                         "이어피스",
                         "얼굴보석",
                         "귀 피어싱",
                         "코 피어싱",
                         "입술 피어싱",
                         "얼굴 피어싱",
                         "헤어 장식",
                         "헤어장식",
                         "헤어 액세서리",
                         "머리 장식",
                         "머리장식",
                         "머리핀",
                         "머리띠",
                         "눈",
                         "입",
                         "얼굴",
                         "후광",
                         "천사 고리",
                         "천사고리",
                         "뿔",
                         "头环",
                         "頭環",
                         "发箍",
                         "髮箍",
                         "皇冠",
                         "眼镜",
                         "眼鏡",
                         "护目镜",
                         "護目鏡",
                         "眼罩",
                         "眼带",
                         "眼帶",
                         "单片眼镜",
                         "單片眼鏡",
                         "面具",
                         "面罩",
                         "口罩",
                         "蒙眼布",
                         "口塞",
                         "口球",
                         "面纱",
                         "面紗",
                         "头纱",
                         "頭紗",
                         "耳环",
                         "耳環",
                         "耳饰",
                         "耳飾",
                         "耳坠",
                         "耳墜",
                         "耳夹",
                         "耳夾",
                         "鼻环",
                         "鼻環",
                         "唇环",
                         "唇環",
                         "面部穿孔",
                         "发饰",
                         "髮飾",
                         "发夹",
                         "髮夾",
                         "发簪",
                         "髮簪",
                         "头饰",
                         "頭飾",
                         "眼",
                         "耳",
                         "嘴",
                         "脸",
                         "臉",
                         "面部",
                         "光环",
                         "光環",
                         "天使光环",
                         "天使光環",
                         "头角",
                         "頭角",
                         "犄角",
                         "兽角",
                         "獸角"})) {
    return Priority({42});
  }
  if (ContainsAny(text, {"helmet",    "helm",       "hood",      "headgear",
                         "head gear", "hat",        "headpiece", "head piece",
                         "headwear",  "head wear",  "cap",       "bandana",
                         "headdress", "head dress", "헬멧",      "투구",
                         "후드",      "모자",       "머리 장비", "머리장비",
                         "헤드기어",  "헤드피스",   "헤드",      "두건",
                         "반다나",    "头盔",       "頭盔",      "兜帽",
                         "帽子",      "头巾",       "頭巾",      "头戴",
                         "頭戴"})) {
    return Priority({30, 42});
  }
  if (ContainsAny(text, {"necklace",   "amulet",    "choker",     "collar",
                         "necktie",    "neck tie",  "tie",        "scarf",
                         "pendant",    "neckcloth", "neck cloth", "neckcover",
                         "neck cover", "dogtag",    "dog tag",    "neck",
                         "gorget",     "목걸이",    "목 장식",    "목장식",
                         "초커",       "칼라",      "카라",       "목보호대",
                         "넥클로스",   "넥타이",    "스카프",     "펜던트",
                         "목덮개",     "도그태그",  "고르겟",     "项链",
                         "項鏈",       "护身符",    "護身符",     "颈圈",
                         "頸圈",       "项圈",      "項圈",       "领圈",
                         "領圈",       "领带",      "領帶",       "围巾",
                         "圍巾",       "吊坠",      "吊墜",       "颈饰",
                         "頸飾",       "狗牌",      "护颈",       "護頸"})) {
    return Priority({35, 32});
  }
  if (ContainsAny(text,
                  {"ring", "finger ring", "finger accessory", "finger jewel",
                   "finger jewelry", "반지", "손가락 반지", "손가락반지",
                   "손가락 장식", "손가락장식", "戒指", "指环", "指環", "指饰",
                   "指飾"})) {
    return Priority({36, 33});
  }
  if (ContainsAny(text,
                  {"bracelet", "bangle",  "bracer",   "armlet",  "wristlet",
                   "wrist",    "armband", "arm band", "armring", "arm ring",
                   "cuff",     "팔찌",    "브레이서", "암링",    "팔 밴드",
                   "팔밴드",   "암렛",    "손목",     "커프",    "手镯",
                   "手鐲",     "手链",    "手鏈",     "护腕",    "護腕",
                   "臂环",     "臂環",    "臂带",     "臂帶",    "腕带",
                   "腕帶"})) {
    return Priority({33});
  }
  if (ContainsAny(text,
                  {"glove",   "gauntlet",  "hands",     "hand",     "mitten",
                   "claw",    "nails",     "nail",      "sleeve",   "vambrace",
                   "forearm", "arm guard", "armguard",  "armmet",   "arms",
                   "arm",     "shoulder",  "pauldron",  "spaulder", "elbow",
                   "장갑",    "건틀릿",    "완갑",      "손 장비",  "손장비",
                   "손톱",    "소매",      "팔보호",    "팔 보호",  "팔 방어",
                   "팔 장비", "팔장비",    "팔 스트랩", " 팔",      "완장",
                   "어깨",    "견갑",      "숄더",      "팔꿈치",   "手套",
                   "护手",    "護手",      "臂甲",      "前臂",     "护臂",
                   "護臂",    "袖",        "肩甲",      "护肩",     "護肩",
                   "肘甲",    "指甲",      "爪"})) {
    return Priority({33});
  }
  if (ContainsAny(text,
                  {"boot",        "shoe",       "heel",        "sandal",
                   "sneaker",     "slipper",    "footwear",    "feet",
                   "foot",        "stocking",   "stock",       "socks",
                   "sock",        "thighhigh",  "thigh high",  "legwear",
                   "leg wear",    "legwarmer",  "leg warmer",  "greave",
                   "kneepad",     "knee pad",   "shin",        "calf",
                   "legmet",      "garter",     "thighband",   "thigh band",
                   "legband",     "leg band",   "legring",     "leg ring",
                   "anklet",      "부츠",       "신발",        "구두",
                   "힐",          "하이힐",     "샌들",        "스니커",
                   "슬리퍼",      "풋웨어",     "스타킹",      "양말",
                   "각반",        "종아리",     "레그워머",    "무릎",
                   "무릎보호대",  "다리 갑옷",  "다리장비",    "가터",
                   "허벅지 밴드", "허벅지밴드", "허벅지 장식", "다리 장식",
                   "다리 보호대", "발찌",       "앵클렛",      "레그 링",
                   "靴",          "鞋",         "高跟鞋",      "凉鞋",
                   "涼鞋",        "运动鞋",     "運動鞋",      "拖鞋",
                   "足部",        "丝袜",       "絲襪",        "长袜",
                   "長襪",        "袜",         "襪",          "护腿",
                   "護腿",        "腿套",       "胫甲",        "脛甲",
                   "护膝",        "護膝",       "小腿",        "吊袜带",
                   "吊襪帶",      "腿环",       "腿環",        "脚链",
                   "腳鏈"})) {
    return Priority({37});
  }

  // These semantic categories all naturally follow the real body garment.
  // This explicit block is what distinguishes a genuine body anchor from an
  // unknown modding-slot item. Unknown items use the explicit slot-32 fallback
  // at the end of this classifier.
  if (ContainsAny(text,
                  {"bra",          "skimpybra",   "brametal",    "breastplate",
                   "chest",        "outer",       "bustier",     "bust",
                   "top",          "shirt",       "blouse",      "sweater",
                   "hoodie",       "jacket",      "coat",        "vest",
                   "tunic",        "upper",       "corset",      "bodice",
                   "apron",        "harness",     "dress",       "gown",
                   "robe",         "bodysuit",    "body suit",   "leotard",
                   "onepiece",     "one piece",   "jumpsuit",    "catsuit",
                   "swimsuit",     "uniform",     "cuirass",     "outfit",
                   "costume",      "bikini",      "lingerie",    "swimwear",
                   "panty",        "panties",     "pantie",      "panie",
                   "thong",        "underwear",   "underpant",   "knickers",
                   "bloomers",     "brief",       "loincloth",   "fundoshi",
                   "inner",        "skirt",       "sarong",      "kilt",
                   "shorts",       "pants",       "sweatpants",  "hotpants",
                   "pantsu",       "pantsmp",     "pantstr",     "pantyhose",
                   "trousers",     "leggings",    "jeans",       "tights",
                   "bottom",       "lower",       "low",         "legs",
                   "belt",         "blet",        "waist",       "waistband",
                   "pelvis",       "hip strap",   "hipstrap",    "hip",
                   "sash",         "girdle",      "obi",         "wing",
                   "tail",         "cloak",       "cape",        "mantle",
                   "backpack",     "back pack",   "backpiece",   "back piece",
                   "back",         "bag",         "purse",       "pouch",
                   "shrug",        "nipple",      "nip",         "nippless",
                   "nipple cover", "navel",       "belly chain", "pasties",
                   "piercing",     "plug",        "dildo",       "syringe",
                   "tattoo",       "tattoos",     "tat",         "clamp",
                   "상의",         "브라",        "흉갑",        "가슴",
                   "셔츠",         "블라우스",    "스웨터",      "후드티",
                   "재킷",         "자켓",        "코트",        "조끼",
                   "튜닉",         "코르셋",      "앞치마",      "전신의상",
                   "메인 의상",    "의상",        "갑옷",        "아머",
                   "드레스",       "가운",        "로브",        "바디수트",
                   "레오타드",     "원피스",      "점프수트",    "캣수트",
                   "수영복",       "교복",        "제복",        "코스튬",
                   "비키니",       "란제리",      "팬티",        "속옷",
                   "언더웨어",     "훈도시",      "치마",        "스커트",
                   "하의",         "바지",        "레깅스",      "반바지",
                   "스웨트팬츠",   "스웨트 팬츠", "핫팬츠",      "판츠",
                   "팬츠",         "팬티스타킹",  "트라우저",    "벨트",
                   "허리",         "허리띠",      "골반",        "오비",
                   "날개",         "꼬리",        "망토",        "케이프",
                   "등 장식",      "등장식",      "백팩",        "가방",
                   "파우치",       "지갑",        "슈러그",      "젖꼭지",
                   "유두",         "니플",        "니플리스",    "니플 커버",
                   "배꼽",         "네이블",      "피어싱",      "플러그",
                   "딜도",         "주사기",      "문신",        "타투",
                   "방어구",       "스윔슈트",    "胸罩",        "乳罩",
                   "胸甲",         "上衣",        "衬衫",        "襯衫",
                   "女衫",         "毛衣",        "卫衣",        "衛衣",
                   "夹克",         "夾克",        "外套",        "背心",
                   "束腰",         "紧身胸衣",    "緊身胸衣",    "围裙",
                   "圍裙",         "连衣裙",      "連衣裙",      "礼服",
                   "禮服",         "长袍",        "長袍",        "紧身衣",
                   "緊身衣",       "连体衣",      "連體衣",      "泳衣",
                   "制服",         "盔甲",        "服装",        "服裝",
                   "衣服",         "比基尼",      "内衣",        "內衣",
                   "泳装",         "泳裝",        "内裤",        "內褲",
                   "丁字裤",       "丁字褲",      "短裤",        "短褲",
                   "灯笼裤",       "燈籠褲",      "缠腰布",      "纏腰布",
                   "裙",           "纱笼",        "紗籠",        "短裙",
                   "裤",           "褲",          "长裤",        "長褲",
                   "牛仔裤",       "牛仔褲",      "紧身裤",      "緊身褲",
                   "下装",         "下裝",        "腰带",        "腰帶",
                   "腰封",         "骨盆",        "臀带",        "臀帶",
                   "饰带",         "飾帶",        "翼",          "翅膀",
                   "尾",           "尾巴",        "斗篷",        "披风",
                   "披風",         "披肩",        "背包",        "背饰",
                   "背飾",         "包",          "手提包",      "小袋",
                   "乳头",         "乳頭",        "肚脐",        "肚臍",
                   "肚脐链",       "肚臍鏈",      "乳贴",        "乳貼",
                   "穿孔",         "穿刺",        "纹身",        "紋身"}) ||
      ContainsAsciiTokenSuffix(identityText, {"panty", "cloak"})) {
    return Priority({32});
  }

  // Pure slot-31 replacement hair returned above. A remaining slot-31 item
  // also occupies an ordinary head slot (30 or 42), so it continues the
  // ordinary head-equipment priority.
  if ((displayMask & Slot(31)) != 0) {
    return Priority({30, 42});
  }

  if (ContainsAny(identityText,
                  {"armor", "armour", "clothes", "clothing", "main", "body",
                   "suit", "mail", "아머", "메일", "盔甲", "铠甲", "鎧甲",
                   "服装", "服裝", "衣服", "套装", "套裝"})) {
    return Priority({32});
  }

  // Generic accessories and weapon/prop names do not reveal a more specific
  // vanilla anchor. The user-selected fallback policy makes them follow the
  // real body garment instead of leaving their automatic state undefined.
  if (ContainsAny(
          text,
          {"weapon",   "sword",     "mace",      "staff",       "bow",
           "quiver",   "dagger",    "axe",       "spear",       "cannon",
           "gun",      "blade",     "knife",     "wakizashi",   "jewel",
           "jewelry",  "jewellery", "accessory", "accessories", "ribbon",
           "bowtie",   "bow tie",   "bowknot",   "bow knot",    "strap",
           "chain",    "flower",    "rose",      "decoration",  "deco",
           "fur",      "glow",      "무기",      "검",          "칼",
           "대검",     "단검",      "활",        "화살통",      "도끼",
           "창",       "총",        "대포",      "지팡이",      "액세서리",
           "악세서리", "장식",      "리본",      "꽃",          "체인",
           "퍼",       "武器",      "剑",        "劍",          "刀",
           "长剑",     "長劍",      "匕首",      "弓",          "箭袋",
           "斧",       "矛",        "枪",        "槍",          "炮",
           "法杖",     "珠宝",      "珠寶",      "首饰",        "首飾",
           "配件",     "饰品",      "飾品",      "丝带",        "絲帶",
           "蝴蝶结",   "蝴蝶結",    "绑带",      "綁帶",        "链",
           "鏈",       "花",        "装饰",      "裝飾",        "毛皮",
           "发光",     "發光"},
          false)) {
    return Priority({32});
  }

  // Slot-only fallback follows the analysis order. Slot 32 must be checked
  // before the component partitions of a multi-slot body outfit. Unsupported
  // modding slots use the user-selected final body-32 fallback below.
  if ((displayMask & Slot(32)) != 0) {
    return Priority({32});
  }
  if ((displayMask & Slot(33)) != 0) {
    return Priority({33});
  }
  if ((displayMask & Slot(37)) != 0) {
    return Priority({37});
  }
  if ((displayMask & Slot(35)) != 0) {
    return Priority({35, 32});
  }
  if ((displayMask & Slot(36)) != 0) {
    return Priority({36, 33});
  }
  if ((displayMask & Slot(39)) != 0) {
    return Priority({39, 32});
  }
  if ((displayMask & Slot(40)) != 0) {
    return Priority({32});
  }
  if ((displayMask & Slot(42)) != 0) {
    return Priority({42});
  }
  if ((displayMask & Slot(30)) != 0) {
    return Priority({30, 42});
  }
  if ((displayMask & Slot(41)) != 0) {
    return Priority({31});
  }
  if ((displayMask & Slot(43)) != 0) {
    return Priority({30, 42});
  }
  if ((displayMask & Slot(34)) != 0) {
    return Priority({33});
  }
  if ((displayMask & Slot(38)) != 0) {
    return Priority({37});
  }
  return Priority({32});
}
} // namespace sfs::workbench
