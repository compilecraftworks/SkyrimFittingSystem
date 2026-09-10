#include "Menu.h"

#include "api/SkyrimFittingSystemAPI.h"

#include "ArmorUtils.h"
#include "Keycode.h"
#include "ThemeConfig.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "native/ExternalEquipmentTransactions.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include "ui/Localization.h"
#include "workbench/AppearanceSlotProtection.h"
#include "workbench/AutomaticEquipmentVisibility.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
constexpr auto kSettingsDirectory = "Data/SKSE/Plugins/SkyrimFittingSystem";
constexpr auto kImGuiIniFilename = "imgui.ini";
constexpr auto kUserSettingsFilename = "settings.json";
constexpr auto kFavoritesFilename = "favorites.json";
constexpr std::uint64_t kPreviousDefaultSpecialEffectProtectedSlotMask =
    std::uint64_t{0x80300000}; // Previous default: slots 50, 51, and 61.

std::uint64_t ParseSpecialEffectProtectedSlots(const nlohmann::json &a_json) {
  auto setting = a_json.find("specialEffectProtectedSlots");
  if (setting == a_json.end()) {
    // One-time migration from the previous setting name.
    setting = a_json.find("manualHideProtectedSlots");
  }
  if (setting == a_json.end()) {
    return sfs::workbench::kDefaultSpecialEffectProtectedSlotMask;
  }
  if (!setting->is_array()) {
    logger::warn("Ignored invalid specialEffectProtectedSlots setting");
    return sfs::workbench::kDefaultSpecialEffectProtectedSlotMask;
  }

  std::uint64_t slotMask = 0;
  for (const auto &entry : *setting) {
    if (!entry.is_number_integer()) {
      continue;
    }
    const auto slotNumber = entry.get<std::int32_t>();
    if (slotNumber < 30 || slotNumber > 61) {
      continue;
    }
    // Slot 39 is controlled only by shieldAppearanceSlotEnabled.
    if (slotNumber == 39) {
      continue;
    }
    slotMask |=
        sfs::armor::GetArmorSlotMask(static_cast<std::uint32_t>(slotNumber));
  }
  if (slotMask == kPreviousDefaultSpecialEffectProtectedSlotMask) {
    // v1.3.1 briefly shipped 50/51/61 as its default. Promote only that exact
    // old default so custom slot lists remain untouched.
    slotMask |= sfs::armor::GetArmorSlotMask(60);
    logger::info("Migrated the previous default special-effect protected "
                 "slots to 50, 51, 60, and 61");
  }
  return slotMask;
}

std::vector<std::uint32_t>
BuildSpecialEffectProtectedSlotNumbers(const std::uint64_t a_slotMask) {
  std::vector<std::uint32_t> slotNumbers;
  for (std::uint32_t slotNumber = 30; slotNumber <= 61; ++slotNumber) {
    if (slotNumber == 39) {
      continue;
    }
    if ((a_slotMask & sfs::armor::GetArmorSlotMask(slotNumber)) != 0) {
      slotNumbers.push_back(slotNumber);
    }
  }
  return slotNumbers;
}

std::uint64_t ParseCustomStripLinkDisabledSlots(
    const nlohmann::json &a_json) {
  const auto setting = a_json.find("customStripLinkDisabledAppearanceSlots");
  if (setting == a_json.end() || !setting->is_array()) {
    return 0;
  }
  std::uint64_t slotMask = 0;
  for (const auto &entry : *setting) {
    if (!entry.is_number_integer()) {
      continue;
    }
    const auto slotNumber = entry.get<std::int32_t>();
    if (slotNumber >= 30 && slotNumber <= 61) {
      slotMask |= sfs::armor::GetArmorSlotMask(
          static_cast<std::uint32_t>(slotNumber));
    }
  }
  return slotMask;
}

std::vector<std::uint32_t>
BuildCustomStripLinkDisabledSlotNumbers(const std::uint64_t a_slotMask) {
  std::vector<std::uint32_t> slotNumbers;
  for (std::uint32_t slotNumber = 30; slotNumber <= 61; ++slotNumber) {
    if ((a_slotMask & sfs::armor::GetArmorSlotMask(slotNumber)) != 0) {
      slotNumbers.push_back(slotNumber);
    }
  }
  return slotNumbers;
}

