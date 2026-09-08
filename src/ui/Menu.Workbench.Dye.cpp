#include "Menu.h"

#include "ArmorUtils.h"
#include "native/FittingDye.h"
#include "ui/catalog/Widgets.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sfs {
namespace {
struct WorkbenchDyePopupState {
  bool openRequested{false};
  bool refreshRequested{false};
  RE::FormID actorFormID{0};
  RE::FormID appearanceFormID{0};
  std::uint64_t slotMask{0};
  std::string appearanceName;
  std::string slotText;
  // First-person candidates remain out of the list and are linked on apply.
  std::vector<native::dye::RenderedShapeInfo> firstPersonShapes;
  std::vector<native::dye::RenderedShapeInfo> shapes;
  int selectedShapeIndex{-1};
  int tintBoundShapeIndex{-1};
  // White is the neutral multiply tint: it displays the original diffuse.
  ImVec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
  std::string status;
};

WorkbenchDyePopupState g_workbenchDyePopup;
bool g_workbenchDyePopupVisible{false};

std::string ActorLabel(RE::Actor *a_actor, const RE::FormID a_actorFormID,
                       const ui::Localization *a_localization) {
  if (a_actor == nullptr) {
    return std::format("{} [{}]",
                       a_localization->Get("dye.popup.actor_unloaded"),
                       armor::FormatFormID(a_actorFormID));
  }
  const auto *name = a_actor->GetDisplayFullName();
  const auto displayName = name != nullptr && *name != '\0'
                               ? std::string(name)
                               : std::string(a_localization->Get(
                                     "dye.popup.actor_unnamed"));
  return std::format("{} [{}]", displayName,
                     armor::FormatFormID(a_actor->GetFormID()));
}

std::string CompactSlotText(std::string a_slotText) {
  std::string compact;
  compact.reserve(a_slotText.size());
  for (const unsigned char character : a_slotText) {
    if (character == ' ') {
      continue;
    }
    compact.push_back(static_cast<char>(std::tolower(character)));
  }
  return compact;
}

const native::dye::RenderedShapeInfo *SelectedShape(
    const WorkbenchDyePopupState &a_state) {
  return a_state.selectedShapeIndex >= 0 &&
                 a_state.selectedShapeIndex <
                     static_cast<int>(a_state.shapes.size())
             ? &a_state.shapes[static_cast<std::size_t>(
                   a_state.selectedShapeIndex)]
             : nullptr;
}

std::vector<std::string>
BuildAppearanceScenePathTokens(const RE::FormID a_appearanceFormID) {
  std::vector<std::string> tokens;
  std::unordered_set<RE::FormID> visitedArmors;
  const auto addArmor = [&](const auto &a_self,
                            const RE::TESObjectARMO *a_armor) -> void {
    if (a_armor == nullptr || !visitedArmors.insert(a_armor->GetFormID()).second) {
      return;
    }
    tokens.push_back(armor::FormatFormID(a_armor->GetFormID()));
    for (const auto *addon : a_armor->armorAddons) {
      if (addon != nullptr) {
        tokens.push_back(armor::FormatFormID(addon->GetFormID()));
      }
    }
    // Template armor can carry the active ARMA list for lightweight records.
    a_self(a_self, a_armor->templateArmor);
  };
  addArmor(addArmor, RE::TESForm::LookupByID<RE::TESObjectARMO>(
                         a_appearanceFormID));
  return tokens;
}

std::string FilenameOnly(const std::string_view a_path) {
  const auto separator = a_path.find_last_of("\\/");
  return std::string(separator == std::string_view::npos
                         ? a_path
                         : a_path.substr(separator + 1));
}

std::vector<native::dye::RenderedShapeInfo>
FilterShapesForRegisteredAppearance(
    const std::vector<native::dye::RenderedShapeInfo> &a_shapes,
    const RE::FormID a_appearanceFormID) {
  const auto tokens = BuildAppearanceScenePathTokens(a_appearanceFormID);
  if (tokens.empty()) {
    return {};
  }

  std::vector<native::dye::RenderedShapeInfo> matches;
  for (const auto &shape : a_shapes) {
    if (!native::dye::IsDyeableAppearanceComponent(shape)) {
      continue;
    }
    const bool ownedBySelectedAppearance = std::ranges::any_of(
        tokens, [&](const std::string &token) {
          return shape.scenePath.find(token) != std::string::npos;
        });
    if (ownedBySelectedAppearance) {
      matches.push_back(shape);
    }
  }
  return matches;
}

std::vector<native::dye::RenderedShapeInfo>
FindFirstPersonCounterparts(
    const native::dye::RenderedShapeInfo &a_thirdPersonShape,
    const std::vector<native::dye::RenderedShapeInfo> &a_firstPersonShapes) {
  std::vector<native::dye::RenderedShapeInfo> counterparts;
  for (const auto &shape : a_firstPersonShapes) {
    if (shape.shapeName == a_thirdPersonShape.shapeName) {
      counterparts.push_back(shape);
    }
  }
  return counterparts;
}

struct CurrentAppearanceComponent {
  native::dye::RenderedShapeInfo thirdPerson;
  std::vector<native::dye::RenderedShapeInfo> firstPerson;
};

std::optional<CurrentAppearanceComponent> ResolveCurrentAppearanceComponent(
    RE::Actor *a_actor, const RE::FormID a_appearanceFormID,
    const native::dye::RenderedShapeInfo &a_identity) {
  if (a_actor == nullptr || !a_actor->Is3DLoaded()) {
    return std::nullopt;
  }
  const auto matchingShapes = FilterShapesForRegisteredAppearance(
      native::dye::ScanLoadedActorShapes(a_actor), a_appearanceFormID);
  std::vector<native::dye::RenderedShapeInfo> thirdPersonMatches;
  std::vector<native::dye::RenderedShapeInfo> firstPersonShapes;
  for (const auto &shape : matchingShapes) {
    if (shape.firstPerson) {
      firstPersonShapes.push_back(shape);
    } else if (shape.shapeName == a_identity.shapeName &&
               shape.diffuseTexture == a_identity.diffuseTexture &&
               shape.scenePath == a_identity.scenePath) {
      thirdPersonMatches.push_back(shape);
    }
  }
  if (thirdPersonMatches.size() != 1) {
    return std::nullopt;
  }
  CurrentAppearanceComponent component{
      .thirdPerson = thirdPersonMatches.front(),
  };
  component.firstPerson = FindFirstPersonCounterparts(
      component.thirdPerson, firstPersonShapes);
  return component;
}
} // namespace

