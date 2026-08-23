#pragma once

namespace sfs::native::smoothcam {

// SmoothCam is optional.  These functions use its public SKSE messaging API
// only when the interface is present; SFS has no load-time DLL dependency.
void RegisterInterfaceListener();
void RequestInterface();

// Returns false only when an active SmoothCam instance explicitly refuses to
// hand camera control to SFS.
[[nodiscard]] bool AcquireCameraControl();
void ReleaseCameraControl();

} // namespace sfs::native::smoothcam