sfs::workbench::AutomaticEquipmentSlotMappings
ParseCustomDirectStripLinkMappings(const nlohmann::json &a_json) {
  sfs::workbench::AutomaticEquipmentSlotMappings mappings{};
  const auto setting = a_json.find("customDirectStripLinkMappings");
  if (setting == a_json.end() || !setting->is_array()) {
    return mappings;
  }
  const auto count = (std::min)(setting->size(), mappings.size());
  for (std::size_t index = 0; index < count; ++index) {
    const auto &entry = (*setting)[index];
    if (!entry.is_number_integer()) {
      continue;
    }
    const auto slotNumber = entry.get<std::int32_t>();
    if (slotNumber >= 30 && slotNumber <= 61) {
      mappings[index] = static_cast<std::uint8_t>(slotNumber);
    }
  }
  return mappings;
}

std::vector<std::uint32_t> BuildCustomDirectStripLinkMappings() {
  const auto mappings =
      sfs::workbench::GetCustomDirectStripLinkMappings();
  return {mappings.begin(), mappings.end()};
}

sfs::workbench::AutomaticEquipmentSlotOverrides
ParseCustomDirectStripLinkOverrides(
    const nlohmann::json &a_json,
    const sfs::workbench::AutomaticEquipmentSlotMappings &a_mappings,
    const bool a_migrateLegacyDirectMappings) {
  sfs::workbench::AutomaticEquipmentSlotOverrides overrides{};
  const auto setting = a_json.find("customDirectStripLinkOverrideSlots");
  if (setting != a_json.end() && setting->is_array()) {
    for (const auto &entry : *setting) {
      if (!entry.is_number_integer()) {
        continue;
      }
      const auto slotNumber = entry.get<std::int32_t>();
      if (slotNumber >= 30 && slotNumber <= 61) {
        overrides[static_cast<std::size_t>(slotNumber - 30)] = true;
      }
    }
    return overrides;
  }

  // Pre-v1.4 development settings stored only the resolved array. Preserve
  // concrete non-zero choices while letting untouched/ambiguous zero entries
  // return to automatic matching.
  if (a_migrateLegacyDirectMappings) {
    for (std::size_t index = 0; index < a_mappings.size(); ++index) {
      overrides[index] = a_mappings[index] != 0;
    }
  }
  return overrides;
}

std::vector<std::uint32_t> BuildCustomDirectStripLinkOverrideSlots() {
  const auto overrides =
      sfs::workbench::GetCustomDirectStripLinkOverrides();
  std::vector<std::uint32_t> slots;
  for (std::size_t index = 0; index < overrides.size(); ++index) {
    if (overrides[index]) {
      slots.push_back(static_cast<std::uint32_t>(index + 30));
    }
  }
  return slots;
}

} // namespace

