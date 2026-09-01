#pragma once

namespace sfs::native::refresh_rules {

enum class Backend {
  None,
  DaveApi,
  DaveEmptyEquipment3D,
  DavFallback3D,
  NativeEmptyEquipment3D,
  NativeEquipment
};

struct Input {
  bool daveApiReady{false};
  bool davLoaded{false};
  bool emptyEquipment3DRefresh{false};
  bool davFallback3DRefresh{false};
  bool nativeProcessAvailable{false};
};

struct Plan {
  Backend backend{Backend::None};
  // Every successful rebuild must share these actor-local follow-ups. BodyMorph
  // itself remains on RaceMenu's attachment/public update callbacks, whose
  // actor activity is established before dispatching this plan.
  bool queuePoseSync{false};
  bool queueDyeRestore{false};
  bool queueHighHeelSync{false};

  [[nodiscard]] constexpr bool HasBackendWork() const noexcept {
    return backend != Backend::None;
  }
};

[[nodiscard]] inline constexpr Plan BuildPlan(const Input &a_input) noexcept {
  Backend backend = Backend::None;
  if (a_input.daveApiReady) {
    backend = a_input.emptyEquipment3DRefresh
                  ? Backend::DaveEmptyEquipment3D
                  : Backend::DaveApi;
  } else if (a_input.davLoaded) {
    backend = a_input.davFallback3DRefresh ? Backend::DavFallback3D
                                           : Backend::None;
  } else if (a_input.emptyEquipment3DRefresh) {
    backend = Backend::NativeEmptyEquipment3D;
  } else if (a_input.nativeProcessAvailable) {
    backend = Backend::NativeEquipment;
  }
  const bool work = backend != Backend::None;
  return {.backend = backend,
          .queuePoseSync = work,
          .queueDyeRestore = work,
          .queueHighHeelSync = work};
}

} // namespace sfs::native::refresh_rules
