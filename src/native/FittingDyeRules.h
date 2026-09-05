#pragma once

#include <cstdint>
#include <string_view>

namespace sfs::native::dye::rules {
[[nodiscard]] constexpr bool ShouldInspectRendererTintPass(
    const bool a_hasWorldTintTargets,
    const bool a_hasWorldTintPreview) noexcept {
  return a_hasWorldTintTargets || a_hasWorldTintPreview;
}

[[nodiscard]] constexpr bool IsRendererContextMatch(
    const std::uintptr_t a_targetContext,
    const std::uintptr_t a_drawContext) noexcept {
  return a_targetContext != 0 && a_targetContext == a_drawContext;
}

enum class DrawHookInstallAction {
  Install,
  Reuse,
  RejectUnknownOwnership,
  UseExistingChainOrShaderBindingFallback
};

// A D3D11 context replacement can legitimately introduce another vtable.
// Each vtable is chained once. If a later renderer becomes the top-level Draw
// owner, never hook it a second time: it may already call the SFS thunk as its
// downstream target, and re-hooking would create a cycle. Keep the existing
// chain and let the BSLighting pass use its scoped shader-binding fallback when
// the later renderer bypasses downstream hooks. Partial/unknown SFS ownership
// is still rejected because there is no registered downstream target to trust.
[[nodiscard]] constexpr DrawHookInstallAction ResolveDrawHookInstallAction(
    const bool a_registeredVtable, const bool a_drawIndexedOwned,
    const bool a_drawOwned) noexcept {
  if (a_registeredVtable) {
    return a_drawIndexedOwned && a_drawOwned
               ? DrawHookInstallAction::Reuse
               : DrawHookInstallAction::UseExistingChainOrShaderBindingFallback;
  }
  return !a_drawIndexedOwned && !a_drawOwned
             ? DrawHookInstallAction::Install
             : DrawHookInstallAction::RejectUnknownOwnership;
}

// Returns true only for character base-body/helper components that must never
// appear in the Fitting Dye component list. The check is deliberately based
// on the already loaded renderer strings and performs no disk or actor scan.
[[nodiscard]] bool IsCharacterBaseComponent(std::string_view a_shapeName,
                                            std::string_view a_diffuseTexture);
} // namespace sfs::native::dye::rules