namespace sfs {
void Menu::MigrateExternalStripLinkModeForWorkbenchVersion(
    const std::uint32_t a_workbenchSerializationVersion) {
  using Mode = workbench::ExternalModStripLinkMode;
  std::optional<Mode> migratedMode;
  if (a_workbenchSerializationVersion >= 2 &&
      a_workbenchSerializationVersion <= 9) {
    migratedMode = Mode::ModSettingsSlots;
  }
  if (!migratedMode.has_value()) {
    return;
  }

  workbench::SetExternalModStripLinkMode(*migratedMode);
  // DeserializeState calls MarkChanged before this legacy version decision is
  // available. If settings.json selected the opposite policy, that call may
  // have briefly populated its runtime cache. Clear both engines and rebuild
  // only the newly selected policy so no state crosses the migration edge.
  native::external_equipment::ClearRuntimeState();
  virtual_tokens::ResetVirtualWornTokenRuntimeState();
  workbench_.ClearAutomaticEquipmentVisibilityBindings();
  virtual_tokens::UpdateVirtualWornTokenCache();
  pendingLegacyExternalStripLinkMode_ = *migratedMode;
  if (!userSettingsPath_.empty()) {
    SaveUserSettings();
    pendingLegacyExternalStripLinkMode_.reset();
  }
  logger::info(
      "Migrated external strip-link mode from ROWS v{} to mode={}",
      a_workbenchSerializationVersion,
      static_cast<std::uint8_t>(*migratedMode));
}

void Menu::LoadUserSettings() {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create settings directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ifstream input(userSettingsPath_);
  if (!input.is_open()) {
    SaveUserSettings();
    return;
  }

  try {
    const auto json = nlohmann::json::parse(input, nullptr, true, true);
    auto &browser = CatalogBrowserState();
    fontSizePixels_ = std::clamp(json.value("fontSizePx", fontSizePixels_),
                                 kMinFontSizePixels, kMaxFontSizePixels);
    pendingFontSizePixels_ = fontSizePixels_;
    uiOpacity_ = std::clamp(json.value("uiOpacity", uiOpacity_), 0.10f, 1.0f);
    fontPath_ = json.value("fontPath", fontPath_);
    localeId_ = json.value("locale", localeId_);
    pauseGameWhenOpen_ = json.value("pauseGameWhileOpen", pauseGameWhenOpen_);
    smoothScroll_ = json.value("smoothScroll", smoothScroll_);
    const auto menuCharacterSide = json.value(
        "menuCharacterSide", static_cast<std::uint8_t>(menuCharacterSide_));
    menuCharacterSide_ =
        menuCharacterSide <=
                static_cast<std::uint8_t>(ui::MenuCharacterSide::Right)
            ? static_cast<ui::MenuCharacterSide>(menuCharacterSide)
            : ui::MenuCharacterSide::Right;
    addCrosshairNpcToActorList_ =
        json.value("addCrosshairNpcToActorList", addCrosshairNpcToActorList_);
    catalogBodyFamilyFilterEnabled_ = json.value(
        "catalogBodyFamilyFilterEnabled", catalogBodyFamilyFilterEnabled_);
    workbench::SetSpecialEffectProtectedSlotMask(
        ParseSpecialEffectProtectedSlots(json));
    workbench::SetShieldAppearanceSlotEnabled(
        json.value("shieldAppearanceSlotEnabled", false));
    const auto externalStripLinkMode = json.value(
        "externalModStripLinkMode",
        static_cast<std::uint8_t>(
            workbench::ExternalModStripLinkMode::ModSettingsSlots));
    workbench::SetExternalModStripLinkMode(
        externalStripLinkMode <= static_cast<std::uint8_t>(
                                     workbench::ExternalModStripLinkMode::DirectSlots)
            ? static_cast<workbench::ExternalModStripLinkMode>(
                  externalStripLinkMode)
            : workbench::ExternalModStripLinkMode::Disabled);
    const auto customStripLinkBaseMode = json.value(
        "customExternalModStripLinkBaseMode",
        static_cast<std::uint8_t>(
            workbench::ExternalModStripLinkMode::ModSettingsSlots));
    workbench::SetCustomExternalModStripLinkBaseMode(
        customStripLinkBaseMode == static_cast<std::uint8_t>(
                                       workbench::ExternalModStripLinkMode::
                                           ModSettingsSlots)
            ? workbench::ExternalModStripLinkMode::ModSettingsSlots
        : customStripLinkBaseMode == static_cast<std::uint8_t>(
                                         workbench::ExternalModStripLinkMode::
                                             DirectSlots)
            ? workbench::ExternalModStripLinkMode::DirectSlots
            : workbench::ExternalModStripLinkMode::VanillaSlots);
    const auto directAutomaticBaseMode = json.value(
        "customDirectStripLinkAutomaticBaseMode",
        static_cast<std::uint8_t>(
            workbench::ExternalModStripLinkMode::ModSettingsSlots));
    workbench::SetCustomDirectStripLinkAutomaticBaseMode(
        directAutomaticBaseMode == static_cast<std::uint8_t>(
                                       workbench::ExternalModStripLinkMode::
                                           VanillaSlots)
            ? workbench::ExternalModStripLinkMode::VanillaSlots
            : workbench::ExternalModStripLinkMode::ModSettingsSlots);
    const auto disabledAppearanceSlots =
        ParseCustomStripLinkDisabledSlots(json);
    workbench::SetCustomStripLinkDisabledAppearanceSlotMask(
        disabledAppearanceSlots);
    auto directMappings = ParseCustomDirectStripLinkMappings(json);
    workbench::SetCustomDirectStripLinkMappings(directMappings);
    auto directOverrides = ParseCustomDirectStripLinkOverrides(
        json, directMappings,
        customStripLinkBaseMode == static_cast<std::uint8_t>(
                                       workbench::ExternalModStripLinkMode::
                                           DirectSlots));
    if (customStripLinkBaseMode == static_cast<std::uint8_t>(
                                       workbench::ExternalModStripLinkMode::
                                           DirectSlots)) {
      for (std::uint32_t slot = 30; slot <= 61; ++slot) {
        if ((disabledAppearanceSlots & sfs::armor::GetArmorSlotMask(slot)) !=
            0) {
          const auto index = static_cast<std::size_t>(slot - 30);
          directMappings[index] = 0;
          directOverrides[index] = true;
        }
      }
      workbench::SetCustomDirectStripLinkMappings(directMappings);
    }
    workbench::SetCustomDirectStripLinkOverrides(directOverrides);
    hideRealEquipmentWithFitting_ = json.value("hideRealEquipmentWithFitting",
                                               hideRealEquipmentWithFitting_);
    toggleKey_ = json.value("toggleKey", toggleKey_);
    toggleModifier_ = json.value("toggleModifier", toggleModifier_);
    if (!keycode::IsValidHotkey(toggleKey_) ||
        keycode::IsKeyModifier(toggleKey_)) {
      toggleKey_ = 0x40;
      toggleModifier_ = 0;
    } else if (toggleModifier_ != 0) {
      const bool toggleKeyIsGamepad = keycode::IsGamepadKey(toggleKey_);
      const bool modifierIsValid =
          toggleKeyIsGamepad ? keycode::IsGamepadKey(toggleModifier_) &&
                                   toggleModifier_ != toggleKey_
                             : keycode::IsKeyModifier(toggleModifier_);
      if (!modifierIsValid) {
        toggleModifier_ = 0;
      }
    }
    themeName_ = json.value("theme", themeName_);
    const auto hostMode = json.value("catalogHostMode", std::string("docked"));
    if (hostMode == "popout") {
      catalogPane_.hostMode = ui::catalog::HostMode::Popout;
      catalogPane_.popoutOpen = true;
    } else {
      catalogPane_.hostMode = ui::catalog::HostMode::Docked;
      catalogPane_.popoutOpen = false;
    }
    browser.previewSelected =
        json.value("catalogPreviewSelected", browser.previewSelected);
    browser.favoritesOnly =
        json.value("catalogFavoritesOnly", browser.favoritesOnly);
    browser.inventoryOnly =
        json.value("catalogInventoryOnly", browser.inventoryOnly);
    browser.hideUnnamedGear =
        json.value("catalogHideUnnamedGear", browser.hideUnnamedGear);
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse user settings from {}: {}", userSettingsPath_,
                 exception.what());
  }

