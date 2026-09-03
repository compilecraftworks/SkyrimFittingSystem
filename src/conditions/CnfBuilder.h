#pragma once

#include "StringUtils.h"
#include "conditions/Definition.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sfs::conditions::cnf {
inline constexpr std::size_t kMaxGroupCount = 4096;
inline constexpr std::size_t kMaxLiteralCount = 16384;

template <class Literal> using OrClause = std::vector<Literal>;
template <class Literal> using Expression = std::vector<OrClause<Literal>>;

namespace detail {
[[nodiscard]] inline const Definition *
FindDefinition(const std::vector<Definition> &a_conditions,
               const std::string_view a_id) {
  for (const auto &definition : a_conditions) {
    if (definition.id == a_id) {
      return std::addressof(definition);
    }
  }
  return nullptr;
}

template <class Literal>
[[nodiscard]] std::optional<std::size_t>
CountLiterals(const Expression<Literal> &a_expression) {
  std::size_t count = 0;
  for (const auto &group : a_expression) {
    if (group.size() > kMaxLiteralCount - count) {
      return std::nullopt;
    }
    count += group.size();
  }
  return count;
}

template <class Literal>
[[nodiscard]] std::optional<Expression<Literal>>
And(const Expression<Literal> &a_left,
    const Expression<Literal> &a_right) {
  if (a_left.size() > kMaxGroupCount ||
      a_right.size() > kMaxGroupCount ||
      a_left.size() > kMaxGroupCount - a_right.size()) {
    return std::nullopt;
  }
  const auto leftLiterals = CountLiterals(a_left);
  const auto rightLiterals = CountLiterals(a_right);
  if (!leftLiterals || !rightLiterals ||
      *leftLiterals > kMaxLiteralCount - *rightLiterals) {
    return std::nullopt;
  }

  Expression<Literal> result = a_left;
  result.insert(result.end(), a_right.begin(), a_right.end());
  return result;
}

template <class Literal>
[[nodiscard]] std::optional<Expression<Literal>>
Or(const Expression<Literal> &a_left,
   const Expression<Literal> &a_right) {
  if (a_left.empty()) {
    return a_right;
  }
  if (a_right.empty()) {
    return a_left;
  }
  if (a_left.size() > kMaxGroupCount ||
      a_right.size() > kMaxGroupCount ||
      a_left.size() > kMaxGroupCount / a_right.size()) {
    return std::nullopt;
  }

  const auto leftLiterals = CountLiterals(a_left);
  const auto rightLiterals = CountLiterals(a_right);
  if (!leftLiterals || !rightLiterals) {
    return std::nullopt;
  }
  if (*leftLiterals > kMaxLiteralCount / a_right.size() ||
      *rightLiterals > kMaxLiteralCount / a_left.size()) {
    return std::nullopt;
  }
  const auto expandedLeft = *leftLiterals * a_right.size();
  const auto expandedRight = *rightLiterals * a_left.size();
  if (expandedLeft > kMaxLiteralCount - expandedRight) {
    return std::nullopt;
  }

  Expression<Literal> result;
  result.reserve(a_left.size() * a_right.size());
  for (const auto &leftGroup : a_left) {
    for (const auto &rightGroup : a_right) {
      OrClause<Literal> merged = leftGroup;
      merged.insert(merged.end(), rightGroup.begin(), rightGroup.end());
      result.push_back(std::move(merged));
    }
  }
  return result;
}

[[nodiscard]] inline std::optional<bool>
NestedConditionPolarity(const Clause &a_clause) {
  if (a_clause.comparator != Comparator::Equal &&
      a_clause.comparator != Comparator::NotEqual) {
    return std::nullopt;
  }
  const auto comparand = sfs::strings::TrimText(a_clause.comparand);
  if (comparand != "0" && comparand != "1") {
    return std::nullopt;
  }

  const bool truthy = comparand == "1";
  return (a_clause.comparator == Comparator::Equal && truthy) ||
         (a_clause.comparator == Comparator::NotEqual && !truthy);
}

struct VisitGuard {
  std::unordered_set<std::string> *visiting{nullptr};
  std::string_view id;

