// Actual drop/setter/reset bodies and the entire Grid adapter. Engine catalog,
// layout projection and renderer boundaries are fakes, not in-game evidence.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "workbench/ActionDrop.h"

static unsigned failures{};
static void Check(bool ok, const char *message) {
  if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
namespace RE {
using FormID = std::uint32_t;
struct TESObjectARMO { FormID id; std::uint64_t mask; };
std::unordered_map<FormID, TESObjectARMO> forms;
struct TESForm {
  template<class T> static T *LookupByID(FormID id) {
    auto it = forms.find(id); return it == forms.end() ? nullptr : &it->second;
  }
};
struct PlayerCharacter {
  static inline bool available{true};
  static PlayerCharacter *GetSingleton() { static PlayerCharacter player; return available ? &player : nullptr; }
  FormID GetFormID() const { return 0x14; }
};
}
namespace logger { template<class... T> void info(T&&...) {} }
namespace sfs {
struct KitEntry { struct Layout { std::vector<RE::FormID> forms; }; };
namespace workbench {
enum class ConditionalVisibilityTargetKind { Actual, Fitting };
struct EquipmentWidgetItem {
  RE::FormID formID{}; bool hidden{}, locked{};
  bool operator==(const EquipmentWidgetItem &) const = default;
};
struct VariantWorkbenchRow {
  std::uint64_t uiIdentity{}, registrationOrder{};
  RE::FormID ownerActorFormID{};
  std::optional<std::string> conditionId;
  std::vector<EquipmentWidgetItem> overrides;
  bool IsSlotRow() const { return true; }
  bool operator==(const VariantWorkbenchRow &) const = default;
};
struct ConditionalVisibilityRule {
  std::string conditionId;
  RE::FormID ownerActorFormID{};
  ConditionalVisibilityTargetKind targetKind{};
  EquipmentWidgetItem target;
  bool visibleWhenTrue{};
  std::uint64_t uiIdentity{}, registrationOrder{};
  bool operator==(const ConditionalVisibilityRule &) const = default;
};
struct ConditionalActionDropRequest {
  condition_drop::Target target;
  std::optional<condition_drop::Target> source;
  RE::FormID formID{};
  ConditionalVisibilityTargetKind targetKind{};
  bool visibleWhenTrue{}, registerFittingTarget{};
};
namespace armor { std::uint64_t GetArmorDisplaySlotMask(const RE::TESObjectARMO *a) { return a->mask; } }
bool IsAppearanceRegistrationProtectedSlotMask(std::uint64_t mask) { return (mask & (1ull << 20)) != 0; }
bool failCatalog{};
bool BuildCatalogItem(RE::FormID id, EquipmentWidgetItem &item) {
  if (failCatalog || !RE::forms.contains(id)) { return false; }
  item.formID = id; return true;
}
struct VariantWorkbench {
  std::vector<VariantWorkbenchRow> rows_;
  std::vector<ConditionalVisibilityRule> conditionalVisibilityRules_;
  std::vector<int> rowOrder_, equippedHiddenByActor_;
  RE::FormID lastSyncedActorFormID_{};
  bool deferRuntimeEffects_{};
  struct Invalidation { RE::FormID actorFormID{}, appearanceFormID{}; std::uint64_t visualSlotMask{}; bool deleted{}; };
  std::vector<Invalidation> deferredInvalidations_;
  unsigned changes{}, invalidations{}, refreshes{}, previews{}, syncs{}, applies{};
  bool projectionUsable{true};
  int AcquireStateLock() { return 0; }
  void MarkChanged() { ++changes; }
  const auto &GetRows() const { return rows_; }
  void Revert() { std::abort(); }
  void InvalidateRemovedAppearanceAutomation(const std::vector<VariantWorkbenchRow> &) { ++invalidations; }
  bool ResetAllRows(const std::vector<int> *);
  void InvalidateAppearanceAutomation(RE::FormID, RE::FormID, std::uint64_t, bool) { ++invalidations; }
  // Never substitute success for an untested conversion branch.
  bool ReplaceConditionalFittingTarget(int, RE::FormID) { std::abort(); }
  bool ConvertConditionalVisibilityRuleToFittingRow(std::size_t, RE::FormID) { std::abort(); }
  bool DeleteRow(int) { std::abort(); }
  bool DeleteOverride(int row, int item) {
    if (row < 0 || row >= static_cast<int>(rows_.size())) { return false; }
    auto &items = rows_[row].overrides;
    if (item < 0 || item >= static_cast<int>(items.size())) { return false; }
    items.erase(items.begin() + item); return true;
  }
  bool SetConditionalVisibilityRuleTarget(std::size_t, ConditionalVisibilityTargetKind, RE::FormID);
  bool SetConditionalVisibilityRuleVisible(std::size_t, bool);
  condition_drop::Status ApplyConditionalActionDropTransaction(const ConditionalActionDropRequest &);
  void ClearPreview(bool) { ++previews; }
  void SyncRowsFromActor(RE::PlayerCharacter *) { ++syncs; }
  static int BuildInitialEquippedState(RE::PlayerCharacter *) { return 0; }
  void RefreshNativeArmorOverridesForActor(RE::FormID actor, unsigned) { Check(actor == 0x14, "Grid refresh is player-local"); ++refreshes; }
  bool ApplyKitLayout(const KitEntry::Layout &layout, bool replace,
                      std::optional<std::string> condition, RE::FormID actor,
                      const int *, const std::vector<int> *indices, bool fallback) {
    ++applies;
    Check(!condition && actor == 0x14 && fallback, "Grid keeps base/player slot fallback contract");
    // Model the existing projection's validate-before-reset boundary; its
    // engine-specific projection is not part of this adapter test.
    if (!projectionUsable) { return false; }
    if (replace) { ResetAllRows(indices); }
    for (auto i : *indices) {
      auto &row = rows_.at(i);
      Check(row.ownerActorFormID == actor && !row.conditionId, "Grid apply scope excludes condition/NPC/global rows");
      for (auto id : layout.forms) {
        if (std::ranges::find(row.overrides, id, &EquipmentWidgetItem::formID) == row.overrides.end()) {
          row.overrides.push_back({id});
        }
      }
    }
    return true;
  }
};
#include "WorkbenchTransactions.production.inc"
}
struct Menu {
  bool gameDataLoaded_{true};
  workbench::VariantWorkbench workbench_;
  struct { unsigned revision{42}; } conditionStore_;
  struct { bool revisionsInitialized{true}; } workbenchDerived_;
  int AcquireConditionStateLock() { return 0; }
  std::optional<KitEntry::Layout> BuildSlotFallbackLayoutFromArmorForms(const std::vector<RE::FormID> &ids) {
    KitEntry::Layout layout;
    for (auto id : ids) { if (RE::forms.contains(id)) { layout.forms.push_back(id); } }
    return layout.forms.empty() ? std::nullopt : std::optional{layout};
  }
  bool ApplyGridInventoryCostume(const std::uint32_t *, std::uint32_t);
  bool ClearGridInventoryCostume();
};
}
#include "GridCostume.production.inc"

static void TestDrops() {
  using namespace sfs::workbench;
  using K = condition_drop::TargetKind;
  using S = condition_drop::Status;
  using V = ConditionalVisibilityTargetKind;
  const auto make = [] {
    VariantWorkbench model;
    model.conditionalVisibilityRules_ = {
      {.conditionId="A", .ownerActorFormID=0x14, .targetKind=V::Actual, .target={10}, .visibleWhenTrue=true, .uiIdentity=1},
      {.conditionId="B", .ownerActorFormID=0x14, .targetKind=V::Actual, .target={20}, .visibleWhenTrue=false, .uiIdentity=2}
    };
    return model;
  };
  const ConditionalActionDropRequest request{
    .target={K::ConditionalVisibilityRule, 2}, .source={{K::ConditionalVisibilityRule, 1}},
    .formID=10, .targetKind=V::Actual, .visibleWhenTrue=true};
  for (bool polarity : {false, true}) {
    for (int invalid = 0; invalid != 3; ++invalid) {
      RE::forms = {{10,{10, 1ull << 2}}, {20,{20, 1ull << 2}}};
      failCatalog = invalid == 2;
      if (invalid == 0) { RE::forms[10].mask = 1ull << 20; }
      if (invalid == 1) { RE::forms.erase(10); }
      for (bool withSource : {false, true}) {
        auto model = make();
        model.conditionalVisibilityRules_[1].visibleWhenTrue = polarity;
        const auto before = model.conditionalVisibilityRules_;
        auto drop = request;
        if (!withSource) { drop.source.reset(); }
        Check(model.ApplyConditionalActionDropTransaction(drop) == S::InvalidRequest, "invalid target must be rejected, regardless of polarity/source");
        Check(model.conditionalVisibilityRules_ == before && model.changes == 0 && model.invalidations == 0,
              "rejected drop preserves both cards and publishes no changes");
      }
    }
  }
  failCatalog = false;
  RE::forms = {{10,{10, 1ull << 2}}, {20,{20, 1ull << 2}}};
  for (auto kind : {V::Actual, V::Fitting}) {
    for (bool polarity : {false, true}) {
      auto model = make();
      auto drop = request; drop.targetKind = kind;
      model.conditionalVisibilityRules_[1].visibleWhenTrue = polarity;
      Check(model.ApplyConditionalActionDropTransaction(drop) == S::Applied, "valid move remains available for actual and fitting visibility");
      Check(model.conditionalVisibilityRules_[0].target.formID == 0 && model.conditionalVisibilityRules_[1].target.formID == 10 &&
            model.conditionalVisibilityRules_[1].visibleWhenTrue && model.conditionalVisibilityRules_[1].targetKind == kind,
            "valid move updates complete target and consumes source exactly once");
      Check(model.ApplyConditionalActionDropTransaction(drop) == S::StaleSource, "replayed delivery cannot mutate again");
    }
  }
  {
    auto model = make(); auto drop = request; drop.source.reset();
    model.conditionalVisibilityRules_[1].target.formID = 10;
    model.conditionalVisibilityRules_[1].target.hidden = true;
    model.conditionalVisibilityRules_[1].target.locked = true;
    Check(model.ApplyConditionalActionDropTransaction(drop) == S::Applied, "same valid target still permits polarity-only change");
    Check(model.conditionalVisibilityRules_[1].target.hidden && model.conditionalVisibilityRules_[1].target.locked,
          "polarity-only edit preserves existing target metadata");
    Check(model.ApplyConditionalActionDropTransaction(drop) == S::NoChange, "same target and polarity remains no-op");
  }
  {
    auto model = make(); auto drop = request; drop.target = *drop.source;
    const auto before = model.conditionalVisibilityRules_;
    Check(model.ApplyConditionalActionDropTransaction(drop) == S::NoChange && model.conditionalVisibilityRules_ == before,
          "self drop remains no-op");
  }
  {
    auto model = make();
    model.conditionalVisibilityRules_.push_back({.conditionId="B", .ownerActorFormID=0x14, .targetKind=V::Actual, .target={10}, .uiIdentity=3});
    const auto before = model.conditionalVisibilityRules_;
    Check(model.ApplyConditionalActionDropTransaction(request) == S::Conflict && model.conditionalVisibilityRules_ == before && model.changes == 0,
          "late duplicate conflict rolls back target and source together");
  }
  {
    auto model = make(); model.rows_ = {{.uiIdentity=3, .conditionId="C", .overrides={{10},{20}}}};
    auto drop = request; drop.source = {{K::ConditionalRow, 3}};
    Check(model.ApplyConditionalActionDropTransaction(drop) == S::Applied && model.rows_[0].overrides == std::vector<EquipmentWidgetItem>{{20}},
          "row-to-rule move removes only the transferred source appearance");
  }
}

static void TestGrid() {
  using namespace sfs::workbench;
  RE::forms = {{10,{10,4}}, {20,{20,4}}, {60,{60,4}}, {70,{70,4}}};
  sfs::Menu menu;
  auto &model = menu.workbench_;
  model.rows_ = {
    {.uiIdentity=1, .ownerActorFormID=0x14, .overrides={{10},{50,false,true}}},
    {.uiIdentity=2, .ownerActorFormID=0x14, .conditionId="A", .overrides={{20,true,false}}},
    {.uiIdentity=3, .ownerActorFormID=0xA1, .conditionId="A", .overrides={{30}}},
    {.uiIdentity=4, .ownerActorFormID=0, .overrides={{40}}},
    {.uiIdentity=5, .ownerActorFormID=0x14, .conditionId="B", .overrides={{50,false,true}}}
  };
  const auto preserved = std::vector(model.rows_.begin() + 1, model.rows_.end());
  for (unsigned cycle = 0; cycle != 128; ++cycle) {
    const RE::FormID costume[] = {0, cycle % 2 ? 60u : 70u, cycle % 2 ? 60u : 70u, 0xDEAD};
    Check(menu.ApplyGridInventoryCostume(costume, 4), "valid Grid costume accepted");
    Check(model.rows_[0].overrides == std::vector<EquipmentWidgetItem>{{50,false,true},{costume[1]}},
          "Grid replaces only unlocked base appearance and deduplicates forms");
    Check(std::vector(model.rows_.begin() + 1, model.rows_.end()) == preserved,
          "Grid preserves conditional visibility, locks, NPC and global cards across switches");
  }
  const auto before = model.rows_;
  const auto refreshes = model.refreshes;
  const RE::FormID invalid[] = {0xDEAD};
  Check(menu.ApplyGridInventoryCostume(nullptr, 0) && menu.ApplyGridInventoryCostume(invalid, 1) && menu.ClearGridInventoryCostume(),
        "empty/non-armor/clear remain successful no-ops");
  Check(model.rows_ == before && model.refreshes == refreshes, "ignored Grid states retain registrations without refresh");
  model.projectionUsable = false;
  const RE::FormID valid[] = {60};
  Check(menu.ApplyGridInventoryCostume(valid, 1) && model.rows_ == before && model.refreshes == refreshes,
        "unusable projection cannot pre-clear even base appearances");
  Check(!menu.ApplyGridInventoryCostume(nullptr, 1), "invalid Grid pointer/count rejected");
  RE::PlayerCharacter::available = false;
  Check(!menu.ApplyGridInventoryCostume(valid, 1), "missing player leaves Grid state untouched");
  RE::PlayerCharacter::available = true;
  menu.gameDataLoaded_ = false;
  Check(!menu.ApplyGridInventoryCostume(valid, 1) && model.rows_ == before, "unready game leaves Grid state untouched");
}
int main() {
  TestDrops(); TestGrid();
  if (failures) { std::fprintf(stderr, "%u transaction checks failed\n", failures); return 1; }
  std::puts("WorkbenchTransactionTests passed: production drop/reset/Grid routing; 128 costume switches (not engine projection/game).");
}
