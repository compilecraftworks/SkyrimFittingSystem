#include "Menu.h"

#include "ArmorUtils.h"
#include "PlayerInventory.h"
#include "kit_generator/UI.h"
#include "ui/Localization.h"
#include "ui/catalog/Widgets.h"
#include "workbench/AppearanceSlotProtection.h"

#include <format>
#include <optional>

namespace {
constexpr std::string_view kFavoritePrefix = "\xEE\x83\xB5 ";
constexpr char kIconPanelRightOpen[] =
    "\xee\x90\xb8"; // ICON_LC_PANEL_RIGHT_OPEN
constexpr char kIconPanelRightClose[] =
    "\xee\x90\xb6"; // ICON_LC_PANEL_RIGHT_CLOSE

std::uint64_t SelectPrimarySlotMask(const std::uint64_t a_slotMask) {
  for (const auto singleSlotMask : sfs::armor::GetAllArmorSlotMasks()) {
    if ((a_slotMask & singleSlotMask) != 0) {
      return singleSlotMask;
    }
  }

  return 0;
}
} // namespace

namespace sfs {
void Menu::ClearCatalogSelection() {
  auto &browser = CatalogBrowserState();
  browser.selectedKey.clear();
  browser.selectedGearKeys.clear();
  pendingKitListMoveDelta_ = 0;
  pendingKitListApply_ = false;
  pendingKitListPreview_ = false;
  pendingKitListBack_ = false;
  pendingKitListNextPane_ = false;
  lastKitListApplyAt_ = {};
  workbench_.ClearPreview();
}

std::string Menu::BuildFavoriteKey(const ui::catalog::BrowserTab a_tab,
                                   const std::string_view a_id) const {
  std::string prefix;
  switch (a_tab) {
  case ui::catalog::BrowserTab::Gear:
    prefix = "gear:";
    break;
  case ui::catalog::BrowserTab::Outfits:
    prefix = "outfit:";
    break;
  case ui::catalog::BrowserTab::Kits:
    prefix = "kit:";
    break;
  case ui::catalog::BrowserTab::Slots:
    prefix = "slot:";
    break;
  case ui::catalog::BrowserTab::Conditions:
    prefix = "condition:";
    break;
  case ui::catalog::BrowserTab::Options:
    prefix = "options:";
    break;
  case ui::catalog::BrowserTab::KitGenerator:
    prefix = "kit-generator:";
    break;
  }

  return prefix + std::string(a_id);
}

bool Menu::IsFavorite(const ui::catalog::BrowserTab a_tab,
                      const std::string_view a_id) const {
  return CatalogBrowserState().favoriteKeys.contains(
      BuildFavoriteKey(a_tab, a_id));
}

void Menu::SetFavorite(const ui::catalog::BrowserTab a_tab,
                       const std::string_view a_id, const bool a_favorite) {
  const auto key = BuildFavoriteKey(a_tab, a_id);
  if (a_favorite) {
    CatalogBrowserState().favoriteKeys.insert(key);
  } else {
    CatalogBrowserState().favoriteKeys.erase(key);
    if (CatalogBrowserState().favoritesOnly &&
        CatalogBrowserState().activeTab == a_tab &&
        CatalogBrowserState().selectedKey == a_id) {
      ClearCatalogSelection();
    }
  }
  ++catalogDerived_.favoritesRevision;
  SaveFavorites();
}

std::string Menu::BuildFavoriteLabel(const std::string_view a_name,
                                     const bool a_favorite) const {
  if (!a_favorite) {
    return std::string(a_name);
  }

  return std::string(kFavoritePrefix) + std::string(a_name);
}

std::optional<KitEntry::Layout> Menu::BuildSlotFallbackLayoutFromArmorForms(
    const std::vector<RE::FormID> &a_formIDs) const {
  KitEntry::Layout layout;
  const auto resolvedFormIDs =
      EquipmentCatalog::Get().ResolveArmorFormIDs(a_formIDs);
  layout.rows.reserve(resolvedFormIDs.size());

  for (const auto formID : resolvedFormIDs) {
    const auto *armorForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (armorForm == nullptr) {
      continue;
    }

    const auto identifier = armor::GetFormIdentifier(armorForm);
    if (identifier.empty()) {
      continue;
    }

    auto slotMask = armor::GetArmorWorkbenchSlotMask(armorForm);
    slotMask = SelectPrimarySlotMask(slotMask);
    if (slotMask == 0) {
      continue;
    }

    KitEntry::LayoutRow row;
    row.targetKind = KitEntry::LayoutTargetKind::Slot;
    row.targetSlotMask = slotMask;
    row.overrideIdentifiers.push_back(identifier);
    layout.rows.push_back(std::move(row));
  }

  if (layout.rows.empty()) {
    return std::nullopt;
  }

  return layout;
}

bool Menu::ApplyCatalogSelectionAsFittingOverrides(
    const std::vector<RE::FormID> &a_formIDs, const bool a_replaceExisting) {
  const auto layout = BuildSlotFallbackLayoutFromArmorForms(a_formIDs);
  if (!layout.has_value()) {
    return false;
  }

  const std::optional<std::string> baseConditionId;
  const auto targetRowIndices =
      BuildWorkbenchTargetRowIndices(baseConditionId);
  const auto ownerActorFormID = ResolveNewWorkbenchRowOwnerActorFormID();
  const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
  workbench_.ClearPreview();

  bool changed = false;
  if (!a_replaceExisting) {
    changed |= workbench_.RemoveOverridesOverlappingCatalogSelection(
        a_formIDs, &targetRowIndices);
  }
  changed |= workbench_.ApplyKitLayout(
      *layout, a_replaceExisting, baseConditionId, ownerActorFormID,
      &initialEquippedState, &targetRowIndices, true);
  if (changed) {
    SyncWorkbenchRowsForCurrentFilter();
    workbench_.RefreshNativeArmorOverridesForActor(
        ownerActorFormID, conditionStore_.revision);
  }
  return changed;
}

bool Menu::PreviewCatalogSelectionAsFittingOverrides(
    const std::string_view a_selectionKey,
    const std::vector<RE::FormID> &a_formIDs) {
  const auto layout = BuildSlotFallbackLayoutFromArmorForms(a_formIDs);
  if (!layout.has_value()) {
    workbench_.ClearPreview();
    return false;
  }

  const std::optional<std::string> baseConditionId;
  const auto targetRowIndices = BuildWorkbenchTargetRowIndices(baseConditionId);
  return workbench_.PreviewKitLayout(a_selectionKey, *layout,
                                     ResolveWorkbenchPreviewActor(),
                                     &targetRowIndices);
}

bool Menu::PreviewExternalGeneratedArmorForms(
    const std::string_view a_selectionKey,
    const std::vector<RE::FormID> &a_formIDs) {
  auto layout = BuildSlotFallbackLayoutFromArmorForms(a_formIDs);
  if (!layout.has_value()) {
    workbench_.ClearPreview();
    return false;
  }

  KitEntry::Layout groupedLayout;
  for (auto &row : layout->rows) {
    auto groupedRow = std::ranges::find(
        groupedLayout.rows, row.targetSlotMask,
        &KitEntry::LayoutRow::targetSlotMask);
    if (groupedRow == groupedLayout.rows.end()) {
      groupedLayout.rows.push_back(std::move(row));
      continue;
    }
    groupedRow->overrideIdentifiers.insert(
        groupedRow->overrideIdentifiers.end(), row.overrideIdentifiers.begin(),
        row.overrideIdentifiers.end());
  }

  const std::optional<std::string> baseConditionId;
  const auto targetRowIndices = BuildWorkbenchTargetRowIndices(baseConditionId);
  return workbench_.PreviewKitLayout(a_selectionKey, groupedLayout,
                                     ResolveWorkbenchPreviewActor(),
                                     &targetRowIndices, true);
}

void Menu::ClearExternalGeneratedKitPreview() { workbench_.ClearPreview(); }

void Menu::RefreshExternalGeneratedKits() {
  QueueCatalogRefresh(ui::catalog::RefreshMode::KitsOnly);
}

void Menu::QueueCatalogRefresh(const ui::catalog::RefreshMode a_mode) {
  auto &browser = CatalogBrowserState();
  browser.refreshQueued = true;
  browser.queuedRefreshMode = a_mode;
  if (a_mode == ui::catalog::RefreshMode::Full) {
    browser.initialized = false;
    browser.pendingSelectionAfterRefresh.clear();
    ClearCatalogSelection();
  }
  InvalidateCatalogDerivedState();
}

void Menu::UpdateCatalogRefresh() {
  auto &browser = CatalogBrowserState();
  auto &catalog = EquipmentCatalog::Get();
  if (browser.refreshQueued && !catalog.IsRefreshing()) {
    catalog.StartRefreshFromGame(browser.queuedRefreshMode ==
                                         ui::catalog::RefreshMode::KitsOnly
                                     ? EquipmentCatalog::RefreshMode::KitsOnly
                                     : EquipmentCatalog::RefreshMode::Full);
    browser.refreshQueued = false;
  }

  if (!catalog.IsRefreshing()) {
    return;
  }

  if (!catalog.ContinueRefreshFromGame(16.0)) {
    browser.initialized = true;
    if (!browser.pendingSelectionAfterRefresh.empty()) {
      browser.selectedKey = browser.pendingSelectionAfterRefresh;
      browser.pendingSelectionAfterRefresh.clear();
    }
  }
}

void Menu::DrawCatalogLoadingPane() const {
  const auto &catalog = EquipmentCatalog::Get();
  const auto avail = ImGui::GetContentRegionAvail();
  const auto barWidth = (std::min)(avail.x, 420.0f);
  const auto progress = std::clamp(catalog.GetRefreshProgress(), 0.0f, 1.0f);
  const auto status = catalog.GetRefreshStatus();

  ImGui::Dummy(ImVec2(0.0f, (std::max)(avail.y * 0.30f, 0.0f)));
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + barWidth);
  ImGui::TextUnformatted(status.data(), status.data() + status.size());
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  ImGui::ProgressBar(progress, ImVec2(barWidth, 0.0f));
  ImGui::Spacing();
  ImGui::TextDisabled("%.0f%%", progress * 100.0f);
}

