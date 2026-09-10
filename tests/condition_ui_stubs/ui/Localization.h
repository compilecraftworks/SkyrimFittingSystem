#pragma once
namespace sfs::ui {
class Localization {
public:
  static Localization *GetSingleton() { static Localization value; return &value; }
  const char *GetCStr(const char *) const { return "No matches"; }
};
}
