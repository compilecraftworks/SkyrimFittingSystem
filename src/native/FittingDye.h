#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
namespace RE { class Actor; }
namespace SKSE { class SerializationInterface; }

namespace sfs::native::dye {
struct RenderedShapeInfo {
  bool firstPerson{false};
  std::string shapeName;
  std::string geometryType;
  std::string shaderType;
  std::string diffuseTexture;
  std::string scenePath;
  std::uintptr_t geometryAddress{0};
  std::uintptr_t shaderPropertyAddress{0};
  std::uintptr_t materialAddress{0};
  std::uintptr_t diffuseRendererTextureAddress{0};
  std::uintptr_t diffuseShaderResourceAddress{0};
  bool likelyBodyOverlay{false};
};

struct WorldTintColor {
  float red{1.0f};
  float green{1.0f};
  float blue{1.0f};
};

// Read-only actor-local scene inspection. This intentionally retains no
// scene pointers and never edits a geometry, property, material, or texture.
[[nodiscard]] std::vector<RenderedShapeInfo> ScanLoadedActorShapes(RE::Actor *a_actor);

// Shared workbench/runtime gate for actual appearance components. This keeps
// body, overlay, collision, and helper geometry out of both the popup and
// durable restoration, including records saved by an older build.
[[nodiscard]] bool
IsDyeableAppearanceComponent(const RenderedShapeInfo &a_shape);

// The hook is dormant until ConfigureWorldTints succeeds. It replaces the
// selected shape's source SRV only inside its SetupGeometry/RestoreGeometry
// bracket, leaving game materials, appearances and save data untouched.
void InstallWorldTintHook();
// Applies one logical appearance component to every verified renderer shape
// supplied by the caller (for example its 3rd- and 1st-person counterparts).
// The operation is fail-closed: no target is armed if any shape is unsupported.
bool ConfigureWorldTints(ID3D11Device *a_device, ID3D11DeviceContext *a_context,
                         RE::FormID a_actorFormID,
                         RE::FormID a_appearanceFormID,
                         const std::vector<RenderedShapeInfo> &a_shapes,
                         float a_red, float a_green, float a_blue,
                         std::string &a_status, std::uint64_t a_restoreTicket = 0);
// A short visual locator for the currently selected UI row.  It is kept
// separate from saved/armed world tints and automatically expires.
bool PreviewWorldTint(ID3D11Device *a_device, ID3D11DeviceContext *a_context,
                      RE::FormID a_actorFormID,
                      RE::FormID a_appearanceFormID,
                      const RenderedShapeInfo &a_shape,
                      std::string &a_status);
void ClearWorldTintPreview();
// Restores only the logical component supplied by the UI, including its
// verified first-person counterpart. Other actors and appearance components
// keep their independent tint selections.
void ClearWorldTints(RE::FormID a_actorFormID,
                     RE::FormID a_appearanceFormID,
                     const RenderedShapeInfo &a_component);
void ClearWorldTint();
[[nodiscard]] std::optional<WorldTintColor>
GetWorldTintColor(const RenderedShapeInfo &a_shape);
// Durable state is intentionally keyed by form IDs and stable scene strings,
// never renderer/material addresses.  The renderer targets remain transient.
void SaveWorldTintForComponent(RE::FormID a_actorFormID,
                               RE::FormID a_appearanceFormID,
                               const RenderedShapeInfo &a_shape,
                               WorldTintColor a_color);
void RemoveSavedWorldTintForComponent(RE::FormID a_actorFormID,
                                      RE::FormID a_appearanceFormID,
                                      const RenderedShapeInfo &a_shape);
[[nodiscard]] std::optional<WorldTintColor>
GetSavedWorldTintColor(RE::FormID a_actorFormID,
                       RE::FormID a_appearanceFormID,
                       const RenderedShapeInfo &a_shape);
void SerializeSavedWorldTints(SKSE::SerializationInterface *a_skse);
void DeserializeSavedWorldTints(SKSE::SerializationInterface *a_skse);
void RevertSavedWorldTints();
// A bounded, actor-local task scheduled after SFS refreshes.  It only restores
// saved components whose exact rendered scene identity exists in that actor's
// newly built 3D; it never performs a world scan or name-only guess.
void QueueSavedWorldTintRestore(RE::Actor *a_actor);
// Real unload/delete boundaries only; retain durable colors for reattachment.
void ReleaseActorSceneResources(RE::FormID a_actorFormID);
void RestoreActorSceneResources(RE::FormID a_actorFormID);
} // namespace sfs::native::dye
