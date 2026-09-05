#pragma once

#include <cstdint>

namespace sfs::ui::menu_interaction {

struct CatalogReleaseState {
  bool hasSelection{false};
  bool mouseReleased{false};
  bool rowHandledRelease{false};
  bool mouseDragPastThreshold{false};
  bool pointerOverWindow{false};
  bool pointerOverItem{false};
};

[[nodiscard]] inline constexpr bool
ShouldClearCatalogSelection(const CatalogReleaseState &a_state) noexcept {
  // A catalog selection is cleared only by a genuine click on unused SFS
  // window space. Scrollbar grabs, scrollbar-track clicks, row controls and
  // drag releases must retain the current selection and scroll position.
  return a_state.hasSelection && a_state.mouseReleased &&
         !a_state.rowHandledRelease && !a_state.mouseDragPastThreshold &&
         a_state.pointerOverWindow && !a_state.pointerOverItem;
}

enum class CameraZoomUpdate : std::uint8_t {
  Refresh,
  SnapCurrentToTarget,
  RestoreSaved,
};

struct CameraZoomValues {
  float target{0.0f};
  float current{0.0f};
};

[[nodiscard]] inline constexpr CameraZoomValues ResolveCameraZoomUpdate(
    const CameraZoomUpdate a_update, const CameraZoomValues a_live,
    const CameraZoomValues a_saved = {}) noexcept {
  switch (a_update) {
  case CameraZoomUpdate::SnapCurrentToTarget:
    return {a_live.target, a_live.target};
  case CameraZoomUpdate::RestoreSaved:
    return a_saved;
  case CameraZoomUpdate::Refresh:
  default:
    return a_live;
  }
}

} // namespace sfs::ui::menu_interaction
