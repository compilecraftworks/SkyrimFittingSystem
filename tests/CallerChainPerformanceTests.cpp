// Actual BuildCallerChain + CallerIdentity; fake Papyrus frames and cached hash.
// Counts function visits, NOT timings, actual SkyUI calls, or native/game work.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>
#include <vector>
#include <unordered_set>
#include <type_traits>
unsigned visits{}, hashQueries{};
namespace RE {
using FormID = std::uint32_t;
struct TESForm { bool token{}; FormID GetFormID() const { return 0; } };
struct TESObjectREFR : TESForm { template<class T> T* As() { return nullptr; } };
struct Actor : TESObjectREFR {};
struct BGSOutfit : TESForm {};
struct BGSKeyword : TESForm {};
struct BGSRefAlias { Actor* GetActorReference() { return nullptr; } };
}
namespace RE::BSScript {
struct IFunction {
  bool native{};
  std::string source{"Audit.psc"}, object{"Audit"}, state, name{"Repeated"};
  bool GetIsNative() const { ++visits; return native; }
  const std::string& GetSourceFilename() const { return source; }
  const std::string& GetObjectTypeName() const { return object; }
  const std::string& GetStateName() const { return state; }
  const std::string& GetName() const { return name; }
};
struct Pointer { IFunction* p{}; IFunction* get() const { return p; } };
struct Value {
  RE::TESForm* form{};
  template<class T> T Unpack() const {
    if constexpr (std::is_same_v<T, RE::TESForm*>) return form;
    else return {};
  }
};
struct Frame { Frame* previousFrame{}; Pointer owningFunction; Value self; };
struct Stack { Frame* top{}; };
}
std::uint64_t FunctionCodeHash(const RE::BSScript::IFunction*) { ++hashQueries; return 42; }
#include "caller-identity.production.inc"
#include "caller-chain.production.inc"
#include "target-native.production.inc"
#include "hook-operation.production.inc"
template<class T> T ReadArgument(const RE::BSScript::Frame&, std::uint32_t) { return {}; }
std::uint32_t SlotMask(std::uint32_t) { return 0; }
// The actual token resolver also accepts token-backed references. This fixture
// controls its answer; only caller routing/preparation is under test here.
std::uint32_t MaskForToken(const RE::TESForm* form) {
  return form && form->token ? 1U : 0U;
}
namespace sfs::native::sexlab_pplus::rules {
// This fixture tests preparation's caller routing only, not P+ mask logic
// (the real mask rules have separate CoreBehaviorRegressionTests coverage).
std::uint32_t ResolveStripSlotMask(std::int32_t, const std::vector<std::int32_t>&,
                                 const std::vector<std::int32_t>&) { return 0; }
}
#include "prepare-operation.production.inc"
void Check(bool ok, const char* text) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
  std::printf("PASS: %s (visits=%u, cached-hash lookups=%u)\n", text, visits, hashQueries);
}
int main() {
  using namespace RE::BSScript;
  constexpr std::size_t count = 128;
  IFunction repeated;
  std::vector<Frame> frames(count + 1);
  for (std::size_t i = 0; i != count; ++i) {
    frames[i].previousFrame = &frames[i+1];
    frames[i+1].owningFunction.p = &repeated;
  }
  Stack stack{&frames[0]};
  auto result = BuildCallerChain(&stack);
  Check(result.size() == 1 && visits == count && hashQueries == 1,
        "128 repeated frames preserve identity with one cached-hash query");
  IFunction deepCaller;
  deepCaller.name = "TrustedStripCaller";
  frames[count].owningFunction.p = &deepCaller;
  visits = hashQueries = 0; result = BuildCallerChain(&stack);
  Check(result.size() == 2 && visits == count && hashQueries == 2,
        "deep trusted caller remains discoverable after repeated frames");
  std::vector<IFunction> unique(count);
  for (std::size_t i = 0; i != count; ++i) {
    unique[i].name = std::to_string(i);
    frames[i+1].owningFunction.p = &unique[i];
  }
  visits = hashQueries = 0; result = BuildCallerChain(&stack);
  Check(result.size() == 24 && visits == 24 && hashQueries == 24,
        "control: 24 unique script callers stop traversal");
  repeated.native = true;
  for (std::size_t i = 0; i != count; ++i) frames[i+1].owningFunction.p = &repeated;
  visits = hashQueries = 0; result = BuildCallerChain(&stack);
  Check(result.empty() && visits == count && hashQueries == 0,
        "native frames bypass identity count and still traverse all 128 frames");
  Check(BuildCallerChain(nullptr).empty(), "null stack stays empty");
  repeated.native = false;
  RE::TESForm token{true}, ordinary{false};
  frames[0].self.form = &token;
  for (int i = static_cast<int>(TargetNative::GetWornForm);
       i <= static_cast<int>(TargetNative::SexLabPPlusUnequipSlots); ++i) {
    visits = hashQueries = 0;
    const auto target = static_cast<TargetNative>(i);
    const auto operation = PrepareOperation(target, &stack);
    const bool bodyQuery = target == TargetNative::WornHasKeyword;
    Check(operation.callerChain.size() == (bodyQuery ? 0U : 1U) &&
          hashQueries == (bodyQuery ? 0U : 1U),
          "only displayed-body query skips caller tracing; strip/redress/filter/DD/P+ retain it");
  }
  for (auto* form : {&ordinary, static_cast<RE::TESForm*>(nullptr), &token}) {
    frames[0].self.form = form;
    for (auto target : {TargetNative::FormHasKeyword, TargetNative::FormGetKeywords,
                        TargetNative::FormGetNumKeywords, TargetNative::FormGetNthKeyword}) {
      visits = hashQueries = 0;
      for (unsigned n = 0; n < 256; ++n) {
        const auto operation = PrepareOperation(target, &stack);
        if (operation.item != form || operation.actor != nullptr ||
            operation.callerChain.size() != (form == &token ? 1U : 0U)) {
          Check(false, "Form self and token-only caller identity are preserved");
        }
      }
      Check(hashQueries == (form == &token ? 256U : 0U),
            "256 keyword reads: ordinary/null skip identity, tokens retain it");
    }
  }
  frames[0].self.form = &ordinary;
  for (auto target : {TargetNative::GetWornForm, TargetNative::EquipItem,
                     TargetNative::UnequipItem, TargetNative::FilterFormsByKeyword,
                     TargetNative::FilterFormsByKeywordString, TargetNative::FilterBySlotMask,
                     TargetNative::FormListContains, TargetNative::HasKeywordSubstring,
                     TargetNative::DeviousDevicesSyncSetting, TargetNative::SexLabPPlusStripByData}) {
    visits = hashQueries = 0;
    Check(PrepareOperation(target, &stack).callerChain.size() == 1 && hashQueries == 1,
          "non-token input does not bypass learning/filter/DD/P+ routes");
  }
  std::puts("Caller-chain regression checks passed; no in-game menu timing claim.");
}
