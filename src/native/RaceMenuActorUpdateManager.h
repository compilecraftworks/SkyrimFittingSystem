#pragma once

#include <atomic>
#include <cstdint>

namespace RE {
class TESObjectREFR;
class TESObjectARMO;
class TESObjectARMA;
class NiAVObject;
class NiNode;
}

namespace sfs::native::racemenu::abi {

// Stable prefix shared by RaceMenu's independently versioned interfaces.
class IPluginInterface {
public:
  virtual ~IPluginInterface() = default;
  virtual std::uint32_t GetVersion() = 0;
  virtual void Revert() = 0;
};

class IAddonAttachmentInterface {
public:
  // No virtual destructor: RaceMenu's observer ABI starts with OnAttach.
  virtual void OnAttach(RE::TESObjectREFR *, RE::TESObjectARMO *,
                        RE::TESObjectARMA *, RE::NiAVObject *, bool,
                        RE::NiNode *, RE::NiNode *) = 0;
};

// Legacy ActorUpdateManager reports version 0. Its Add*Update helpers are
// NON-virtual, so AddInterface is MSVC x64 slot 3, not slot 11.
// Verified against upstream 87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc,
// skee/ActorUpdateManager.h, and the installed legacy skee64.dll.
class IActorUpdateManagerV0 : public IPluginInterface {
public:
  virtual void AddInterface(IAddonAttachmentInterface *) = 0;
  virtual void RemoveInterface(IAddonAttachmentInterface *) = 0;
  virtual void OnAttach(RE::TESObjectREFR *, RE::TESObjectARMO *,
                        RE::TESObjectARMA *, RE::NiAVObject *, bool,
                        RE::NiNode *, RE::NiNode *) = 0;
};

// Public versions 1/2 share this prefix from skee64/IPluginInterface.h.
// Upstream revision: 9ebcb733e17be695f994cd2e9cc383043446bc02.
// Version 2 appends flush callbacks after RemoveInterface; SFS never calls
// them. Keep every preceding virtual: AddInterface is MSVC x64 slot 11.
class IActorUpdateManagerV1 : public IPluginInterface {
public:
  virtual void AddBodyUpdate(std::uint32_t) = 0;
  virtual void AddTransformUpdate(std::uint32_t) = 0;
  virtual void AddOverlayUpdate(std::uint32_t) = 0;
  virtual void AddNodeOverrideUpdate(std::uint32_t) = 0;
  virtual void AddWeaponOverrideUpdate(std::uint32_t) = 0;
  virtual void AddAddonOverrideUpdate(std::uint32_t) = 0;
  virtual void AddSkinOverrideUpdate(std::uint32_t) = 0;
  virtual void Flush() = 0;
  virtual void AddInterface(IAddonAttachmentInterface *) = 0;
  virtual void RemoveInterface(IAddonAttachmentInterface *) = 0;
};

enum class AttachmentRegistrationStatus : std::uint8_t {
  Registered,
  AlreadyRegistered,
  Unavailable,
  UnsupportedVersion,
};

struct AttachmentRegistrationResult {
  AttachmentRegistrationStatus status;
  std::uint32_t version{0};
};

// The caller must hold the existing initialization mutex. This function does
// not acquire any other lock or access actor/render/save/equipment state.
// Read GetVersion on the stable base BEFORE any version-specific cast/call.
// Publish registration only after AddInterface returns successfully.
[[nodiscard]] AttachmentRegistrationResult RegisterAttachmentObserver(
    IPluginInterface *a_plugin, IAddonAttachmentInterface *a_observer,
    std::atomic_bool &a_registered);

} // namespace sfs::native::racemenu::abi