  EnsureDefaultConditions();
}

void Menu::SaveUserSettings() const {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create settings directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ofstream output(userSettingsPath_, std::ios::trunc);
  if (!output.is_open()) {
    logger::error("Failed to write settings to {}", userSettingsPath_);
    return;
  }

  nlohmann::json json = {
      {"fontSizePx", fontSizePixels_},
      {"uiOpacity", uiOpacity_},
      {"fontPath", fontPath_},
      {"locale", localeId_},
      {"pauseGameWhileOpen", pauseGameWhenOpen_},
      {"smoothScroll", smoothScroll_},
      {"menuCharacterSide", static_cast<std::uint8_t>(menuCharacterSide_)},
      {"addCrosshairNpcToActorList", addCrosshairNpcToActorList_},
      {"catalogBodyFamilyFilterEnabled", catalogBodyFamilyFilterEnabled_},
      {"specialEffectProtectedSlots",
       BuildSpecialEffectProtectedSlotNumbers(
           workbench::GetSpecialEffectProtectedSlotMask())},
      {"shieldAppearanceSlotEnabled",
       workbench::IsShieldAppearanceSlotEnabled()},
      {"externalModStripLinkMode",
       static_cast<std::uint8_t>(
           workbench::GetExternalModStripLinkMode())},
      {"customExternalModStripLinkBaseMode",
       static_cast<std::uint8_t>(
           workbench::GetCustomExternalModStripLinkBaseMode())},
      {"customDirectStripLinkAutomaticBaseMode",
       static_cast<std::uint8_t>(
           workbench::GetCustomDirectStripLinkAutomaticBaseMode())},
      {"customStripLinkDisabledAppearanceSlots",
       BuildCustomStripLinkDisabledSlotNumbers(
           workbench::GetCustomStripLinkDisabledAppearanceSlotMask())},
      {"customDirectStripLinkMappings",
       BuildCustomDirectStripLinkMappings()},
      {"customDirectStripLinkOverrideSlots",
       BuildCustomDirectStripLinkOverrideSlots()},
      {"hideRealEquipmentWithFitting", hideRealEquipmentWithFitting_},
      {"toggleKey", toggleKey_},
      {"toggleModifier", toggleModifier_},
      {"theme", themeName_},
      {"catalogPreviewSelected", CatalogBrowserState().previewSelected},
      {"catalogFavoritesOnly", CatalogBrowserState().favoritesOnly},
      {"catalogInventoryOnly", CatalogBrowserState().inventoryOnly},
      {"catalogHideUnnamedGear", CatalogBrowserState().hideUnnamedGear},
      {"catalogHostMode", catalogPane_.hostMode == ui::catalog::HostMode::Popout
                              ? "popout"
                              : "docked"}};
  output << json.dump(2) << '\n';
}

