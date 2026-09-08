#include "Menu.h"

#include "native/ExternalEquipmentTransactions.h"
#include "features/devious_devices/DeviousDevicesIntegration.h"
#include "features/virtual_tokens/VirtualWornTokens.h"

#include "ArmorUtils.h"
#include "ThemeConfig.h"
#include "Utf8Path.h"
#include "backends/imgui_impl_dx11.h"
#include "ui/IconSupport.h"
#include "ui/Localization.h"
#include "workbench/AppearanceSlotProtection.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <unordered_set>

namespace sfs {
namespace {
constexpr std::array<ImWchar, 3> kLucideIconRanges = {0xE038, 0xE63F, 0};

bool IsFontFile(const std::filesystem::path &a_path) {
  std::string extension;
  try {
    extension = utf8::PathToUtf8String(a_path.extension());
  } catch (const std::exception &) {
    return false;
  }
  if (extension.empty()) {
    return false;
  }

  std::string lowerExtension;
  lowerExtension.reserve(extension.size());
  for (const auto ch : extension) {
    lowerExtension.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }

  return lowerExtension == ".ttf" || lowerExtension == ".otf" ||
         lowerExtension == ".ttc" || lowerExtension == ".otc";
}

int CompareFontText(std::string_view a_left, std::string_view a_right) {
  const auto leftSize = a_left.size();
  const auto rightSize = a_right.size();
  const auto count = (std::min)(leftSize, rightSize);

  for (std::size_t index = 0; index < count; ++index) {
    const auto left = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_left[index])));
    const auto right = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_right[index])));
    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
  }

  if (leftSize < rightSize) {
    return -1;
  }
  if (leftSize > rightSize) {
    return 1;
  }
  return 0;
}

std::string BuildFontLabel(const std::filesystem::path &a_path) {
  if (const auto stem = utf8::PathToUtf8String(a_path.stem()); !stem.empty()) {
    return stem;
  }

  return utf8::PathToUtf8String(a_path.filename());
}

void SortFontOptions(std::vector<Menu::FontOption> &a_options) {
  std::ranges::sort(a_options, [](const auto &left, const auto &right) {
    return CompareFontText(left.label, right.label) < 0;
  });
}

std::vector<std::filesystem::path> BuildSystemFontDirectories() {
  std::vector<std::filesystem::path> directories;
  auto getEnvironmentPath =
      [](const wchar_t *name) -> std::optional<std::filesystem::path> {
    wchar_t *value = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&value, &length, name) != 0 || value == nullptr ||
        length <= 1) {
      free(value);
      return std::nullopt;
    }

    std::filesystem::path result(value);
    free(value);
    return result;
  };

  if (const auto windowsDir = getEnvironmentPath(L"WINDIR")) {
    directories.emplace_back(*windowsDir / "Fonts");
  } else {
    directories.emplace_back("C:/Windows/Fonts");
  }

  if (const auto localAppData = getEnvironmentPath(L"LOCALAPPDATA")) {
    directories.emplace_back(*localAppData / "Microsoft/Windows/Fonts");
  }

  std::vector<std::filesystem::path> uniqueDirectories;
  for (const auto &directory : directories) {
    if (std::ranges::find(uniqueDirectories, directory) ==
        uniqueDirectories.end()) {
      uniqueDirectories.push_back(directory);
    }
  }
  return uniqueDirectories;
}

std::vector<std::filesystem::path> BuildDefaultSystemFontCandidates() {
  static constexpr std::array kCandidateFileNames{
      "malgun.ttf",  "malgunsl.ttf", "malgunbd.ttf", "msyh.ttc",
      "msyhbd.ttc",  "simsun.ttc",   "simhei.ttf",   "mingliu.ttc",
      "YuGothM.ttc", "msgothic.ttc", "segoeui.ttf",  "segoeuib.ttf",
      "arial.ttf",   "tahoma.ttf"};

  std::vector<std::filesystem::path> candidates;
  std::unordered_set<std::filesystem::path> seenPaths;
  for (const auto &directory : BuildSystemFontDirectories()) {
    for (const auto *fileName : kCandidateFileNames) {
      auto path = directory / fileName;
      std::error_code error;
      if (!std::filesystem::exists(path, error)) {
        continue;
      }

      auto normalizedPath = path.lexically_normal();
      if (seenPaths.insert(normalizedPath).second) {
        candidates.push_back(std::move(normalizedPath));
      }
    }
  }
  return candidates;
}

