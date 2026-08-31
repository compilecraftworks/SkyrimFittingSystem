#include "catalog/BodyFamily.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace sfs::body_family {
namespace {
std::vector<std::string> Tokenize(const std::string_view a_text) {
  std::vector<std::string> tokens;
  std::string current;
  current.reserve(24);

  const auto flush = [&]() {
    if (!current.empty()) {
      tokens.push_back(std::move(current));
      current.clear();
    }
  };

  for (const unsigned char character : a_text) {
    if (std::isalnum(character) != 0) {
      current.push_back(static_cast<char>(std::tolower(character)));
    } else {
      flush();
    }
  }
  flush();
  return tokens;
}

bool HasToken(const std::vector<std::string> &a_tokens,
              const std::string_view a_value) {
  return std::ranges::find(a_tokens, a_value) != a_tokens.end();
}

bool HasDistinctiveFragment(const std::vector<std::string> &a_tokens,
                            const std::string_view a_value) {
  return std::ranges::any_of(a_tokens, [&](const std::string &a_token) {
    return a_token.find(a_value) != std::string::npos;
  });
}

Mask MergeSex(const Mask a_left, const Mask a_right, const Sex a_sex) {
  auto merged = (a_left | a_right) & SexFamilies(a_sex);
  if ((merged & NonVanillaFamilies(a_sex)) != 0) {
    merged &= ~VanillaFamily(a_sex);
  }
  return merged;
}
} // namespace

Mask DetectText(const std::string_view a_text, const Sex a_sex) {
  const auto tokens = Tokenize(a_text);
  if (tokens.empty()) {
    return 0;
  }

  Mask detected = 0;
  if (a_sex == Sex::Female) {
    const bool cbbe = HasDistinctiveFragment(tokens, "cbbe");
    const bool threeBa = HasDistinctiveFragment(tokens, "3ba");
    const bool unp = HasDistinctiveFragment(tokens, "bhunp") ||
                     HasDistinctiveFragment(tokens, "uunp") ||
                     HasDistinctiveFragment(tokens, "unpb") ||
                     HasToken(tokens, "unp");
    const bool ube = HasToken(tokens, "ube") || HasToken(tokens, "ube2") ||
                     HasDistinctiveFragment(tokens, "ubebody");

    if (cbbe || threeBa) {
      detected |= Bit(Family::Cbbe);
    }
    if (unp) {
      detected |= Bit(Family::Unp);
    }
    if (ube) {
      detected |= Bit(Family::Ube);
    }

    // 3BBB exists in both the CBBE and BHUNP ecosystems. It only reinforces a
    // family already established by an unambiguous signal and never decides a
    // family by itself.
    const bool threeBbb = HasDistinctiveFragment(tokens, "3bbb");
    if (threeBbb && (cbbe || threeBa)) {
      detected |= Bit(Family::Cbbe);
    }
    if (threeBbb && unp) {
      detected |= Bit(Family::Unp);
    }
    return detected;
  }

  if (HasDistinctiveFragment(tokens, "himbo")) {
    detected |= Bit(Family::Himbo);
  }
  if (HasToken(tokens, "sam") || HasToken(tokens, "samlight") ||
      HasToken(tokens, "samhighpoly")) {
    detected |= Bit(Family::Sam);
  }
  return detected;
}

Mask MergeCatalogMasks(const Mask a_left, const Mask a_right) {
  return MergeSex(a_left, a_right, Sex::Female) |
         MergeSex(a_left, a_right, Sex::Male);
}

std::string_view Name(const Mask a_singleFamily) {
  if (a_singleFamily == Bit(Family::Cbbe)) {
    return "CBBE";
  }
  if (a_singleFamily == Bit(Family::Unp)) {
    return "UNP";
  }
  if (a_singleFamily == Bit(Family::Ube)) {
    return "UBE";
  }
  if (a_singleFamily == Bit(Family::Himbo)) {
    return "HIMBO";
  }
  if (a_singleFamily == Bit(Family::Sam)) {
    return "SAM";
  }
  if (a_singleFamily == Bit(Family::FemaleVanilla) ||
      a_singleFamily == Bit(Family::MaleVanilla)) {
    return "Vanilla";
  }
  return "Unknown";
}
} // namespace sfs::body_family
