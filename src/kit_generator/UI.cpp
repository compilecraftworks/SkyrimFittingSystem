#include "UI.h"

#include "Generator.h"
#include "Localization.h"
#include "ui/Menu.h"
#if defined(SFS_PERSONAL_KIT_COMPLETION)
#include "../../private/personal_kit_completion/PersonalKitCompletion.h"
#endif

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <numeric>
#include <unordered_set>

namespace {
std::string NormalizeSearchText(const std::string_view a_text) {
  std::string result;
  result.reserve(a_text.size());
  bool previousSpace = true;
  for (const unsigned char ch : a_text) {
    if (ch >= 128 || std::isalnum(ch)) {
      result.push_back(ch < 128 ? static_cast<char>(std::tolower(ch))
                                : static_cast<char>(ch));
      previousSpace = false;
    } else if (!previousSpace) {
      result.push_back(' ');
      previousSpace = true;
    }
  }
  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  return result;
}

bool MatchesSearch(const std::string_view a_text,
                   const std::string_view a_search) {
  const auto needle = NormalizeSearchText(a_search);
  return needle.empty() ||
         NormalizeSearchText(a_text).find(needle) != std::string::npos;
}

#if defined(SFS_PERSONAL_KIT_COMPLETION)
constexpr std::array<std::string_view, 9> kPersonalPrefixStyles{
    "아머", "판타지", "드레스", "일상", "코스튬", "수영복", "란제리", "동양", "사이버"};

std::string TrimPrefixName(std::string a_name) {
  const auto first = a_name.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = a_name.find_last_not_of(" \t\r\n");
  return a_name.substr(first, last - first + 1);
}

void ParsePersonalPrefix(const std::string_view a_name, bool &a_isNsfw,
                         std::size_t &a_styleIndex,
                         std::array<char, 512> &a_nameBuffer) {
  auto remaining = TrimPrefixName(std::string(a_name));
  a_isNsfw = remaining.starts_with("[N]");
  if (a_isNsfw || remaining.starts_with("[S]")) {
    remaining.erase(0, 3);
    remaining = TrimPrefixName(std::move(remaining));
  }
  a_styleIndex = 0;
  for (std::size_t index = 0; index < kPersonalPrefixStyles.size(); ++index) {
    const auto tag = std::format("[{}]", kPersonalPrefixStyles[index]);
    if (!remaining.starts_with(tag)) {
      continue;
    }
    a_styleIndex = index;
    remaining.erase(0, tag.size());
    remaining = TrimPrefixName(std::move(remaining));
    break;
  }
  a_nameBuffer.fill('\0');
  const auto copySize = (std::min)(remaining.size(), a_nameBuffer.size() - 1);
  std::ranges::copy_n(remaining.begin(), copySize, a_nameBuffer.begin());
}
#endif

bool IsCurrentTableRowHovered() {
  const auto *table = ImGui::GetCurrentTable();
  if (table == nullptr || table->RowPosY2 <= table->RowPosY1) {
    return false;
  }
  return ImGui::IsMouseHoveringRect(
      {table->WorkRect.Min.x, table->RowPosY1},
      {table->WorkRect.Max.x, table->RowPosY2}, false);
}

// A scrolling table owns an inner ImGui window. SetScrollHereY() can target
// the surrounding catalog window depending on the active column/child stack,
// which leaves keyboard focus below the visible list. Address the table's
// actual scroll owner directly after TableNextRow() has established its row.
void ScrollCurrentTableRowIntoView(const float a_centerRatio = 0.45F) {
  const auto *table = ImGui::GetCurrentTable();
  if (table == nullptr || table->InnerWindow == nullptr ||
      table->RowPosY2 <= table->RowPosY1) {
    return;
  }
  const auto rowCenter =
      ImLerp(table->RowPosY1, table->RowPosY2, a_centerRatio);
  ImGui::SetScrollFromPosY(table->InnerWindow,
                            rowCenter - table->InnerWindow->Pos.y,
                            a_centerRatio);
}

std::string FormatArmorSlots(const std::uint32_t a_slotMask) {
  std::string result;
  for (int slot = 30; slot <= 61; ++slot) {
    const auto bit = 1U << (slot - 30);
    if ((a_slotMask & bit) == 0) {
      continue;
    }
    if (!result.empty()) {
      result.append(", ");
    }
    result.append(std::to_string(slot));
  }
  return result.empty() ? "-" : result;
}
} // namespace

namespace sfs::kit_generator {
UI &UI::Get() {
  static UI instance;
  return instance;
}

void UI::DrawRightAlignedButton(const char *a_label,
                                const std::function<void()> &a_action,
                                const bool a_enabled) {
  const auto &style = ImGui::GetStyle();
  const auto width = ImGui::CalcTextSize(a_label).x +
                     style.FramePadding.x * 2.0F;
  ImGui::SameLine();
  const auto right = ImGui::GetWindowContentRegionMax().x;
  ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), right - width));
  ImGui::BeginDisabled(!a_enabled);
  if (ImGui::Button(a_label) && a_enabled) {
    a_action();
  }
  ImGui::EndDisabled();
}

void UI::SetCreationStatus(std::string a_message, const bool a_isError) {
  creationStatus_ = std::move(a_message);
  creationStatusIsError_ = a_isError;
  creationStatusExpiresAt_ = creationStatus_.empty()
                                 ? 0.0
                                 : ImGui::GetTime() + (a_isError ? 8.0 : 4.0);
}

void UI::Draw() {
  Localization::Get().SyncWithHost();

  if (restorePreviewOnNextDraw_ && previewedKitIndex_.has_value() &&
      previewedCandidateIndex_.has_value()) {
    const auto kitIndex = *previewedKitIndex_;
    const auto candidateIndex = *previewedCandidateIndex_;
    restorePreviewOnNextDraw_ = false;
    if (view_ == View::CandidateEditor && editorPreviewFormID_.has_value()) {
      PreviewCandidatePiece(kitIndex, candidateIndex, *editorPreviewFormID_);
    } else {
      PreviewCandidate(kitIndex, candidateIndex);
    }
  }

  switch (view_) {
  case View::PluginSelection:
    DrawPluginSelection();
    break;
  case View::ScanProgress:
    DrawScanProgress();
    break;
  case View::CandidateList:
    DrawCandidateList();
    break;
  case View::CandidateDetail:
    DrawCandidateDetail();
    break;
  case View::CandidateEditor:
    DrawCandidateEditor();
    break;
  }
}