void Menu::LoadFavorites() {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create favorites directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  CatalogBrowserState().favoriteKeys.clear();

  std::ifstream input(favoritesPath_);
  if (!input.is_open()) {
    SaveFavorites();
    return;
  }

  try {
    const auto json = nlohmann::json::parse(input, nullptr, true, true);
    if (const auto it = json.find("favorites");
        it != json.end() && it->is_array()) {
      for (const auto &entry : *it) {
        if (entry.is_string()) {
          CatalogBrowserState().favoriteKeys.insert(entry.get<std::string>());
        }
      }
    }
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse favorites from {}: {}", favoritesPath_,
                 exception.what());
  }
}

void Menu::SaveFavorites() const {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create favorites directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ofstream output(favoritesPath_, std::ios::trunc);
  if (!output.is_open()) {
    logger::error("Failed to write favorites to {}", favoritesPath_);
    return;
  }

  std::vector<std::string> favorites(CatalogBrowserState().favoriteKeys.begin(),
                                     CatalogBrowserState().favoriteKeys.end());
  std::ranges::sort(favorites);
  const nlohmann::json json = {{"favorites", favorites}};
  output << json.dump(2) << '\n';
}

void Menu::ApplyStyle() {
  ImGui::StyleColorsDark();
  auto &style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.ChildRounding = 3.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;
  style.FrameBorderSize = 1.0f;
  style.WindowBorderSize = 1.0f;
  style.WindowPadding = {12.0f, 12.0f};
  style.FramePadding = {8.0f, 6.0f};
  style.ItemSpacing = {8.0f, 6.0f};
  style.CellPadding = {6.0f, 5.0f};
  ThemeConfig::GetSingleton()->ApplyToImGui();
}

void Menu::NormalizeSelectedLocaleId() {
  if (localeId_.empty()) {
    localeId_ = kDefaultLocaleId;
  }

  auto *localization = ui::Localization::GetSingleton();
  localization->SelectLocale(localeId_);
  localeId_ = localization->GetCurrentLocaleId();
}

void Menu::OpenToggleKeyCapture() {
  awaitingToggleKeyCapture_ = true;
  openToggleKeyPopup_ = true;
  toggleKeyCaptureError_.clear();
}

