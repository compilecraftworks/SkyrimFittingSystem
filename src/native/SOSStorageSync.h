#pragma once

namespace sfs::native {
// Reads SOS_RevealingArmors and SOS_ConcealingArmors directly through the
// PapyrusUtil StorageUtil API. No SFS quest, ESP, or bridge script is needed.
void RequestSOSStorageSync();
void CancelSOSStorageSync();
} // namespace sfs::native