  ~VisitGuard() {
    if (visiting != nullptr && !id.empty()) {
      visiting->erase(std::string(id));
    }
  }
};

template <class Literal, class LeafBuilder>
[[nodiscard]] std::optional<Expression<Literal>> BuildDefinition(
    const Definition &a_definition,
    const std::vector<Definition> &a_conditions,
    std::unordered_set<std::string> &a_visiting, bool a_negated,
    LeafBuilder &a_leafBuilder);

template <class Literal, class LeafBuilder>
[[nodiscard]] std::optional<Expression<Literal>> BuildClause(
    const Clause &a_clause, const std::vector<Definition> &a_conditions,
    std::unordered_set<std::string> &a_visiting, const bool a_negated,
    LeafBuilder &a_leafBuilder) {
  if (!a_clause.customConditionId.empty()) {
    const auto *definition =
        FindDefinition(a_conditions, a_clause.customConditionId);
    const auto polarity = NestedConditionPolarity(a_clause);
    if (definition == nullptr || !polarity.has_value()) {
      return std::nullopt;
    }

    // The outer NOT and the custom-condition checkbox are folded before CNF
    // expansion. In particular, NOT(NOT(A)) reaches A with no negation.
    const bool nestedNegated = a_negated ^ !*polarity;
    return BuildDefinition<Literal>(*definition, a_conditions, a_visiting,
                                    nestedNegated, a_leafBuilder);
  }

  auto literal = a_leafBuilder(a_clause, a_negated);
  if (!literal) {
    return std::nullopt;
  }
  return Expression<Literal>{OrClause<Literal>{std::move(*literal)}};
}

template <class Literal, class LeafBuilder>
[[nodiscard]] std::optional<Expression<Literal>> BuildDefinition(
    const Definition &a_definition,
    const std::vector<Definition> &a_conditions,
    std::unordered_set<std::string> &a_visiting, const bool a_negated,
    LeafBuilder &a_leafBuilder) {
  if (a_definition.clauses.empty()) {
    return std::nullopt;
  }

  if (!a_definition.id.empty() &&
      !a_visiting.insert(a_definition.id).second) {
    return std::nullopt;
  }
  VisitGuard guard{a_definition.id.empty() ? nullptr : &a_visiting,
                   a_definition.id};

  auto currentBlock = BuildClause<Literal>(
      a_definition.clauses.front(), a_conditions, a_visiting, a_negated,
      a_leafBuilder);
  if (!currentBlock) {
    return std::nullopt;
  }

  Expression<Literal> result;
  bool hasResult = false;
  for (std::size_t index = 1; index < a_definition.clauses.size(); ++index) {
    auto clause = BuildClause<Literal>(a_definition.clauses[index],
                                       a_conditions, a_visiting, a_negated,
                                       a_leafBuilder);
    if (!clause) {
      return std::nullopt;
    }

    const auto connective =
        a_definition.clauses[index - 1].connectiveToNext;
    if (connective == Connective::Or) {
      currentBlock = a_negated ? And(*currentBlock, *clause)
                               : Or(*currentBlock, *clause);
      if (!currentBlock) {
        return std::nullopt;
      }
      continue;
    }

    if (!hasResult) {
      result = std::move(*currentBlock);
      hasResult = true;
    } else {
      auto combined =
          a_negated ? Or(result, *currentBlock) : And(result, *currentBlock);
      if (!combined) {
        return std::nullopt;
      }
      result = std::move(*combined);
    }
    currentBlock = std::move(clause);
  }

  if (!hasResult) {
    result = std::move(*currentBlock);
  } else {
    auto combined =
        a_negated ? Or(result, *currentBlock) : And(result, *currentBlock);
    if (!combined) {
      return std::nullopt;
    }
    result = std::move(*combined);
  }
  return result;
}
} // namespace detail

template <class Literal, class LeafBuilder>
[[nodiscard]] std::optional<Expression<Literal>>
Build(const Definition &a_definition,
      const std::vector<Definition> &a_conditions,
      LeafBuilder &&a_leafBuilder) {
  std::unordered_set<std::string> visiting;
  auto leafBuilder = std::forward<LeafBuilder>(a_leafBuilder);
  return detail::BuildDefinition<Literal>(a_definition, a_conditions, visiting,
                                          false, leafBuilder);
}
} // namespace sfs::conditions::cnf
