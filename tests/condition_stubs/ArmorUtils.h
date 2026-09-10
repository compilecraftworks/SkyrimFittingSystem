#pragma once
#include <RE/Skyrim.h>
namespace sfs::armor {
inline std::string GetEditorID(const RE::TESForm *form) {
  return form ? form->editorID : std::string{};
}
}