std::vector<RE::FormID>
Menu::BuildOutfitFittingFormIDs(const OutfitEntry &a_entry) const {
  std::vector<RE::FormID> fittingFormIDs;
  std::uint64_t claimedSlotMask = 0;

  for (const auto formID : EquipmentCatalog::Get().ResolveArmorFormIDs(
           a_entry.GetArmorFormIDs())) {
    const auto *armorForm =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (armorForm == nullptr ||
        workbench::IsAppearanceRegistrationProtectedSlotMask(
            armor::GetArmorDisplaySlotMask(armorForm))) {
      continue;
    }

    const auto slotMask = armor::GetArmorDisplaySlotMask(armorForm);
    if (slotMask == 0 || (slotMask & claimedSlotMask) != 0) {
      continue;
    }

    fittingFormIDs.push_back(formID);
    claimedSlotMask |= slotMask;
  }

  return fittingFormIDs;
}

void Menu::AddGearEntryToWorkbench(const GearEntry &a_entry) {
  (void)ApplyCatalogSelectionAsFittingOverrides(
      std::vector<RE::FormID>{a_entry.formID}, false);
}

void Menu::AddOutfitEntryToWorkbench(const OutfitEntry &a_entry) {
  (void)ApplyCatalogSelectionAsFittingOverrides(
      BuildOutfitFittingFormIDs(a_entry), true);
}

