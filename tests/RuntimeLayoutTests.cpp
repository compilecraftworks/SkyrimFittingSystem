#include "runtime/RuntimeLayouts.h"

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

  if (g_failures != 0) {
    std::cerr << g_failures << " runtime layout test(s) failed\n";
    return 1;
  }
  std::cout << "Runtime layout boundary tests passed\n";
  return 0;
}

