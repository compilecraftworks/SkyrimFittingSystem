#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sfs::conditions {
inline constexpr std::string_view kDefaultConditionId = "condition-1";

enum class Comparator : std::uint8_t {
  Equal,
  NotEqual,
  Greater,
  GreaterOrEqual,
  Less,
  LessOrEqual
};

enum class Connective : std::uint8_t { And, Or };

struct Clause {
  std::string functionName;
  std::string customConditionId;
  std::array<std::string, 2> arguments{};
  Comparator comparator{Comparator::Equal};
  std::string comparand{"1"};
  Connective connectiveToNext{Connective::And};
};

struct Color {
  float x{0.55f};
  float y{0.55f};
  float z{0.55f};
  float w{1.0f};
};

struct CatalogProperties {
  Color color{};
};

struct Definition {
  std::string id;
  std::string name;
  std::string description;
  std::optional<CatalogProperties> catalog;
  std::vector<Clause> clauses;

  [[nodiscard]] bool IsCatalog() const { return true; }

  [[nodiscard]] const CatalogProperties *GetCatalog() const {
    return catalog ? &*catalog : nullptr;
  }

  [[nodiscard]] CatalogProperties *GetCatalog() {
    return catalog ? &*catalog : nullptr;
  }

  CatalogProperties &EnsureCatalog() {
    if (!catalog) {
      catalog = CatalogProperties{};
    }
    return *catalog;
  }
};
} // namespace sfs::conditions
