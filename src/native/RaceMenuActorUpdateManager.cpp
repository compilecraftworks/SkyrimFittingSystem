#include "native/RaceMenuActorUpdateManager.h"

namespace sfs::native::racemenu::abi {

AttachmentRegistrationResult RegisterAttachmentObserver(
    IPluginInterface *a_plugin, IAddonAttachmentInterface *a_observer,
    std::atomic_bool &a_registered) {
  if (a_registered.load()) {
    return {AttachmentRegistrationStatus::AlreadyRegistered};
  }
  if (a_plugin == nullptr || a_observer == nullptr) {
    return {AttachmentRegistrationStatus::Unavailable};
  }

  const auto version = a_plugin->GetVersion();
  switch (version) {
  case 0:
    static_cast<IActorUpdateManagerV0 *>(a_plugin)->AddInterface(a_observer);
    break;
  case 1:
  case 2:
    static_cast<IActorUpdateManagerV1 *>(a_plugin)->AddInterface(a_observer);
    break;
  default:
    return {AttachmentRegistrationStatus::UnsupportedVersion, version};
  }

  a_registered.store(true);
  return {AttachmentRegistrationStatus::Registered, version};
}

} // namespace sfs::native::racemenu::abi