std::optional<std::string> FindDefaultSystemFontPath() {
  for (const auto &path : BuildDefaultSystemFontCandidates()) {
    return utf8::PathToUtf8String(path);
  }
  return std::nullopt;
}

std::optional<std::filesystem::path>
NormalizeUtf8Path(const std::string_view a_path) {
  if (a_path.empty()) {
    return std::nullopt;
  }

  try {
    return utf8::PathFromUtf8(a_path).lexically_normal();
  } catch (const std::exception &exception) {
    logger::warn("SFS ignored invalid UTF-8 font path: {}", exception.what());
    return std::nullopt;
  }
}

ImFont *AddFontFromPath(ImFontAtlas *a_atlas,
                        const std::filesystem::path &a_path,
                        const float a_sizePixels, ImFontConfig *a_config,
                        const ImWchar *a_glyphRanges = nullptr) {
  if (!a_atlas) {
    return nullptr;
  }

  std::error_code error;
  if (!std::filesystem::exists(a_path, error) || error) {
    return nullptr;
  }

  try {
    const auto utf8Path = utf8::PathToUtf8String(a_path.lexically_normal());
    return a_atlas->AddFontFromFileTTF(utf8Path.c_str(), a_sizePixels,
                                       a_config, a_glyphRanges);
  } catch (const std::exception &exception) {
    logger::warn("SFS failed to load font path as UTF-8: {}",
                 exception.what());
    return nullptr;
  }
}

const char *
FindSelectedFontLabel(const std::vector<Menu::FontOption> &a_options,
                      const std::string &a_fontPath) {
  if (const auto it = std::ranges::find_if(a_options,
                                           [&](const Menu::FontOption &option) {
                                             return option.path == a_fontPath;
                                           });
      it != a_options.end()) {
    return it->label.c_str();
  }

  return nullptr;
}

std::string BuildProtectedSlotPreview(const std::uint64_t a_slotMask,
                                      const std::string_view a_noneLabel) {
  std::vector<std::string> slotNumbers;
  for (std::uint32_t slotNumber = 30; slotNumber <= 61; ++slotNumber) {
    if ((a_slotMask & sfs::armor::GetArmorSlotMask(slotNumber)) != 0) {
      slotNumbers.push_back(std::to_string(slotNumber));
    }
  }
  return slotNumbers.empty() ? std::string(a_noneLabel)
                             : sfs::armor::JoinStrings(slotNumbers);
}
} // namespace

void Menu::RefreshAvailableFonts() {
  bundledFontOptions_.clear();
  systemFontOptions_.clear();

  std::unordered_set<std::filesystem::path> seenPaths;
  const auto iconFontPath =
      utf8::PathFromUtf8(kDefaultIconFontPath).lexically_normal();

  auto collectFonts = [&](const std::filesystem::path &directory,
                          std::vector<FontOption> &target, bool isBundled) {
    std::error_code error;
    if (!std::filesystem::exists(directory, error) ||
        !std::filesystem::is_directory(directory, error)) {
      return;
    }

    for (const auto &entry :
         std::filesystem::directory_iterator(directory, error)) {
      std::error_code entryError;
      if (error || !entry.is_regular_file(entryError) || entryError) {
        continue;
      }

      const auto &path = entry.path();
      if (!IsFontFile(path)) {
        continue;
      }

      const auto normalizedPath = path.lexically_normal();
      if (normalizedPath == iconFontPath ||
          !seenPaths.insert(normalizedPath).second) {
        continue;
      }

      try {
        target.push_back({.label = BuildFontLabel(path),
                          .path = utf8::PathToUtf8String(normalizedPath),
                          .isBundled = isBundled});
      } catch (const std::exception &exception) {
        logger::warn("SFS skipped font with invalid Unicode path: {}",
                     exception.what());
      }
    }
  };

  collectFonts(kBundledFontDirectory, bundledFontOptions_, true);
  for (const auto &directory : BuildSystemFontDirectories()) {
    collectFonts(directory, systemFontOptions_, false);
  }

  SortFontOptions(bundledFontOptions_);
  SortFontOptions(systemFontOptions_);
}

