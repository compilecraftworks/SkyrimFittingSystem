// Actual deserializer + actual built-in definitions. Engine command lookup,
// name-collision helper and materialization cache side effects are stubbed.
// Test names are deliberately unique and no runtime materialization is claimed.
#include <algorithm>
#include <charconv>
#include <iostream>
#include <limits>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include "conditions/Defaults.h"
#include "conditions/Store.h"
#include "StringUtils.h"
namespace RE {
struct SCRIPT_FUNCTION { static void* LocateScriptCommand(const char*) { return nullptr; } };
}
namespace sfs::conditions {
const Definition* FindDefinitionByName(const std::vector<Definition>& all, std::string_view name) {
  const auto found=std::ranges::find_if(all, [&](const auto& d){return sfs::strings::EqualsInsensitive(d.name,name);});
  return found==all.end()?nullptr:&*found;
}
inline int rebuilds=0, invalidations=0;
void RebuildConditionDependencyMetadata(std::vector<Definition>&) { ++rebuilds; }
void InvalidateConditionMaterializationCaches(std::vector<Definition>&) { ++invalidations; }
}
namespace sfs::ui {
namespace conditions {
using Comparator=sfs::conditions::Comparator;
using Color=sfs::conditions::Color;
using Connective=sfs::conditions::Connective;
using Definition=sfs::conditions::Definition;
using Clause=sfs::conditions::Clause;
}
namespace condition_editor {
std::string BuildUniqueConditionName(std::string_view name,const std::vector<sfs::conditions::Definition>&) {return std::string(name)+"-renamed";}
}
}
#include "Conditions.production.inc"

int main() {
  int errors=0;
  auto check=[&](bool pass,const char* label){ if(!pass){std::cerr<<"FAILED: "<<label<<'\n'; ++errors;} };
  auto condition=[](std::string id,std::string name,std::string function){
    return nlohmann::json{{"id",id},{"name",name},{"clauses",nlohmann::json::array({{{"function",function}}})}};
  };
  sfs::conditions::Store store;
  std::string error;
  auto root=nlohmann::json{{"samplesSeeded",true},{"conditions",nlohmann::json::array({
      condition("condition-7","TestCombat","IsInCombat"),
      condition("condition-8","TestExterior","IsInInterior")})}};
  check(DeserializeConditionStore(root,store,&error)&&store.nextConditionId==9,
        "ordinary unique IDs preserve definitions and next ID");
  const auto priorSize=store.definitions.size();
  const auto originalId=store.definitions.back().id;
  auto rejectUnchanged=[&](const auto& input,const char* label){
    const auto before=sfs::conditions::invalidations;
    const auto rebuilt=sfs::conditions::rebuilds;
    check(!DeserializeConditionStore(input,store,&error)&&!error.empty()&&
          store.definitions.size()==priorSize&&store.definitions.back().id==originalId&&
          store.nextConditionId==9&&sfs::conditions::invalidations==before&&
          sfs::conditions::rebuilds==rebuilt,label);
  };
  root["conditions"][1]["id"]="condition-7";
  rejectUnchanged(root,"duplicate IDs rejected before store/cache publication");
  root["conditions"]=nlohmann::json::array({condition("builtin-city","Shadow","IsInCombat")});
  rejectUnchanged(root,"built-in definition cannot be replaced");
  root["conditions"]=nlohmann::json::array({condition("condition-2147483647","Overflow","IsInCombat")});
  rejectUnchanged(root,"next ID overflow rejected");
  root["conditions"]=nlohmann::json::array(); root["nextConditionId"]=2147483647;
  rejectUnchanged(root,"serialized next ID overflow rejected");
  rejectUnchanged(nlohmann::json::array(),"non-object rejected atomically");

  root.erase("nextConditionId");
  root["conditions"]=nlohmann::json::array({condition("condition-9","Nested","")});
  root["conditions"][0]["clauses"][0]["customConditionId"]="builtin-city";
  check(DeserializeConditionStore(root,store,&error)&&store.definitions.back().clauses[0].customConditionId=="builtin-city",
        "normal reference TO built-in remains valid");
  root["conditions"]=nlohmann::json::array({condition("condition-10","FutureExtension","UnrecognizedPluginFunction")});
  root["conditions"][0]["clauses"][0]["arg1"]="WhiterunBanneredMare";
  check(DeserializeConditionStore(root,store,&error)&&
        store.definitions.back().clauses[0].functionName=="UnrecognizedPluginFunction"&&
        store.definitions.back().clauses[0].arguments[0]=="WhiterunBanneredMare",
        "unknown external function and EditorID text preserved");
  root["conditions"].push_back(condition("condition-11","FutureExtension","IsInCombat"));
  check(DeserializeConditionStore(root,store,&error)&&store.definitions.back().id=="condition-11"&&
        store.definitions.back().name=="FutureExtension-renamed",
        "ordinary name collision renames display name, not stable ID");
  check(DeserializeConditionStore(nlohmann::json::object(),store,&error)&&store.samplesSeeded&&store.nextConditionId>=4,
        "first-run sample seeding remains enabled");
  check(DeserializeConditionStore(nlohmann::json{{"samplesSeeded",true}},store,&error)&&
        store.definitions.size()==sfs::conditions::BuildBuiltInConditions().size(),
        "intentionally empty custom list is not replaced with samples");
  std::cout<<"ConditionStoreIntegrityTests: "<<(errors?"FAILED":"PASSED")
           <<" (actual deserializer, host lookup/cache boundaries)\n";
  return errors?1:0;
}
