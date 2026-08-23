#include "kit_generator/Localization.h"

#include "ui/Localization.h"

namespace sfs::kit_generator {
Localization &Localization::Get() {
  static Localization instance;
  return instance;
}

std::string Localization::Text(const std::string_view a_key) const {
  std::string fullKey{"kit_generator."};
  fullKey.append(a_key);
  return std::string(ui::Localization::GetSingleton()->Get(fullKey));
}
} // namespace sfs::kit_generator