void Menu::NormalizeSelectedFontPath() {
  if (fontPath_.empty()) {
    fontPath_ = FindDefaultSystemFontPath().value_or(kDefaultFontPath);
  }

  auto matchesPath = [&](const FontOption &option) {
    return option.path == fontPath_;
  };

  if (std::ranges::find_if(bundledFontOptions_, matchesPath) !=
          bundledFontOptions_.end() ||
      std::ranges::find_if(systemFontOptions_, matchesPath) !=
          systemFontOptions_.end()) {
    return;
  }

  fontPath_ = FindDefaultSystemFontPath().value_or(kDefaultFontPath);
}

void Menu::RebuildFontAtlas() {
  auto &io = ImGui::GetIO();
  auto &style = ImGui::GetStyle();
  ImFontConfig fontConfig{};
  fontConfig.SizePixels = static_cast<float>(fontSizePixels_);
  fontConfig.OversampleH = 1;
  fontConfig.OversampleV = 1;
  fontConfig.PixelSnapH = true;

  io.Fonts->Clear();
  io.FontDefault = nullptr;
  const auto selectedFontPath = NormalizeUtf8Path(fontPath_);
  if (selectedFontPath) {
    io.FontDefault =
        AddFontFromPath(io.Fonts, *selectedFontPath,
                        static_cast<float>(fontSizePixels_), &fontConfig);
  }
  const auto defaultSystemFontPathText =
      FindDefaultSystemFontPath().value_or(kDefaultFontPath);
  const auto defaultSystemFontPath =
      NormalizeUtf8Path(defaultSystemFontPathText);
  if (!io.FontDefault && defaultSystemFontPath) {
    io.FontDefault =
        AddFontFromPath(io.Fonts, *defaultSystemFontPath,
                        static_cast<float>(fontSizePixels_), &fontConfig);
  }
  if (!io.FontDefault) {
    io.FontDefault = io.Fonts->AddFontDefaultVector(&fontConfig);
  }
  if (io.FontDefault) {
    std::unordered_set<std::filesystem::path> mergedFontPaths;
    if (selectedFontPath) {
      mergedFontPaths.insert(*selectedFontPath);
    }
    if (defaultSystemFontPath) {
      mergedFontPaths.insert(*defaultSystemFontPath);
    }

    for (const auto &fallbackPath : BuildDefaultSystemFontCandidates()) {
      const auto normalizedPath = fallbackPath.lexically_normal();
      if (!mergedFontPaths.insert(normalizedPath).second) {
        continue;
      }

      ImFontConfig fallbackConfig = fontConfig;
      fallbackConfig.MergeMode = true;
      fallbackConfig.PixelSnapH = true;
      fallbackConfig.DstFont = io.FontDefault;
      AddFontFromPath(io.Fonts, normalizedPath,
                      static_cast<float>(fontSizePixels_), &fallbackConfig);
    }
  }
  bool iconFontAvailable = false;
  if (io.FontDefault) {
    ImFontConfig iconConfig{};
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphOffset.y = 3.0f;
    iconConfig.DstFont = io.FontDefault;
    if (const auto iconFontPath = NormalizeUtf8Path(kDefaultIconFontPath)) {
      iconFontAvailable =
          AddFontFromPath(io.Fonts, *iconFontPath,
                          static_cast<float>(fontSizePixels_), &iconConfig,
                          kLucideIconRanges.data()) != nullptr;
    }
  }
  ui::icons::SetIconFontAvailable(iconFontAvailable);
  io.Fonts->Build();
  style.FontScaleMain = 1.0f;
  style._NextFrameFontSizeBase = static_cast<float>(fontSizePixels_);
  style.FontSizeBase = static_cast<float>(fontSizePixels_);

  if (initialized_) {
    ImGui_ImplDX11_InvalidateDeviceObjects();
  }
}

