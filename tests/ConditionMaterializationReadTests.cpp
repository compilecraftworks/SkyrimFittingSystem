// Real cache/accessor/invalidation bodies, with controlled emission and metadata
// copy counters. NativeConditionStorage is production code with engine stubs.
#include "conditions/NativeConditionStorage.h"
#include "conditions/GraphMetadata.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
namespace logger { template<class... T> void warn(T&&...) {} }
namespace sfs::conditions {
unsigned emissions{}, metadataCopies{}, refreshBuilds{};
struct DisplayCnf : std::vector<std::vector<std::string>> {
  using Base = std::vector<std::vector<std::string>>;
  using Base::Base;
  DisplayCnf() = default;
  DisplayCnf(DisplayCnf&&) = default;
  DisplayCnf& operator=(DisplayCnf&&) = default;
  DisplayCnf(const DisplayCnf& rhs) : Base(rhs) { ++metadataCopies; }
  DisplayCnf& operator=(const DisplayCnf& rhs) { Base::operator=(rhs); ++metadataCopies; return *this; }
};
struct Clause { std::string customConditionId; };
struct Definition { std::string id; std::vector<Clause> clauses; bool fail{}; };
struct RefreshTargets { std::vector<std::uint32_t> actorFormIDs; bool useNearbyFallback{}; };
struct Lowered {
  std::shared_ptr<RE::TESCondition> condition;
  std::string signature;
  DisplayCnf displayCnf;
};
std::weak_ptr<NativeConditionStorage> lastOwner;
Definition* FindDefinitionById(std::vector<Definition>& definitions, std::string_view id) {
  for (auto& d : definitions) if (d.id == id) return &d;
  return nullptr;
}
std::optional<Lowered> LowerAndEmitCondition(Definition& definition, std::vector<Definition>&) {
  ++emissions;
  if (definition.fail) return std::nullopt;
  auto storage = std::make_shared<NativeConditionStorage>();
  auto* text = static_cast<RE::BSFixedString*>(storage->StoreText(definition.id));
  storage->condition.head = new RE::TESConditionItem{nullptr, text};
  lastOwner = storage;
  return Lowered{std::shared_ptr<RE::TESCondition>(storage, &storage->condition),
                 "signature:" + definition.id, DisplayCnf{{definition.id, "display clause"}}};
}
RefreshTargets BuildRefreshTargets(const std::shared_ptr<RE::TESCondition>&) {
  ++refreshBuilds; return {{0x14, 0x1234}, true};
}
void PruneConditionStatusCache(std::vector<Definition>&) {}
void ClearConditionStatusCache() {}
void EraseConditionStatusCache(const std::string&) {}
}
#include "MaterializerDeclaration.production.inc"
#include "MaterializerState.production.inc"
#include "Materializer.production.inc"
int main() {
  using namespace sfs::conditions;
  std::vector<Definition> definitions{{"base"}, {"parent", {{"base"}}}, {"unrelated"}, {"broken", {}, true}};
  RebuildConditionDependencyMetadata(definitions);
  auto executable = AcquireExecutableConditionById("base", definitions);
  Check(executable && emissions == 1 && refreshBuilds == 1, "cold executable access builds complete shared cache once");
  auto owner = lastOwner;
  metadataCopies = 0;
  for (unsigned i = 0; i < 10000; ++i) {
    Check(AcquireExecutableConditionById("base", definitions) == executable,
          "warm execution shares exactly the same owning condition");
  }
  Check(metadataCopies == 0 && emissions == 1 && refreshBuilds == 1,
        "10000 executable reads copy no UI CNF and perform no emission/target rebuild");
  auto ui = MaterializeConditionById("base", definitions);
  Check(ui && ui->condition == executable && ui->signature == "signature:base" &&
        ui->displayCnf.size() == 1 && ui->displayCnf[0][0] == "base" &&
        ui->refreshTargets.actorFormIDs == std::vector<std::uint32_t>{0x14, 0x1234} &&
        ui->refreshTargets.useNearbyFallback && metadataCopies == 1,
        "UI still gets full display and refresh metadata from the same cache");
  ui->displayCnf[0][0] = "edited copy";
  Check(MaterializeConditionById("base", definitions)->displayCnf[0][0] == "base",
        "UI metadata copies remain independent of cached data");
  auto parent = AcquireExecutableConditionById("parent", definitions);
  auto unrelated = AcquireExecutableConditionById("unrelated", definitions);
  InvalidateConditionMaterializationCachesFrom(definitions, "base");
  Check(!owner.expired() && std::string(executable->head->text->c_str()) == "base",
        "cache invalidation cannot free strings still owned by an active reader");
  Check(AcquireExecutableConditionById("base", definitions) != executable &&
        AcquireExecutableConditionById("parent", definitions) != parent &&
        AcquireExecutableConditionById("unrelated", definitions) == unrelated,
        "targeted invalidation updates both readers and dependents without evicting unrelated condition");
  executable.reset(); ui.reset();
  Check(owner.expired(), "last old reader releases old condition and its string storage");
  const auto beforeFailure = emissions;
  Check(!AcquireExecutableConditionById("broken", definitions) &&
        !MaterializeConditionById("broken", definitions) && emissions == beforeFailure + 1,
        "failed emission is consistently cached in both accessors");
  definitions.back().fail = false;
  InvalidateConditionMaterializationCachesFrom(definitions, "broken");
  Check(AcquireExecutableConditionById("broken", definitions) != nullptr,
        "edited failed condition retries after the same invalidation as the UI");
  Check(!AcquireExecutableConditionById("missing", definitions) &&
        !MaterializeConditionById("missing", definitions), "unknown condition has no executable or metadata");
  auto removed = AcquireExecutableConditionById("unrelated", definitions);
  definitions.erase(definitions.begin() + 2);
  RebuildConditionDependencyMetadata(definitions);
  Check(!AcquireExecutableConditionById("unrelated", definitions) &&
        !GetConditionRuntimeMap().contains("unrelated") && removed->head->text,
        "deletion prunes cache without invalidating a live reader");
  parent.reset(); unrelated.reset(); removed.reset();
  // The engine-string fixture has a single-threaded live-set. Leave only the
  // reader's one condition in flight during the concurrent ownership test.
  InvalidateConditionMaterializationCaches(definitions);
  std::thread reader([&] {
    for (unsigned i = 0; i < 2000; ++i) {
      auto current = AcquireExecutableConditionById("base", definitions);
      Check(current && std::string(current->head->text->c_str()) == "base",
            "concurrent cache reset preserves owning executable read");
    }
  });
  for (unsigned i = 0; i < 2000; ++i) InvalidateConditionMaterializationCaches(definitions);
  reader.join();
  InvalidateConditionMaterializationCaches(definitions);
  Check(GetConditionRuntimeMap().empty() && RE::BSFixedString::live.empty(),
        "reset and last reader destruction leave no condition strings or cache entries");
  std::puts("Condition materialization read regressions passed: 10000 warm reads, zero metadata copies; UI, invalidation, failures, ownership/reset preserved.");
}
