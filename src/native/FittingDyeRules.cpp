#include "native/FittingDyeRules.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace sfs::native::dye::rules {
namespace {
[[nodiscard]] std::string LowerAscii(const std::string_view a_value) {
  std::string normalized(a_value);
  std::ranges::transform(normalized, normalized.begin(),
                         [](const unsigned char a_character) {
                           return static_cast<char>(
                               std::tolower(a_character));
                         });
  return normalized;
}

[[nodiscard]] std::string_view Filename(const std::string_view a_path) {
  const auto separator = a_path.find_last_of("\\/");
  return separator == std::string_view::npos ? a_path
                                              : a_path.substr(separator + 1);
}

template <std::size_t N>
[[nodiscard]] bool StartsWithAny(
    const std::string_view a_value,
    const std::array<std::string_view, N> &a_prefixes) {
  return std::ranges::any_of(a_prefixes, [&](const auto a_prefix) {
    return a_value.starts_with(a_prefix);
  });
}

[[nodiscard]] bool IsBodyFamilyShapeName(const std::string_view a_value) {
  static constexpr std::array kExactBodyNames{
      std::string_view{"cbbe"},       std::string_view{"3ba"},
      std::string_view{"3bbb"},       std::string_view{"bhunp"},
      std::string_view{"bhunp_3bbb"}, std::string_view{"unp"},
      std::string_view{"unpb"},       std::string_view{"unpb body"},
      std::string_view{"uunp"},       std::string_view{"himbo"},
      std::string_view{"ube"},        std::string_view{"ube2"},
      std::string_view{"ube 2.0"},    std::string_view{"ube se 2.0"},
      std::string_view{"sam"},        std::string_view{"samlight"},
      std::string_view{"sam light"},  std::string_view{"sam_light"},
      std::string_view{"sam high poly"},
      std::string_view{"samhighpoly"}, std::string_view{"sam_highpoly"},
  };
  if (std::ranges::find(kExactBodyNames, a_value) != kExactBodyNames.end()) {
    return true;
  }

  static constexpr std::array kFamilyPrefixes{
      std::string_view{"cbbe"}, std::string_view{"3ba"},
      std::string_view{"3bbb"}, std::string_view{"bhunp"},
      std::string_view{"uunp"}, std::string_view{"unpb"},
      std::string_view{"unp"}, std::string_view{"himbo"},
      std::string_view{"ube"}, std::string_view{"sam"},
  };
  static constexpr std::array kAnatomySuffixes{
      std::string_view{"anus"},    std::string_view{"clit"},
      std::string_view{"genital"}, std::string_view{"labia"},
      std::string_view{"penis"},   std::string_view{"rectum"},
      std::string_view{"vagina"},
  };
  for (const auto family : kFamilyPrefixes) {
    if (!a_value.starts_with(family) || a_value.size() <= family.size()) {
      continue;
    }
    auto suffix = a_value.substr(family.size());
    if (suffix.front() != '_' && suffix.front() != '-' &&
        suffix.front() != ' ') {
      continue;
    }
    suffix.remove_prefix(1);
    if (StartsWithAny(suffix, kAnatomySuffixes)) {
      return true;
    }
  }

  static constexpr std::array kExplicitBodyPrefixes{
      std::string_view{"cbbebody"},  std::string_view{"cbbe body"},
      std::string_view{"3babody"},   std::string_view{"3ba body"},
      std::string_view{"3bbbbody"},  std::string_view{"3bbb body"},
      std::string_view{"bhunpbody"}, std::string_view{"bhunp body"},
      std::string_view{"uunpbody"},  std::string_view{"uunp body"},
      std::string_view{"unpbbody"},  std::string_view{"unpb body"},
      std::string_view{"unpbody"},   std::string_view{"unp body"},
      std::string_view{"himbobody"}, std::string_view{"himbo body"},
      std::string_view{"ubebody"},   std::string_view{"ube body"},
      std::string_view{"sambody"},   std::string_view{"sam body"},
      std::string_view{"samlightbody"},
      std::string_view{"samhighpolybody"},
  };
  if (StartsWithAny(a_value, kExplicitBodyPrefixes)) {
    return true;
  }

  // UBE/SAM projects often include the release name before the final Body
  // token. Do not treat arbitrary family-labelled outfit names as base bodies.
  const bool familyLabelled = a_value.starts_with("ube ") ||
                              a_value.starts_with("ube_") ||
                              a_value.starts_with("ube-") ||
                              a_value.starts_with("sam ") ||
                              a_value.starts_with("sam_") ||
                              a_value.starts_with("sam-");
  return familyLabelled &&
         (a_value.ends_with(" body") || a_value.ends_with("_body") ||
          a_value.ends_with("-body"));
}

[[nodiscard]] bool IsAnatomicalHelperShape(
    const std::string_view a_value) {
  static constexpr std::array kAnatomyPrefixes{
      std::string_view{"anus"},     std::string_view{"clit"},
      std::string_view{"genital"},  std::string_view{"labia"},
      std::string_view{"penis"},    std::string_view{"rectum"},
      std::string_view{"scrotum"},  std::string_view{"testicle"},
      std::string_view{"urethra"},  std::string_view{"vagina"},
  };
  if (StartsWithAny(a_value, kAnatomyPrefixes)) {
    return true;
  }

  // UBE body references use names such as "NPC RB Anus2". Restrict the
  // contains check to NPC helper names so ordinary armor names remain valid.
  if (!a_value.starts_with("npc ")) {
    return false;
  }
  return std::ranges::any_of(kAnatomyPrefixes, [&](const auto a_prefix) {
    return a_value.find(a_prefix) != std::string_view::npos;
  });
}

[[nodiscard]] bool IsCollisionHelperShape(const std::string_view a_value) {
  // Most meshes spell this out (for example LowerCollision), while UBE also
  // uses compact names such as ArmColli, FeetColli and ButtLegColli. Match
  // only the complete compact suffix so ordinary outfit names containing
  // "colli" remain available for dyeing.
  return a_value.find("collision") != std::string_view::npos ||
         a_value.ends_with("colli");
}

[[nodiscard]] bool IsCharacterBaseShapeName(
    const std::string_view a_shapeName) {
  static constexpr std::array kVanillaBodyPrefixes{
      std::string_view{"femalebody"},  std::string_view{"malebody"},
      std::string_view{"femalehands"}, std::string_view{"malehands"},
      std::string_view{"femalefeet"},  std::string_view{"malefeet"},
      std::string_view{"femalehead"},  std::string_view{"malehead"},
      std::string_view{"vanillabody"},
      std::string_view{"femalegenital"},
      std::string_view{"malegenital"},
  };
  static constexpr std::array kGenericBodyNames{
      std::string_view{"body"},     std::string_view{"hands"},
      std::string_view{"feet"},     std::string_view{"face"},
      std::string_view{"head"},     std::string_view{"genital"},
      std::string_view{"genitals"},
  };

  return IsBodyFamilyShapeName(a_shapeName) ||
         IsAnatomicalHelperShape(a_shapeName) ||
         StartsWithAny(a_shapeName, kVanillaBodyPrefixes) ||
         std::ranges::find(kGenericBodyNames, a_shapeName) !=
             kGenericBodyNames.end() ||
         a_shapeName.starts_with("virtual") ||
         IsCollisionHelperShape(a_shapeName);
}

[[nodiscard]] bool IsCharacterBaseDiffuseFilename(
    const std::string_view a_filename) {
  static constexpr std::array kVanillaBodyPrefixes{
      std::string_view{"femalebody"},  std::string_view{"malebody"},
      std::string_view{"femalehands"}, std::string_view{"malehands"},
      std::string_view{"femalefeet"},  std::string_view{"malefeet"},
      std::string_view{"femalehead"},  std::string_view{"malehead"},
      std::string_view{"vanillabody"},
  };
  static constexpr std::array kExplicitFamilyBodyPrefixes{
      std::string_view{"cbbebody"},  std::string_view{"3babody"},
      std::string_view{"3bbbbody"},  std::string_view{"bhunpbody"},
      std::string_view{"uunpbody"},  std::string_view{"unpbbody"},
      std::string_view{"unpbody"},   std::string_view{"ubebody"},
      std::string_view{"himbobody"}, std::string_view{"sambody"},
  };
  return StartsWithAny(a_filename, kVanillaBodyPrefixes) ||
         StartsWithAny(a_filename, kExplicitFamilyBodyPrefixes);
}
} // namespace

bool IsCharacterBaseComponent(const std::string_view a_shapeName,
                              const std::string_view a_diffuseTexture) {
  const auto normalizedShapeName = LowerAscii(a_shapeName);
  const auto normalizedDiffuseFilename =
      LowerAscii(Filename(a_diffuseTexture));
  return IsCharacterBaseShapeName(normalizedShapeName) ||
         IsCharacterBaseDiffuseFilename(normalizedDiffuseFilename);
}
} // namespace sfs::native::dye::rules