void Menu::OpenWorkbenchDyePopup(
    const RE::FormID a_actorFormID,
    const workbench::EquipmentWidgetItem &a_item,
    const std::uint64_t a_slotMask) {
  // This transient editor never changes the owning appearance registration,
  // equipment state, condition, or external strip policy.
  g_workbenchDyePopup = {
      .openRequested = true,
      .refreshRequested = true,
      .actorFormID = a_actorFormID,
      .appearanceFormID = a_item.formID,
      .slotMask = a_slotMask,
      .appearanceName = a_item.name,
      .slotText = a_item.slotText,
  };
}

bool Menu::IsWorkbenchDyePopupVisible() const {
  return g_workbenchDyePopupVisible;
}

void Menu::DrawWorkbenchDyePopup() {
  auto &state = g_workbenchDyePopup;
  auto *localization = ui::Localization::GetSingleton();
  const auto popupTitle =
      std::format("{}###workbench-dye-popup", localization->Get("dye.popup.title"));
  if (state.openRequested) {
    ImGui::OpenPopup(popupTitle.c_str());
    state.openRequested = false;
  }

  bool popupOpen = true;
  // Start comfortably large without auto-resizing. The user may still drag a
  // corner; only the lists scroll when a chosen window size is too small.
  ImGui::SetNextWindowSize(ImVec2(1360.0f, 880.0f), ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f, 600.0f),
                                      ImVec2(FLT_MAX, FLT_MAX));
  // Match SFS Kit Generator dialogs: stay modal for input safety, but do not
  // wash out the workbench behind the Dye editor.
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg,
                        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  const bool popupVisible = ImGui::BeginPopupModal(
      popupTitle.c_str(), &popupOpen,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleColor();
  g_workbenchDyePopupVisible = popupVisible;
  if (!popupVisible) {
    native::dye::ClearWorldTintPreview();
    return;
  }

  auto *actor = RE::TESForm::LookupByID<RE::Actor>(state.actorFormID);
  if (state.refreshRequested) {
    state.refreshRequested = false;
    state.shapes.clear();
    state.firstPersonShapes.clear();
    state.selectedShapeIndex = -1;
    state.tintBoundShapeIndex = -1;
    if (actor == nullptr || !actor->Is3DLoaded()) {
      state.status = localization->Get("dye.popup.status.actor_not_loaded");
    } else {
      const auto allShapes = native::dye::ScanLoadedActorShapes(actor);
      const auto matchingShapes =
          FilterShapesForRegisteredAppearance(allShapes, state.appearanceFormID);
      for (const auto &shape : matchingShapes) {
        if (shape.firstPerson) {
          state.firstPersonShapes.push_back(shape);
        } else {
          state.shapes.push_back(shape);
        }
      }
      state.status = state.shapes.empty()
                         ? std::string(localization->Get("dye.popup.status.empty"))
                         : std::string(localization->Get("dye.popup.status.ready"));
    }
  }

  // The compact header keeps the registered-appearance identity at left and
  // the next-action guidance at right, before the editor's full-width divider.
  if (ImGui::BeginTable("##workbench-dye-header", 2,
                        ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("##dye-header-identity",
                            ImGuiTableColumnFlags_WidthFixed, 620.0f);
    ImGui::TableSetupColumn("##dye-header-guide",
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextColumn();
    ImGui::Text("%s: %s", localization->GetCStr("dye.popup.actor"),
                ActorLabel(actor, state.actorFormID, localization).c_str());
    ImGui::TextUnformatted(state.appearanceName.c_str());
    if (!state.slotText.empty()) {
      const auto compactSlotText = CompactSlotText(state.slotText);
      ImGui::TextDisabled("%s", compactSlotText.c_str());
    }
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", localization->GetCStr("dye.popup.guide"));
    if (ImGui::Button(localization->GetCStr("dye.popup.rescan"))) {
      state.refreshRequested = true;
    }
    ImGui::SameLine();
    const auto componentCountValue = state.shapes.size();
    const auto componentCount = sfs::strings::SafeVFormat(
        std::string(localization->Get("dye.popup.component_count")),
        std::make_format_args(componentCountValue));
    ImGui::TextDisabled("%s", componentCount.c_str());
    if (!state.status.empty()) {
      ImGui::TextDisabled("%s", state.status.c_str());
    }
    ImGui::EndTable();
  }
  ImGui::Separator();
  ImGui::Spacing();
  if (ImGui::BeginTable("##workbench-dye-editor", 2,
                        ImGuiTableFlags_SizingStretchProp)) {
    // Keep the editor controls compact on the left and reserve the prominent
    // right side for browsing actual appearance-component source assets.
    ImGui::TableSetupColumn("##dye-editor-controls",
                            ImGuiTableColumnFlags_WidthFixed, 620.0f);
    ImGui::TableSetupColumn("##dye-editor-components",
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextColumn();
    const float controlsStartY = ImGui::GetCursorPosY();
    const auto *selected = SelectedShape(state);
    const auto currentComponent =
        selected != nullptr
            ? ResolveCurrentAppearanceComponent(
                  actor, state.appearanceFormID, *selected)
            : std::nullopt;
    const auto *currentSelected = currentComponent.has_value()
                                      ? &currentComponent->thirdPerson
                                      : nullptr;
    if (selected != nullptr &&
        state.tintBoundShapeIndex != state.selectedShapeIndex) {
      state.tintBoundShapeIndex = state.selectedShapeIndex;
      if (const auto worldTint = currentSelected != nullptr
                                     ? native::dye::GetWorldTintColor(
                                           *currentSelected)
                                     : std::nullopt;
          worldTint.has_value()) {
        state.tint = ImVec4(worldTint->red, worldTint->green, worldTint->blue,
                            1.0f);
      } else if (const auto savedTint = native::dye::GetSavedWorldTintColor(
                     state.actorFormID, state.appearanceFormID, *selected)) {
        state.tint = ImVec4(savedTint->red, savedTint->green, savedTint->blue,
                            1.0f);
      } else {
        state.tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      }
    }
    ImGui::Spacing();
    if (ImGui::BeginTable("##workbench-dye-color-preview", 2,
                          ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("##dye-color-picker",
                              ImGuiTableColumnFlags_WidthFixed, 340.0f);
      ImGui::TableSetupColumn("##dye-color-thumbnails",
                              ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextColumn();
      ImGui::PushItemWidth(320.0f);
      ImGui::ColorPicker3("##workbench-dye-color",
                          &state.tint.x, ImGuiColorEditFlags_NoAlpha |
                                             ImGuiColorEditFlags_NoSidePreview);
      ImGui::PopItemWidth();
      ImGui::TableNextColumn();
      constexpr ImVec2 previewSize(130.0f, 130.0f);
      const bool hasPreviewTexture =
          currentSelected != nullptr &&
          currentSelected->diffuseShaderResourceAddress != 0;
      const auto texture = hasPreviewTexture
                               ? ImTextureRef(static_cast<ImTextureID>(
                                     currentSelected
                                         ->diffuseShaderResourceAddress))
                               : ImTextureRef{};
      const auto drawPreview = [&](const char *a_label, const ImU32 a_tint) {
        ImGui::TextDisabled("%s", a_label);
        const auto origin = ImGui::GetCursorScreenPos();
        auto *drawList = ImGui::GetWindowDrawList();
        if (hasPreviewTexture) {
          drawList->AddImage(texture, origin,
                             ImVec2(origin.x + previewSize.x,
                                    origin.y + previewSize.y),
                             ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), a_tint);
        } else {
          drawList->AddRectFilled(origin,
                                  ImVec2(origin.x + previewSize.x,
                                         origin.y + previewSize.y),
                                  ImGui::GetColorU32(ImGuiCol_FrameBg));
          drawList->AddRect(origin,
                            ImVec2(origin.x + previewSize.x,
                                   origin.y + previewSize.y),
                            ImGui::GetColorU32(ImGuiCol_Border));
        }
        ImGui::Dummy(previewSize);
      };
      drawPreview(localization->GetCStr("dye.popup.preview.original"),
                  IM_COL32_WHITE);
      drawPreview(localization->GetCStr("dye.popup.preview.tinted"),
                  ImGui::ColorConvertFloat4ToU32(state.tint));
      ImGui::EndTable();
    }
    ImGui::TextDisabled("%s", localization->GetCStr("dye.popup.multiply_note"));
    ImGui::BeginDisabled(currentSelected == nullptr);
    if (ImGui::Button(localization->GetCStr("dye.popup.apply"))) {
      std::vector<native::dye::RenderedShapeInfo> targets{
          currentComponent->thirdPerson};
      targets.insert(targets.end(), currentComponent->firstPerson.begin(),
                     currentComponent->firstPerson.end());
      std::string rendererStatus;
      if (native::dye::ConfigureWorldTints(
          device_, context_, state.actorFormID, state.appearanceFormID, targets,
          state.tint.x, state.tint.y, state.tint.z, rendererStatus)) {
        native::dye::SaveWorldTintForComponent(
            state.actorFormID, state.appearanceFormID,
            currentComponent->thirdPerson,
            {.red = state.tint.x, .green = state.tint.y, .blue = state.tint.z});
        state.status = localization->Get("dye.popup.status.applied");
      } else {
        logger::warn("Fitting Dye apply failed: {}", rendererStatus);
        state.status = localization->Get("dye.popup.status.apply_failed");
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(selected == nullptr);
    if (ImGui::Button(localization->GetCStr("dye.popup.restore_original"))) {
      native::dye::ClearWorldTints(state.actorFormID,
                                   state.appearanceFormID, *selected);
      native::dye::RemoveSavedWorldTintForComponent(
          state.actorFormID, state.appearanceFormID, *selected);
      state.tintBoundShapeIndex = -1;
      state.status = localization->Get("dye.popup.status.restored");
    }
    ImGui::EndDisabled();
    const float editorHeight =
        (std::max)(220.0f, ImGui::GetCursorPosY() - controlsStartY);
    ImGui::TableNextColumn();
    if (ImGui::BeginTable("##workbench-dye-components", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_Resizable,
                          ImVec2(0.0f, editorHeight))) {
      ImGui::TableSetupColumn(localization->GetCStr("dye.popup.column.component"),
                              ImGuiTableColumnFlags_WidthFixed, 185.0f);
      ImGui::TableSetupColumn(localization->GetCStr("dye.popup.column.path"),
                              ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();
      for (std::size_t index = 0; index < state.shapes.size(); ++index) {
        const auto &shape = state.shapes[index];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const bool selectedRow = state.selectedShapeIndex == static_cast<int>(index);
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::Selectable(shape.shapeName.c_str(), selectedRow,
                              ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_DontClosePopups)) {
          state.selectedShapeIndex = static_cast<int>(index);
          std::string previewStatus;
          const auto current = ResolveCurrentAppearanceComponent(
              actor, state.appearanceFormID, shape);
          if (current.has_value() &&
              native::dye::PreviewWorldTint(
                  device_, context_, state.actorFormID,
                  state.appearanceFormID, current->thirdPerson,
                  previewStatus)) {
            state.status = localization->Get("dye.popup.status.highlight");
          } else {
            logger::warn("Fitting Dye preview failed: {}", previewStatus);
            state.status = localization->Get("dye.popup.status.preview_failed");
          }
        }
        ImGui::PopID();
        ImGui::TableSetColumnIndex(1);
        const auto filename = FilenameOnly(shape.diffuseTexture);
        const auto compactFilename = ui::catalog::TruncateTextToWidth(
            filename, ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(compactFilename.c_str());
      }
      ImGui::EndTable();
    }
    ImGui::EndTable();
  }

  const auto closeLabel = localization->GetCStr("dye.popup.close");
  const auto closeWidth = ImGui::CalcTextSize(closeLabel).x +
                          ImGui::GetStyle().FramePadding.x * 2.0f;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       (ImGui::GetContentRegionAvail().x - closeWidth) * 0.5f);
  if (ImGui::Button(closeLabel)) {
    native::dye::ClearWorldTintPreview();
    g_workbenchDyePopupVisible = false;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}
} // namespace sfs
