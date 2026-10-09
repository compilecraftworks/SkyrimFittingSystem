// Mechanically extracted production ticket operations. Engine boundaries are
// host fakes; this is deliberately not presented as an in-game scene test.
#include "native/OStimAppearanceRules.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <objbase.h>
namespace logger {
template<class... T> void info(T&&...) {}
template<class... T> void error(T&&...) {}
}
namespace RE {
using FormID = std::uint32_t;
using VMStackID = std::uint32_t;
struct Actor { FormID id; FormID GetFormID() const { return id; } };
struct BSFixedString {
  std::string value;
  BSFixedString() = default;
  BSFixedString(std::string s) : value(std::move(s)) {}
  const char *c_str() const { return value.c_str(); }
};
struct TESObjectARMO {
  bool noStrip{};
  bool ContainsKeywordString(std::string_view) const { return noStrip; }
};
struct TESForm {
  inline static std::unordered_map<FormID, TESObjectARMO> armors;
  template<class T> static T *LookupByID(FormID id) {
    const auto it = armors.find(id);
    return it == armors.end() ? nullptr : &it->second;
  }
};
}
namespace sfs::virtual_tokens {
using RuntimeClock = std::chrono::steady_clock;
bool policy = true;
bool IsModSettingsStripLinkActive() { return policy; }
RE::FormID ActorID(RE::Actor *actor) { return actor ? actor->id : 0; }
int refreshes = 0;
void ApplyOwnedMask(RE::FormID, bool refresh = true) { if (refresh) { ++refreshes; } }
#include "OStim.production.inc"
}
using namespace sfs::virtual_tokens;
int main() {
  int failed = 0;
  const auto check = [&](bool ok, const char *what) {
    if (!ok) { ++failed; std::cerr << "FAIL: " << what << '\n'; }
  };
  RE::Actor actor{1}, removed{2};
  BeginOStimEquipmentPass(100,&actor);
  check(g_stackObservations[100].ostimPassActorID==1,"Pass is actor-local");
  EndOStimEquipmentPass(100);
  check(g_stackObservations.empty(),"Bridge-only stack marker released");
  g_stackObservations[100].actorID=2;
  g_stackObservations[100].queriedMask=4;
  BeginOStimEquipmentPass(100,&actor);
  EndOStimEquipmentPass(100);
  check(g_stackObservations[100].actorID==2 && g_stackObservations[100].queriedMask==4 &&
        !g_stackObservations[100].ostimEquipmentPass,"Nested caller's evidence preserved");
  g_stackObservations.clear();
  RE::TESForm::armors = {{10, {}}, {20, {true}}, {30, {}}, {40, {}}};
  g_registeredAppearances[1] = {
    {.identity="body", .actorID=1, .armorID=10, .slotMask=4, .tokenSlotMask=128, .generation=1},
    {.identity="protected", .actorID=1, .armorID=20, .slotMask=8, .tokenSlotMask=8, .generation=2},
    {.identity="wig", .actorID=1, .armorID=30, .slotMask=2, .tokenSlotMask=2, .generation=3, .observedWig=true},
    {.identity="multi", .actorID=1, .armorID=40, .slotMask=48, .tokenSlotMask=48, .generation=4}
  };
  policy = false;
  StripOStimAppearances(&actor, 0, ~0U, true);
  check(g_suppressionTickets.empty(), "Disabled/Vanilla must retain original control path");
  policy = true;
  StripOStimAppearances(&actor, 0, ~0U, false);
  check(g_suppressionTickets.size() == 1 && g_suppressionTickets[0].appearances.size() == 2,
        "NoStrip and HairTint wig excluded, body/multi accepted");
  check(g_suppressionTickets[0].restoreItemID == 0, "No fake item in real-gear cache");
  auto session = GetOStimRedressSession(&actor);
  g_suppressionTickets[0].transactionID += 1000; // Co-save rebasing, key stays stable.
  check(GetOStimSessionMask(&actor, session) == (128U|48U), "Control mask, not visual slots");
  RestoreOStimAppearances(&actor, 0, 4);
  check(GetOStimSessionMask(&actor, session) == (128U|48U), "Direct mapping preserved on redress");
  RestoreOStimAppearances(&actor, 0, 16);
  check(GetOStimSessionMask(&actor, session) == 128U, "Multi-slot appearance restores atomically");
  const auto selected = GetRegisteredAppearances(1,128);
  static_cast<void>(AddTicket(1, 10, selected, "OtherMod"));
  RestoreOStimSession(&actor, session, ~0U);
  check(g_suppressionTickets.size() == 1 && g_suppressionTickets[0].source == "OtherMod",
        "Other mod suppression ownership untouched");
  g_suppressionTickets.clear();
  for (int i=0; i<128; ++i) {
    StripOStimAppearances(&actor, 0, ~0U, true);
    check(g_suppressionTickets[0].appearances.size() == 3, "Wig setting enabled, NoStrip remains excluded");
    auto oldSession = GetOStimRedressSession(&actor);
    ObserveOStimSceneEnd(&actor,0);
    ObserveOStimSceneStart(&actor);
    check(g_suppressionTickets.empty(), "Migration cleans only ended ownership");
    StripOStimAppearances(&actor,0,128,false);
    RestoreOStimSession(&actor,oldSession,~0U);
    check(g_suppressionTickets.size()==1, "Delayed redress cannot clear new thread-ID-0 session");
    RestoreOStimAppearances(&actor,0,~0U);
    check(g_suppressionTickets.empty(), "No ticket growth after repeated transitions");
  }
  // Archived / API-absent OStim: no native stop/start callback is available.
  // Capturing animated redress must close only that episode before its wait.
  StripOStimAppearances(&actor,0,128,false);
  auto legacySession=GetOStimRedressSession(&actor);
  check(g_suppressionTickets.size()==1 &&
        g_suppressionTickets[0].source.starts_with("OStimEnded:"),
        "Animated capture closes ownership without restoring visibility");
  StripOStimAppearances(&actor,0,128,false);
  check(g_suppressionTickets.size()==2 &&
        !sfs::native::ostim::rules::SameSession(g_suppressionTickets.back().source,legacySession.c_str()),
        "API-absent new strip cannot reuse a pending animated session key");
  RestoreOStimSession(&actor,legacySession,~0U);
  check(g_suppressionTickets.size()==1 && g_suppressionTickets[0].source.starts_with("OStim:"),
        "API-absent old animated redress preserves the newer strip");
  RestoreOStimAppearances(&actor,0,~0U);
  check(g_suppressionTickets.empty(),"API-absent regular redress releases the current episode");

  g_registeredAppearances[2] = {{.identity="npc", .actorID=2, .armorID=10, .slotMask=4, .tokenSlotMask=4, .generation=5}};
  StripOStimAppearances(&removed,77,4,false);
  ObserveOStimSceneEnd(&removed,77);
  ObserveOStimSceneStart(&actor);
  check(g_suppressionTickets.size()==1, "Removed NPC retains animated/redress-off state");
  auto npcSession=GetOStimRedressSession(&removed);
  g_registeredAppearances[2][0].generation++;
  RestoreOStimSession(&removed,npcSession,4);
  check(g_suppressionTickets.empty(), "Stale appearance generation released for owner only");
  StripOStimAppearances(nullptr,0,~0U,true);
  RestoreOStimAppearances(nullptr,0,~0U);
  check(g_suppressionTickets.empty(), "None actor harmless");
  if (!failed) { std::cout << "OStim appearance production regressions passed\n"; }
  return failed ? 1 : 0;
}
