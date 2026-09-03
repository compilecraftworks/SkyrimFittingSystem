#pragma once

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

[[nodiscard]] inline constexpr bool ShouldCommitCharacterCamera(
    const bool a_presentationActive, const bool a_commitPending,
    const bool a_thirdPersonCameraReady) noexcept {
  // MenuHost receives kShow before the engine has finished registering the
  // menu's pause flag/count. Defer the one camera Update until the first normal
  // rendered menu frame, when the active third-person state is stable.
  return a_presentationActive && a_commitPending && a_thirdPersonCameraReady;
}

} // namespace sfs::ui::menu_interaction