void Menu::CloseToggleKeyCapture() {
  awaitingToggleKeyCapture_ = false;
  openToggleKeyPopup_ = false;
  toggleKeyCaptureError_.clear();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Menu::HandleToggleKeyCapture(const std::uint32_t a_scanCode,
                                  const std::uint32_t a_modifierScanCode) {
  if (a_scanCode == 0x01) {
    CloseToggleKeyCapture();
    return;
  }

  if (a_scanCode == 0x14) {
    toggleKey_ = 0x40;
    toggleModifier_ = 0;
    SaveUserSettings();
    CloseToggleKeyCapture();
    return;
  }

  const bool keyIsGamepad = keycode::IsGamepadKey(a_scanCode);
  const bool modifierIsValid =
      a_modifierScanCode == 0 ||
      (keyIsGamepad ? keycode::IsGamepadKey(a_modifierScanCode) &&
                          a_modifierScanCode != a_scanCode
                    : keycode::IsKeyModifier(a_modifierScanCode));
  if (!keycode::IsValidHotkey(a_scanCode) ||
      keycode::IsKeyModifier(a_scanCode) || !modifierIsValid) {
    toggleKeyCaptureError_ = std::string(ui::Localization::GetSingleton()->Get(
        "options.toggle_capture.invalid"));
    return;
  }

  toggleKey_ = a_scanCode;
  toggleModifier_ = a_modifierScanCode;
  SaveUserSettings();
  CloseToggleKeyCapture();
}

std::string Menu::GetToggleKeyLabel() const {
  if (toggleModifier_ != 0) {
    return keycode::GetKeyName(toggleModifier_) + " + " +
           keycode::GetKeyName(toggleKey_);
  }

  return keycode::GetKeyName(toggleKey_);
}

void Menu::PrepareUserSettingsStorage() {
  if (settingsDirectory_.empty()) {
    settingsDirectory_ = kSettingsDirectory;
    imguiIniPath_ =
        (std::filesystem::path(settingsDirectory_) / kImGuiIniFilename)
            .string();
    userSettingsPath_ =
        (std::filesystem::path(settingsDirectory_) / kUserSettingsFilename)
            .string();
    favoritesPath_ =
        (std::filesystem::path(settingsDirectory_) / kFavoritesFilename)
            .string();
    ResetSaveDataPath();
  }

  std::error_code error;
  const bool settingsExist =
      std::filesystem::is_regular_file(userSettingsPath_, error);
  if (error) {
    logger::warn("Could not inspect SFS user settings path {}: {}",
                 userSettingsPath_, error.message());
  } else if (!settingsExist) {
    SaveUserSettings();
  }

  error.clear();
  const auto absolutePath = std::filesystem::absolute(userSettingsPath_, error);
  logger::info("SFS user settings path: {}",
               error ? userSettingsPath_ : absolutePath.string());
}

void Menu::Init(IDXGISwapChain *a_swapChain, ID3D11Device *a_device,
                ID3D11DeviceContext *a_context) {
  if (initialized_) {
    return;
  }

  PrepareUserSettingsStorage();
  RefreshAvailableFonts();
  ui::Localization::GetSingleton()->RefreshAvailableLocales(kLocaleDirectory);
  LoadUserSettings();
  if (pendingLegacyExternalStripLinkMode_.has_value()) {
    workbench::SetExternalModStripLinkMode(
        *pendingLegacyExternalStripLinkMode_);
    SaveUserSettings();
    pendingLegacyExternalStripLinkMode_.reset();
  }
  NormalizeSelectedFontPath();
  NormalizeSelectedLocaleId();
  LoadFavorites();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  auto &io = ImGui::GetIO();
  io.IniFilename = imguiIniPath_.c_str();
  io.LogFilename = nullptr;
  ImGui::GetStyle().FontScaleMain = 1.0f;
  pendingFontSizePixels_ = fontSizePixels_;
  RebuildFontAtlas();

  DXGI_SWAP_CHAIN_DESC description{};
  a_swapChain->GetDesc(&description);

  ImGui_ImplWin32_Init(description.OutputWindow);
  ImGui_ImplDX11_Init(a_device, a_context);

  device_ = a_device;
  context_ = a_context;

  ThemeConfig::GetSingleton()->Load(themeName_);
  ApplyStyle();
  initialized_ = true;
  api::SetMenuInitialized(true);

  logger::info("Initialized Dear ImGui menu");
}
} // namespace sfs
