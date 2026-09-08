SFS - Dynamic Footprints Compatibility Patch v1.6.0

Requirements
- Skyrim Fitting System v1.4.4 or later (SFSCore.dll)
- Dynamic Footprints SKSE BASE v3.0 only

Install this after Dynamic Footprints.  It adds SFS_DynamicFootprintsPatch.dll;
it does not replace SFSCore.dll or NMN_DynamicFootprints.dll.

When SFS manages an actor, Dynamic Footprints reads the final slot-37 result:
visible registered footwear first, otherwise visible actual footwear, or an
explicit barefoot result when SFS hid every feet-slot source.  Actors not
managed by SFS keep Dynamic Footprints' normal actual-equipment lookup.

v1.6.0 keeps the original host API v1 contract and adds an optional query that
distinguishes an unmanaged actor from an SFS-managed explicit barefoot result.
It remains backward compatible with older SFS hosts through the original v1
function, without treating a missing extension as an installation failure.

Safety boundary
- No global Actor::GetWornArmor hook is installed.
- The patch redirects exactly one feet-slot lookup compiled inside the verified
  Dynamic Footprints v3.0 DLL.
- The DLL SHA-256 and the expected call/function bytes are checked before any
  code is changed.  A different Dynamic Footprints build stays unpatched.
- The patch allocates its branch trampoline from the exact verified call site,
  avoiding the rel32 range edge case reported by CommonLib's trampoline.
- The patch only reads SFS display state.  It never changes equipment,
  inventories, keywords, saved workbench data, strip/redress state, or actors.
- It is a narrow SFS footwear-query bridge, not a fix for Dynamic Footprints'
  own footprint rules, meshes, textures, scripts, saves, or mod conflicts.

Supported Dynamic Footprints DLL SHA-256
9DF616D77A1F90F38FBEF5B446E4A95B38EC43F372496EFDD557022C46E57B71