void Menu::PreviewGearEntry(const GearEntry &a_entry) {
  (void)PreviewCatalogSelectionAsFittingOverrides(
      a_entry.id, std::vector<RE::FormID>{a_entry.formID});
}

void Menu::PreviewOutfitEntry(const OutfitEntry &a_entry) {
  (void)PreviewCatalogSelectionAsFittingOverrides(
      a_entry.id, BuildOutfitFittingFormIDs(a_entry));
}

void Menu::DrawCatalogHostControls(const bool) {}

void Menu::DrawCatalogPaneBody() {
  auto &browser = CatalogBrowserState();
  if (browser.activeTab == ui::catalog::BrowserTab::Slots) {
    browser.activeTab = ui::catalog::BrowserTab::Gear;
  }
  bool catalogRowClicked = false;
  if (browser.activeTab != ui::catalog::BrowserTab::Conditions &&
      browser.activeTab != ui::catalog::BrowserTab::Options &&
      browser.activeTab != ui::catalog::BrowserTab::KitGenerator &&
      EquipmentCatalog::Get().IsRefreshing()) {
    DrawCatalogLoadingPane();
  } else {
    if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
      catalogRowClicked = DrawGearTab();
    } else if (browser.activeTab == ui::catalog::BrowserTab::Outfits) {
      catalogRowClicked = DrawOutfitTab();
    } else if (browser.activeTab == ui::catalog::BrowserTab::Kits) {
      catalogRowClicked = DrawKitTab();
    } else if (browser.activeTab == ui::catalog::BrowserTab::Conditions) {
      catalogRowClicked = DrawConditionTab();
    } else if (browser.activeTab == ui::catalog::BrowserTab::Options) {
      DrawOptionsTab();
    } else if (browser.activeTab == ui::catalog::BrowserTab::KitGenerator) {
      kit_generator::UI::Get().Draw();
    } else {
      browser.activeTab = ui::catalog::BrowserTab::Gear;
      catalogRowClicked = DrawGearTab();
    }

    if (!browser.selectedKey.empty() &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !catalogRowClicked) {
      ClearCatalogSelection();
    }
  }
}

