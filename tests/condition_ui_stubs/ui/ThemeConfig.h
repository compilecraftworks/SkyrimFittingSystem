#pragma once
#include <imgui.h>
namespace sfs {
class ThemeConfig {
public:
  static ThemeConfig *GetSingleton() { static ThemeConfig value; return &value; }
  ImU32 GetColorU32(const char *) const { return IM_COL32_WHITE; }
};
}
