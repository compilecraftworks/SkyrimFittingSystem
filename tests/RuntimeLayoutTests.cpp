#include "runtime/RuntimeLayouts.h"
#include "ui/MenuCameraProjection.h"

#include <cmath>
#include <iostream>

namespace {
int g_failures = 0;

void Expect(const bool a_condition, const char *a_message) {
  if (a_condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAILED: " << a_message << '\n';
}
} // namespace

int main() {
  using sfs::runtime::FlatRuntimeProfile;
  using sfs::runtime::ResolveHookLayout;

  // Every requested SKSE-supported AE release, not only family endpoints.
  // This checks routing, not the contents of each game's executable. Epic
  // 1.6.678 has an existing profile entry but no official SKSE support.
  for (const auto patch : {317, 318, 323, 342, 353, 629, 640, 659,
                           1130, 1170, 1179}) {
    const auto layout = ResolveHookLayout(
        REL::Version{1, 6, static_cast<std::uint16_t>(patch), 0});
    Expect(layout && layout->isAE &&
               layout->profile == (patch < 629
                   ? FlatRuntimeProfile::SkyrimAEPre629
                   : FlatRuntimeProfile::SkyrimAEPost629),
           "every requested SKSE-supported AE release must select its profile");
    Expect(layout && layout->inputPollRelocationID == 68617 &&
               layout->registerClassRelocationID == 77226 &&
               layout->d3dInitRelocationID == 77226 &&
               layout->presentRelocationID == 77246 &&
               layout->armorUpdateRelocationID == 24736 &&
               layout->wornMaskRelocationID == 24724 &&
               layout->customSkinRelocationID == 24725,
           "every AE profile must retain the audited relocation IDs");
  }

  const auto se = ResolveHookLayout(REL::Version{1, 5, 97, 0});
  Expect(se.has_value(), "Skyrim SE 1.5.97 must be supported");
  Expect(se && se->profile == FlatRuntimeProfile::SkyrimSE1597,
         "Skyrim SE 1.5.97 must select the SE profile");
  Expect(se && !se->isAE && se->registerClassCallOffset == 0x8E &&
             se->customSkinCallOffset == 0x81,
         "Skyrim SE hook offsets must remain at their verified values");

  const auto aeFirst = ResolveHookLayout(REL::Version{1, 6, 317, 0});
  const auto aeLegacy = ResolveHookLayout(REL::Version{1, 6, 353, 0});
  Expect(aeFirst &&
             aeFirst->profile == FlatRuntimeProfile::SkyrimAEPre629,
         "first supported AE runtime must select the pre-629 profile");
  Expect(aeLegacy &&
             aeLegacy->profile == FlatRuntimeProfile::SkyrimAEPre629,
         "Skyrim AE 1.6.353 must select the pre-629 profile");

  const auto aeBoundary = ResolveHookLayout(REL::Version{1, 6, 629, 0});
  const auto aeSteam = ResolveHookLayout(REL::Version{1, 6, 1170, 0});
  const auto aeGog = ResolveHookLayout(REL::Version{1, 6, 1179, 0});
  Expect(aeBoundary &&
             aeBoundary->profile == FlatRuntimeProfile::SkyrimAEPost629,
         "Skyrim AE 1.6.629 must select the post-629 profile");
  Expect(aeSteam && aeSteam->isAE &&
             aeSteam->registerClassCallOffset == 0x15C &&
             aeSteam->customSkinCallOffset == 0x1EF,
         "Skyrim AE 1.6.1170 hook offsets must remain verified");
  Expect(aeGog && aeGog->profile == FlatRuntimeProfile::SkyrimAEPost629,
         "Skyrim AE 1.6.1179 must select the post-629 profile");

  Expect(!ResolveHookLayout(REL::Version{1, 5, 96, 0}),
         "unverified SE runtimes must fail closed");
  Expect(!ResolveHookLayout(REL::Version{1, 6, 628, 0}),
         "unverified AE boundary runtimes must fail closed");
  Expect(!ResolveHookLayout(REL::Version{1, 6, 641, 0}),
         "unknown AE runtimes must fail closed");
  Expect(!ResolveHookLayout(REL::Version{1, 4, 15, 0}),
         "Skyrim VR must not select a flat runtime profile");
  Expect(!ResolveHookLayout(REL::Version{1, 7, 99, 0}),
         "future unverified runtimes must fail closed");

  Expect(sfs::runtime::kPapyrusGetScriptObjectTypeVtableIndex == 0x09,
         "Papyrus GetScriptObjectType vtable contract changed");
  Expect(sfs::runtime::kPapyrusNativeCallVtableIndex == 0x0F,
         "Papyrus native Call vtable contract changed");
  Expect(sfs::runtime::kPapyrusNativeFunctionVtableEntryCount == 0x17,
         "Papyrus native function vtable extent changed");
  Expect(sfs::runtime::kPapyrusBindNativeMethodVtableIndex == 0x18,
         "Papyrus BindNativeMethod vtable contract changed");
  Expect(sfs::runtime::kBSLightingShaderSetupGeometryVtableIndex == 0x06,
         "BSLighting SetupGeometry vtable contract changed");
  Expect(sfs::runtime::kBSLightingShaderRestoreGeometryVtableIndex == 0x07,
         "BSLighting RestoreGeometry vtable contract changed");
  Expect(sfs::runtime::kD3D11DeviceContextDrawIndexedVtableIndex == 0x0C,
         "D3D11 DrawIndexed COM vtable contract changed");
  Expect(sfs::runtime::kD3D11DeviceContextDrawVtableIndex == 0x0D,
         "D3D11 Draw COM vtable contract changed");

  RE::NiFrustum perspective{-0.916331f, 0.916331f, 0.515436f,
                            -0.515436f, 0.1f, 10000.0f, false};
  const auto originalAspect = perspective.fTop / perspective.fRight;
  Expect(sfs::ui::camera_projection::SetHorizontalFov(perspective, 70.0f),
         "perspective menu camera FOV must be adjustable");
  Expect(std::abs(perspective.fRight - 0.700208f) < 0.00001f &&
             std::abs(perspective.fLeft + 0.700208f) < 0.00001f,
         "menu camera frustum must encode horizontal FOV 70");
  Expect(std::abs(perspective.fTop / perspective.fRight - originalAspect) <
             0.00001f,
         "menu camera FOV scaling must preserve viewport aspect ratio");
  Expect(perspective.fNear == 0.1f && perspective.fFar == 10000.0f,
         "menu camera FOV scaling must not change near or far planes");

  const auto onceApplied = perspective;
  Expect(sfs::ui::camera_projection::SetHorizontalFov(perspective, 70.0f) &&
             std::abs(perspective.fRight - onceApplied.fRight) < 0.000001f &&
             std::abs(perspective.fTop - onceApplied.fTop) < 0.000001f,
         "reapplying menu camera FOV must be stable");

  RE::NiFrustum orthographic{-1.0f, 1.0f, 1.0f, -1.0f,
                             0.1f, 10000.0f, true};
  Expect(!sfs::ui::camera_projection::SetHorizontalFov(orthographic, 70.0f) &&
             orthographic.fRight == 1.0f,
         "orthographic camera projection must fail closed");

  RE::NiFrustum invalid{0.0f, 0.0f, 1.0f, -1.0f,
                        0.1f, 10000.0f, false};
  Expect(!sfs::ui::camera_projection::SetHorizontalFov(invalid, 70.0f),
         "degenerate perspective frustum must fail closed");

  if (g_failures != 0) {
    std::cerr << g_failures << " runtime layout test(s) failed\n";
    return 1;
  }
  std::cout << "Runtime layout boundary tests passed\n";
  return 0;
}