void Menu::DrawCatalogHostBody(const bool a_drawBodyChild) {
  auto &browser = CatalogBrowserState();
  if (browser.activeTab == ui::catalog::BrowserTab::Slots) {
    browser.activeTab = ui::catalog::BrowserTab::Gear;
  }

  {
    UpdateCatalogRefresh();
    const auto applySelectedPreview = [&]() {
      if (EquipmentCatalog::Get().IsRefreshing()) {
        return;
      }
      if (!browser.previewSelected || browser.selectedKey.empty()) {
        return;
      }

      if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
        if (browser.selectedGearKeys.size() > 1) {
          std::vector<RE::FormID> selectedFormIDs;
          selectedFormIDs.reserve(browser.selectedGearKeys.size());
          const auto &gearEntries = EquipmentCatalog::Get().GetGear();
          for (const auto &selectedKey : browser.selectedGearKeys) {
            const auto selectedIt =
                std::ranges::find(gearEntries, selectedKey, &GearEntry::id);
            if (selectedIt != gearEntries.end()) {
              selectedFormIDs.push_back(selectedIt->formID);
            }
          }
          workbench_.ClearPreview();
          (void)PreviewCatalogSelectionAsFittingOverrides(
              "gear:multi-selection", selectedFormIDs);
          return;
        }
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetGear(), browser.selectedKey,
            [](const GearEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetGear().end()) {
          PreviewGearEntry(*entry);
        }
      } else if (browser.activeTab == ui::catalog::BrowserTab::Outfits) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetOutfits(), browser.selectedKey,
            [](const OutfitEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetOutfits().end()) {
          PreviewOutfitEntry(*entry);
        }
      } else if (browser.activeTab == ui::catalog::BrowserTab::Kits) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetKits(), browser.selectedKey,
            [](const KitEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetKits().end()) {
          PreviewKitEntry(*entry);
        }
      } else if (browser.activeTab == ui::catalog::BrowserTab::Conditions ||
                 browser.activeTab == ui::catalog::BrowserTab::Options ||
                 browser.activeTab ==
                     ui::catalog::BrowserTab::KitGenerator) {
        workbench_.ClearPreview();
      }
    };

    const bool inPopout =
        catalogPane_.hostMode == ui::catalog::HostMode::Popout;
    const char *hostToggleIcon =
        inPopout ? kIconPanelRightClose : kIconPanelRightOpen;
    auto *localization = ui::Localization::GetSingleton();
    const auto gearTabLabel = localization->Get("tabs.gear");
    const auto outfitsTabLabel = localization->Get("tabs.outfits");
    const auto kitsTabLabel = localization->Get("tabs.kits");
    const auto conditionsTabLabel = localization->Get("tabs.conditions");
    const auto kitGeneratorTabLabel =
        localization->Get("kit_generator.tab.title");
    const auto optionsTabLabel = localization->Get("tabs.options");
    const auto favoritesOnlyLabel = localization->Get("catalog.favorites_only");
    const auto inventoryOnlyLabel = localization->Get("catalog.inventory_only");
    const auto hideUnnamedLabel = localization->Get("catalog.hide_unnamed");
    const auto previewSelectedLabel =
        localization->Get("catalog.preview_selected");
    std::optional<ui::catalog::BrowserTab> forcedActiveTab;
    if (pendingCatalogTabSelection_) {
      forcedActiveTab = browser.activeTab;
      pendingCatalogTabSelection_ = false;
    }
    const auto tabSelectionFlags = [&](const ui::catalog::BrowserTab a_tab) {
      return forcedActiveTab.has_value() && *forcedActiveTab == a_tab
                 ? ImGuiTabItemFlags_SetSelected
                 : ImGuiTabItemFlags_None;
    };
    const auto notifyGeneratorClosed = [&](const ui::catalog::BrowserTab a_tab) {
      if (browser.activeTab == ui::catalog::BrowserTab::KitGenerator &&
          a_tab != ui::catalog::BrowserTab::KitGenerator) {
        kit_generator::UI::Get().NotifyTabClosed();
      }
    };
    if (ImGui::BeginTabBar("##catalog-tabs")) {
      const bool gearTabOpen =
          ImGui::BeginTabItem(gearTabLabel.data(), nullptr,
                              tabSelectionFlags(ui::catalog::BrowserTab::Gear));
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:gear-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.gear.1").data(),
           localization->Get("help.gear.2").data(),
           localization->Get("help.gear.3").data()});
      if (gearTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Gear) {
          notifyGeneratorClosed(ui::catalog::BrowserTab::Gear);
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Gear;
        ImGui::EndTabItem();
      }

      const bool outfitsTabOpen = ImGui::BeginTabItem(
          outfitsTabLabel.data(), nullptr,
          tabSelectionFlags(ui::catalog::BrowserTab::Outfits));
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:outfits-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.outfits.1").data(),
           localization->Get("help.outfits.2").data()});
      if (outfitsTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Outfits) {
          notifyGeneratorClosed(ui::catalog::BrowserTab::Outfits);
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Outfits;
        ImGui::EndTabItem();
      }

      const bool conditionsTabOpen = ImGui::BeginTabItem(
          conditionsTabLabel.data(), nullptr,
          tabSelectionFlags(ui::catalog::BrowserTab::Conditions));
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:conditions-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.conditions.1").data(),
           localization->Get("help.conditions.2").data(),
           localization->Get("help.conditions.3").data()});
      if (conditionsTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Conditions) {
          notifyGeneratorClosed(ui::catalog::BrowserTab::Conditions);
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Conditions;
        ImGui::EndTabItem();
      }

      const bool kitsTabOpen =
          ImGui::BeginTabItem(kitsTabLabel.data(), nullptr,
                              tabSelectionFlags(ui::catalog::BrowserTab::Kits));
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:kits-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.kits.1").data(),
           localization->Get("help.kits.2").data(),
           localization->Get("help.kits.3").data()});
      if (kitsTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Kits) {
          notifyGeneratorClosed(ui::catalog::BrowserTab::Kits);
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Kits;
        ImGui::EndTabItem();
      }

      const bool kitGeneratorTabOpen = ImGui::BeginTabItem(
          kitGeneratorTabLabel.data(), nullptr,
          tabSelectionFlags(ui::catalog::BrowserTab::KitGenerator));
      if (kitGeneratorTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::KitGenerator) {
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::KitGenerator;
        ImGui::EndTabItem();
      }

      const bool optionsTabOpen = ImGui::BeginTabItem(
          optionsTabLabel.data(), nullptr,
          tabSelectionFlags(ui::catalog::BrowserTab::Options));
      if (optionsTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Options) {
          notifyGeneratorClosed(ui::catalog::BrowserTab::Options);
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Options;
        ImGui::EndTabItem();
      }

      const bool hostToggleHovered =
          ImGui::TabItemButton(hostToggleIcon, ImGuiTabItemFlags_Trailing);
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:host-toggle", ImGui::IsItemHovered(),
          {inPopout ? localization->Get("help.host_toggle.docked").data()
                    : localization->Get("help.host_toggle.popout").data()});
      if (hostToggleHovered) {
        if (inPopout) {
          catalogPane_.hostMode = ui::catalog::HostMode::Docked;
          catalogPane_.popoutOpen = false;
        } else {
          catalogPane_.hostMode = ui::catalog::HostMode::Popout;
          catalogPane_.popoutOpen = true;
        }
        SaveUserSettings();
      }

      ImGui::EndTabBar();
    }

    ImGui::Separator();
    DrawCatalogFilters();
    if (browser.activeTab != ui::catalog::BrowserTab::Conditions &&
        browser.activeTab != ui::catalog::BrowserTab::Options &&
        browser.activeTab != ui::catalog::BrowserTab::KitGenerator) {
      if (ImGui::Checkbox(favoritesOnlyLabel.data(), &browser.favoritesOnly)) {
        if (browser.favoritesOnly && !browser.selectedKey.empty() &&
            !IsFavorite(browser.activeTab, browser.selectedKey)) {
          ClearCatalogSelection();
        }
        SaveUserSettings();
      }
    }
    if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
      ImGui::SameLine();
      if (ImGui::Checkbox(inventoryOnlyLabel.data(), &browser.inventoryOnly) &&
          browser.inventoryOnly && !browser.selectedKey.empty()) {
        const auto &catalog = EquipmentCatalog::Get().GetGear();
        const auto selectedIt =
            std::ranges::find(catalog, browser.selectedKey, &GearEntry::id);
        const auto inventoryFormIDs =
            player_inventory::GetInventoryArmorFormIDs();
        if (selectedIt != catalog.end() &&
            !inventoryFormIDs.contains(selectedIt->formID)) {
          ClearCatalogSelection();
        }
        SaveUserSettings();
      }

      ImGui::SameLine();
      if (ImGui::Checkbox(hideUnnamedLabel.data(), &browser.hideUnnamedGear) &&
          browser.hideUnnamedGear && !browser.selectedKey.empty()) {
        const auto &catalog = EquipmentCatalog::Get().GetGear();
        const auto selectedIt =
            std::ranges::find(catalog, browser.selectedKey, &GearEntry::id);
        if (selectedIt != catalog.end() && !selectedIt->hasDisplayName) {
          ClearCatalogSelection();
        }
        SaveUserSettings();
      }
    }
    if (browser.activeTab != ui::catalog::BrowserTab::Conditions &&
        browser.activeTab != ui::catalog::BrowserTab::Options &&
        browser.activeTab != ui::catalog::BrowserTab::KitGenerator) {
      ImGui::SameLine();
      if (ImGui::Checkbox(previewSelectedLabel.data(),
                          &browser.previewSelected)) {
        if (!browser.previewSelected) {
          workbench_.ClearPreview();
        } else {
          applySelectedPreview();
        }
        SaveUserSettings();
      }
    }
    if (a_drawBodyChild &&
        ImGui::BeginChild("##catalog-pane", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_Borders)) {
      DrawCatalogPaneBody();
      ImGui::EndChild();
    } else if (a_drawBodyChild) {
      ImGui::EndChild();
    }
  }
}

