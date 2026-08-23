#pragma once

#include <string>
#include <string_view>

namespace sfs::ui::icons {
inline bool &IconFontAvailableStorage() {
  static bool available = false;
  return available;
}

inline void SetIconFontAvailable(const bool a_available) {
  IconFontAvailableStorage() = a_available;
}

inline bool IsIconFontAvailable() { return IconFontAvailableStorage(); }

inline const char *Choose(const char *a_icon, const char *a_fallback) {
  return IsIconFontAvailable() ? a_icon : a_fallback;
}

inline std::string Prefix(const char *a_icon, const char *a_fallback) {
  return std::string(Choose(a_icon, a_fallback)) + " ";
}

inline std::string Label(const char *a_icon, const char *a_fallback,
                         const std::string_view a_text) {
  if (!IsIconFontAvailable() || !a_fallback || a_fallback[0] == '\0') {
    return std::string(a_text);
  }

  return std::string(IsIconFontAvailable() ? a_icon : a_fallback) + " " +
         std::string(a_text);
}

inline constexpr char kCircleHelp[] = "\xee\x82\x82";      // ICON_LC_CIRCLE_HELP
inline constexpr char kEllipsis[] = "\xee\x82\xba";        // ICON_LC_ELLIPSIS
inline constexpr char kFavorite[] = "\xee\x83\xb5";        // ICON_LC_STAR
inline constexpr char kFileCode[] = "\xee\x83\x87";        // ICON_LC_FILE_CODE
inline constexpr char kFormId[] = "\xee\x83\xb2";          // ICON_LC_HASH
inline constexpr char kGripVertical[] = "\xee\x83\xae";    // ICON_LC_GRIP_VERTICAL
inline constexpr char kIdentifier[] = "\xee\x84\x87";      // ICON_LC_LINK
inline constexpr char kEditorId[] = "\xee\x84\x8b";        // ICON_LC_LIST
inline constexpr char kPlugin[] = "\xee\x84\xac";          // ICON_LC_PACKAGE
inline constexpr char kTrash[] = "\xee\x86\x8c";           // ICON_LC_TRASH
inline constexpr char kSlot[] = "\xee\x87\x89";            // ICON_LC_SHIRT
inline constexpr char kPanelRightClose[] = "\xee\x90\xb6"; // ICON_LC_PANEL_RIGHT_CLOSE
inline constexpr char kPanelRightOpen[] = "\xee\x90\xb8";  // ICON_LC_PANEL_RIGHT_OPEN
inline constexpr char kCollection[] = "\xee\x97\xbf";      // ICON_LC_FOLDER_CODE

inline const char *CircleHelp() { return Choose(kCircleHelp, "?"); }
inline const char *Ellipsis() { return Choose(kEllipsis, "..."); }
inline const char *EditorId() { return Choose(kEditorId, "ID"); }
inline const char *FileCode() { return Choose(kFileCode, "File"); }
inline const char *FormId() { return Choose(kFormId, "#"); }
inline const char *GripVertical() { return Choose(kGripVertical, "::"); }
inline const char *Identifier() { return Choose(kIdentifier, "Link"); }
inline const char *PanelRightClose() { return Choose(kPanelRightClose, "<"); }
inline const char *PanelRightOpen() { return Choose(kPanelRightOpen, ">"); }
inline const char *Plugin() { return Choose(kPlugin, "Mod"); }
inline const char *Slot() { return Choose(kSlot, "Slot"); }
inline const char *Trash() { return Choose(kTrash, "X"); }

inline std::string CollectionLabel() { return Prefix(kCollection, "Kit"); }
inline std::string FavoritePrefix() { return Prefix(kFavorite, "*"); }
inline std::string DeleteLabel(const std::string_view a_text) {
  return Label(kTrash, "X", a_text);
}
} // namespace sfs::ui::icons
