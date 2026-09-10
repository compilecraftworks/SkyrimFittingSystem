#include "conditions/Defaults.h"

#include <algorithm>

namespace sfs::conditions {
bool IsBuiltInCondition(const std::string_view id) { return id.starts_with("builtin-"); }
bool IsActorOwnershipCondition(const Definition &definition) {
  return definition.clauses.size() == 1 &&
         definition.clauses.front().customConditionId.empty() &&
         definition.clauses.front().functionName == "GetIsReference";
}


namespace {
Definition MakeCondition(std::string id, std::string name, std::string description,
                         Color color, std::vector<Clause> clauses) {
  Definition value;
  value.id = std::move(id); value.name = std::move(name);
  value.description = std::move(description);
  value.EnsureCatalog().color = color; value.clauses = std::move(clauses);
  return value;
}
Clause Test(std::string function, std::string arg = {}, std::string value = "1",
            Comparator comparator = Comparator::Equal,
            Connective join = Connective::And) {
  Clause clause; clause.functionName = std::move(function);
  clause.arguments[0] = std::move(arg); clause.comparand = std::move(value);
  clause.comparator = comparator; clause.connectiveToNext = join; return clause;
}
Clause Ref(std::string id) { Clause clause; clause.customConditionId = std::move(id); return clause; }
} // namespace

std::vector<Definition> BuildBuiltInConditions() {
  const Color location{0.95f, 0.58f, 0.22f, 1.0f}, combat{0.92f, 0.28f, 0.25f, 1.0f};
  const Color time{0.62f, 0.43f, 0.92f, 1.0f}, environment{0.20f, 0.66f, 0.82f, 1.0f};
  const Color state{0.25f, 0.72f, 0.52f, 1.0f};
  return {
    MakeCondition("builtin-interior", "Interior", "Applies while indoors.", location, {Test("IsInInterior")}),
    MakeCondition("builtin-exterior", "Exterior", "Applies while outdoors.", location, {Test("IsInInterior", {}, "0")}),
    MakeCondition("builtin-city", "City", "Applies in a city.", location, {Test("LocationHasKeyword", "LocTypeCity")}),
    MakeCondition("builtin-town", "Town", "Applies in a town.", location, {Test("LocationHasKeyword", "LocTypeTown")}),
    MakeCondition("builtin-dungeon", "Dungeon", "Applies in a dungeon.", location, {Test("LocationHasKeyword", "LocTypeDungeon")}),
    MakeCondition("builtin-home", "Home", "Applies in a player home.", location, {Test("LocationHasKeyword", "LocTypePlayerHouse")}),
    MakeCondition("builtin-combat", "Combat", "Applies during combat.", combat, {Test("IsInCombat")}),
    MakeCondition("builtin-noncombat", "Non-combat", "Applies outside combat.", combat, {Test("IsInCombat", {}, "0")}),
    MakeCondition("builtin-day", "Day", "Applies from 06:00 to 18:00.", time, {Test("GetCurrentTime", {}, "6", Comparator::GreaterOrEqual), Test("GetCurrentTime", {}, "18", Comparator::Less)}),
    MakeCondition("builtin-night", "Night", "Applies from 18:00 to 06:00.", time, {Test("GetCurrentTime", {}, "18", Comparator::GreaterOrEqual, Connective::Or), Test("GetCurrentTime", {}, "6", Comparator::Less)}),
    MakeCondition("builtin-rain", "Rain", "Applies while raining.", environment, {Test("IsRaining")}),
    MakeCondition("builtin-snow", "Snow", "Applies while snowing.", environment, {Test("IsSnowing")}),
    MakeCondition("builtin-underwater", "Underwater", "Applies while underwater.", environment, {Test("IsSwimming")}),
    MakeCondition("builtin-sneaking", "Sneaking", "Applies while sneaking.", state, {Test("IsSneaking")}),
    MakeCondition("builtin-weapon", "Weapon", "Applies while a weapon is drawn.", state, {Test("IsWeaponOut")})
  };
}

std::vector<Definition> BuildSampleConditions() {
  return {
    MakeCondition("sample-city-life", "City Life", "City AND non-combat", {0.92f, 0.52f, 0.24f, 1.0f}, {Ref("builtin-city"), Ref("builtin-noncombat")}),
    MakeCondition("sample-dungeon-exploration", "Dungeon Exploration", "Dungeon AND combat", {0.78f, 0.30f, 0.24f, 1.0f}, {Ref("builtin-dungeon"), Ref("builtin-combat")}),
    MakeCondition("sample-night-infiltration", "Night Infiltration", "Night AND sneaking", {0.45f, 0.38f, 0.82f, 1.0f}, {Ref("builtin-night"), Ref("builtin-sneaking")})
  };
}

std::string BuildConditionId(const int a_nextConditionId) {
  return "condition-" + std::to_string((std::max)(a_nextConditionId, 1));
}
} // namespace sfs::conditions
