#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace sfs::kit_generator {
class UI {
public:
  static UI &Get();
  void Draw();
  void NotifyTabClosed();

private:
  enum class View : std::uint8_t {
    PluginSelection,
    ScanProgress,
    CandidateList,
    CandidateDetail,
    CandidateEditor
  };
  enum class EditorPane : std::uint8_t { CurrentPieces, AvailablePieces };

  void DrawPluginSelection();
  void DrawScanProgress();
  void DrawCandidateList();
  void DrawCandidateDetail();
  void DrawCandidateEditor();
  void PreviewCandidate(std::size_t a_kitIndex, std::size_t a_candidateIndex);
  void PreviewCandidatePiece(std::size_t a_kitIndex,
                             std::size_t a_candidateIndex,
                             std::uint32_t a_formID);
  void ClearCandidatePreview();
  void SetCreationStatus(std::string a_message, bool a_isError);
  void DrawRightAlignedButton(const char *a_label,
                              const std::function<void()> &a_action,
                              bool a_enabled = true);

  View view_{View::PluginSelection};
  bool includeSafetyPrefix_{false};
  bool previewSelected_{true};
  std::optional<std::size_t> detailKitIndex_;
  std::optional<std::size_t> editorKitIndex_;
  std::optional<std::size_t> editorCandidateIndex_;
  std::optional<std::size_t> focusedKitIndex_;
  std::optional<std::size_t> focusedPluginIndex_;
  std::optional<std::size_t> renamingKitIndex_;
  std::array<char, 512> renameBuffer_{};
#if defined(SFS_PERSONAL_KIT_COMPLETION)
  std::optional<std::size_t> prefixKitIndex_;
  std::array<char, 512> prefixNameBuffer_{};
  bool prefixIsNsfw_{false};
  std::size_t prefixStyleIndex_{0};
#endif
  std::array<char, 256> pluginSearchBuffer_{};
  std::array<char, 256> kitSearchBuffer_{};
  std::array<char, 256> candidateSearchBuffer_{};
  std::vector<bool> candidateGroupSelections_;
  std::vector<bool> editorPieceSelections_;
  EditorPane editorFocusedPane_{EditorPane::AvailablePieces};
  std::optional<std::size_t> editorFocusedCurrentPieceIndex_;
  std::optional<std::size_t> editorFocusedAvailablePieceIndex_;
  std::string creationStatus_;
  bool creationStatusIsError_{false};
  double creationStatusExpiresAt_{0.0};
  bool restorePreviewOnNextDraw_{false};
  std::optional<std::size_t> previewedKitIndex_;
  std::optional<std::size_t> previewedCandidateIndex_;
  std::optional<std::uint32_t> editorPreviewFormID_;
};
} // namespace sfs::kit_generator
