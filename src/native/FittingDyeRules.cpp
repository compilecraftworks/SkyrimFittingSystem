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

[[nodiscard]] bool HasSamBodyPrefix(const std::string_view a_value) {
  return a_value == "sam" || a_value.starts_with("sam_") ||
         a_value.starts_with("sam-") || a_value.starts_with("sam ") ||
         a_value.starts_with("sambody") ||
         a_value.starts_with("samlight") ||
         a_value.starts_with("samhighpoly");
}

[[nodiscard]] bool HasUbeBodyPrefix(const std::string_view a_value) {
  return a_value == "ube" || a_value.starts_with("ube_") ||
         a_value.starts_with("ube-") || a_value.starts_with("ube ") ||
         a_value.starts_with("ube2") || a_value.starts_with("ubebody");
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
  static constexpr std::array kFamilyPrefixes{
      std::string_view{"3ba"},   std::string_view{"3bbb"},
      std::string_view{"cbbe"},  std::string_view{"bhunp"},
      std::string_view{"uunp"},  std::string_view{"unpb"},
      std::string_view{"unp"},   std::string_view{"himbo"},
  };
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

  return StartsWithAny(a_shapeName, kFamilyPrefixes) ||
         HasUbeBodyPrefix(a_shapeName) || HasSamBodyPrefix(a_shapeName) ||
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
