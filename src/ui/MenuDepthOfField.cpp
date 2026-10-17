#include "ui/MenuDepthOfField.h"
#include "ui/DepthOfFieldLease.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <Windows.h>
#include <Psapi.h>

#ifndef EXCLUSIVE_SKYRIM_FLAT
#error SFS menu depth of field requires the SE/AE-only CommonLib layout
#endif

namespace sfs::ui::depth_of_field {
namespace {
std::atomic_bool g_menuOpen{false};
using IsActive = bool (*)(RE::ImageSpaceEffectDepthOfField *);
std::atomic<IsActive> g_originalIsActive{nullptr};

bool IsDepthOfFieldActive(RE::ImageSpaceEffectDepthOfField *effect) {
  const auto original = g_originalIsActive.load(std::memory_order_acquire);
  const bool active = original && original(effect);
  // Do not overwrite saved INI values, effect objects, camera parameters or
  // another mod's requested state. Closing SFS immediately restores dispatch.
  return active && !g_menuOpen.load(std::memory_order_acquire);
}

// Optional ENB SDK 1000/1001 exported C ABI, not a new DLL dependency. Audited
// declarations: ENBSeriesSDK.h at ee16b0b5bd317d91ed73a283c4f301328cbe208d
// (clayne/skyrim-ao-toggler). Only the boolean parameter and callbacks are used.
struct EnbParameter {
  unsigned char data[16]{};
  std::uint32_t size{};
  std::int32_t type{};
};
static_assert(sizeof(EnbParameter) == 24);
using EnbParameterCall = bool (*)(const char *, const char *, const char *, EnbParameter *);
using EnbCallback = void (WINAPI *)(std::int32_t);
EnbParameterCall g_enbGet = nullptr, g_enbSet = nullptr;
FrameLease g_enbFrame;

std::optional<bool> ReadEnbDepthOfField() {
  EnbParameter value;
  if (!g_enbGet("enbseries.ini", "EFFECT", "EnableDepthOfField", &value) ||
      value.type != 4 || value.size != sizeof(std::int32_t)) { return {}; }
  std::int32_t enabled{};
  std::memcpy(&enabled, value.data, sizeof(enabled));
  return enabled != 0;
}

bool WriteEnbDepthOfField(bool enabled) {
  EnbParameter value;
  value.size = sizeof(std::int32_t);
  value.type = 4;
  const std::int32_t data = enabled ? 1 : 0;
  std::memcpy(value.data, &data, sizeof(data));
  return g_enbSet("enbseries.ini", "EFFECT", "EnableDepthOfField", &value);
}

void WINAPI EnbFrameCallback(std::int32_t type) {
  switch (type) {
  case 2: // BeginFrame
    // Retry a failed restore before acquiring the next frame.
    g_enbFrame.End(ReadEnbDepthOfField, WriteEnbDepthOfField);
    g_enbFrame.Begin(g_menuOpen.load(std::memory_order_acquire),
                     ReadEnbDepthOfField, WriteEnbDepthOfField);
    break;
  case 1: // EndFrame: never leave the user's setting disabled between frames.
  case 3: // PreSave: never save SFS's temporary value into a preset.
  case 6: // OnExit
  case 7: // PreReset
    g_enbFrame.End(ReadEnbDepthOfField, WriteEnbDepthOfField);
    break;
  case 4: // PostLoad: freshly loaded values replace the old preset.
    g_enbFrame.PresetLoaded();
    break;
  default:
    break;
  }
}

void ConnectEnb() {
  // ENB can be renamed/chained. Enumerate loaded modules once, never per frame.
  DWORD bytes{};
  std::vector<HMODULE> modules(128);
  if (!K32EnumProcessModules(GetCurrentProcess(), modules.data(),
                            static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &bytes)) { return; }
  if (bytes > modules.size() * sizeof(HMODULE)) {
    modules.resize(bytes / sizeof(HMODULE));
    if (!K32EnumProcessModules(GetCurrentProcess(), modules.data(), bytes, &bytes)) { return; }
  }
  const auto count = (std::min)(modules.size(), static_cast<std::size_t>(bytes / sizeof(HMODULE)));
  for (std::size_t i = 0; i < count; ++i) {
    const auto module = modules[i];
    const auto version = reinterpret_cast<long (*)()>(GetProcAddress(module, "ENBGetSDKVersion"));
    if (!version) { continue; }
    const auto sdk = version();
    if (sdk < 1000 || sdk >= 2000) { continue; }
    const auto get = reinterpret_cast<EnbParameterCall>(GetProcAddress(module, "ENBGetParameter"));
    const auto set = reinterpret_cast<EnbParameterCall>(GetProcAddress(module, "ENBSetParameter"));
    const auto subscribe = reinterpret_cast<void (*)(EnbCallback)>(GetProcAddress(module, "ENBSetCallbackFunction"));
    if (!get || !set || !subscribe) { continue; }
    g_enbGet = get;
    g_enbSet = set;
    subscribe(EnbFrameCallback);
    logger::info("SFS menu depth-of-field suppression connected to ENB SDK {}", sdk);
    break;
  }
}
} // namespace

void Install() {
  static std::once_flag installed;
  std::call_once(installed, [] {
    // CommonLib's SE/AE flat ImageSpaceEffect vtable: IsActive is slot 6 in
    // both supported families. No runtime-sized effect-array traversal.
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_ImageSpaceEffectDepthOfField[0]};
    auto *slot = reinterpret_cast<IsActive *>(table.address()) + 6;
    const auto original = *slot;
    const auto replacement = &IsDepthOfFieldActive;
    g_originalIsActive.store(original, std::memory_order_release);
    if (!original || !REL::safe_write(reinterpret_cast<std::uintptr_t>(slot),
            &replacement, sizeof(replacement), &original, sizeof(original))) {
      g_originalIsActive.store(nullptr, std::memory_order_release);
      logger::warn("SFS menu depth-of-field hook could not be installed");
    }
    ConnectEnb();
  });
}

void SetMenuOpen(bool a_open) {
  g_menuOpen.store(a_open, std::memory_order_release);
}
} // namespace sfs::ui::depth_of_field
