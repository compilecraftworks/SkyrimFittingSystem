#pragma once

#include <cwctype>
#include <filesystem>
#include <optional>
#include <string_view>

namespace sfs::catalog {
// Validate the final relative key, including overwrite paths obtained from
// the catalog. No filesystem mutation occurs here. Ordinary nested/Unicode
// collections remain valid; drive roots, parent traversal and Windows aliases
// must never turn a kit save into a write outside the kit directory.
[[nodiscard]] inline std::optional<std::filesystem::path>
ResolveKitWritePath(const std::filesystem::path &a_root,
                    const std::filesystem::path &a_relative) {
  if (a_relative.empty() || a_relative.has_root_path()) {
    return std::nullopt;
  }
  for (const auto &part : a_relative) {
    const auto text = part.wstring();
    if (text.empty() || text == L".." || text.back() == L' ' ||
        (text != L"." && text.back() == L'.') ||
        text.find_first_of(L"<>:\"|?*") != std::wstring::npos) {
      return std::nullopt;
    }
    for (const auto character : text) {
      if (character < L' ') { return std::nullopt; }
    }
  }

  std::error_code error;
  const auto root = std::filesystem::weakly_canonical(a_root, error);
  if (error || root.empty()) { return std::nullopt; }
  const auto target = std::filesystem::weakly_canonical(root / a_relative, error);
  if (error) { return std::nullopt; }
  auto targetIt = target.begin();
  for (const auto &rootPart : root) {
    if (targetIt == target.end()) { return std::nullopt; }
    auto left = rootPart.wstring();
    auto right = targetIt->wstring();
    if (left.size() != right.size()) { return std::nullopt; }
    for (std::size_t i = 0; i < left.size(); ++i) {
      if (std::towlower(left[i]) != std::towlower(right[i])) {
        return std::nullopt;
      }
    }
    ++targetIt;
  }
  // Keep the caller's virtual Data path for the actual write (MO2/USVFS).
  // Canonicalization is a containment check, not a change of write destination.
  return targetIt == target.end() ? std::nullopt
      : std::optional{(a_root / a_relative).lexically_normal()};
}
} // namespace sfs::catalog
