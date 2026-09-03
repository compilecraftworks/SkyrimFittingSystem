#include "conditions/CnfBuilder.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using sfs::conditions::Clause;
using sfs::conditions::Comparator;
using sfs::conditions::Connective;
using sfs::conditions::Definition;

struct Literal {
  std::string name;
  bool negated{false};

  [[nodiscard]] bool operator==(const Literal &) const = default;
};

using Expression = sfs::conditions::cnf::Expression<Literal>;

void Require(const bool a_condition, const std::string_view a_message) {
  if (!a_condition) {
    throw std::runtime_error(std::string(a_message));
  }
}

Clause Native(std::string a_name, const bool a_truth = true,
              const Connective a_next = Connective::And) {
  Clause clause;
  clause.functionName = std::move(a_name);
  clause.comparand = a_truth ? "1" : "0";
  clause.connectiveToNext = a_next;
  return clause;
}

Clause Reference(std::string a_id, const bool a_truth,
                 const Connective a_next = Connective::And,
                 const Comparator a_comparator = Comparator::Equal) {
  Clause clause;
  clause.customConditionId = std::move(a_id);
  clause.comparator = a_comparator;
  clause.comparand = a_truth ? "1" : "0";
  clause.connectiveToNext = a_next;
  return clause;
}

std::optional<Expression> Build(const Definition &a_definition,
                                const std::vector<Definition> &a_all) {
  return sfs::conditions::cnf::Build<Literal>(
      a_definition, a_all,
      [](const Clause &a_clause,
         const bool a_negated) -> std::optional<Literal> {
        return Literal{a_clause.functionName,
                       a_negated != (a_clause.comparand == "0")};
      });
}

void TestNestedBooleanComparatorTruthTable() {
  Definition base{.id = "A", .clauses = {Native("x")}};
  const std::vector definitions{base};

  const auto expectPolarity = [&](const Comparator a_comparator,
                                  const bool a_comparand,
                                  const bool a_expectedNegated) {
    Definition reference{
        .id = "B",
        .clauses = {Reference("A", a_comparand, Connective::And,
                              a_comparator)}};
    auto all = definitions;
    all.push_back(reference);
    const auto expression = Build(all.back(), all);
    Require(expression.has_value() && expression->size() == 1 &&
                expression->front().size() == 1 &&
                expression->front().front().negated == a_expectedNegated,
            "Nested boolean comparator polarity must match the editor value");
  };

  expectPolarity(Comparator::Equal, true, false);
  expectPolarity(Comparator::Equal, false, true);
  expectPolarity(Comparator::NotEqual, false, false);
  expectPolarity(Comparator::NotEqual, true, true);
}

void TestNativeDoubleNegationCollapses() {
  Definition inner{.id = "A", .clauses = {Native("x", false)}};
  Definition outer{.id = "B", .clauses = {Reference("A", false)}};
  const std::vector definitions{inner, outer};

  const auto expression = Build(definitions[1], definitions);
  Require(expression.has_value() && expression->size() == 1 &&
              expression->front().size() == 1 &&
              expression->front().front() == Literal{"x", false},
          "NOT of a false native clause must collapse before expansion");
}

void TestNestedDoubleNegationPreservesMixedExpression() {
  Definition base{
      .id = "C",
      .clauses = {Native("a", true, Connective::Or),
                  Native("b", true, Connective::And),
                  Native("c", true, Connective::Or), Native("d")}};
  Definition firstNegation{
      .id = "A", .clauses = {Reference("C", false)}};
  Definition secondNegation{
      .id = "B", .clauses = {Reference("A", false)}};
  const std::vector definitions{base, firstNegation, secondNegation};

  const auto baseExpression = Build(definitions[0], definitions);
  const auto twiceNegated = Build(definitions[2], definitions);
  Require(baseExpression.has_value() && twiceNegated.has_value() &&
              *baseExpression == *twiceNegated,
          "Nested NOT(NOT(C)) must retain C without CNF cross-product growth");

  const auto onceNegated = Build(definitions[1], definitions);
  Require(onceNegated.has_value() && onceNegated->size() == 4,
          "A single negation must still preserve De Morgan semantics");
  for (const auto &group : *onceNegated) {
    Require(group.size() == 2 && group[0].negated && group[1].negated,
            "Negated mixed-expression literals must retain polarity");
  }
}

void TestExpansionBudgetFailsClosed() {
  Definition base{.id = "C"};
  for (int group = 0; group < 13; ++group) {
    base.clauses.push_back(
        Native("left-" + std::to_string(group), true, Connective::Or));
    base.clauses.push_back(
        Native("right-" + std::to_string(group), true, Connective::And));
  }
  Definition negated{.id = "A", .clauses = {Reference("C", false)}};
  const std::vector definitions{base, negated};

  Require(!Build(definitions[1], definitions).has_value(),
          "Pathological CNF expansion must stop at the fixed budget");
}

void TestCircularReferencesFailClosed() {
  Definition first{.id = "A", .clauses = {Reference("B", true)}};
  Definition second{.id = "B", .clauses = {Reference("A", false)}};
  const std::vector definitions{first, second};
  Require(!Build(definitions[0], definitions).has_value(),
          "Circular custom-condition references must fail closed");
}
} // namespace

int main() {
  try {
    TestNativeDoubleNegationCollapses();
    TestNestedBooleanComparatorTruthTable();
    TestNestedDoubleNegationPreservesMixedExpression();
    TestExpansionBudgetFailsClosed();
    TestCircularReferencesFailClosed();
    std::cout << "ConditionCnfLogicTests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "ConditionCnfLogicTests failed: " << error.what() << '\n';
    return 1;
  }
}