void Menu::DrawOptionsTab() {
  auto *localization = ui::Localization::GetSingleton();
  const auto interfaceLabel = localization->Get("options.interface");
  const auto descriptionLabel = localization->Get("options.description");
  const auto controlsLabel = localization->Get("options.controls");
  const auto languageLabel = localization->Get("options.language");
  const auto fontLabel = localization->Get("options.font");
  const auto defaultLabel = localization->Get("common.default");
  const auto bundledFontsLabel = localization->Get("options.font.bundled");
  const auto systemFontsLabel = localization->Get("options.font.system");
  const auto fontSizeLabel = localization->Get("options.font_size");
  const auto minFontSize = kMinFontSizePixels;
  const auto maxFontSize = kMaxFontSizePixels;
  const auto fontSizeRangeLabel = sfs::strings::SafeVFormat(
      std::string(localization->Get("options.font_size.range")),
      std::make_format_args(minFontSize, maxFontSize));
  const auto uiOpacityLabel = localization->Get("options.ui_opacity");
  const auto characterPositionLabel =
      localization->Get("options.character_position");
  const auto characterPositionTooltip =
      localization->Get("options.character_position.tooltip");
  const std::array characterPositionModes{
      ui::MenuCharacterSide::Disabled, ui::MenuCharacterSide::Left,
      ui::MenuCharacterSide::Right};
  const std::array characterPositionModeLabels{
      localization->Get("options.character_position.disabled"),
      localization->Get("options.character_position.left"),
      localization->Get("options.character_position.right")};
  const auto resetToDefaultLabel = localization->Get("options.reset_default");
  const auto toggleUIButtonLabel =
      localization->Get("options.toggle_ui_button");
  const auto pauseGameLabel =
      localization->Get("options.pause_game_while_open");
  const auto smoothScrollingLabel =
      localization->Get("options.smooth_scrolling");
  const auto addCrosshairNpcLabel =
      localization->Get("options.add_crosshair_npc");
  const auto bodyFamilyFilterLabel =
      localization->Get("options.catalog_body_family_filter");
  const auto bodyFamilyFilterTooltip =
      localization->Get("options.catalog_body_family_filter.tooltip");
  const auto externalStripLinkLabel =
      localization->Get("options.external_mod_strip_link");
  const auto externalStripLinkTooltip =
      localization->Get("options.external_mod_strip_link.tooltip");
  const std::array externalStripLinkModes{
      workbench::ExternalModStripLinkMode::ModSettingsSlots,
      workbench::ExternalModStripLinkMode::VanillaSlots,
      workbench::ExternalModStripLinkMode::Custom,
      workbench::ExternalModStripLinkMode::Disabled};
  const std::array externalStripLinkModeLabels{
      localization->Get("options.external_mod_strip_link.mod_settings_slots"),
      localization->Get("options.external_mod_strip_link.vanilla_slots"),
      localization->Get("options.external_mod_strip_link.custom"),
      localization->Get("options.external_mod_strip_link.disabled")};
  const std::array externalStripLinkModeTooltipKeys{
      "options.external_mod_strip_link.mod_settings_slots.tooltip",
      "options.external_mod_strip_link.vanilla_slots.tooltip",
      "options.external_mod_strip_link.custom.tooltip",
      "options.external_mod_strip_link.disabled.tooltip"};
  const auto protectedSlotsLabel =
      localization->Get("options.special_effect_protected_slots");
  const auto protectedSlotsTooltip =
      localization->Get("options.special_effect_protected_slots.tooltip");
  const auto protectedSlotsResetLabel =
      localization->Get("options.special_effect_protected_slots.reset");
  const auto shieldAppearanceLabel =
      localization->Get("options.shield_appearance_slot");
  const auto shieldAppearanceTooltip =
      localization->Get("options.shield_appearance_slot.tooltip");
  const auto saveDataLabel = localization->Get("options.save_data");
  const auto saveDataPathHint =
      localization->Get("options.save_data.path_hint");
  const auto exportJsonLabel = localization->Get("options.export_json");
  const auto importJsonLabel = localization->Get("options.import_json");
  const auto importJsonErrorTitle =
      localization->Get("options.import_json.error_title");
  const auto toggleCaptureTitle =
      localization->Get("options.toggle_capture.title");
  const auto toggleCaptureBody =
      localization->Get("options.toggle_capture.body");
  const auto toggleCaptureHint =
      localization->Get("options.toggle_capture.hint");
  const auto cancelLabel = localization->Get("common.cancel");

  ClearCatalogSelection();
  ImGui::TextUnformatted(interfaceLabel.data());
  ImGui::Separator();

  if (ImGui::BeginChild("##options-font-panel", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_Borders |
                            ImGuiChildFlags_AutoResizeY)) {
    if (ImGui::BeginTable("##options-layout", 2,
                          ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0.0f, 0.0f))) {
      ImGui::TableSetupColumn(descriptionLabel.data(),
                              ImGuiTableColumnFlags_WidthStretch, 1.15f);
      ImGui::TableSetupColumn(controlsLabel.data(),
                              ImGuiTableColumnFlags_WidthStretch, 0.85f);
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(languageLabel.data());

      ImGui::TableSetColumnIndex(1);
      {
        const auto &localeOptions = localization->GetAvailableLocales();
        const auto &selectedLocaleName = localization->GetCurrentLocaleName();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##locale", selectedLocaleName.c_str())) {
          for (const auto &option : localeOptions) {
            const bool selected = option.id == localeId_;
            if (ImGui::Selectable(option.name.c_str(), selected)) {
              localeId_ = option.id;
              NormalizeSelectedLocaleId();
              SaveUserSettings();
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
      }

      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(fontLabel.data());

      ImGui::TableSetColumnIndex(1);
      const char *selectedFontLabel =
          FindSelectedFontLabel(bundledFontOptions_, fontPath_);
      if (!selectedFontLabel) {
        selectedFontLabel =
            FindSelectedFontLabel(systemFontOptions_, fontPath_);
      }
      if (!selectedFontLabel) {
        selectedFontLabel = defaultLabel.data();
      }

      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::BeginCombo("##font-path", selectedFontLabel)) {
        if (!bundledFontOptions_.empty()) {
          ImGui::SeparatorText(bundledFontsLabel.data());
          for (const auto &option : bundledFontOptions_) {
            const bool selected = option.path == fontPath_;
            if (ImGui::Selectable(option.label.c_str(), selected)) {
              fontPath_ = option.path;
              SaveUserSettings();
              pendingFontAtlasRebuild_ = true;
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
        }

        if (!systemFontOptions_.empty()) {
          ImGui::SeparatorText(systemFontsLabel.data());
          for (const auto &option : systemFontOptions_) {
            const bool selected = option.path == fontPath_;
            if (ImGui::Selectable(option.label.c_str(), selected)) {
              fontPath_ = option.path;
              SaveUserSettings();
              pendingFontAtlasRebuild_ = true;
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
        }

        ImGui::EndCombo();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(fontSizeLabel.data());
      ImGui::TextDisabled("%s", fontSizeRangeLabel.c_str());

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::SliderInt("##font-size", &pendingFontSizePixels_,
                       kMinFontSizePixels, kMaxFontSizePixels, "%d px");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        fontSizePixels_ = pendingFontSizePixels_;
        SaveUserSettings();
        pendingFontAtlasRebuild_ = true;
      }

      if (ImGui::Button(resetToDefaultLabel.data())) {
        fontPath_ = FindDefaultSystemFontPath().value_or(kDefaultFontPath);
        fontSizePixels_ = kDefaultFontSizePixels;
        pendingFontSizePixels_ = fontSizePixels_;
        SaveUserSettings();
        pendingFontAtlasRebuild_ = true;
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(uiOpacityLabel.data());
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      float opacityPercent = uiOpacity_ * 100.0f;
      ImGui::SliderFloat("##ui-opacity", &opacityPercent, 10.0f, 100.0f,
                         "%.0f%%");
      const float newOpacity =
          std::clamp(opacityPercent / 100.0f, 0.10f, 1.0f);
      if (std::abs(newOpacity - uiOpacity_) > 0.0001f) {
        uiOpacity_ = newOpacity;
        SaveUserSettings();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(characterPositionLabel.data());
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("%s", characterPositionTooltip.data());
      }

      ImGui::TableSetColumnIndex(1);
      auto selectedCharacterPosition = std::size_t{0};
      if (const auto selectedIt =
              std::ranges::find(characterPositionModes, menuCharacterSide_);
          selectedIt != characterPositionModes.end()) {
        selectedCharacterPosition = static_cast<std::size_t>(
            std::distance(characterPositionModes.begin(), selectedIt));
      }
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::BeginCombo(
              "##menu-character-position",
              characterPositionModeLabels[selectedCharacterPosition].data())) {
        for (std::size_t index = 0; index < characterPositionModes.size();
             ++index) {
          const bool selected = menuCharacterSide_ == characterPositionModes[index];
          if (ImGui::Selectable(characterPositionModeLabels[index].data(),
                                selected)) {
            menuCharacterSide_ = characterPositionModes[index];
            SaveUserSettings();
            ui::MenuCharacterPresentation::GetSingleton()->Apply(
                menuCharacterSide_, ResolveWorkbenchPreviewActor());
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(toggleUIButtonLabel.data());

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::Button(GetToggleKeyLabel().c_str(), ImVec2(-FLT_MIN, 0.0f))) {
        OpenToggleKeyCapture();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(pauseGameLabel.data());

      ImGui::TableSetColumnIndex(1);
      bool pauseGameWhenOpen = pauseGameWhenOpen_;
      if (ImGui::Checkbox("##pause-game-while-open", &pauseGameWhenOpen)) {
        pauseGameWhenOpen_ = pauseGameWhenOpen;
        SaveUserSettings();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(smoothScrollingLabel.data());

      ImGui::TableSetColumnIndex(1);
      bool smoothScroll = smoothScroll_;
      if (ImGui::Checkbox("##smooth-scrolling", &smoothScroll)) {
        smoothScroll_ = smoothScroll;
        pendingSmoothWheelDelta_ = 0.0f;
        smoothScrollWindowId_ = 0;
        smoothScrollTargetY_ = 0.0f;
        SaveUserSettings();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(addCrosshairNpcLabel.data());

      ImGui::TableSetColumnIndex(1);
      bool addCrosshairNpc = addCrosshairNpcToActorList_;
      if (ImGui::Checkbox("##add-crosshair-npc", &addCrosshairNpc)) {
        addCrosshairNpcToActorList_ = addCrosshairNpc;
        SaveUserSettings();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(bodyFamilyFilterLabel.data());
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("%s", bodyFamilyFilterTooltip.data());
      }

      ImGui::TableSetColumnIndex(1);
      bool bodyFamilyFilterEnabled = catalogBodyFamilyFilterEnabled_;
      if (ImGui::Checkbox("##catalog-body-family-filter",
                          &bodyFamilyFilterEnabled)) {
        catalogBodyFamilyFilterEnabled_ = bodyFamilyFilterEnabled;
        ClearCatalogSelection();
        InvalidateCatalogDerivedState();
        SaveUserSettings();
      }

      ImGui::EndTable();

      const float optionBlockStartX = ImGui::GetCursorPosX();
      const float optionBlockWidth = ImGui::GetContentRegionAvail().x;
      const float optionControlX =
          optionBlockStartX + optionBlockWidth * .575f;

      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(externalStripLinkLabel.data());
      ImGui::SameLine();
      ImGui::SetCursorPosX(optionControlX);
      const auto externalStripLinkMode =
          workbench::GetExternalModStripLinkMode();
      const auto selectedModeIt = std::ranges::find(
          externalStripLinkModes, externalStripLinkMode);
      const auto selectedModeIndex = selectedModeIt != externalStripLinkModes.end()
                                         ? static_cast<std::size_t>(
                                               selectedModeIt -
                                               externalStripLinkModes.begin())
                                         : 0;
      const auto externalStripLinkPreview =
          externalStripLinkModeLabels[selectedModeIndex];
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::BeginCombo("##external-mod-strip-link",
                            externalStripLinkPreview.data())) {
        for (std::size_t index = 0;
             index < externalStripLinkModeLabels.size(); ++index) {
          const auto mode = externalStripLinkModes[index];
          const bool selected = externalStripLinkMode == mode;
          if (ImGui::Selectable(externalStripLinkModeLabels[index].data(),
                                selected)) {
            if (mode == workbench::ExternalModStripLinkMode::Custom) {
              OpenStripLinkDialog();
            } else {
              workbench::SetExternalModStripLinkMode(mode);
              native::external_equipment::ClearRuntimeState();
              virtual_tokens::ResetVirtualWornTokenRuntimeState();
              virtual_tokens::UpdateVirtualWornTokenCache();
              if (mode == workbench::ExternalModStripLinkMode::
                              ModSettingsSlots) {
                static_cast<void>(devious_devices::
                                      RefreshDeviousDevicesHiderSettings());
              }
              SaveUserSettings();
              workbench_.RefreshNativeArmorOverrides(
                  ConditionDefinitions(), conditionStore_.revision, true);
            }
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "%s", localization->GetCStr(
                          externalStripLinkModeTooltipKeys[index]));
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", externalStripLinkTooltip.data());
      ImGui::PopTextWrapPos();

      ImGui::Spacing();

      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(protectedSlotsLabel.data());
      ImGui::SameLine();
      ImGui::SetCursorPosX(optionControlX);
      auto protectedSlotMask =
          workbench::GetSpecialEffectProtectedSlotMask();
      const auto protectedSlotPreview = BuildProtectedSlotPreview(
          protectedSlotMask, localization->Get("common.none"));
      bool protectedSlotsChanged = false;
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::BeginCombo("##special-effect-protected-slots",
                            protectedSlotPreview.c_str())) {
        for (std::uint32_t slotNumber = 30; slotNumber <= 61; ++slotNumber) {
          if (slotNumber == 39) {
            continue;
          }
          const auto slotMask = armor::GetArmorSlotMask(slotNumber);
          const auto slotLabels = armor::GetArmorSlotLabels(slotMask);
          const auto slotLabel = armor::JoinStrings(slotLabels);
          const bool selected = (protectedSlotMask & slotMask) != 0;
          if (ImGui::Selectable(slotLabel.c_str(), selected,
                                ImGuiSelectableFlags_DontClosePopups)) {
            protectedSlotMask = selected ? protectedSlotMask & ~slotMask
                                         : protectedSlotMask | slotMask;
            protectedSlotsChanged = true;
          }
        }
        ImGui::Separator();
        if (ImGui::Selectable(protectedSlotsResetLabel.data(), false,
                              ImGuiSelectableFlags_DontClosePopups)) {
          protectedSlotMask =
              workbench::kDefaultSpecialEffectProtectedSlotMask;
          protectedSlotsChanged = true;
        }
        ImGui::EndCombo();
      }
      if (protectedSlotsChanged) {
        workbench::SetSpecialEffectProtectedSlotMask(protectedSlotMask);
        workbench_.RemoveProtectedAppearanceRegistrations();
        native::external_equipment::ClearRuntimeState();
        virtual_tokens::ResetVirtualWornTokenRuntimeState();
        virtual_tokens::UpdateVirtualWornTokenCache();
        SaveUserSettings();
        workbench_.RefreshNativeArmorOverrides(
            ConditionDefinitions(), conditionStore_.revision, true);
      }
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", protectedSlotsTooltip.data());
      ImGui::PopTextWrapPos();

      ImGui::Spacing();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(shieldAppearanceLabel.data());
      ImGui::SameLine();
      ImGui::SetCursorPosX(optionControlX);
      bool shieldAppearanceEnabled =
          workbench::IsShieldAppearanceSlotEnabled();
      if (ImGui::Checkbox("##shield-appearance-slot",
                          &shieldAppearanceEnabled)) {
        workbench::SetShieldAppearanceSlotEnabled(shieldAppearanceEnabled);
        workbench_.RemoveProtectedAppearanceRegistrations();
        native::external_equipment::ClearRuntimeState();
        virtual_tokens::ResetVirtualWornTokenRuntimeState();
        virtual_tokens::UpdateVirtualWornTokenCache();
        SaveUserSettings();
        workbench_.RefreshNativeArmorOverrides(
            ConditionDefinitions(), conditionStore_.revision, true);
      }
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", shieldAppearanceTooltip.data());
      ImGui::PopTextWrapPos();

      ImGui::Spacing();
      if (ImGui::BeginTable("##options-save-data-layout", 2,
                            ImGuiTableFlags_SizingStretchProp,
                            ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn(descriptionLabel.data(),
                                ImGuiTableColumnFlags_WidthStretch, 1.15f);
        ImGui::TableSetupColumn(controlsLabel.data(),
                                ImGuiTableColumnFlags_WidthStretch, 0.85f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(saveDataLabel.data());

        ImGui::TableSetColumnIndex(1);
        if (saveDataPath_[0] == '\0') {
          ResetSaveDataPath();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##save-data-path", saveDataPathHint.data(),
                                 saveDataPath_.data(), saveDataPath_.size());
        if (ImGui::Button(exportJsonLabel.data())) {
          std::string error;
          if (ExportSaveDataJson(saveDataPath_.data(), error)) {
            const std::string pathText(saveDataPath_.data());
            saveDataStatus_ = sfs::strings::SafeVFormat(
                std::string(localization->Get("options.export_json.success")),
                std::make_format_args(pathText));
            saveDataStatusIsError_ = false;
          } else {
            saveDataStatus_ = std::move(error);
            saveDataStatusIsError_ = true;
          }
        }
        ImGui::SameLine();
        if (ImGui::Button(importJsonLabel.data())) {
          std::string error;
          if (ImportSaveDataJson(saveDataPath_.data(), error)) {
            const std::string pathText(saveDataPath_.data());
            saveDataStatus_ = sfs::strings::SafeVFormat(
                std::string(localization->Get("options.import_json.success")),
                std::make_format_args(pathText));
            saveDataStatusIsError_ = false;
          } else {
            saveDataStatus_ = std::move(error);
            saveDataStatusIsError_ = true;
            openSaveDataErrorPopup_ = true;
          }
        }
        if (!saveDataStatus_.empty()) {
          const auto color = ThemeConfig::GetSingleton()->GetColor(
              saveDataStatusIsError_ ? "ERROR" : "SUCCESS");
          ImGui::TextColored(color, "%s", saveDataStatus_.c_str());
        }

        ImGui::EndTable();
      }
    }
    ImGui::EndChild();
  }

  if (openSaveDataErrorPopup_) {
    ImGui::OpenPopup("##save-data-error");
    openSaveDataErrorPopup_ = false;
  }

  ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("##save-data-error", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    ImGui::TextUnformatted(importJsonErrorTitle.data());
    ImGui::Separator();
    ImGui::TextWrapped("%s", saveDataStatus_.c_str());
    ImGui::Spacing();
    if (ImGui::Button(localization->GetCStr("common.ok"),
                      ImVec2(-FLT_MIN, 0.0f))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (openToggleKeyPopup_) {
    ImGui::OpenPopup("##toggle-key-capture");
    openToggleKeyPopup_ = false;
  }

  ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("##toggle-key-capture", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    ImGui::TextUnformatted(toggleCaptureTitle.data());
    ImGui::Separator();
    ImGui::TextWrapped("%s", toggleCaptureBody.data());
    ImGui::TextWrapped("%s", toggleCaptureHint.data());
    if (!toggleKeyCaptureError_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ThemeConfig::GetSingleton()->GetColor("ERROR"), "%s",
                         toggleKeyCaptureError_.c_str());
    }
    ImGui::Spacing();
    if (ImGui::Button(cancelLabel.data(), ImVec2(-FLT_MIN, 0.0f))) {
      CloseToggleKeyCapture();
      ImGui::CloseCurrentPopup();
    }
    if (!awaitingToggleKeyCapture_) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
} // namespace sfs