void Menu::DrawCatalogWindow() {
  if (catalogPane_.hostMode != ui::catalog::HostMode::Popout) {
    return;
  }
  auto *localization = ui::Localization::GetSingleton();
  const auto catalogWindowTitle = localization->Get("window.catalog");

  auto &io = ImGui::GetIO();
  ImGui::SetNextWindowSize(
      ImVec2(io.DisplaySize.x * 0.42f, io.DisplaySize.y * 0.70f),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.32f, io.DisplaySize.y * 0.52f),
      ImGuiCond_FirstUseEver, ImVec2(0.50f, 0.50f));

  bool popoutOpen = catalogPane_.popoutOpen;
  ImGuiWindowFlags catalogWindowFlags = ImGuiWindowFlags_NoCollapse;
  if (!ImGui::Begin(catalogWindowTitle.data(), &popoutOpen,
                    catalogWindowFlags)) {
    ImGui::End();
    catalogPane_.popoutOpen = popoutOpen;
    if (!catalogPane_.popoutOpen) {
      catalogPane_.hostMode = ui::catalog::HostMode::Docked;
      SaveUserSettings();
    }
    return;
  }
  catalogPane_.popoutOpen = popoutOpen;

  DrawCatalogHostBody(true);
  ImGui::End();

  if (!catalogPane_.popoutOpen) {
    catalogPane_.hostMode = ui::catalog::HostMode::Docked;
    SaveUserSettings();
  }
}