void UI::DrawPluginSelection() {
  int moveDelta = 0;
  bool applySelected = false;
  bool previewSelected = false;
  Menu::GetSingleton()->ConsumeKitListCommands(
      moveDelta, applySelected, previewSelected);
  static_cast<void>(Menu::GetSingleton()->ConsumeKitListBack());
  const auto openResultsRequested =
      Menu::GetSingleton()->ConsumeKitListNextPane();

  auto &generator = Generator::Get();
  auto &sources = generator.PluginSources();
  if (openResultsRequested && !generator.GeneratedKits().empty()) {
    // Result-list Left only changes screens and keeps the scan session. Its
    // opposite direction reopens that same retained session.
    view_ = View::CandidateList;
    return;
  }
  auto &localization = Localization::Get();
  const auto title = localization.Text("plugins.title");
  const auto selectAll = localization.Text("common.select_all");
  const auto clearAll = localization.Text("common.clear_all");
  const auto scan = localization.Text("plugins.scan");

  ImGui::TextUnformatted(title.c_str());
  if (ImGui::Button(selectAll.c_str())) {
    for (auto &source : sources) {
      source.selected = true;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(clearAll.c_str())) {
    for (auto &source : sources) {
      source.selected = false;
    }
  }
  const auto selectedCount = static_cast<std::size_t>(std::ranges::count_if(
      sources, [](const auto &source) { return source.selected; }));
  DrawRightAlignedButton(
      scan.c_str(),
      [&]() {
        if (generator.StartScan(includeSafetyPrefix_)) {
          SetCreationStatus({}, false);
          view_ = View::ScanProgress;
        }
      },
      selectedCount > 0);

  const auto pluginSearchHint = localization.Text("search.plugins_hint");
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##plugin-search", pluginSearchHint.c_str(),
                           pluginSearchBuffer_.data(),
                           pluginSearchBuffer_.size());
  ImGui::Separator();
  const auto armorCount = std::accumulate(
      sources.begin(), sources.end(), std::size_t{},
      [](const auto total, const auto &source) {
        return total + (source.selected ? source.armors.size() : 0);
      });
  const auto selectedSummary = localization.Format(
      "plugins.selected_summary", selectedCount, sources.size(), armorCount);
  ImGui::TextUnformatted(selectedSummary.c_str());

  if (sources.empty()) {
    const auto noPlugins = localization.Text("plugins.none");
    ImGui::TextDisabled("%s", noPlugins.c_str());
    return;
  }

  if (ImGui::BeginTable("##kit-generator-plugins", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable,
                        ImVec2(0.0F, 0.0F))) {
    const auto pluginColumn = localization.Text("plugins.column.plugin");
    const auto armorColumn = localization.Text("plugins.column.new_armo");
    ImGui::TableSetupColumn(pluginColumn.c_str(),
                            ImGuiTableColumnFlags_WidthStretch |
                                ImGuiTableColumnFlags_DefaultSort,
                            0.0F, 0);
    ImGui::TableSetupColumn(armorColumn.c_str(),
                            ImGuiTableColumnFlags_WidthFixed,
                            110.0F, 1);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    std::vector<std::size_t> visibleSourceIndices;
    visibleSourceIndices.reserve(sources.size());
    for (std::size_t index = 0; index < sources.size(); ++index) {
      if (MatchesSearch(sources[index].name, pluginSearchBuffer_.data())) {
        visibleSourceIndices.push_back(index);
      }
    }
    if (auto *sortSpecs = ImGui::TableGetSortSpecs();
        sortSpecs && sortSpecs->SpecsCount > 0) {
      const auto &sort = sortSpecs->Specs[0];
      std::ranges::stable_sort(
          visibleSourceIndices, [&](const auto leftIndex,
                                    const auto rightIndex) {
            const auto &left = sources[leftIndex];
            const auto &right = sources[rightIndex];
            int comparison = 0;
            if (sort.ColumnUserID == 1) {
              comparison = left.armors.size() < right.armors.size()
                               ? -1
                               : left.armors.size() > right.armors.size() ? 1
                                                                          : 0;
            }
            if (comparison == 0) {
              comparison = NormalizeSearchText(left.name).compare(
                  NormalizeSearchText(right.name));
            }
            return sort.SortDirection == ImGuiSortDirection_Descending
                       ? comparison > 0
                       : comparison < 0;
          });
      sortSpecs->SpecsDirty = false;
    }

    moveDelta = std::clamp(moveDelta, -1, 1);
    int focusedRowIndex = -1;
    for (std::size_t rowIndex = 0; rowIndex < visibleSourceIndices.size();
         ++rowIndex) {
      if (focusedPluginIndex_.has_value() &&
          *focusedPluginIndex_ == visibleSourceIndices[rowIndex]) {
        focusedRowIndex = static_cast<int>(rowIndex);
        break;
      }
    }
    if (!visibleSourceIndices.empty() && focusedRowIndex < 0) {
      focusedRowIndex = 0;
      focusedPluginIndex_ = visibleSourceIndices.front();
    }
    if (focusedRowIndex >= 0 && moveDelta != 0) {
      focusedRowIndex = std::clamp(
          focusedRowIndex + moveDelta, 0,
          static_cast<int>(visibleSourceIndices.size()) - 1);
      focusedPluginIndex_ =
          visibleSourceIndices[static_cast<std::size_t>(focusedRowIndex)];
    }
    if (applySelected && focusedPluginIndex_.has_value() &&
        *focusedPluginIndex_ < sources.size()) {
      auto &focused = sources[*focusedPluginIndex_];
      focused.selected = !focused.selected;
    }
    static_cast<void>(previewSelected);

    int requestedScrollRowIndex = moveDelta != 0 ? focusedRowIndex : -1;

    for (std::size_t rowIndex = 0; rowIndex < visibleSourceIndices.size();
         ++rowIndex) {
      const auto sourceIndex = visibleSourceIndices[rowIndex];
      auto &source = sources[sourceIndex];
      const auto unsuitable =
          source.groupingAssessment->load(std::memory_order_acquire) ==
          OriginalGroupingAssessment::Unsuitable;
      ImGui::PushID(source.name.c_str());
      ImGui::TableNextRow();
      if (focusedPluginIndex_.has_value() &&
          *focusedPluginIndex_ == sourceIndex) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(72, 115, 176, 86));
      }
      ImGui::TableSetColumnIndex(0);
      ImGui::Checkbox("##selected", &source.selected);
      ImGui::SameLine();
      if (unsuitable) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(1.0F, 0.25F, 0.25F, 1.0F));
      }
      ImGui::TextUnformatted(source.name.c_str());
      if (unsuitable) {
        ImGui::PopStyleColor();
      }
      ImGui::TableSetColumnIndex(1);
      if (unsuitable) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(1.0F, 0.25F, 0.25F, 1.0F));
      }
      ImGui::Text("%zu", source.armors.size());
      if (unsuitable) {
        ImGui::PopStyleColor();
      }
      if (requestedScrollRowIndex == static_cast<int>(rowIndex)) {
        ScrollCurrentTableRowIntoView();
      }
      ImGui::PopID();
    }
    if (visibleSourceIndices.empty()) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      const auto none = localization.Text("search.none");
      ImGui::TextDisabled("%s", none.c_str());
    }
    ImGui::EndTable();
  }
}

