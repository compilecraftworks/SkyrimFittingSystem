#pragma once

namespace sfs::ui::workbench {
inline constexpr char kVariantItemPayloadType[] = "SFS_VARIANT_ITEM";
inline constexpr char kConditionPayloadType[] = "SFS_CONDITION";
inline constexpr char kIconEllipsis[] = "\xee\x82\xba"; // ICON_LC_ELLIPSIS
inline constexpr float kWorkbenchRowGapY = 20.0f;
inline constexpr float kWorkbenchOverrideGapY = 5.0f;
inline constexpr char kWorkbenchOverflowPopupId[] =
    "##workbench-toolbar-overflow";
} // namespace sfs::ui::workbench
