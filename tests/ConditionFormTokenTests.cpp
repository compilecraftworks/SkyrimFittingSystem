// Compile the production resolver against a small recording engine fixture.
#include "conditions/FormTokens.h"
#include "conditions/FormTokenRules.h"
#include <cstdlib>
#include <iostream>

void Expect(bool value, const char *message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
  using namespace sfs::conditions;
  Expect(ParseFormIDToken(" 0x000123Ab ") == 0x123ABu, "prefixed hex");
  Expect(ParseFormIDToken("FE123ABC") == 0xFE123ABC, "light runtime ID");
  for (const auto *bad : {"", "0", "0x", "-1", "123xyz", "100000000", "12 34"}) {
    Expect(!ParseFormIDToken(bad), "malformed/overflow/zero ID rejected");
  }
  const auto plugin = ParsePluginFormToken(" Example.esp | 0x1234 ");
  Expect(plugin && plugin->plugin == "Example.esp" && plugin->localID == 0x1234,
         "stable plugin token");
  for (const auto *bad : {"|1234", "A.esp|", "A.esp|12zz", "A.esp|12|34", "A.esp|01001234"}) {
    Expect(!ParsePluginFormToken(bad), "strict plugin/local parse");
  }
  RE::TESDataHandler data;
  RE::TESDataHandler::singleton = &data;
  RE::TESObjectCELL normal, interior, persistent, exterior, unnamed, removed;
  RE::TESObjectCELL *cells[] = {&normal, &interior, &persistent, &exterior, &unnamed, &removed};
  for (int i = 0; i < 6; ++i) { cells[i]->id = 0x12340 + i; RE::TESForm::byID[cells[i]->id] = cells[i]; }
  interior.editorID = "WhiterunInterior";
  exterior.editorID = "ExteriorCell";
  removed.deleted = true;
  data.cells = {&normal, &interior, nullptr};
  data.interiorCells = {&interior, nullptr};
  Expect(CollectCellForms().size() == 5, "normal/interior/registered cells, dedup/null/deleted");
  Expect(RE::TESForm::tableLock.readers == 0, "snapshot lock released");
  Expect(LookupFormToken<RE::TESObjectCELL>("whiteruninterior") == &interior, "case-insensitive interior fallback");
  Expect(LookupFormToken<RE::TESObjectCELL>("ExteriorCell") == &exterior, "world cell fallback");
  Expect(LookupFormToken<RE::TESObjectCELL>("0X12344") == &unnamed, "no EditorID needed");
  Expect(!LookupFormToken<RE::TESObjectCELL>("12345"), "deleted form rejected");
  data.pluginForms[{"Skyrim.esm", 0x12341}] = &interior;
  Expect(LookupFormToken<RE::TESObjectCELL>("Skyrim.esm|00012341") == &interior, "plugin CELL");
  Expect(!LookupFormToken<RE::TESRace>("Skyrim.esm|00012341"), "wrong typed ID rejected");
  RE::TESRace race; RE::TESFaction faction; RE::BGSKeyword keyword; RE::BGSLocation location;
  RE::TESObjectREFR reference;
  RE::TESForm *forms[] = {&race, &faction, &keyword, &location, &reference};
  for (int i = 0; i < 5; ++i) {
    forms[i]->id = 0x20000 + i;
    forms[i]->editorID = "TestForm" + std::to_string(i);
    RE::TESForm::byID[forms[i]->id] = forms[i];
    if (i != 4) { data.formArrays[0].push_back(forms[i]); }
    data.pluginForms[{"Mod.esp", static_cast<RE::FormID>(i + 1)}] = forms[i];
  }
  Expect(LookupFormToken<RE::TESRace>("20000") == &race, "race FormID");
  Expect(LookupFormToken<RE::TESFaction>("testform1") == &faction, "faction fallback");
  Expect(LookupFormToken<RE::BGSKeyword>("Mod.esp|3") == &keyword, "keyword plugin token");
  Expect(LookupFormToken<RE::BGSLocation>("0x20003") == &location, "location FormID");
  Expect(LookupFormToken<RE::TESObjectREFR>("Mod.esp|5") == &reference, "reference same resolver");
  Expect(LookupFormToken<RE::TESObjectREFR>("testform4") == &reference,
         "reference EditorID outside normal arrays resolves through registry snapshot");
  Expect(LookupFormToken("testform2") == &keyword, "generic form");
  Expect(!LookupFormToken<RE::TESRace>("20003"), "wrong typed raw ID rejected");
  RE::Actor actor; RE::TESNPC actorBase;
  actor.id = 0x14; actorBase.id = 7;
  RE::TESForm::byID[0x14] = &actor; RE::TESForm::byID[7] = &actorBase;
  Expect(LookupFormToken<RE::Actor>("14") == &actor, "actor reference");
  Expect(!LookupFormToken<RE::Actor>("7"), "actor base is not an actor reference");
  Expect(LookupFormToken<RE::TESNPC>("7") == &actorBase, "actor base remains separate");
  keyword.ignored = true;
  Expect(!LookupFormToken("Mod.esp|3"), "ignored form rejected");
  RE::TESForm::byEditorID["ABC"] = &race;
  RE::TESForm::byID[0xABC] = &location;
  Expect(LookupFormToken("ABC") == &race, "EditorID precedence preserved");
  Expect(!LookupFormToken<RE::TESObjectCELL>("whiteruninterior", false), "UI direct lookup avoids full scan");
  RE::TESDataHandler::singleton = nullptr;
  Expect(CollectCellForms().empty(), "missing handler is safe");
  Expect(!LookupFormToken("Mod.esp|5"), "missing handler plugin resolution safe");
  std::cout << "Condition form-token production resolver tests passed\n";
}