void UI::DrawScanProgress() {
  int ignoredMove = 0;
  bool ignoredApply = false;
  bool ignoredPreview = false;
  Menu::GetSingleton()->ConsumeKitListCommands(
      ignoredMove, ignoredApply, ignoredPreview);
  static_cast<void>(Menu::GetSingleton()->ConsumeKitListBack());

  auto snapshot = Generator::Get().GetProgressSnapshot();
  if (snapshot.state == ScanState::Complete) {
    view_ = View::CandidateList;
    DrawCandidateList();
    return;
  }
  if (snapshot.state == ScanState::Ready) {
    view_ = View::PluginSelection;
    DrawPluginSelection();
    return;
  }

  auto &localization = Localization::Get();
  const auto title = localization.Text("scan.title");
  const auto overall = localization.Format(
      "scan.overall", snapshot.overall * 100.0F, snapshot.pluginIndex,
      snapshot.pluginCount);
  const auto current = localization.Format(
      snapshot.parallel ? "scan.parallel" : "scan.current",
      snapshot.currentPlugin * 100.0F, snapshot.currentPluginName);
  ImGui::TextUnformatted(title.c_str());
  const auto cancelling = snapshot.state == ScanState::Cancelling;
  const auto cancelLabel = localization.Text(
      cancelling ? "scan.cancelling" : "scan.cancel");
  DrawRightAlignedButton(
      cancelLabel.c_str(), []() { Generator::Get().CancelScan(); },
      !cancelling && snapshot.state == ScanState::Scanning);
  ImGui::TextUnformatted(overall.c_str());
  ImGui::ProgressBar(snapshot.overall, ImVec2(-1.0F, 0.0F));
  ImGui::TextUnformatted(current.c_str());
  ImGui::ProgressBar(snapshot.currentPlugin, ImVec2(-1.0F, 0.0F),
                     snapshot.detail.c_str());
  ImGui::Separator();
  const auto consoleTitle = localization.Text("scan.console_output");
  ImGui::TextUnformatted(consoleTitle.c_str());

  if (ImGui::BeginChild("##kit-generator-log", ImVec2(0.0F, 0.0F),
                        ImGuiChildFlags_Borders,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    const bool wasAtBottom =
        ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0F;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(snapshot.logLines.size()));
    while (clipper.Step()) {
      for (int index = clipper.DisplayStart; index < clipper.DisplayEnd;
           ++index) {
        ImGui::TextUnformatted(snapshot.logLines[index].c_str());
      }
    }
    if (wasAtBottom) {
      ImGui::SetScrollHereY(1.0F);
    }
  }
  ImGui::EndChild();

  if (snapshot.state == ScanState::Failed) {
    const auto failedTitle = localization.Text("scan.failed_title");
    const auto failedMessage = localization.Text("scan.failed_message");
    const auto back = localization.Text("common.back");
    ImGui::OpenPopup(failedTitle.c_str());
    if (ImGui::BeginPopupModal(failedTitle.c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted(failedMessage.c_str());
      if (ImGui::Button(back.c_str())) {
        view_ = View::PluginSelection;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
}

void UI::DrawCandidateList() {
  auto &generator = Generator::Get();
  auto &kits = generator.GeneratedKits();
  auto &localization = Localization::Get();
  const auto title = localization.Text("candidates.title");
  const auto rename = localization.Text("candidates.rename");
  const auto merge = localization.Text("candidates.merge");
  const auto deleteItems = localization.Text("candidates.delete");
  const auto cancelCreation =
      localization.Text("candidates.cancel_creation");
  const auto create = localization.Text("candidates.create");
  if (candidateGroupSelections_.size() != kits.size()) {
    candidateGroupSelections_.assign(kits.size(), false);
  }
  const auto mergeSelectionCount = static_cast<std::size_t>(
      std::ranges::count(candidateGroupSelections_, true));
  ImGui::TextUnformatted(title.c_str());
  ImGui::BeginDisabled(mergeSelectionCount != 1);
  if (ImGui::Button(rename.c_str()) && mergeSelectionCount == 1) {
    const auto selected = std::ranges::find(candidateGroupSelections_, true);
    if (selected != candidateGroupSelections_.end()) {
      const auto index = static_cast<std::size_t>(
          std::distance(candidateGroupSelections_.begin(), selected));
      renamingKitIndex_ = index;
      renameBuffer_.fill('\0');
      const auto copySize =
          (std::min)(kits[index].name.size(), renameBuffer_.size() - 1);
      std::ranges::copy_n(kits[index].name.begin(), copySize,
                          renameBuffer_.begin());
      ImGui::OpenPopup("##rename-kit-popup");
    }
  }
  ImGui::EndDisabled();
#if defined(SFS_PERSONAL_KIT_COMPLETION)
  ImGui::SameLine();
  ImGui::BeginDisabled(mergeSelectionCount != 1);
  if (ImGui::Button("접두사") && mergeSelectionCount == 1) {
    const auto selected = std::ranges::find(candidateGroupSelections_, true);
    if (selected != candidateGroupSelections_.end()) {
      const auto index = static_cast<std::size_t>(
          std::distance(candidateGroupSelections_.begin(), selected));
      prefixKitIndex_ = index;
      const auto candidateIndex =
          (std::min)(kits[index].selectedCandidate,
                     kits[index].candidates.empty()
                         ? std::size_t{0}
                         : kits[index].candidates.size() - 1);
      const auto autoPrefixedName = kits[index].candidates.empty()
                                         ? kits[index].name
                                         : sfs::personal_kit_completion::
                                               BuildPersonalOutputName(
                                                   kits[index],
                                                   kits[index].candidates[
                                                       candidateIndex]);
      ParsePersonalPrefix(autoPrefixedName, prefixIsNsfw_, prefixStyleIndex_,
                          prefixNameBuffer_);
      ImGui::OpenPopup("##personal-prefix-kit-popup");
    }
  }
  ImGui::EndDisabled();
#endif
  ImGui::SameLine();
  ImGui::BeginDisabled(mergeSelectionCount < 2);
  if (ImGui::Button(merge.c_str()) && mergeSelectionCount >= 2) {
    std::vector<std::size_t> selectedIndices;
    for (std::size_t index = 0; index < candidateGroupSelections_.size();
         ++index) {
      if (candidateGroupSelections_[index]) {
        selectedIndices.push_back(index);
      }
    }
    std::string error;
    if (generator.MergeGeneratedKits(selectedIndices, error)) {
      focusedKitIndex_.reset();
      ClearCandidatePreview();
      candidateGroupSelections_.assign(kits.size(), false);
      SetCreationStatus(localization.Text("candidates.merge_success"), false);
    } else {
      SetCreationStatus(std::move(error), true);
    }
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(mergeSelectionCount == 0);
  if (ImGui::Button(deleteItems.c_str()) && mergeSelectionCount > 0) {
    std::vector<std::size_t> selectedIndices;
    for (std::size_t index = 0; index < candidateGroupSelections_.size();
         ++index) {
      if (candidateGroupSelections_[index]) {
        selectedIndices.push_back(index);
      }
    }
    const auto deleted = generator.DeleteGeneratedKits(selectedIndices);
    focusedKitIndex_.reset();
    ClearCandidatePreview();
    candidateGroupSelections_.assign(kits.size(), false);
    SetCreationStatus(
        localization.Format("candidates.delete_success", deleted), false);
  }
  ImGui::EndDisabled();
  const auto &style = ImGui::GetStyle();
  const auto cancelWidth = ImGui::CalcTextSize(cancelCreation.c_str()).x +
                           style.FramePadding.x * 2.0F;
  const auto createWidth = ImGui::CalcTextSize(create.c_str()).x +
                           style.FramePadding.x * 2.0F;
  ImGui::SameLine();
  const auto right = ImGui::GetWindowContentRegionMax().x;
  ImGui::SetCursorPosX((std::max)(
      ImGui::GetCursorPosX(),
      right - cancelWidth - style.ItemSpacing.x - createWidth));
  auto creationCancelled = false;
  ImGui::BeginDisabled(kits.empty());
  if (ImGui::Button(create.c_str())) {
    std::string error;
    const auto written = generator.CreateKitFiles(error);
    if (!error.empty()) {
      SetCreationStatus(
          localization.Format("create.partial_error", written, error), true);
    } else {
      SetCreationStatus(localization.Format("create.success", written), false);
      Menu::GetSingleton()->RefreshExternalGeneratedKits();
    }
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button(cancelCreation.c_str())) {
    generator.DiscardScanResults();
    candidateGroupSelections_.clear();
    renamingKitIndex_.reset();
#if defined(SFS_PERSONAL_KIT_COMPLETION)
    prefixKitIndex_.reset();
#endif
    detailKitIndex_.reset();
    focusedKitIndex_.reset();
    SetCreationStatus({}, false);
    ClearCandidatePreview();
    view_ = View::PluginSelection;
    creationCancelled = true;
  }
  if (creationCancelled) {
    return;
  }

  const auto kitSearchHint = localization.Text("search.kits_hint");
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##kit-search", kitSearchHint.c_str(),
                           kitSearchBuffer_.data(), kitSearchBuffer_.size());
  const auto previewSelectedLabel =
      localization.Text("preview.selected");
  if (ImGui::Checkbox(previewSelectedLabel.c_str(), &previewSelected_)) {
    if (!previewSelected_) {
      ClearCandidatePreview();
    } else if (focusedKitIndex_.has_value() &&
               *focusedKitIndex_ < kits.size()) {
      PreviewCandidate(*focusedKitIndex_,
                       kits[*focusedKitIndex_].selectedCandidate);
    }
  }
  const auto multiSelectGuide = localization.Text("candidates.multi_select_guide");
  ImGui::SameLine();
  const auto multiSelectGuideWidth =
      ImGui::CalcTextSize(multiSelectGuide.c_str()).x;
  ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(),
                                  ImGui::GetWindowContentRegionMax().x -
                                      multiSelectGuideWidth));
  ImGui::TextDisabled("%s", multiSelectGuide.c_str());

  if (renamingKitIndex_.has_value() && *renamingKitIndex_ < kits.size()) {
    const auto renameTitle = localization.Text("candidates.rename_title");
    const auto renameLabel = localization.Text("candidates.rename_label");
    const auto apply = localization.Text("candidates.rename_apply");
    const auto cancel = localization.Text("candidates.rename_cancel");
    // The rename dialog is intentionally modal without washing out the
    // workbench behind it. Input is still gated explicitly below because the
    // row hit-testing helper is not constrained by ImGui's popup hover rules.
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg,
                          ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    if (ImGui::BeginPopupModal("##rename-kit-popup", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted(renameTitle.c_str());
      ImGui::SetNextItemWidth(420.0F);
      const auto enterPressed = ImGui::InputText(
          renameLabel.c_str(), renameBuffer_.data(), renameBuffer_.size(),
          ImGuiInputTextFlags_EnterReturnsTrue);
      const auto enteredName = std::string(renameBuffer_.data());
      const auto hasVisibleCharacter = std::ranges::any_of(
          enteredName, [](const auto ch) {
            return std::isspace(static_cast<unsigned char>(ch)) == 0;
          });
      ImGui::BeginDisabled(!hasVisibleCharacter);
      if ((ImGui::Button(apply.c_str()) || enterPressed) &&
          hasVisibleCharacter) {
        kits[*renamingKitIndex_].name = enteredName;
        SetCreationStatus(localization.Text("candidates.rename_success"),
                          false);
        renamingKitIndex_.reset();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button(cancel.c_str())) {
        renamingKitIndex_.reset();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
  }
#if defined(SFS_PERSONAL_KIT_COMPLETION)
  if (prefixKitIndex_.has_value() && *prefixKitIndex_ < kits.size()) {
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg,
                          ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    if (ImGui::BeginPopupModal("##personal-prefix-kit-popup", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("접두사 설정");
      ImGui::TextDisabled("N/S와 분류를 선택한 뒤 이름을 수정할 수 있습니다.");
      if (ImGui::Button("N", ImVec2(46.0F, 0.0F))) {
        prefixIsNsfw_ = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("S", ImVec2(46.0F, 0.0F))) {
        prefixIsNsfw_ = false;
      }
      ImGui::SameLine();
      ImGui::TextDisabled("현재: %s", prefixIsNsfw_ ? "N" : "S");
      for (std::size_t index = 0; index < kPersonalPrefixStyles.size();
           ++index) {
        if (index != 0 && index % 4 != 0) {
          ImGui::SameLine();
        }
        const auto selected = prefixStyleIndex_ == index;
        if (selected) {
          ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(42, 120, 158, 255));
        }
        if (ImGui::Button(kPersonalPrefixStyles[index].data())) {
          prefixStyleIndex_ = index;
        }
        if (selected) {
          ImGui::PopStyleColor();
        }
      }
      ImGui::SetNextItemWidth(420.0F);
      const auto enterPressed = ImGui::InputText(
          "이름", prefixNameBuffer_.data(), prefixNameBuffer_.size(),
          ImGuiInputTextFlags_EnterReturnsTrue);
      const auto enteredName = TrimPrefixName(prefixNameBuffer_.data());
      ImGui::BeginDisabled(enteredName.empty());
      if ((ImGui::Button("적용") || enterPressed) && !enteredName.empty()) {
        kits[*prefixKitIndex_].name = std::format(
            "[{}][{}] {}", prefixIsNsfw_ ? "N" : "S",
            kPersonalPrefixStyles[prefixStyleIndex_], enteredName);
        SetCreationStatus("접두사 적용 완료", false);
        prefixKitIndex_.reset();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("취소")) {
        prefixKitIndex_.reset();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
  }
#endif
  const bool renamePopupOpen = ImGui::IsPopupOpen("##rename-kit-popup");
#if defined(SFS_PERSONAL_KIT_COMPLETION)
  const bool prefixPopupOpen =
      ImGui::IsPopupOpen("##personal-prefix-kit-popup");
#else
  const bool prefixPopupOpen = false;
#endif
  const bool modalPopupOpen = renamePopupOpen || prefixPopupOpen;
  ImGui::Separator();

  if (!creationStatus_.empty() && creationStatusExpiresAt_ > 0.0 &&
      ImGui::GetTime() >= creationStatusExpiresAt_) {
    SetCreationStatus({}, false);
  }
  if (!creationStatus_.empty()) {
    if (creationStatusIsError_) {
      ImGui::TextColored(ImVec4(1.0F, 0.35F, 0.35F, 1.0F), "%s",
                         creationStatus_.c_str());
    } else {
      ImGui::TextColored(ImVec4(0.35F, 0.85F, 0.55F, 1.0F), "%s",
                         creationStatus_.c_str());
    }
  }

  struct CandidateListRow {
    std::size_t kitIndex{0};
    std::string displayName;
    std::string pluginDisplay;
  };
  const auto hasAmbiguous = std::ranges::any_of(kits, [](const auto &kit) {
    return kit.candidates.size() > 1;
  });
  std::vector<CandidateListRow> candidateRows;
  candidateRows.reserve(kits.size());
  for (std::size_t kitIndex = 0; kitIndex < kits.size(); ++kitIndex) {
    auto &kit = kits[kitIndex];
    if (kit.candidates.size() <= 1) {
      continue;
    }
    std::vector<std::string> pluginNames;
    const auto appendPluginName = [&](const ArmorRecord &item) {
      if (!item.pluginName.empty() &&
          std::ranges::find(pluginNames, item.pluginName) ==
              pluginNames.end()) {
        pluginNames.push_back(item.pluginName);
      }
    };
    if (!kit.sourceItems.empty()) {
      for (const auto &item : kit.sourceItems) {
        appendPluginName(item);
      }
    } else {
      for (const auto &candidate : kit.candidates) {
        for (const auto &item : candidate.items) {
          appendPluginName(item);
        }
      }
    }
    std::string pluginDisplay;
    for (const auto &pluginName : pluginNames) {
      if (!pluginDisplay.empty()) {
        pluginDisplay.append(", ");
      }
      pluginDisplay.append(pluginName);
    }
    auto displayName = kit.name;
#if defined(SFS_PERSONAL_KIT_COMPLETION)
    if (!kit.candidates.empty()) {
      const auto selected =
          (std::min)(kit.selectedCandidate, kit.candidates.size() - 1);
      // The personal result list is a preview of the emitted JSON name, not
      // merely the unprefixed internal group key.
      displayName = sfs::personal_kit_completion::BuildPersonalOutputName(
          kit, kit.candidates[selected]);
    }
#else
    if (generator.IncludeSafetyPrefix() && !kit.candidates.empty()) {
      const auto nsfw = kit.IsSelectedCandidateNsfw();
      displayName =
          std::format("[{}] {}", nsfw ? "NSFW" : "SFW", kit.name);
    }
#endif
    const auto searchText = std::format("{} {}", displayName, pluginDisplay);
    if (!MatchesSearch(searchText, kitSearchBuffer_.data())) {
      continue;
    }
    candidateRows.push_back(
        {kitIndex, std::move(displayName), std::move(pluginDisplay)});
  }

  const auto nameColumn = localization.Text("candidates.column.kit_name");
  const auto espColumn = localization.Text("candidates.column.esp");
  const auto countColumn =
      localization.Text("candidates.column.candidate_count");
  auto countColumnContentWidth = ImGui::CalcTextSize(countColumn.c_str()).x;
  for (const auto &row : candidateRows) {
    const auto countLabel = std::format(
        "{}  >", localization.Format("candidates.count",
                                     kits[row.kitIndex].candidates.size()));
    countColumnContentWidth =
        (std::max)(countColumnContentWidth,
                   ImGui::CalcTextSize(countLabel.c_str()).x);
  }
  const auto countColumnWidth =
      countColumnContentWidth + ImGui::GetStyle().CellPadding.x * 2.0F +
      ImGui::GetFontSize() * 1.1F;

  int moveDelta = 0;
  bool applySelected = false;
  bool previewSelected = false;
  Menu::GetSingleton()->ConsumeKitListCommands(moveDelta, applySelected,
                                              previewSelected);
  // InputManager owns keyboard/gamepad list commands. Do not also consume
  // ImGui's physical key state here, or the same press advances twice.
  moveDelta = std::clamp(moveDelta, -1, 1);
  auto backRequested = Menu::GetSingleton()->ConsumeKitListBack();
  static_cast<void>(Menu::GetSingleton()->ConsumeKitListNextPane());
  if (modalPopupOpen) {
    // Do not let global list navigation, preview, or Back leak through the
    // modal while the text field owns keyboard input.
    moveDelta = 0;
    applySelected = false;
    previewSelected = false;
    backRequested = false;
  }
  const auto previewRequested = previewSelected || moveDelta != 0;

  bool focusChanged = false;
  bool openDetailRequested = false;

  if (ImGui::BeginTable("##kit-candidate-groups-v2", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable,
                        ImVec2(0.0F, 0.0F))) {
    ImGui::TableSetupColumn(nameColumn.c_str(),
                            ImGuiTableColumnFlags_WidthStretch |
                                ImGuiTableColumnFlags_DefaultSort,
                            0.48F, 0);
    ImGui::TableSetupColumn(espColumn.c_str(),
                            ImGuiTableColumnFlags_WidthStretch, 0.34F, 1);
    ImGui::TableSetupColumn(countColumn.c_str(),
                            ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_NoResize,
                            countColumnWidth, 2);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    ImGui::TableSetColumnIndex(0);
    ImGui::TableHeader(nameColumn.c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::TableHeader(espColumn.c_str());
    ImGui::TableSetColumnIndex(2);
    ImGui::TableHeader("##candidate-count-header");
    const auto countHeaderMin = ImGui::GetItemRectMin();
    const auto countHeaderMax = ImGui::GetItemRectMax();
    const auto countHeaderPadding = ImGui::GetStyle().CellPadding;
    ImGui::RenderTextClipped(
        ImVec2(countHeaderMin.x + countHeaderPadding.x,
               countHeaderMin.y + countHeaderPadding.y),
        ImVec2(countHeaderMax.x - countHeaderPadding.x,
               countHeaderMax.y - countHeaderPadding.y),
        countColumn.c_str(), nullptr, nullptr, ImVec2(1.0F, 0.5F));

    if (auto *sortSpecs = ImGui::TableGetSortSpecs();
        sortSpecs && sortSpecs->SpecsCount > 0) {
      const auto &sort = sortSpecs->Specs[0];
      std::ranges::stable_sort(
          candidateRows, [&](const auto &left, const auto &right) {
            int comparison = 0;
            if (sort.ColumnUserID == 1) {
              comparison = NormalizeSearchText(left.pluginDisplay)
                               .compare(NormalizeSearchText(
                                   right.pluginDisplay));
            } else if (sort.ColumnUserID == 2) {
              const auto leftCount = kits[left.kitIndex].candidates.size();
              const auto rightCount = kits[right.kitIndex].candidates.size();
              comparison = leftCount < rightCount
                               ? -1
                               : leftCount > rightCount ? 1 : 0;
            }
            if (comparison == 0) {
              comparison = NormalizeSearchText(left.displayName)
                               .compare(NormalizeSearchText(
                                   right.displayName));
            }
            return sort.SortDirection == ImGuiSortDirection_Descending
                       ? comparison > 0
                       : comparison < 0;
          });
      sortSpecs->SpecsDirty = false;
    }

    // Navigation must run after the same local row order used for drawing.
    // Previously this ran before ImGui's sortable table reordered
    // candidateRows, so an Up/Down press advanced the preview in one order
    // while the visible focus appeared to skip unrelated rows.
    int focusedRowIndex = -1;
    for (std::size_t rowIndex = 0; rowIndex < candidateRows.size();
         ++rowIndex) {
      if (focusedKitIndex_.has_value() &&
          *focusedKitIndex_ == candidateRows[rowIndex].kitIndex) {
        focusedRowIndex = static_cast<int>(rowIndex);
        break;
      }
    }
    if (focusedRowIndex < 0) {
      for (std::size_t rowIndex = 0; rowIndex < candidateRows.size();
           ++rowIndex) {
        if (candidateGroupSelections_[candidateRows[rowIndex].kitIndex]) {
          focusedRowIndex = static_cast<int>(rowIndex);
          break;
        }
      }
    }
    if (!candidateRows.empty() && focusedRowIndex < 0) {
      focusedRowIndex = 0;
    }
    if (focusedRowIndex >= 0) {
      focusedKitIndex_ =
          candidateRows[static_cast<std::size_t>(focusedRowIndex)].kitIndex;
      if (moveDelta != 0) {
        focusedRowIndex = std::clamp(
            focusedRowIndex + moveDelta, 0,
            static_cast<int>(candidateRows.size()) - 1);
        focusedKitIndex_ = candidateRows[static_cast<std::size_t>(
            focusedRowIndex)].kitIndex;
        // Keyboard/gamepad traversal establishes one current result.  This
        // prevents a prior mouse selection from looking like a second focus
        // row while leaving Ctrl+left-click multi-selection intact until the
        // user starts a new traversal.
        candidateGroupSelections_.assign(kits.size(), false);
        candidateGroupSelections_[*focusedKitIndex_] = true;
        focusChanged = true;
      } else if (applySelected) {
        openDetailRequested = true;
      }

      if (focusChanged || previewRequested) {
        const auto focusedIndex = *focusedKitIndex_;
        if (previewSelected_ && focusedIndex < kits.size()) {
          PreviewCandidate(focusedIndex,
                           kits[focusedIndex].selectedCandidate);
        } else if (!previewSelected_) {
          ClearCandidatePreview();
        }
      }
    } else {
      focusedKitIndex_.reset();
      ClearCandidatePreview();
    }

    for (const auto &row : candidateRows) {
      const auto kitIndex = row.kitIndex;
      auto &kit = kits[kitIndex];
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      // Keep the result list on the same scroll-safe row input path as the
      // main Gear/Outfits/Kits tables.  The old manual IsMouseHoveringRect()
      // check used table coordinates directly; after a long vertical scroll
      // it could resolve a click to the wrong generated-kit index.
      const auto rowContentPos = ImGui::GetCursorScreenPos();
      const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
      ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
      const bool focused = focusedKitIndex_.has_value() &&
                           *focusedKitIndex_ == kitIndex;
      const bool selectedForMerge =
          kitIndex < candidateGroupSelections_.size() &&
          candidateGroupSelections_[kitIndex];
      ImGui::PushID(static_cast<int>(kitIndex));
      ImGui::Selectable("##candidate-group-row", selectedForMerge,
                        ImGuiSelectableFlags_SpanAllColumns |
                            ImGuiSelectableFlags_AllowOverlap |
                            ImGuiSelectableFlags_AllowDoubleClick,
                        ImVec2(0.0F, rowHeight));
      const bool rowHovered = !modalPopupOpen && ImGui::IsItemHovered();
      ImGui::PopStyleColor(3);
      ImGui::SetCursorScreenPos(rowContentPos);
      if (focused || selectedForMerge) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               focused ? IM_COL32(72, 115, 176, 86)
                                       : IM_COL32(72, 115, 176, 42));
      }
      const auto openCandidateDetail = [&]() {
        focusedKitIndex_ = kitIndex;
        kit.draftCandidate = kit.selectedCandidate;
        detailKitIndex_ = kitIndex;
        view_ = View::CandidateDetail;
        PreviewCandidate(kitIndex, kit.draftCandidate);
      };
      const auto updateCandidateGroupSelection = [&](const bool a_multiple) {
        if (a_multiple) {
          candidateGroupSelections_[kitIndex] =
              !candidateGroupSelections_[kitIndex];
        } else {
          candidateGroupSelections_.assign(kits.size(), false);
          candidateGroupSelections_[kitIndex] = true;
        }
        if (candidateGroupSelections_[kitIndex]) {
          focusedKitIndex_ = kitIndex;
          PreviewCandidate(kitIndex, kit.selectedCandidate);
          return;
        }

        const auto nextSelection =
            std::ranges::find(candidateGroupSelections_, true);
        if (nextSelection == candidateGroupSelections_.end()) {
          focusedKitIndex_.reset();
          ClearCandidatePreview();
          return;
        }
        const auto nextKitIndex = static_cast<std::size_t>(std::distance(
            candidateGroupSelections_.begin(), nextSelection));
        focusedKitIndex_ = nextKitIndex;
        PreviewCandidate(nextKitIndex,
                         kits[nextKitIndex].selectedCandidate);
      };
      // One click anywhere on a result selects it and previews its current
      // candidate. Double-click, Enter, or the game's Activate action is the
      // deliberate transition into the candidate list. Ctrl+left-click is
      // the deliberate multi-select gesture for rename/merge/delete.
      ImGui::TextUnformatted(row.displayName.c_str());
      if (focused && focusChanged) {
        ScrollCurrentTableRowIntoView();
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(row.pluginDisplay.c_str());
      ImGui::TableSetColumnIndex(2);
      const auto candidateCount = localization.Format(
          "candidates.count", kit.candidates.size());
      const auto openLabel = std::format("{}  >", candidateCount);
      const auto textSize = ImGui::CalcTextSize(openLabel.c_str());
      const auto cursorX = ImGui::GetCursorPosX();
      const auto availableWidth = ImGui::GetContentRegionAvail().x;
      ImGui::SetCursorPosX(
          cursorX + (std::max)(0.0F, availableWidth - textSize.x));
      ImGui::TextUnformatted(openLabel.c_str());
      const auto doubleClicked =
          rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
      const auto releasedOnRow =
          rowHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
      if (doubleClicked) {
        openCandidateDetail();
      } else if (releasedOnRow) {
        updateCandidateGroupSelection(ImGui::GetIO().KeyCtrl);
      }
      if (rowHovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      }
      ImGui::PopID();
    }
    if (candidateRows.empty()) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      const auto none = localization.Text(
          hasAmbiguous ? "search.none" : "candidates.none_ambiguous");
      ImGui::TextDisabled("%s", none.c_str());
    }
    ImGui::EndTable();
  }

  if (openDetailRequested && focusedKitIndex_.has_value() &&
      *focusedKitIndex_ < kits.size()) {
    const auto focusedKitIdx = *focusedKitIndex_;
    auto &kit = kits[focusedKitIdx];
    kit.draftCandidate = kit.selectedCandidate;
    detailKitIndex_ = focusedKitIdx;
    view_ = View::CandidateDetail;
    PreviewCandidate(focusedKitIdx, kit.draftCandidate);
    return;
  }
  if (backRequested) {
    ClearCandidatePreview();
    view_ = View::PluginSelection;
  }
}

void UI::PreviewCandidate(const std::size_t a_kitIndex,
                          const std::size_t a_candidateIndex) {
  focusedKitIndex_ = a_kitIndex;
  editorPreviewFormID_.reset();
  if (!previewSelected_) {
    ClearCandidatePreview();
    return;
  }
  const auto &kits = Generator::Get().GeneratedKits();
  if (a_kitIndex >= kits.size() ||
      a_candidateIndex >= kits[a_kitIndex].candidates.size()) {
    ClearCandidatePreview();
    return;
  }
  std::vector<std::uint32_t> formIDs;
  for (const auto &item : kits[a_kitIndex].candidates[a_candidateIndex].items) {
    formIDs.push_back(item.runtimeFormID);
  }
  if (Menu::GetSingleton()->PreviewExternalGeneratedArmorForms(
          std::format("kit-generator:{}:{}", a_kitIndex, a_candidateIndex),
          formIDs)) {
    previewedKitIndex_ = a_kitIndex;
    previewedCandidateIndex_ = a_candidateIndex;
  } else {
    previewedKitIndex_.reset();
    previewedCandidateIndex_.reset();
    editorPreviewFormID_.reset();
  }
}

void UI::PreviewCandidatePiece(const std::size_t a_kitIndex,
                               const std::size_t a_candidateIndex,
                               const std::uint32_t a_formID) {
  focusedKitIndex_ = a_kitIndex;
  if (!previewSelected_ || a_formID == 0) {
    ClearCandidatePreview();
    return;
  }

  const auto &kits = Generator::Get().GeneratedKits();
  if (a_kitIndex >= kits.size() ||
      a_candidateIndex >= kits[a_kitIndex].candidates.size()) {
    ClearCandidatePreview();
    return;
  }

  const std::vector<RE::FormID> formIDs{a_formID};
  if (Menu::GetSingleton()->PreviewExternalGeneratedArmorForms(
          std::format("kit-generator:editor-piece:{}:{}:{:08X}", a_kitIndex,
                      a_candidateIndex, a_formID),
          formIDs)) {
    editorPreviewFormID_ = a_formID;
    // Closing the tab returns to the persisted candidate, not this temporary
    // one-piece editor preview.
    previewedKitIndex_ = a_kitIndex;
    previewedCandidateIndex_ = a_candidateIndex;
  } else {
    previewedKitIndex_.reset();
    previewedCandidateIndex_.reset();
    editorPreviewFormID_.reset();
  }
}

void UI::ClearCandidatePreview() {
  previewedKitIndex_.reset();
  previewedCandidateIndex_.reset();
  editorPreviewFormID_.reset();
  Menu::GetSingleton()->ClearExternalGeneratedKitPreview();
}

void UI::DrawCandidateDetail() {
  auto &kits = Generator::Get().GeneratedKits();
  if (!detailKitIndex_.has_value() || *detailKitIndex_ >= kits.size()) {
    detailKitIndex_.reset();
    view_ = View::CandidateList;
    return;
  }
  const auto kitIndex = *detailKitIndex_;
  auto &kit = kits[kitIndex];
  auto &localization = Localization::Get();
  const auto detailTitle =
      localization.Format("candidates.detail_title", kit.name);
  const auto back = localization.Text("common.back");
  int moveDelta = 0;
  bool applySelected = false;
  bool previewSelected = false;
  Menu::GetSingleton()->ConsumeKitListCommands(moveDelta, applySelected,
                                              previewSelected);
  const auto backRequested = Menu::GetSingleton()->ConsumeKitListBack();
  static_cast<void>(Menu::GetSingleton()->ConsumeKitListNextPane());
  moveDelta = std::clamp(moveDelta, -1, 1);
  const auto previewRequested = previewSelected || moveDelta != 0;

  // A candidate choice is not a temporary editor transaction.  Keep the
  // visible radio selection, preview, generated JSON, and (where enabled)
  // automatic safety prefix on the same candidate immediately.
  const auto selectCandidate = [&](const std::size_t a_candidateIndex) {
    if (a_candidateIndex >= kit.candidates.size()) {
      return;
    }
    kit.draftCandidate = a_candidateIndex;
    kit.selectedCandidate = a_candidateIndex;
    kit.safetyPrefixOverride.reset();
    PreviewCandidate(kitIndex, a_candidateIndex);
  };

  const auto openCandidateEditor = [&](const std::size_t a_candidateIndex) {
    if (a_candidateIndex >= kit.candidates.size()) {
      return;
    }
    selectCandidate(a_candidateIndex);
    editorKitIndex_ = kitIndex;
    editorCandidateIndex_ = a_candidateIndex;
    editorPieceSelections_.assign(kit.candidates[a_candidateIndex].items.size(),
                                  false);
    editorFocusedPane_ = EditorPane::CurrentPieces;
    if (!kit.candidates[a_candidateIndex].items.empty()) {
      editorFocusedCurrentPieceIndex_ = 0;
      PreviewCandidatePiece(
          kitIndex, a_candidateIndex,
          kit.candidates[a_candidateIndex].items.front().runtimeFormID);
    } else {
      editorFocusedCurrentPieceIndex_.reset();
      ClearCandidatePreview();
    }
    editorFocusedAvailablePieceIndex_.reset();
    view_ = View::CandidateEditor;
  };

  std::vector<std::size_t> visibleCandidateIndices;
  visibleCandidateIndices.reserve(kit.candidates.size());
  for (std::size_t candidateIndex = 0; candidateIndex < kit.candidates.size();
       ++candidateIndex) {
    const auto &candidate = kit.candidates[candidateIndex];
    const auto profile = candidate.profile == "base"
                             ? localization.Text("candidates.profile.base")
                             : candidate.profile;
    const auto label =
        localization.Format("candidates.candidate_label", candidateIndex + 1, profile);
    std::string pieces;
    for (const auto &item : candidate.items) {
      if (!pieces.empty()) {
        pieces.append(", ");
      }
      pieces.append(item.DisplayName());
    }
    if (MatchesSearch(std::format("{} {}", label, pieces),
                     candidateSearchBuffer_.data())) {
      visibleCandidateIndices.push_back(candidateIndex);
    }
  }

  const auto determineVisibleIndex = [&](const std::size_t a_candidateIndex) {
    const auto it = std::find(visibleCandidateIndices.begin(),
                              visibleCandidateIndices.end(),
                              a_candidateIndex);
    return static_cast<int>(
        it == visibleCandidateIndices.end() ? 0 : std::distance(
                                                 visibleCandidateIndices.begin(),
                                                 it));
  };

  bool focusChanged = false;
  int focusedVisibleIndex = visibleCandidateIndices.empty()
                               ? -1
                               : determineVisibleIndex(kit.draftCandidate);
  if (focusedVisibleIndex < 0 && !visibleCandidateIndices.empty()) {
    focusedVisibleIndex = 0;
    if (visibleCandidateIndices.size() > 0) {
      kit.draftCandidate = visibleCandidateIndices[static_cast<std::size_t>(
          focusedVisibleIndex)];
    }
  }
  if (!visibleCandidateIndices.empty() && focusedVisibleIndex >= 0) {
    if (moveDelta != 0) {
      focusedVisibleIndex =
          std::clamp(focusedVisibleIndex + moveDelta, 0,
                     static_cast<int>(visibleCandidateIndices.size()) - 1);
      selectCandidate(visibleCandidateIndices[static_cast<std::size_t>(
          focusedVisibleIndex)]);
      focusChanged = true;
    }

    if (focusChanged || previewRequested) {
      if (previewSelected_ && previewRequested) {
        PreviewCandidate(kitIndex, kit.draftCandidate);
      } else if (!previewSelected_) {
        ClearCandidatePreview();
      }
    }

    if (applySelected) {
      openCandidateEditor(kit.draftCandidate);
      return;
    }
  }

  if (backRequested) {
    editorPieceSelections_.clear();
    editorFocusedCurrentPieceIndex_.reset();
    editorFocusedAvailablePieceIndex_.reset();
    editorKitIndex_.reset();
    editorCandidateIndex_.reset();
    detailKitIndex_.reset();
    view_ = View::CandidateList;
    PreviewCandidate(kitIndex, kit.selectedCandidate);
    return;
  }

  if (ImGui::Button(back.c_str())) {
    editorPieceSelections_.clear();
    editorFocusedCurrentPieceIndex_.reset();
    editorFocusedAvailablePieceIndex_.reset();
    editorKitIndex_.reset();
    editorCandidateIndex_.reset();
    detailKitIndex_.reset();
    view_ = View::CandidateList;
    PreviewCandidate(kitIndex, kit.selectedCandidate);
    return;
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(detailTitle.c_str());
  ImGui::Separator();
  const auto candidateSearchHint =
      localization.Text("search.candidates_hint");
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##candidate-search",
                           candidateSearchHint.c_str(),
                           candidateSearchBuffer_.data(),
                           candidateSearchBuffer_.size());
  const auto previewSelectedLabel =
      localization.Text("preview.selected");
  if (ImGui::Checkbox(previewSelectedLabel.c_str(), &previewSelected_)) {
    if (previewSelected_) {
      PreviewCandidate(kitIndex, kit.draftCandidate);
    } else {
      ClearCandidatePreview();
    }
  }
  ImGui::Separator();

  if (!creationStatus_.empty() && creationStatusExpiresAt_ > 0.0 &&
      ImGui::GetTime() >= creationStatusExpiresAt_) {
    SetCreationStatus({}, false);
  }
  if (!creationStatus_.empty()) {
    const auto color = creationStatusIsError_
                           ? ImVec4(1.0F, 0.35F, 0.35F, 1.0F)
                           : ImVec4(0.35F, 0.85F, 0.55F, 1.0F);
    ImGui::TextColored(color, "%s", creationStatus_.c_str());
  }

  const auto candidateColumn =
      localization.Text("candidates.column.candidate");
  const auto editColumn = localization.Text("candidates.column.edit");
  const auto selectionColumn =
      localization.Text("candidates.column.selection");
  const auto &style = ImGui::GetStyle();
  const auto editButtonWidth = ImGui::CalcTextSize(editColumn.c_str()).x +
                               style.FramePadding.x * 2.0F;
  const auto editColumnWidth =
      editButtonWidth + style.CellPadding.x * 2.0F +
      ImGui::GetFontSize() * 0.45F;
  const auto selectionColumnWidth =
      (std::max)(ImGui::CalcTextSize(selectionColumn.c_str()).x +
                     style.CellPadding.x * 2.0F,
                 ImGui::GetFrameHeight() + style.CellPadding.x * 2.0F +
                     12.0F);

  if (ImGui::BeginTable("##kit-variation-candidates-v2", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingStretchProp,
                        ImVec2(0.0F, 0.0F))) {
    ImGui::TableSetupColumn(editColumn.c_str(),
                            ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_NoResize,
                            editColumnWidth);
    ImGui::TableSetupColumn(candidateColumn.c_str(),
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(selectionColumn.c_str(),
                            ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_NoResize,
                            selectionColumnWidth);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    bool foundSearchResult = false;
    for (std::size_t candidateIndex = 0;
         candidateIndex < kit.candidates.size(); ++candidateIndex) {
      const auto &candidate = kit.candidates[candidateIndex];
      const auto profile = candidate.profile == "base"
                               ? localization.Text("candidates.profile.base")
                               : candidate.profile;
      const auto label = localization.Format(
          "candidates.candidate_label", candidateIndex + 1, profile);
      std::string pieces;
      for (const auto &item : candidate.items) {
        if (!pieces.empty()) {
          pieces.append(", ");
        }
        pieces.append(item.DisplayName());
      }
      const auto searchText = std::format("{} {}", label, pieces);
      if (!MatchesSearch(searchText, candidateSearchBuffer_.data())) {
        continue;
      }
      foundSearchResult = true;
      ImGui::PushID(static_cast<int>(candidateIndex));
      const bool checked = kit.draftCandidate == candidateIndex;
      ImGui::TableNextRow();
      if (checked) {
        // The radio button is the single active candidate. Keep the whole
        // row focused with it for mouse, keyboard, and gamepad navigation.
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(72, 115, 176, 86));
      }
      ImGui::TableSetColumnIndex(0);
      const auto editCursorX = ImGui::GetCursorPosX();
      const auto editAvailableWidth = ImGui::GetContentRegionAvail().x;
      ImGui::SetCursorPosX(
          editCursorX +
          (std::max)(0.0F, (editAvailableWidth - editButtonWidth) * 0.5F));
      const auto editButtonLabel =
          std::format("{}##edit-candidate", editColumn);
      if (ImGui::SmallButton(editButtonLabel.c_str())) {
        openCandidateEditor(candidateIndex);
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(label.c_str());
      ImGui::TableSetColumnIndex(2);
      constexpr auto selectionRightMargin = 12.0F;
      const auto radioWidth = ImGui::GetFrameHeight();
      ImGui::SetCursorPosX((std::max)(
          ImGui::GetCursorPosX(), ImGui::GetCursorPosX() +
                                      ImGui::GetContentRegionAvail().x -
                                      radioWidth - selectionRightMargin));
      if (ImGui::RadioButton("##candidate-selected", checked)) {
        selectCandidate(candidateIndex);
      }
      if (checked && focusChanged) {
        ScrollCurrentTableRowIntoView();
      }
      const auto rowHovered = IsCurrentTableRowHovered();
      if (rowHovered) {
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          openCandidateEditor(candidateIndex);
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
          selectCandidate(candidateIndex);
        }
      }
      ImGui::PopID();
    }
    if (!foundSearchResult) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      const auto none = localization.Text("search.none");
      ImGui::TextDisabled("%s", none.c_str());
    }
    ImGui::EndTable();
  }
}

void UI::DrawCandidateEditor() {
  auto &kits = Generator::Get().GeneratedKits();
  if (!editorKitIndex_.has_value() || !editorCandidateIndex_.has_value() ||
      *editorKitIndex_ >= kits.size() ||
      *editorCandidateIndex_ >= kits[*editorKitIndex_].candidates.size()) {
    editorPieceSelections_.clear();
    editorFocusedCurrentPieceIndex_.reset();
    editorFocusedAvailablePieceIndex_.reset();
    editorKitIndex_.reset();
    editorCandidateIndex_.reset();
    view_ = detailKitIndex_.has_value() ? View::CandidateDetail
                                        : View::CandidateList;
    return;
  }

  const auto kitIndex = *editorKitIndex_;
  const auto candidateIndex = *editorCandidateIndex_;
  auto &kit = kits[kitIndex];
  auto &candidate = kit.candidates[candidateIndex];
  auto &localization = Localization::Get();
  if (editorPieceSelections_.size() != candidate.items.size()) {
    editorPieceSelections_.assign(candidate.items.size(), false);
  }

  const auto profile = candidate.profile == "base"
                           ? localization.Text("candidates.profile.base")
                           : candidate.profile;
  const auto candidateLabel = localization.Format(
      "candidates.candidate_label", candidateIndex + 1, profile);
  const auto title = localization.Format("candidates.editor.title", kit.name,
                                         candidateLabel);
  const auto back = localization.Text("common.back");
  int moveDelta = 0;
  bool applySelected = false;
  bool previewSelected = false;
  Menu::GetSingleton()->ConsumeKitListCommands(
      moveDelta, applySelected, previewSelected);
  const auto backRequested = Menu::GetSingleton()->ConsumeKitListBack();
  const auto nextPaneRequested = Menu::GetSingleton()->ConsumeKitListNextPane();
  // Left and Right first move between the editor's two columns. Only Left
  // while already focused on the left/current-pieces column goes back.
  const bool moveToCurrentPane =
      backRequested && editorFocusedPane_ == EditorPane::AvailablePieces;
  const bool leaveEditor =
      backRequested && editorFocusedPane_ == EditorPane::CurrentPieces;
  if (leaveEditor) {
    editorPieceSelections_.clear();
    editorFocusedCurrentPieceIndex_.reset();
    editorFocusedAvailablePieceIndex_.reset();
    editorKitIndex_.reset();
    editorCandidateIndex_.reset();
    view_ = View::CandidateDetail;
    PreviewCandidate(kitIndex, kit.draftCandidate);
    return;
  }
  if (ImGui::Button(back.c_str())) {
    editorPieceSelections_.clear();
    editorFocusedCurrentPieceIndex_.reset();
    editorFocusedAvailablePieceIndex_.reset();
    editorKitIndex_.reset();
    editorCandidateIndex_.reset();
    view_ = View::CandidateDetail;
    PreviewCandidate(kitIndex, kit.draftCandidate);
    return;
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(title.c_str());
  const auto instruction =
      localization.Text("candidates.editor.replace_instruction");
  ImGui::TextDisabled("%s", instruction.c_str());
  ImGui::Separator();

  std::vector<ArmorRecord> availablePieces;
  std::unordered_set<std::uint32_t> seenFormIDs;
  const auto appendAvailable = [&](const ArmorRecord &item) {
    if (seenFormIDs.insert(item.runtimeFormID).second) {
      availablePieces.push_back(item);
    }
  };
  if (!kit.sourceItems.empty()) {
    for (const auto &item : kit.sourceItems) {
      appendAvailable(item);
    }
  } else {
    for (const auto &sourceCandidate : kit.candidates) {
      for (const auto &item : sourceCandidate.items) {
        appendAvailable(item);
      }
    }
  }
  std::ranges::stable_sort(availablePieces, {}, &ArmorRecord::sourceOrder);

  moveDelta = std::clamp(moveDelta, -1, 1);
  const auto paneChanged = nextPaneRequested || moveToCurrentPane;
  if (nextPaneRequested) {
    editorFocusedPane_ = EditorPane::AvailablePieces;
  } else if (moveToCurrentPane) {
    editorFocusedPane_ = EditorPane::CurrentPieces;
  }
  if (!candidate.items.empty() &&
      (!editorFocusedCurrentPieceIndex_.has_value() ||
       *editorFocusedCurrentPieceIndex_ >= candidate.items.size())) {
    editorFocusedCurrentPieceIndex_ = 0;
  } else if (candidate.items.empty()) {
    editorFocusedCurrentPieceIndex_.reset();
  }
  if (!availablePieces.empty() &&
      (!editorFocusedAvailablePieceIndex_.has_value() ||
       *editorFocusedAvailablePieceIndex_ >= availablePieces.size())) {
    editorFocusedAvailablePieceIndex_ = 0;
  } else if (availablePieces.empty()) {
    editorFocusedAvailablePieceIndex_.reset();
  }

  if (moveDelta != 0) {
    if (editorFocusedPane_ == EditorPane::CurrentPieces &&
        editorFocusedCurrentPieceIndex_.has_value()) {
      const auto next = static_cast<std::ptrdiff_t>(*editorFocusedCurrentPieceIndex_) +
                        moveDelta;
      editorFocusedCurrentPieceIndex_ = static_cast<std::size_t>(
          std::clamp(next, std::ptrdiff_t{0},
                     static_cast<std::ptrdiff_t>(candidate.items.size()) - 1));
      editorPieceSelections_.assign(candidate.items.size(), false);
      editorPieceSelections_[*editorFocusedCurrentPieceIndex_] = true;
    } else if (editorFocusedPane_ == EditorPane::AvailablePieces &&
               editorFocusedAvailablePieceIndex_.has_value()) {
      const auto next = static_cast<std::ptrdiff_t>(*editorFocusedAvailablePieceIndex_) +
                        moveDelta;
      editorFocusedAvailablePieceIndex_ = static_cast<std::size_t>(
          std::clamp(next, std::ptrdiff_t{0},
                     static_cast<std::ptrdiff_t>(availablePieces.size()) - 1));
    }
  }

  const auto applyAvailablePiece = [&](const ArmorRecord &a_item) {
    const auto included = std::ranges::any_of(
        candidate.items, [&](const auto &current) {
          return current.runtimeFormID == a_item.runtimeFormID;
        });
    if (!included) {
      std::erase_if(candidate.items, [&](const auto &current) {
        return (current.sourceSlotMask & a_item.sourceSlotMask) != 0;
      });
      candidate.items.push_back(a_item);
      std::ranges::stable_sort(candidate.items, {}, &ArmorRecord::sourceOrder);
      candidate.score = 0;
      editorPieceSelections_.assign(candidate.items.size(), false);
      editorFocusedCurrentPieceIndex_ = 0;
    }
    PreviewCandidatePiece(kitIndex, candidateIndex, a_item.runtimeFormID);
  };

  const auto previewRequested = previewSelected || moveDelta != 0 || paneChanged;
  if (editorFocusedPane_ == EditorPane::AvailablePieces &&
      editorFocusedAvailablePieceIndex_.has_value()) {
    const auto focusedIndex = *editorFocusedAvailablePieceIndex_;
    if (applySelected) {
      applyAvailablePiece(availablePieces[focusedIndex]);
    } else if (previewRequested) {
      const auto &focusedItem = availablePieces[focusedIndex];
      PreviewCandidatePiece(kitIndex, candidateIndex,
                            focusedItem.runtimeFormID);
    }
  } else if (editorFocusedPane_ == EditorPane::CurrentPieces &&
             editorFocusedCurrentPieceIndex_.has_value()) {
    if (applySelected) {
      const auto focusedIndex = *editorFocusedCurrentPieceIndex_;
      editorPieceSelections_[focusedIndex] = !editorPieceSelections_[focusedIndex];
    }
    if (previewRequested) {
      const auto focusedIndex = *editorFocusedCurrentPieceIndex_;
      PreviewCandidatePiece(kitIndex, candidateIndex,
                            candidate.items[focusedIndex].runtimeFormID);
    }
  }

  if (ImGui::BeginTable("##candidate-piece-editor-columns", 2,
                        ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingStretchSame,
                        ImVec2(0.0F, 0.0F))) {
    ImGui::TableSetupColumn("##current-candidate-pieces",
                            ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("##all-kit-pieces",
                            ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    const auto currentTitle = localization.Format(
        "candidates.editor.current_pieces", candidate.items.size());
    ImGui::TextUnformatted(currentTitle.c_str());
    const auto selectedPieceCount = static_cast<std::size_t>(
        std::ranges::count(editorPieceSelections_, true));
    const auto deletePieces =
        localization.Text("candidates.editor.delete_pieces");
    ImGui::SameLine();
    const auto deleteDisabled =
        selectedPieceCount == 0 || selectedPieceCount >= candidate.items.size();
    ImGui::BeginDisabled(deleteDisabled);
    if (ImGui::Button(deletePieces.c_str()) && !deleteDisabled) {
      std::vector<ArmorRecord> kept;
      kept.reserve(candidate.items.size() - selectedPieceCount);
      for (std::size_t index = 0; index < candidate.items.size(); ++index) {
        if (!editorPieceSelections_[index]) {
          kept.push_back(candidate.items[index]);
        }
      }
      candidate.items = std::move(kept);
      candidate.score = 0;
      editorPieceSelections_.assign(candidate.items.size(), false);
      editorFocusedPane_ = EditorPane::CurrentPieces;
      if (!editorFocusedCurrentPieceIndex_.has_value() ||
          *editorFocusedCurrentPieceIndex_ >= candidate.items.size()) {
        editorFocusedCurrentPieceIndex_ = 0;
      }
      PreviewCandidatePiece(
          kitIndex, candidateIndex,
          candidate.items[*editorFocusedCurrentPieceIndex_].runtimeFormID);
    }
    ImGui::EndDisabled();
    if (deleteDisabled && selectedPieceCount == candidate.items.size() &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      const auto minimumOne =
          localization.Text("candidates.editor.minimum_one_piece");
      ImGui::SetTooltip("%s", minimumOne.c_str());
    }

    if (ImGui::BeginChild("##candidate-current-pieces-scroll",
                          ImVec2(0.0F, 0.0F),
                          ImGuiChildFlags_Borders)) {
      if (ImGui::BeginTable("##candidate-current-pieces-table", 2,
                            ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_BordersInnerH |
                                ImGuiTableFlags_SizingStretchProp)) {
        const auto pieceColumn =
            localization.Text("candidates.editor.column.piece");
        const auto slotColumn =
            localization.Text("candidates.editor.column.slots");
        ImGui::TableSetupColumn(pieceColumn.c_str(),
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(slotColumn.c_str(),
                                ImGuiTableColumnFlags_WidthFixed, 86.0F);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (std::size_t itemIndex = 0; itemIndex < candidate.items.size();
             ++itemIndex) {
          const auto &item = candidate.items[itemIndex];
          ImGui::PushID(static_cast<int>(itemIndex));
          ImGui::TableNextRow();
          const bool focused =
              editorFocusedPane_ == EditorPane::CurrentPieces &&
              editorFocusedCurrentPieceIndex_.has_value() &&
              *editorFocusedCurrentPieceIndex_ == itemIndex;
          const bool selectedForDeletion = editorPieceSelections_[itemIndex];
          if (focused || selectedForDeletion) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                   focused ? IM_COL32(72, 115, 176, 86)
                                           : IM_COL32(72, 115, 176, 42));
          }
          ImGui::TableSetColumnIndex(0);
          ImGui::TextUnformatted(item.DisplayName().data(),
                                 item.DisplayName().data() +
                                     item.DisplayName().size());
          ImGui::TableSetColumnIndex(1);
          const auto slots = FormatArmorSlots(item.sourceSlotMask);
          ImGui::TextUnformatted(slots.c_str());
          const auto rowHovered = IsCurrentTableRowHovered();
          if (rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (ImGui::GetIO().KeyCtrl) {
              editorPieceSelections_[itemIndex] =
                  !editorPieceSelections_[itemIndex];
            } else {
              editorPieceSelections_.assign(candidate.items.size(), false);
              editorPieceSelections_[itemIndex] = true;
            }
            editorFocusedPane_ = EditorPane::CurrentPieces;
            editorFocusedCurrentPieceIndex_ = itemIndex;
            PreviewCandidatePiece(kitIndex, candidateIndex,
                                  item.runtimeFormID);
          }
          if (focused && (moveDelta != 0 || paneChanged)) {
            ScrollCurrentTableRowIntoView();
          }
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
    }
    ImGui::EndChild();

    ImGui::TableSetColumnIndex(1);
    const auto allTitle = localization.Format(
        "candidates.editor.all_pieces", availablePieces.size());
    ImGui::TextUnformatted(allTitle.c_str());
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(0.0F, ImGui::GetFrameHeight()));
    if (ImGui::BeginChild("##candidate-all-pieces-scroll",
                          ImVec2(0.0F, 0.0F),
                          ImGuiChildFlags_Borders)) {
      if (ImGui::BeginTable("##candidate-all-pieces-table", 2,
                            ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_BordersInnerH |
                                ImGuiTableFlags_SizingStretchProp)) {
        const auto pieceColumn =
            localization.Text("candidates.editor.column.piece");
        const auto slotColumn =
            localization.Text("candidates.editor.column.slots");
        ImGui::TableSetupColumn(pieceColumn.c_str(),
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(slotColumn.c_str(),
                                ImGuiTableColumnFlags_WidthFixed, 86.0F);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (std::size_t itemIndex = 0; itemIndex < availablePieces.size();
             ++itemIndex) {
          const auto &item = availablePieces[itemIndex];
          ImGui::PushID(static_cast<int>(item.runtimeFormID));
          ImGui::TableNextRow();
          const bool focused =
              editorFocusedPane_ == EditorPane::AvailablePieces &&
              editorFocusedAvailablePieceIndex_.has_value() &&
              *editorFocusedAvailablePieceIndex_ == itemIndex;
          if (focused) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                   IM_COL32(72, 115, 176, 86));
          }
          ImGui::TableSetColumnIndex(0);
          ImGui::TextUnformatted(item.DisplayName().data(),
                                 item.DisplayName().data() +
                                     item.DisplayName().size());
          ImGui::TableSetColumnIndex(1);
          const auto slots = FormatArmorSlots(item.sourceSlotMask);
          ImGui::TextUnformatted(slots.c_str());
          const auto rowHovered = IsCurrentTableRowHovered();
          if (rowHovered) {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
              editorFocusedPane_ = EditorPane::AvailablePieces;
              editorFocusedAvailablePieceIndex_ = itemIndex;
              applyAvailablePiece(item);
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
              editorFocusedPane_ = EditorPane::AvailablePieces;
              editorFocusedAvailablePieceIndex_ = itemIndex;
              PreviewCandidatePiece(kitIndex, candidateIndex,
                                    item.runtimeFormID);
            }
          }
          if (focused && (moveDelta != 0 || paneChanged)) {
            ScrollCurrentTableRowIntoView();
          }
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
    }
    ImGui::EndChild();
    ImGui::EndTable();
  }
}

void UI::NotifyTabClosed() {
  restorePreviewOnNextDraw_ = previewedKitIndex_.has_value() &&
                              previewedCandidateIndex_.has_value();
  Menu::GetSingleton()->ClearExternalGeneratedKitPreview();
}
} // namespace sfs::kit_generator
