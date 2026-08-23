#include "SkyrimFittingSystemAPI.h"

#include <Windows.h>

#include <iostream>

int main() {
  // The real consumer runs inside Skyrim where SKSE has already loaded SFS.
  // DONT_RESOLVE_DLL_REFERENCES lets this standalone ABI smoke test inspect the
  // same PE exports without initializing the Skyrim/CommonLib runtime.
  const auto loaded = ::LoadLibraryExW(SkyrimFittingSystem_ModuleName, nullptr,
                                       DONT_RESOLVE_DLL_REFERENCES);
  auto module = ::GetModuleHandleW(SkyrimFittingSystem_ModuleName);
  if (loaded == nullptr) {
    std::cerr << "Failed to load SFSCore.dll\n";
    return 1;
  }
  // DONT_RESOLVE_DLL_REFERENCES is not guaranteed to add the image to the
  // process module list. In Skyrim this GetModuleHandleW call is the primary
  // path; the standalone test uses the inspection handle when Windows omits it.
  if (module == nullptr) {
    module = loaded;
  }

  const auto openSfs = reinterpret_cast<SkyrimFittingSystem_Open_t>(
      ::GetProcAddress(module, SkyrimFittingSystem_Open_Name));
  const auto closeSfs = reinterpret_cast<SkyrimFittingSystem_Close_t>(
      ::GetProcAddress(module, SkyrimFittingSystem_Close_Name));
  const auto isSfsOpen = reinterpret_cast<SkyrimFittingSystem_IsMenuOpen_t>(
      ::GetProcAddress(module, SkyrimFittingSystem_IsMenuOpen_Name));
  const auto setHotkeyEnabled =
      reinterpret_cast<SkyrimFittingSystem_SetHotkeyEnabled_t>(
          ::GetProcAddress(module,
                           SkyrimFittingSystem_SetHotkeyEnabled_Name));
  if (openSfs == nullptr || closeSfs == nullptr || isSfsOpen == nullptr ||
      setHotkeyEnabled == nullptr) {
    std::cerr << "Failed to resolve the SFS runtime menu API\n";
    return 2;
  }

  if (openSfs() || isSfsOpen()) {
    std::cerr << "Pre-initialization API state was not closed\n";
    return 3;
  }
  closeSfs();
  closeSfs();
  setHotkeyEnabled(false);
  if (openSfs() || isSfsOpen()) {
    std::cerr << "Hotkey state changed the pre-initialization menu API\n";
    return 4;
  }
  setHotkeyEnabled(true);

  std::cout << "All four SFS runtime menu API exports resolved; "
               "pre-init state safe\n";
  ::FreeLibrary(loaded);
  return 0;
}