void Menu::DrawWindow() {
  auto *localization = ui::Localization::GetSingleton();
  const auto windowTitle = localization->Get("window.main");
  const auto browserTitle = localization->Get("window.browser_title");
  const auto toggleKeyLabel = GetToggleKeyLabel();
  const auto toggleHint = sfs::strings::SafeVFormat(
      std::string(localization->Get("window.toggle_hint")),
      std::make_format_args(toggleKeyLabel),
      "| " + toggleKeyLabel + " toggles visibility");
  const auto catalogColumn = localization->Get("window.catalog_column");
  const auto variantsColumn = localization->Get("window.variants_column");
  auto &io = ImGui::GetIO();
  ImGui::SetNextWindowSize(
      ImVec2(io.DisplaySize.x * 0.50f, io.DisplaySize.y * 0.50f),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.50f, io.DisplaySize.y * 0.50f),
      ImGuiCond_FirstUseEver, ImVec2(0.50f, 0.50f));

  bool open = enabled_;
  ImGuiWindowFlags mainWindowFlags = ImGuiWindowFlags_NoCollapse;
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                      std::clamp(windowAlpha_ * uiOpacity_, 0.0f, 1.0f));
  if (!ImGui::Begin(windowTitle.data(), &open, mainWindowFlags)) {
    ImGui::End();
    DrawCatalogWindow();
    DrawCreateKitDialog();
    DrawDeleteKitDialog();
    DrawRenameKitDialog();
    DrawApplyWithConditionOverridesDialog();
    DrawConditionEditorDialog();
    DrawStripLinkDialog();
    ImGui::PopStyleVar();
    if (!open) {
      Close();
    }
    return;
  }

  ImGui::TextUnformatted(browserTitle.data());
  ImGui::SameLine();
  ImGui::TextDisabled("%s", toggleHint.c_str());
  ImGui::Separator();
  DrawCatalogWindow();

  if (catalogPane_.hostMode == ui::catalog::HostMode::Docked) {
    DrawCatalogHostBody(false);
    if (ImGui::BeginTable("##browser-layout", 2,
                          ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
      ImGui::TableSetupColumn(catalogColumn.data(),
                              ImGuiTableColumnFlags_WidthStretch, 1.20f);
      ImGui::TableSetupColumn(variantsColumn.data(),
                              ImGuiTableColumnFlags_WidthStretch, 0.95f);
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      if (ImGui::BeginChild("##catalog-pane", ImVec2(0.0f, 0.0f),
                            ImGuiChildFlags_Borders)) {
        DrawCatalogPaneBody();
      }
      ImGui::EndChild();

      ImGui::TableSetColumnIndex(1);
      if (ImGui::BeginChild("##variant-pane", ImVec2(0.0f, 0.0f),
                            ImGuiChildFlags_Borders)) {
        DrawVariantWorkbenchPane();
      }
      ImGui::EndChild();

      ImGui::EndTable();
    }
  } else {
    if (ImGui::BeginChild("##variant-pane", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_Borders)) {
      DrawVariantWorkbenchPane();
    }
    ImGui::EndChild();
  }

  workbench_.RefreshNativeArmorOverrides(ConditionDefinitions(),
                                         conditionStore_.revision);

  DrawCreateKitDialog();
  DrawDeleteKitDialog();
  DrawRenameKitDialog();
  DrawApplyWithConditionOverridesDialog();
  DrawConditionEditorDialog();
  DrawStripLinkDialog();

  ImGui::End();
  ImGui::PopStyleVar();

  if (!open) {
    Close();
  }
}
} // namespace sfs
