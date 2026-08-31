# Dynamic Footprints Displayed Footwear API

SFS exposes this optional read-only ABI for Dynamic Footprints. The separate
SFS Dynamic Footprints Compatibility Patch uses this ABI for its verified
Dynamic Footprints SKSE BASE v3.0 build. Resolve the functions with
`GetModuleHandleW(L"SFSCore.dll")` and `GetProcAddress`; never statically link
or force-load SFS. Resolve after SKSE `kPostPostLoad` (or retry at
`kDataLoaded`), not during `SKSEPlugin_Load`, because the two DLLs may load in
either filename order.

```cpp
extern "C" __declspec(dllexport)
std::uint32_t SkyrimFittingSystem_GetDynamicFootprintsHostAPIVersion();

extern "C" __declspec(dllexport)
std::uint32_t SkyrimFittingSystem_GetDisplayedFootwearFormID(
    std::uint32_t actorFormID);
```

## ABI v1 behavior

- `SkyrimFittingSystem_GetDynamicFootprintsHostAPIVersion()` returns `1` when
  this ABI is available.
- `SkyrimFittingSystem_GetDisplayedFootwearFormID(actorFormID)` returns the
  FormID of the actor-local, currently visible SFS registered appearance that
  owns Feet slot 37.
- It returns `0` when no visible SFS registered footwear owns Feet. In that
  case the consumer must keep its normal actual-equipment lookup unchanged.
- Calls must run on Skyrim's game thread. The function is read-only: it does
  not equip or unequip anything, mutate armor keywords, write SFS workbench
  state, or serialize data.

The result already follows SFS's final actor-local visibility state. It is
therefore valid with native, DAV, and DAVE rendering backends and remains
independent of external strip/redress transactions.

## Consumer integration boundary

Dynamic Footprints should call this function immediately before its existing
footwear classification. When it receives a non-zero FormID that resolves to
an ARMO, it may classify that ARMO's armor type, heel data, and footprint
keywords. Otherwise it must use the currently worn footwear exactly as it did
before.

SFS deliberately does not hook `Actor::GetWornArmor()` or alter actual armor
keywords to force this result. Those global changes would make gameplay and
other native plugins observe a cosmetic item as inventory equipment. The
consumer-side fallback above keeps the boundary local and safe.
