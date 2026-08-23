#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace sfs::kit_generator {
struct ArmorRecord {
  std::string pluginName;
  std::uint32_t runtimeFormID{0};
  std::uint32_t localFormID{0};
  std::string editorID;
  std::string name;
  std::uint32_t sourceSlotMask{0};
  std::uint32_t visualSlotMask{0};
  std::uint32_t layoutSlotMask{0};
  std::vector<std::uint32_t> armorAddonFormIDs;
  std::vector<std::string> armorModelPaths;
  std::size_t sourceOrder{0};
  bool enchanted{false};
  bool adultVariant{false};
  bool nsfw{false};

  [[nodiscard]] std::string_view DisplayName() const;
  [[nodiscard]] std::string Identifier() const;
};

enum class OriginalGroupingAssessment : std::uint8_t {
  Pending,
  Suitable,
  Unsuitable
};

struct PluginSource {
  std::string name;
  std::vector<ArmorRecord> armors;
  bool selected{false};
  std::shared_ptr<std::atomic<OriginalGroupingAssessment>> groupingAssessment{
      std::make_shared<std::atomic<OriginalGroupingAssessment>>(
          OriginalGroupingAssessment::Pending)};
};

struct KitCandidate {
  std::string profile;
  std::vector<ArmorRecord> items;
  std::int64_t score{0};
};

struct GeneratedKit {
  std::string name;
  std::vector<KitCandidate> candidates;
  std::vector<ArmorRecord> sourceItems;
  std::size_t selectedCandidate{0};
  std::size_t draftCandidate{0};
  std::optional<bool> safetyPrefixOverride;

  [[nodiscard]] bool IsSelectedCandidateNsfw() const;
  void FreezeSafetyPrefixFromSelectedCandidate();
  void ToggleSafetyPrefix();
};

enum class ScanState : std::uint8_t {
  Ready,
  Scanning,
  Cancelling,
  Complete,
  Failed
};

struct ProgressSnapshot {
  ScanState state{ScanState::Ready};
  float overall{0.0F};
  float currentPlugin{0.0F};
  bool parallel{false};
  std::size_t pluginIndex{0};
  std::size_t pluginCount{0};
  std::string currentPluginName;
  std::string detail;
  std::vector<std::string> logLines;
};

class Generator {
public:
  static Generator &Get();

  void SnapshotLoadedArmorForms();
  [[nodiscard]] std::vector<PluginSource> &PluginSources();
  [[nodiscard]] const std::vector<PluginSource> &PluginSources() const;
  [[nodiscard]] const std::vector<GeneratedKit> &GeneratedKits() const;
  [[nodiscard]] std::vector<GeneratedKit> &GeneratedKits();
  [[nodiscard]] ProgressSnapshot GetProgressSnapshot() const;
  [[nodiscard]] bool StartScan(bool a_includeSafetyPrefix);
  void CancelScan();
  void DiscardScanResults();
  [[nodiscard]] bool MergeGeneratedKits(
      const std::vector<std::size_t> &a_indices, std::string &a_error);
  [[nodiscard]] bool MergeKitCandidates(
      std::size_t a_kitIndex,
      const std::vector<std::size_t> &a_candidateIndices,
      std::string &a_error);
  [[nodiscard]] std::size_t DeleteGeneratedKits(
      const std::vector<std::size_t> &a_indices);
  [[nodiscard]] std::size_t CreateKitFiles(std::string &a_error);
  [[nodiscard]] bool IncludeSafetyPrefix() const;

private:
  Generator() = default;
  ~Generator();
  Generator(const Generator &) = delete;
  Generator &operator=(const Generator &) = delete;

  void RunScan(std::stop_token a_stopToken,
               std::vector<PluginSource> a_sources);
  void SetProgress(std::size_t a_completedPluginCount,
                   std::size_t a_pluginCount, float a_overallProgress,
                   float a_activeProgress, bool a_parallel,
                   std::string a_pluginName, std::string a_detail);
  void AppendLog(std::string a_line);

  std::vector<PluginSource> pluginSources_;
  std::vector<GeneratedKit> generatedKits_;
  std::jthread assessmentWorker_;
  std::jthread worker_;
  std::atomic<ScanState> state_{ScanState::Ready};
  std::atomic<float> overallProgress_{0.0F};
  std::atomic<float> pluginProgress_{0.0F};
  std::atomic_bool parallelScan_{false};
  mutable std::mutex progressMutex_;
  std::size_t progressPluginIndex_{0};
  std::size_t progressPluginCount_{0};
  std::string progressPluginName_;
  std::string progressDetail_;
  std::vector<std::string> logLines_;
  bool includeSafetyPrefix_{false};
};
} // namespace sfs::kit_generator
