#pragma once

#include <REL/Version.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace sfs::runtime {
enum class FlatRuntimeProfile : std::uint8_t {
  SkyrimSE1597,
  SkyrimAEPre629,
  SkyrimAEPost629,
};

struct HookLayout {
  FlatRuntimeProfile profile{};
  std::string_view name;
  bool isAE{false};

  std::uint64_t inputPollRelocationID{0};
  std::uintptr_t inputPollCallOffset{0};
  std::uint64_t registerClassRelocationID{0};
  std::uintptr_t registerClassCallOffset{0};
  std::uint64_t d3dInitRelocationID{0};
  std::uintptr_t d3dInitCallOffset{0};
  std::uint64_t presentRelocationID{0};
  std::uintptr_t presentCallOffset{0};

  std::uint64_t armorUpdateRelocationID{0};
  std::uintptr_t davInitWornOffset{0};
  std::uintptr_t vanillaArmorOffset{0};
  std::uint64_t wornMaskRelocationID{0};
  std::uintptr_t wornMaskCallOffset{0};
  std::uint64_t customSkinRelocationID{0};
  std::uintptr_t customSkinCallOffset{0};
};

inline constexpr HookLayout kSkyrimSE1597Layout{
    .profile = FlatRuntimeProfile::SkyrimSE1597,
    .name = "Skyrim SE 1.5.97",
    .isAE = false,
    .inputPollRelocationID = 67315,
    .inputPollCallOffset = 0x7B,
    .registerClassRelocationID = 75591,
    .registerClassCallOffset = 0x8E,
    .d3dInitRelocationID = 75595,
    .d3dInitCallOffset = 0x50,
    .presentRelocationID = 75461,
    .presentCallOffset = 0x9,
    .armorUpdateRelocationID = 24232,
    .davInitWornOffset = 0x2F0,
    .vanillaArmorOffset = 0x302,
    .wornMaskRelocationID = 24220,
    .wornMaskCallOffset = 0x7C,
    .customSkinRelocationID = 24231,
    .customSkinCallOffset = 0x81,
};

inline constexpr HookLayout kSkyrimAEPre629Layout{
    .profile = FlatRuntimeProfile::SkyrimAEPre629,
    .name = "Skyrim AE 1.6.317-1.6.353",
    .isAE = true,
    .inputPollRelocationID = 68617,
    .inputPollCallOffset = 0x7B,
    .registerClassRelocationID = 77226,
    .registerClassCallOffset = 0x15C,
    .d3dInitRelocationID = 77226,
    .d3dInitCallOffset = 0x2BC,
    .presentRelocationID = 77246,
    .presentCallOffset = 0x9,
    .armorUpdateRelocationID = 24736,
    .davInitWornOffset = 0x2F0,
    .vanillaArmorOffset = 0x302,
    .wornMaskRelocationID = 24724,
    .wornMaskCallOffset = 0x80,
    .customSkinRelocationID = 24725,
    .customSkinCallOffset = 0x1EF,
};

inline constexpr HookLayout kSkyrimAEPost629Layout{
    .profile = FlatRuntimeProfile::SkyrimAEPost629,
    .name = "Skyrim AE 1.6.629+",
    .isAE = true,
    .inputPollRelocationID = 68617,
    .inputPollCallOffset = 0x7B,
    .registerClassRelocationID = 77226,
    .registerClassCallOffset = 0x15C,
    .d3dInitRelocationID = 77226,
    .d3dInitCallOffset = 0x2BC,
    .presentRelocationID = 77246,
    .presentCallOffset = 0x9,
    .armorUpdateRelocationID = 24736,
    .davInitWornOffset = 0x2F0,
    .vanillaArmorOffset = 0x302,
    .wornMaskRelocationID = 24724,
    .wornMaskCallOffset = 0x80,
    .customSkinRelocationID = 24725,
    .customSkinCallOffset = 0x1EF,
};

[[nodiscard]] constexpr bool IsSupportedAEVersion(const REL::Version a_version) {
  if (a_version.major() != 1 || a_version.minor() != 6 ||
      a_version.build() != 0) {
    return false;
  }

  switch (a_version.patch()) {
  case 317:
  case 318:
  case 323:
  case 342:
  case 353:
  case 629:
  case 640:
  case 659:
  case 678:
  case 1130:
  case 1170:
  case 1179:
    return true;
  default:
    return false;
  }
}

[[nodiscard]] constexpr std::optional<HookLayout>
ResolveHookLayout(const REL::Version a_version) {
  if (a_version == REL::Version{1, 5, 97, 0}) {
    return kSkyrimSE1597Layout;
  }
  if (!IsSupportedAEVersion(a_version)) {
    return std::nullopt;
  }
  return a_version.patch() < 629 ? kSkyrimAEPre629Layout
                                 : kSkyrimAEPost629Layout;
}

// Papyrus VM and native-function slots are identical across the supported
// SE/AE flat runtimes. Keep them centralized and covered by boundary tests
// instead of duplicating raw indices throughout the plugin.
inline constexpr std::size_t kPapyrusGetScriptObjectTypeVtableIndex = 0x09;
inline constexpr std::size_t kPapyrusNativeCallVtableIndex = 0x0F;
inline constexpr std::size_t kPapyrusNativeFunctionVtableEntryCount = 0x17;
inline constexpr std::size_t kPapyrusBindNativeMethodVtableIndex = 0x18;

// BSLightingShader's renderer binding bracket is shared by every verified
// SE/AE profile. Keep these slots centralized so a future runtime layout
// change fails review instead of silently spreading raw indices.
inline constexpr std::size_t kBSLightingShaderSetupGeometryVtableIndex = 0x06;
inline constexpr std::size_t kBSLightingShaderRestoreGeometryVtableIndex = 0x07;

// ID3D11DeviceContext inherits IUnknown (0-2) and ID3D11DeviceChild (3-6).
// These are stable Direct3D 11 COM ABI slots, not Skyrim runtime layouts.
inline constexpr std::size_t kD3D11DeviceContextDrawIndexedVtableIndex = 0x0C;
inline constexpr std::size_t kD3D11DeviceContextDrawVtableIndex = 0x0D;
} // namespace sfs::runtime
