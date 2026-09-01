# Skyrim Fitting System runtime menu API

Available in Skyrim Fitting System v1.3.0 and later.

The current v1.5.2 runtime module is `SFSCore.dll`. The DLL was renamed in
v1.4.0; the four export names and their C ABI remain unchanged from v1.3.x. A
consumer that supports both release lines can try `SFSCore.dll` first and then
the legacy `SkyrimFittingSystem.dll`.

```cpp
extern "C" __declspec(dllexport) bool SkyrimFittingSystem_Open();
extern "C" __declspec(dllexport) void SkyrimFittingSystem_Close();
extern "C" __declspec(dllexport) bool SkyrimFittingSystem_IsMenuOpen();
extern "C" __declspec(dllexport) void SkyrimFittingSystem_SetHotkeyEnabled(bool enabled);
```

The API is a stable C ABI and exposes no C++ standard-library types or
exceptions.

`SkyrimFittingSystem_Open` returns `true` when the menu is already active or an
open request was accepted. It returns `false` before SFS UI initialization or
before game data is loaded. Open requests are handled on SFS's render/UI thread,
so `SkyrimFittingSystem_IsMenuOpen` may remain `false` until the following frame.

`SkyrimFittingSystem_Close` is idempotent. `SkyrimFittingSystem_IsMenuOpen`
returns `true` throughout Opening, Open, and Closing, and becomes `false` only
after the menu is fully Closed.

`SkyrimFittingSystem_SetHotkeyEnabled(false)` temporarily disables SFS's native
menu shortcut, whether it is the default F6 binding or a user-defined key and
modifier. It does not close an already-open menu, alter the saved shortcut, or
affect `Open`, `Close`, or `IsMenuOpen`. Passing `true` restores the saved SFS
binding. The state is runtime-only: it is not written to SFS JSON, the save, or
the SKSE co-save, and it always starts as `true` when the plugin is loaded in a
new game process. The menu's shortcut-capture UI remains available while the
menu is open.

## GetProcAddress example

```cpp
#include <Windows.h>

using OpenSfs = bool (*)();
using CloseSfs = void (*)();
using IsSfsOpen = bool (*)();
using SetSfsHotkeyEnabled = void (*)(bool);

auto module = GetModuleHandleW(L"SFSCore.dll");
if (!module) {
  module = GetModuleHandleW(L"SkyrimFittingSystem.dll"); // v1.3.x
}
if (!module) {
  return; // SFS is not loaded.
}
auto openSfs = reinterpret_cast<OpenSfs>(
    GetProcAddress(module, "SkyrimFittingSystem_Open"));
auto closeSfs = reinterpret_cast<CloseSfs>(
    GetProcAddress(module, "SkyrimFittingSystem_Close"));
auto isSfsOpen = reinterpret_cast<IsSfsOpen>(
    GetProcAddress(module, "SkyrimFittingSystem_IsMenuOpen"));
auto setSfsHotkeyEnabled = reinterpret_cast<SetSfsHotkeyEnabled>(
    GetProcAddress(module, "SkyrimFittingSystem_SetHotkeyEnabled"));

if (openSfs && closeSfs && isSfsOpen && setSfsHotkeyEnabled) {
  setSfsHotkeyEnabled(false); // Runtime only; the saved SFS binding is unchanged.
  openSfs();
}
```

Resolve the functions only after SKSE has loaded SFS. Do not call
`LoadLibrary` or `LoadLibraryEx` to load SFS from an external mod; use the
module handle for the instance that SKSE already loaded.

A small consumer header with matching function-pointer types is provided at
`extras/SkyrimFittingSystemAPI.h`.
