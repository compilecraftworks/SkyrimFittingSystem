#pragma once

#include <cstdint>
#include <string_view>

namespace RE {
class Actor;
class TESObjectARMO;
}

namespace sfs::body_family {
using Mask = std::uint32_t;

enum class Sex : std::uint8_t { Male, Female };

enum class Family : Mask {
  FemaleVanilla = 1U << 0U,
  Cbbe = 1U << 1U,
  Unp = 1U << 2U,
  Ube = 1U << 3U,
  MaleVanilla = 1U << 4U,
  Himbo = 1U << 5U,
  Sam = 1U << 6U,
};

[[nodiscard]] constexpr Mask Bit(const Family a_family) {
  return static_cast<Mask>(a_family);
}

inline constexpr Mask kFemaleFamilies =
    Bit(Family::FemaleVanilla) | Bit(Family::Cbbe) | Bit(Family::Unp) |
    Bit(Family::Ube);
inline constexpr Mask kMaleFamilies =
    Bit(Family::MaleVanilla) | Bit(Family::Himbo) | Bit(Family::Sam);
inline constexpr Mask kAllFamilies = kFemaleFamilies | kMaleFamilies;

[[nodiscard]] constexpr Mask SexFamilies(const Sex a_sex) {
  return a_sex == Sex::Female ? kFemaleFamilies : kMaleFamilies;
}

[[nodiscard]] constexpr Mask VanillaFamily(const Sex a_sex) {
  return a_sex == Sex::Female ? Bit(Family::FemaleVanilla)
                              : Bit(Family::MaleVanilla);
}

[[nodiscard]] constexpr Mask NonVanillaFamilies(const Sex a_sex) {
  return SexFamilies(a_sex) & ~VanillaFamily(a_sex);
}

// Detects only explicit family evidence. It deliberately returns zero for an
// unlabelled value; catalog and actor fallbacks are applied by their callers.
[[nodiscard]] Mask DetectText(std::string_view a_text, Sex a_sex);

// Combines catalog evidence without allowing an unlabelled/Vanilla component
// to make an otherwise family-specific outfit or kit appear Vanilla.
[[nodiscard]] Mask MergeCatalogMasks(Mask a_left, Mask a_right);

// Returns explicit body-family evidence for an armor's ARMO/ARMA metadata.
// No Vanilla fallback is added here.
[[nodiscard]] Mask DetectArmor(const RE::TESObjectARMO *a_armor);

// Returns the catalog mask for an armor, including a per-sex Vanilla fallback
// only where that armor has a model for the corresponding sex.
[[nodiscard]] Mask ClassifyCatalogArmor(const RE::TESObjectARMO *a_armor);

// Resolves one family for the selected actor. This is actor-local and cached by
// the actor's effective Skin/Race/3D signature; it never scans nearby actors.
[[nodiscard]] Mask ResolveActor(RE::Actor *a_actor);

// Clears actor and installed-framework runtime caches across save transitions.
void ResetRuntimeCaches();

[[nodiscard]] constexpr bool Matches(const Mask a_itemFamilies,
                                     const Mask a_actorFamily) {
  // Preserve the pre-filter catalog when no valid actor can be resolved.
  if (a_actorFamily == 0) {
    return true;
  }

  Mask compatibleFamilies = a_actorFamily;
  if ((a_actorFamily & kFemaleFamilies) != 0) {
    compatibleFamilies |= Bit(Family::FemaleVanilla);
  }
  if ((a_actorFamily & kMaleFamilies) != 0) {
    compatibleFamilies |= Bit(Family::MaleVanilla);
  }
  return (a_itemFamilies & compatibleFamilies) != 0;
}

[[nodiscard]] std::string_view Name(Mask a_singleFamily);
} // namespace sfs::body_family
