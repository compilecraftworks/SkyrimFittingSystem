#include "native/FittingDye.h"
#include "ArmorUtils.h"
#include "runtime/RuntimeLayouts.h"
#include "ui/Menu.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <d3dcompiler.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <wrl/client.h>

namespace sfs::native::dye {
namespace {
[[nodiscard]] std::string RttiName(const RE::NiObject *a_object) {
  if (!a_object || !a_object->GetRTTI()) {
    return "Unknown";
  }
  return a_object->GetRTTI()->GetName();
}

[[nodiscard]] std::string ObjectName(const RE::NiAVObject *a_object) {
  return a_object && !a_object->name.empty() ? a_object->name.c_str()
                                              : "<unnamed>";
}

[[nodiscard]] bool IsLikelyBodyOverlay(const std::string_view a_shapeName,
                                       std::string a_texture) {
  std::ranges::transform(a_texture, a_texture.begin(), [](const char a_char) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(a_char)));
  });
  return a_shapeName.find("[Ovl") != std::string_view::npos ||
         a_texture.find("\\overlays\\") != std::string::npos ||
         a_texture.find("\\spank\\") != std::string::npos;
}

void CollectShapes(RE::NiAVObject *a_object, const bool a_firstPerson,
                   std::unordered_set<RE::NiAVObject *> &a_visited,
                   std::vector<RenderedShapeInfo> &a_shapes,
                   const std::string_view a_parentPath) {
  if (!a_object || !a_visited.insert(a_object).second) {
    return;
  }

  if (auto *geometry = a_object->AsGeometry()) {
    auto &runtime = geometry->GetGeometryRuntimeData();
    auto *shader = runtime.shaderProperty.get();
    RE::NiSourceTexture *texture = shader ? shader->GetBaseTexture() : nullptr;
    const auto shapeName = ObjectName(a_object);
    const std::string diffuseTexture = texture && !texture->name.empty()
                                           ? texture->name.c_str()
                                           : "<none>";
    a_shapes.push_back({
        .firstPerson = a_firstPerson,
        .shapeName = shapeName,
        .geometryType = RttiName(geometry),
        .shaderType = RttiName(shader),
        .diffuseTexture = diffuseTexture,
        .scenePath = std::string(a_parentPath) + "/" + shapeName,
        .geometryAddress = reinterpret_cast<std::uintptr_t>(geometry),
        .shaderPropertyAddress = reinterpret_cast<std::uintptr_t>(shader),
        .materialAddress = shader ? reinterpret_cast<std::uintptr_t>(shader->GetBaseMaterial()) : 0,
        .diffuseRendererTextureAddress = texture && texture->rendererTexture
                                            ? reinterpret_cast<std::uintptr_t>(texture->rendererTexture)
                                            : 0,
        .diffuseShaderResourceAddress = texture && texture->rendererTexture
                                            ? reinterpret_cast<std::uintptr_t>(texture->rendererTexture->resourceView)
                                            : 0,
        .likelyBodyOverlay = IsLikelyBodyOverlay(shapeName, diffuseTexture),
    });
  }

  if (auto *node = a_object->AsNode()) {
    const auto path = std::string(a_parentPath) + "/" + ObjectName(a_object);
    for (const auto &child : node->GetChildren()) {
      CollectShapes(child.get(), a_firstPerson, a_visited, a_shapes, path);
    }
  }
}
} // namespace

std::vector<RenderedShapeInfo> ScanLoadedActorShapes(RE::Actor *a_actor) {
  std::vector<RenderedShapeInfo> shapes;
  if (!a_actor || !a_actor->Is3DLoaded()) {
    return shapes;
  }

  for (const bool firstPerson : {false, true}) {
    std::unordered_set<RE::NiAVObject *> visited;
    CollectShapes(a_actor->Get3D(firstPerson), firstPerson, visited, shapes,
                  firstPerson ? "1st-person" : "3rd-person");
  }
  return shapes;
}

bool IsDyeableAppearanceComponent(const RenderedShapeInfo &a_shape) {
  if (a_shape.likelyBodyOverlay) {
    return false;
  }

  const auto lowerAscii = [](std::string a_value) {
    std::ranges::transform(a_value, a_value.begin(),
                           [](const unsigned char a_character) {
                             return static_cast<char>(
                                 std::tolower(a_character));
                           });
    return a_value;
  };
  const auto normalizedShapeName = lowerAscii(a_shape.shapeName);
  const auto separator = a_shape.diffuseTexture.find_last_of("\\/");
  const auto normalizedDiffuseFilename = lowerAscii(
      separator == std::string::npos
          ? a_shape.diffuseTexture
          : a_shape.diffuseTexture.substr(separator + 1));

  const bool characterBaseGeometry =
      normalizedShapeName.starts_with("3ba") ||
      normalizedShapeName == "3bbb" ||
      normalizedShapeName.starts_with("3bbb_") ||
      normalizedShapeName.starts_with("cbbe") ||
      normalizedShapeName.starts_with("virtual") ||
      normalizedShapeName.find("collision") != std::string::npos ||
      normalizedShapeName == "body" || normalizedShapeName == "hands" ||
      normalizedShapeName == "feet" || normalizedShapeName == "face" ||
      normalizedShapeName == "head";
  const bool characterBaseDiffuse =
      normalizedDiffuseFilename.starts_with("femalebody");
  if (characterBaseGeometry || characterBaseDiffuse) {
    return false;
  }

  return a_shape.shaderType == "BSLightingShaderProperty" &&
         a_shape.geometryAddress != 0 &&
         a_shape.shaderPropertyAddress != 0 &&
         a_shape.diffuseRendererTextureAddress != 0 &&
         a_shape.diffuseShaderResourceAddress != 0;
}

namespace {
using Microsoft::WRL::ComPtr;

struct WorldTintTarget {
  RE::FormID actorFormID{0};
  RE::FormID appearanceFormID{0};
  std::uintptr_t geometryAddress{0};
  std::uintptr_t shaderPropertyAddress{0};
  std::uintptr_t rendererTextureAddress{0};
  std::uintptr_t sourceViewAddress{0};
  std::string componentShapeName;
  std::string componentDiffuseTexture;
  std::string componentScenePath;
  WorldTintColor color{};
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11ShaderResourceView> tintedView;
};

struct ActiveRenderPass {
  RE::BSRenderPass *pass{nullptr};
  ID3D11DeviceContext *context{nullptr};
  ComPtr<ID3D11ShaderResourceView> originalView;
  ComPtr<ID3D11ShaderResourceView> tintedView;
};

std::mutex g_worldTintMutex;
std::unordered_map<std::uintptr_t, WorldTintTarget> g_worldTintTargets;

// This state deliberately contains only durable game identifiers and stable
// renderer names. Renderer/material addresses are process-local and must
// never cross a save/load boundary.
struct SavedWorldTintComponent {
  std::string shapeName;
  std::string diffuseTexture;
  std::string scenePath;
  WorldTintColor color{};
};

struct SavedWorldTintRestoreItem {
  RE::FormID appearanceFormID{0};
  SavedWorldTintComponent component;
};

using SavedWorldTintComponents = std::vector<SavedWorldTintComponent>;
using SavedWorldTintAppearances =
    std::unordered_map<RE::FormID, SavedWorldTintComponents>;

constexpr std::uint32_t kSavedWorldTintRecordType = 'DYES';
constexpr std::uint32_t kSavedWorldTintRecordVersion = 1;
std::mutex g_savedWorldTintMutex;
std::unordered_map<RE::FormID, SavedWorldTintAppearances> g_savedWorldTints;
std::mutex g_savedWorldTintRestoreQueueMutex;
std::unordered_set<RE::FormID> g_queuedSavedWorldTintRestores;
std::atomic<std::uint64_t> g_savedWorldTintRestoreGeneration{0};

[[nodiscard]] ComPtr<ID3D11DeviceContext> SnapshotHookedContext();

[[nodiscard]] bool IsDyeableRendererShape(const RenderedShapeInfo &a_shape) {
  return IsDyeableAppearanceComponent(a_shape);
}

[[nodiscard]] bool HasSameRuntimeBinding(
    const RenderedShapeInfo &a_left, const RenderedShapeInfo &a_right) {
  return a_left.firstPerson == a_right.firstPerson &&
         a_left.geometryAddress == a_right.geometryAddress &&
         a_left.shaderPropertyAddress == a_right.shaderPropertyAddress &&
         a_left.diffuseRendererTextureAddress ==
             a_right.diffuseRendererTextureAddress &&
         a_left.diffuseShaderResourceAddress ==
             a_right.diffuseShaderResourceAddress;
}

[[nodiscard]] bool IsRegisteredAppearanceForActor(
    const RE::FormID a_actorFormID,
    const RE::FormID a_appearanceFormID) {
  if (a_actorFormID == 0 || a_appearanceFormID == 0) {
    return false;
  }
  auto &variantWorkbench = sfs::Menu::GetSingleton()->GetWorkbench();
  auto stateLock = variantWorkbench.AcquireStateLock();
  const auto *player = RE::PlayerCharacter::GetSingleton();
  const auto playerFormID = player != nullptr ? player->GetFormID() : 0;
  return std::ranges::any_of(variantWorkbench.GetRows(), [&](const auto &a_row) {
    const bool ownedByActor =
        a_row.ownerActorFormID == a_actorFormID ||
        (a_row.ownerActorFormID == 0 && a_actorFormID == playerFormID);
    return ownedByActor &&
           std::ranges::any_of(a_row.overrides, [&](const auto &a_item) {
             return a_item.formID == a_appearanceFormID;
           });
  });
}

[[nodiscard]] bool SceneBelongsToAppearance(
    const RenderedShapeInfo &a_shape,
    const RE::FormID a_appearanceFormID) {
  std::unordered_set<RE::FormID> visited;
  const auto belongs = [&](const auto &a_self,
                           const RE::TESObjectARMO *a_armor) -> bool {
    if (a_armor == nullptr ||
        !visited.insert(a_armor->GetFormID()).second) {
      return false;
    }
    if (a_shape.scenePath.find(armor::FormatFormID(a_armor->GetFormID())) !=
        std::string::npos) {
      return true;
    }
    for (const auto *addon : a_armor->armorAddons) {
      if (addon != nullptr &&
          a_shape.scenePath.find(armor::FormatFormID(addon->GetFormID())) !=
              std::string::npos) {
        return true;
      }
    }
    return a_self(a_self, a_armor->templateArmor);
  };
  return belongs(belongs, RE::TESForm::LookupByID<RE::TESObjectARMO>(
                              a_appearanceFormID));
}

[[nodiscard]] bool IsSavedTintComponentValid(
    const SavedWorldTintComponent &a_component) {
  const auto validChannel = [](const float a_channel) {
    return std::isfinite(a_channel) && a_channel >= 0.0f &&
           a_channel <= 1.0f;
  };
  return !a_component.shapeName.empty() &&
         !a_component.diffuseTexture.empty() &&
         a_component.scenePath.starts_with("3rd-person/") &&
         validChannel(a_component.color.red) &&
         validChannel(a_component.color.green) &&
         validChannel(a_component.color.blue);
}

[[nodiscard]] bool MatchesSavedTintComponent(
    const RenderedShapeInfo &a_shape,
    const SavedWorldTintComponent &a_component) {
  // Exact scene identity is deliberate.  A matching texture/name on another
  // outfit, body mesh, or NPC is not sufficient authority to recolor it.
  return !a_shape.firstPerson && IsDyeableRendererShape(a_shape) &&
         a_shape.shapeName == a_component.shapeName &&
         a_shape.diffuseTexture == a_component.diffuseTexture &&
         a_shape.scenePath == a_component.scenePath;
}

[[nodiscard]] bool HasSavedWorldTintForActor(const RE::FormID a_actorFormID) {
  std::scoped_lock lock(g_savedWorldTintMutex);
  const auto actor = g_savedWorldTints.find(a_actorFormID);
  return actor != g_savedWorldTints.end() && !actor->second.empty();
}

void ClearRuntimeWorldTintsForActor(const RE::FormID a_actorFormID) {
  std::scoped_lock lock(g_worldTintMutex);
  std::erase_if(g_worldTintTargets,
                [a_actorFormID](const auto &a_entry) {
                  return a_entry.second.actorFormID == a_actorFormID;
                });
}

[[nodiscard]] std::vector<SavedWorldTintRestoreItem>
SnapshotSavedWorldTintsForActor(const RE::FormID a_actorFormID) {
  std::vector<SavedWorldTintRestoreItem> result;
  std::scoped_lock lock(g_savedWorldTintMutex);
  const auto actor = g_savedWorldTints.find(a_actorFormID);
  if (actor == g_savedWorldTints.end()) {
    return result;
  }
  for (const auto &[appearanceFormID, components] : actor->second) {
    for (const auto &component : components) {
      result.push_back({appearanceFormID, component});
    }
  }
  return result;
}

void RestoreSavedWorldTintsForActor(RE::Actor *a_actor) {
  if (!a_actor || !a_actor->Is3DLoaded()) {
    return;
  }
  const auto savedComponents =
      SnapshotSavedWorldTintsForActor(a_actor->GetFormID());
  if (savedComponents.empty()) {
    return;
  }
  const auto shapes = ScanLoadedActorShapes(a_actor);
  for (const auto &saved : savedComponents) {
    if (!IsRegisteredAppearanceForActor(a_actor->GetFormID(),
                                        saved.appearanceFormID)) {
      continue;
    }
    const auto &component = saved.component;
    std::vector<RenderedShapeInfo> matchedShapes;
    for (const auto &shape : shapes) {
      if (MatchesSavedTintComponent(shape, component)) {
        matchedShapes.push_back(shape);
      }
    }
    // A duplicate exact identity is ambiguous. Fail closed instead of
    // guessing which mesh should receive a saved component color.
    if (matchedShapes.size() != 1) {
      if (matchedShapes.size() > 1) {
        logger::warn("Fitting Dye skipped ambiguous saved component for actor {:08X}: {}",
                     a_actor->GetFormID(), component.shapeName);
      }
      continue;
    }
    const auto thirdPersonShape = matchedShapes.front();
    for (const auto &shape : shapes) {
      // First-person counterparts are not independently stored or listed;
      // they are only linked to this exact third-person component by name.
      if (shape.firstPerson && IsDyeableRendererShape(shape) &&
          shape.shapeName == thirdPersonShape.shapeName &&
          SceneBelongsToAppearance(shape, saved.appearanceFormID)) {
        matchedShapes.push_back(shape);
      }
    }
    auto *source = reinterpret_cast<ID3D11ShaderResourceView *>(
        thirdPersonShape.diffuseShaderResourceAddress);
    if (!source) {
      continue;
    }
    auto context = SnapshotHookedContext();
    if (!context) {
      ComPtr<ID3D11Device> sourceDevice;
      source->GetDevice(sourceDevice.GetAddressOf());
      if (!sourceDevice) {
        continue;
      }
      sourceDevice->GetImmediateContext(context.GetAddressOf());
    }
    ComPtr<ID3D11Device> device;
    if (context) {
      context->GetDevice(device.GetAddressOf());
    }
    if (!device || !context) {
      continue;
    }
    std::string status;
    if (!ConfigureWorldTints(device.Get(), context.Get(),
                             a_actor->GetFormID(), saved.appearanceFormID,
                             matchedShapes,
                             component.color.red, component.color.green,
                             component.color.blue, status)) {
      logger::warn("Fitting Dye could not restore saved component for actor {:08X}: {}",
                   a_actor->GetFormID(), status);
    }
  }
}

void FinishQueuedSavedWorldTintRestore(const RE::FormID a_actorFormID,
                                       const std::uint64_t a_generation) {
  std::scoped_lock lock(g_savedWorldTintRestoreQueueMutex);
  if (g_savedWorldTintRestoreGeneration.load(std::memory_order_acquire) ==
      a_generation) {
    g_queuedSavedWorldTintRestores.erase(a_actorFormID);
  }
}

void QueueSavedWorldTintRestoreTask(const RE::FormID a_actorFormID,
                                    const std::uint64_t a_generation,
                                    const std::uint8_t a_remainingFrames) {
  if (g_savedWorldTintRestoreGeneration.load(std::memory_order_acquire) !=
      a_generation) {
    return;
  }
  auto *taskInterface = SKSE::GetTaskInterface();
  if (!taskInterface) {
    FinishQueuedSavedWorldTintRestore(a_actorFormID, a_generation);
    return;
  }
  taskInterface->AddTask([a_actorFormID, a_generation, a_remainingFrames] {
    if (g_savedWorldTintRestoreGeneration.load(std::memory_order_acquire) !=
        a_generation) {
      return;
    }
    auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
    if (actor && actor->Is3DLoaded()) {
      RestoreSavedWorldTintsForActor(actor);
      FinishQueuedSavedWorldTintRestore(a_actorFormID, a_generation);
      return;
    }
    if (a_remainingFrames > 0) {
      QueueSavedWorldTintRestoreTask(a_actorFormID, a_generation,
                                     a_remainingFrames - 1);
      return;
    }
    FinishQueuedSavedWorldTintRestore(a_actorFormID, a_generation);
  });
}

struct WorldTintPreview {
  WorldTintTarget target;
  std::chrono::steady_clock::time_point beganAt;
  std::chrono::steady_clock::time_point expiresAt;
};
std::optional<WorldTintPreview> g_worldTintPreview;
thread_local std::vector<ActiveRenderPass> g_activeRenderPasses;

using SetupGeometryFn = void (*)(RE::BSShader *, RE::BSRenderPass *, std::uint32_t);
using RestoreGeometryFn = void (*)(RE::BSShader *, RE::BSRenderPass *, std::uint32_t);
using DrawIndexedFn = void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *, UINT,
                                                 UINT, INT);
using DrawFn = void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *, UINT, UINT);
std::array<SetupGeometryFn, 3> g_originalSetupGeometry{};
std::array<RestoreGeometryFn, 3> g_originalRestoreGeometry{};
std::atomic<DrawIndexedFn> g_originalDrawIndexed{nullptr};
std::atomic<DrawFn> g_originalDraw{nullptr};
ID3D11DeviceContext *g_hookedContext{nullptr};
std::mutex g_contextHookMutex;

ComPtr<ID3D11DeviceContext> SnapshotHookedContext() {
  std::scoped_lock lock(g_contextHookMutex);
  ComPtr<ID3D11DeviceContext> context;
  if (g_hookedContext) {
    context = g_hookedContext;
  }
  return context;
}

[[nodiscard]] bool CompileShader(const char *a_source, const char *a_entry,
                                 const char *a_target, ComPtr<ID3DBlob> &a_blob,
                                 std::string &a_error) {
  ComPtr<ID3DBlob> errors;
  const auto result = D3DCompile(a_source, std::strlen(a_source), "SFS Fitting Dye",
                                 nullptr, nullptr, a_entry, a_target,
                                 D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                                 a_blob.GetAddressOf(), errors.GetAddressOf());
  if (FAILED(result)) {
    a_error = errors ? static_cast<const char *>(errors->GetBufferPointer())
                     : "D3DCompile failed";
    return false;
  }
  return true;
}

[[nodiscard]] bool CreateTintedView(ID3D11Device *a_device,
                                    ID3D11DeviceContext *a_context,
                                    ID3D11ShaderResourceView *a_sourceView,
                                    const float a_red, const float a_green,
                                    const float a_blue,
                                    ComPtr<ID3D11ShaderResourceView> &a_output,
                                    std::string &a_error) {
  if (!a_device || !a_context || !a_sourceView) {
    a_error = "The selected diffuse GPU resource is unavailable.";
    return false;
  }

  ComPtr<ID3D11Resource> sourceResource;
  a_sourceView->GetResource(sourceResource.GetAddressOf());
  ComPtr<ID3D11Texture2D> sourceTexture;
  if (!sourceResource || FAILED(sourceResource.As(&sourceTexture))) {
    a_error = "The selected diffuse resource is not a 2D texture.";
    return false;
  }
  D3D11_TEXTURE2D_DESC sourceDesc{};
  sourceTexture->GetDesc(&sourceDesc);
  if (sourceDesc.Width == 0 || sourceDesc.Height == 0 || sourceDesc.ArraySize != 1 ||
      sourceDesc.SampleDesc.Count != 1) {
    a_error = "The selected diffuse texture has an unsupported layout.";
    return false;
  }

  D3D11_TEXTURE2D_DESC outputDesc{};
  outputDesc.Width = sourceDesc.Width;
  outputDesc.Height = sourceDesc.Height;
  outputDesc.MipLevels = 0;
  outputDesc.ArraySize = 1;
  outputDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  outputDesc.SampleDesc.Count = 1;
  outputDesc.Usage = D3D11_USAGE_DEFAULT;
  outputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  outputDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

  ComPtr<ID3D11Texture2D> outputTexture;
  ComPtr<ID3D11RenderTargetView> outputRtv;
  ComPtr<ID3D11ShaderResourceView> outputSrv;
  if (FAILED(a_device->CreateTexture2D(&outputDesc, nullptr, outputTexture.GetAddressOf())) ||
      FAILED(a_device->CreateRenderTargetView(outputTexture.Get(), nullptr, outputRtv.GetAddressOf())) ||
      FAILED(a_device->CreateShaderResourceView(outputTexture.Get(), nullptr, outputSrv.GetAddressOf()))) {
    a_error = "Could not allocate the private tint texture.";
    return false;
  }

  static constexpr char vertexSource[] = R"(
struct Out { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
Out VS(uint id : SV_VertexID) {
  Out o;
  const float2 positions[3] = { float2(-1, -1), float2(-1, 3), float2(3, -1) };
  const float2 uvs[3] = { float2(0, 1), float2(0, -1), float2(2, 1) };
  o.position = float4(positions[id], 0, 1); o.uv = uvs[id]; return o;
})";
  static constexpr char pixelSource[] = R"(
Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);
cbuffer Tint : register(b0) { float3 tint; float unused; };
float4 PS(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
  float4 color = sourceTexture.Sample(sourceSampler, uv);
  return float4(color.rgb * tint, color.a);
})";
  ComPtr<ID3DBlob> vertexBlob;
  ComPtr<ID3DBlob> pixelBlob;
  if (!CompileShader(vertexSource, "VS", "vs_5_0", vertexBlob, a_error) ||
      !CompileShader(pixelSource, "PS", "ps_5_0", pixelBlob, a_error)) {
    return false;
  }
  ComPtr<ID3D11VertexShader> vertexShader;
  ComPtr<ID3D11PixelShader> pixelShader;
  if (FAILED(a_device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr, vertexShader.GetAddressOf())) ||
      FAILED(a_device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr, pixelShader.GetAddressOf()))) {
    a_error = "Could not create the private tint shaders.";
    return false;
  }
  D3D11_BUFFER_DESC constantDesc{};
  constantDesc.ByteWidth = 16;
  constantDesc.Usage = D3D11_USAGE_DEFAULT;
  constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  const float tintData[4]{a_red, a_green, a_blue, 0.0f};
  D3D11_SUBRESOURCE_DATA constantData{};
  constantData.pSysMem = tintData;
  ComPtr<ID3D11Buffer> tintBuffer;
  if (FAILED(a_device->CreateBuffer(&constantDesc, &constantData, tintBuffer.GetAddressOf()))) {
    a_error = "Could not create the tint constants.";
    return false;
  }
  D3D11_SAMPLER_DESC samplerDesc{};
  samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
  samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
  ComPtr<ID3D11SamplerState> sampler;
  if (FAILED(a_device->CreateSamplerState(&samplerDesc, sampler.GetAddressOf()))) {
    a_error = "Could not create the tint sampler.";
    return false;
  }
  D3D11_DEPTH_STENCIL_DESC depthDesc{};
  depthDesc.DepthEnable = FALSE;
  depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
  ComPtr<ID3D11DepthStencilState> noDepth;
  if (FAILED(a_device->CreateDepthStencilState(&depthDesc, noDepth.GetAddressOf()))) {
    a_error = "Could not create the tint depth state.";
    return false;
  }

  std::array<ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> oldRtvs;
  std::array<ID3D11RenderTargetView *, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> oldRtvPointers{};
  ComPtr<ID3D11DepthStencilView> oldDsv;
  for (std::size_t i = 0; i < oldRtvs.size(); ++i) oldRtvPointers[i] = oldRtvs[i].Get();
  a_context->OMGetRenderTargets(static_cast<UINT>(oldRtvs.size()), oldRtvPointers.data(), oldDsv.GetAddressOf());
  for (std::size_t i = 0; i < oldRtvs.size(); ++i) oldRtvs[i].Attach(oldRtvPointers[i]);
  ComPtr<ID3D11BlendState> oldBlend;
  float oldBlendFactor[4]{};
  UINT oldSampleMask{};
  a_context->OMGetBlendState(oldBlend.GetAddressOf(), oldBlendFactor, &oldSampleMask);
  ComPtr<ID3D11DepthStencilState> oldDepth;
  UINT oldStencilRef{};
  a_context->OMGetDepthStencilState(oldDepth.GetAddressOf(), &oldStencilRef);
  ComPtr<ID3D11RasterizerState> oldRasterizer;
  a_context->RSGetState(oldRasterizer.GetAddressOf());
  std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> oldViewports{};
  UINT oldViewportCount = static_cast<UINT>(oldViewports.size());
  a_context->RSGetViewports(&oldViewportCount, oldViewports.data());
  ComPtr<ID3D11InputLayout> oldInputLayout;
  a_context->IAGetInputLayout(oldInputLayout.GetAddressOf());
  D3D11_PRIMITIVE_TOPOLOGY oldTopology{};
  a_context->IAGetPrimitiveTopology(&oldTopology);
  ComPtr<ID3D11VertexShader> oldVertexShader;
  ComPtr<ID3D11PixelShader> oldPixelShader;
  a_context->VSGetShader(oldVertexShader.GetAddressOf(), nullptr, nullptr);
  a_context->PSGetShader(oldPixelShader.GetAddressOf(), nullptr, nullptr);
  ComPtr<ID3D11ShaderResourceView> oldSource;
  ComPtr<ID3D11SamplerState> oldSampler;
  a_context->PSGetShaderResources(0, 1, oldSource.GetAddressOf());
  a_context->PSGetSamplers(0, 1, oldSampler.GetAddressOf());
  ComPtr<ID3D11Buffer> oldConstant;
  a_context->PSGetConstantBuffers(0, 1, oldConstant.GetAddressOf());

  const D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(outputDesc.Width), static_cast<float>(outputDesc.Height), 0.0f, 1.0f};
  ID3D11RenderTargetView *targetRtv = outputRtv.Get();
  ID3D11ShaderResourceView *sourceSrv = a_sourceView;
  ID3D11SamplerState *sourceSampler = sampler.Get();
  ID3D11Buffer *constants = tintBuffer.Get();
  a_context->OMSetRenderTargets(1, &targetRtv, nullptr);
  a_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
  a_context->OMSetDepthStencilState(noDepth.Get(), 0);
  a_context->RSSetState(nullptr);
  a_context->RSSetViewports(1, &viewport);
  a_context->IASetInputLayout(nullptr);
  a_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  a_context->VSSetShader(vertexShader.Get(), nullptr, 0);
  a_context->PSSetShader(pixelShader.Get(), nullptr, 0);
  a_context->PSSetShaderResources(0, 1, &sourceSrv);
  a_context->PSSetSamplers(0, 1, &sourceSampler);
  a_context->PSSetConstantBuffers(0, 1, &constants);
  a_context->Draw(3, 0);
  a_context->GenerateMips(outputSrv.Get());

  for (std::size_t i = 0; i < oldRtvs.size(); ++i) oldRtvPointers[i] = oldRtvs[i].Get();
  a_context->OMSetRenderTargets(static_cast<UINT>(oldRtvs.size()), oldRtvPointers.data(), oldDsv.Get());
  a_context->OMSetBlendState(oldBlend.Get(), oldBlendFactor, oldSampleMask);
  a_context->OMSetDepthStencilState(oldDepth.Get(), oldStencilRef);
  a_context->RSSetState(oldRasterizer.Get());
  a_context->RSSetViewports(oldViewportCount, oldViewports.data());
  a_context->IASetInputLayout(oldInputLayout.Get());
  a_context->IASetPrimitiveTopology(oldTopology);
  a_context->VSSetShader(oldVertexShader.Get(), nullptr, 0);
  a_context->PSSetShader(oldPixelShader.Get(), nullptr, 0);
  ID3D11ShaderResourceView *restoreSource = oldSource.Get();
  ID3D11SamplerState *restoreSampler = oldSampler.Get();
  ID3D11Buffer *restoreConstant = oldConstant.Get();
  a_context->PSSetShaderResources(0, 1, &restoreSource);
  a_context->PSSetSamplers(0, 1, &restoreSampler);
  a_context->PSSetConstantBuffers(0, 1, &restoreConstant);
  a_output = std::move(outputSrv);
  return true;
}

template <class DrawCall>
void DrawWithTintIfSelected(ID3D11DeviceContext *a_context,
                            DrawCall &&a_draw) {
  if (g_activeRenderPasses.empty()) {
    a_draw();
    return;
  }
  const auto &active = g_activeRenderPasses.back();
  if (active.context != a_context || !active.originalView ||
      !active.tintedView) {
    a_draw();
    return;
  }

  std::array<ID3D11ShaderResourceView *,
             D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>
      rawViews{};
  a_context->PSGetShaderResources(
      0, static_cast<UINT>(rawViews.size()), rawViews.data());
  std::vector<std::pair<UINT, ComPtr<ID3D11ShaderResourceView>>> restored;
  for (UINT slot = 0; slot < rawViews.size(); ++slot) {
    ComPtr<ID3D11ShaderResourceView> original;
    original.Attach(rawViews[slot]);
    if (original.Get() != active.originalView.Get()) {
      continue;
    }
    ID3D11ShaderResourceView *tinted = active.tintedView.Get();
    a_context->PSSetShaderResources(slot, 1, &tinted);
    restored.emplace_back(slot, std::move(original));
  }

  a_draw();
  for (const auto &[slot, original] : restored) {
    ID3D11ShaderResourceView *view = original.Get();
    a_context->PSSetShaderResources(slot, 1, &view);
  }
}

void STDMETHODCALLTYPE DrawIndexedHook(ID3D11DeviceContext *a_context,
                                       const UINT a_indexCount,
                                       const UINT a_startIndex,
                                       const INT a_baseVertex) {
  const auto original = g_originalDrawIndexed.load(std::memory_order_acquire);
  if (!original) {
    return;
  }
  DrawWithTintIfSelected(a_context, [&] {
    original(a_context, a_indexCount, a_startIndex, a_baseVertex);
  });
}

void STDMETHODCALLTYPE DrawHook(ID3D11DeviceContext *a_context,
                                const UINT a_vertexCount,
                                const UINT a_startVertex) {
  const auto original = g_originalDraw.load(std::memory_order_acquire);
  if (!original) {
    return;
  }
  DrawWithTintIfSelected(a_context,
                         [&] { original(a_context, a_vertexCount, a_startVertex); });
}

[[nodiscard]] bool InstallDrawHooks(ID3D11DeviceContext *a_context,
                                    std::string &a_status) {
  if (!a_context) {
    a_status = "The game D3D11 context is unavailable.";
    return false;
  }

  std::scoped_lock lock(g_contextHookMutex);
  if (g_hookedContext == a_context &&
      g_originalDrawIndexed.load(std::memory_order_acquire) &&
      g_originalDraw.load(std::memory_order_acquire)) {
    return true;
  }
  if (g_hookedContext) {
    a_status = "The game replaced its D3D11 context after the Dye hook was installed.";
    return false;
  }

  auto *vtable = *reinterpret_cast<std::uintptr_t **>(a_context);
  if (!vtable) {
    a_status = "The game D3D11 context has no vtable.";
    return false;
  }
  auto *drawIndexedSlot = std::addressof(
      vtable[sfs::runtime::kD3D11DeviceContextDrawIndexedVtableIndex]);
  auto *drawSlot = std::addressof(
      vtable[sfs::runtime::kD3D11DeviceContextDrawVtableIndex]);
  const auto originalDrawIndexed = *drawIndexedSlot;
  const auto originalDraw = *drawSlot;
  const auto replacementDrawIndexed =
      reinterpret_cast<std::uintptr_t>(DrawIndexedHook);
  const auto replacementDraw = reinterpret_cast<std::uintptr_t>(DrawHook);
  if (originalDrawIndexed == replacementDrawIndexed ||
      originalDraw == replacementDraw) {
    a_status = "The D3D11 draw hook is already owned by Fitting Dye.";
    return false;
  }
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(drawIndexedSlot),
                       std::addressof(replacementDrawIndexed),
                       sizeof(replacementDrawIndexed),
                       std::addressof(originalDrawIndexed),
                       sizeof(originalDrawIndexed))) {
    a_status = "The D3D11 draw vtable changed before Dye could hook it.";
    return false;
  }
  if (!REL::safe_write(reinterpret_cast<std::uintptr_t>(drawSlot),
                       std::addressof(replacementDraw), sizeof(replacementDraw),
                       std::addressof(originalDraw), sizeof(originalDraw))) {
    REL::safe_write(reinterpret_cast<std::uintptr_t>(drawIndexedSlot),
                    std::addressof(originalDrawIndexed),
                    sizeof(originalDrawIndexed),
                    std::addressof(replacementDrawIndexed),
                    sizeof(replacementDrawIndexed));
    a_status = "The D3D11 Draw hook vtable changed before Dye could hook it.";
    return false;
  }

  g_originalDrawIndexed.store(
      reinterpret_cast<DrawIndexedFn>(originalDrawIndexed),
      std::memory_order_release);
  g_originalDraw.store(reinterpret_cast<DrawFn>(originalDraw),
                       std::memory_order_release);
  g_hookedContext = a_context;
  logger::info("Fitting Dye: installed guarded D3D11 draw hooks");
  return true;
}

void SetupGeometryHook(const std::size_t a_vtableIndex, RE::BSShader *a_shader,
                       RE::BSRenderPass *a_pass, std::uint32_t a_flags) {
  ActiveRenderPass active{.pass = a_pass};
  if (a_pass && a_pass->shaderProperty) {
    std::scoped_lock lock(g_worldTintMutex);
    const auto activateTarget = [&](const WorldTintTarget &worldTint) {
      if (!a_pass->geometry ||
          reinterpret_cast<std::uintptr_t>(a_pass->geometry) !=
              worldTint.geometryAddress) {
        return;
      }
      if (reinterpret_cast<std::uintptr_t>(a_pass->shaderProperty) !=
          worldTint.shaderPropertyAddress) {
        return;
      }
      auto *source = a_pass->shaderProperty->GetBaseTexture();
      auto *rendererTexture = source ? source->rendererTexture : nullptr;
      if (rendererTexture &&
          reinterpret_cast<std::uintptr_t>(rendererTexture) ==
              worldTint.rendererTextureAddress &&
          reinterpret_cast<std::uintptr_t>(rendererTexture->resourceView) ==
              worldTint.sourceViewAddress) {
        active.context = worldTint.context.Get();
        active.originalView = rendererTexture->resourceView;
        active.tintedView = worldTint.tintedView;
      }
    };
    const auto geometryAddress =
        reinterpret_cast<std::uintptr_t>(a_pass->geometry);
    const auto now = std::chrono::steady_clock::now();
    if (g_worldTintPreview && now >= g_worldTintPreview->expiresAt) {
      g_worldTintPreview.reset();
    }
    if (g_worldTintPreview &&
        g_worldTintPreview->target.geometryAddress == geometryAddress) {
      constexpr auto kPreviewHalfPulse = std::chrono::milliseconds(130);
      const auto elapsed = now - g_worldTintPreview->beganAt;
      if ((elapsed / kPreviewHalfPulse) % 2 == 0) {
        activateTarget(g_worldTintPreview->target);
      }
    }
    if (!active.tintedView) {
      const auto target = g_worldTintTargets.find(geometryAddress);
      if (target != g_worldTintTargets.end() && target->second.tintedView) {
        activateTarget(target->second);
      }
    }
  }
  if (a_pass) {
    g_activeRenderPasses.push_back(std::move(active));
  }
  g_originalSetupGeometry[a_vtableIndex](a_shader, a_pass, a_flags);
}

void RestoreGeometryHook(const std::size_t a_vtableIndex, RE::BSShader *a_shader,
                         RE::BSRenderPass *a_pass, std::uint32_t a_flags) {
  g_originalRestoreGeometry[a_vtableIndex](a_shader, a_pass, a_flags);
  const auto active = std::ranges::find_if(
      g_activeRenderPasses | std::views::reverse,
      [a_pass](const ActiveRenderPass &a_active) {
        return a_active.pass == a_pass;
      });
  if (active != g_activeRenderPasses.rend()) {
    g_activeRenderPasses.erase(std::next(active).base());
  }
}

template <std::size_t I>
void SetupGeometryThunk(RE::BSShader *a_shader, RE::BSRenderPass *a_pass, std::uint32_t a_flags) {
  SetupGeometryHook(I, a_shader, a_pass, a_flags);
}

template <std::size_t I>
void RestoreGeometryThunk(RE::BSShader *a_shader, RE::BSRenderPass *a_pass,
                          std::uint32_t a_flags) {
  RestoreGeometryHook(I, a_shader, a_pass, a_flags);
}

} // namespace

void InstallWorldTintHook() {
  static std::once_flag installed;
  std::call_once(installed, [] {
    const auto install = []<std::size_t I>(auto a_setup, auto a_restore) {
      REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_BSLightingShader[I]};
      g_originalSetupGeometry[I] = reinterpret_cast<SetupGeometryFn>(
          vtable.write_vfunc(
              sfs::runtime::kBSLightingShaderSetupGeometryVtableIndex,
              a_setup));
      g_originalRestoreGeometry[I] =
          reinterpret_cast<RestoreGeometryFn>(vtable.write_vfunc(
              sfs::runtime::kBSLightingShaderRestoreGeometryVtableIndex,
              a_restore));
    };
    install.template operator()<0>(SetupGeometryThunk<0>, RestoreGeometryThunk<0>);
    install.template operator()<1>(SetupGeometryThunk<1>, RestoreGeometryThunk<1>);
    install.template operator()<2>(SetupGeometryThunk<2>, RestoreGeometryThunk<2>);
    logger::info("Fitting Dye: installed dormant BSLighting renderer binding hooks (3 variants)");
  });
}

bool ConfigureWorldTints(
    ID3D11Device *a_device, ID3D11DeviceContext *a_context,
    const RE::FormID a_actorFormID,
    const RE::FormID a_appearanceFormID,
    const std::vector<RenderedShapeInfo> &a_shapes, const float a_red,
    const float a_green, const float a_blue, std::string &a_status) {
  if (!IsRegisteredAppearanceForActor(a_actorFormID, a_appearanceFormID)) {
    a_status = "The selected registered appearance is no longer available for this actor.";
    return false;
  }
  const auto validChannel = [](const float a_value) {
    return std::isfinite(a_value) && a_value >= 0.0f && a_value <= 1.0f;
  };
  if (!validChannel(a_red) || !validChannel(a_green) ||
      !validChannel(a_blue)) {
    a_status = "The selected dye color is invalid.";
    return false;
  }
  if (a_shapes.empty()) {
    a_status = "No renderer shape was selected for world tinting.";
    return false;
  }
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
  if (!actor || !actor->Is3DLoaded()) {
    a_status = "The selected actor no longer has loaded 3D.";
    return false;
  }
  const auto currentShapes = ScanLoadedActorShapes(actor);
  const auto &component = a_shapes.front();
  if (component.firstPerson) {
    a_status = "A first-person shape cannot be used as a saved dye component.";
    return false;
  }
  for (std::size_t index = 0; index < a_shapes.size(); ++index) {
    const auto &shape = a_shapes[index];
    if (!IsDyeableRendererShape(shape) ||
        !SceneBelongsToAppearance(shape, a_appearanceFormID) ||
        (index > 0 &&
         (!shape.firstPerson || shape.shapeName != component.shapeName))) {
      a_status =
          "A linked renderer shape is outside the selected appearance component.";
      return false;
    }
    const auto currentMatches = std::ranges::count_if(
        currentShapes, [&](const RenderedShapeInfo &a_current) {
          return HasSameRuntimeBinding(a_current, shape);
        });
    if (currentMatches != 1) {
      a_status = "A linked renderer shape is no longer present in this actor's current 3D.";
      return false;
    }
  }
  if (!InstallDrawHooks(a_context, a_status)) {
    return false;
  }

  std::vector<WorldTintTarget> targets;
  targets.reserve(a_shapes.size());
  for (const auto &shape : a_shapes) {
    ComPtr<ID3D11ShaderResourceView> tintedView;
    if (!CreateTintedView(
            a_device, a_context,
            reinterpret_cast<ID3D11ShaderResourceView *>(
                shape.diffuseShaderResourceAddress),
            a_red, a_green, a_blue, tintedView, a_status)) {
      return false;
    }
    targets.push_back({
        .actorFormID = a_actorFormID,
        .appearanceFormID = a_appearanceFormID,
        .geometryAddress = shape.geometryAddress,
        .shaderPropertyAddress = shape.shaderPropertyAddress,
        .rendererTextureAddress = shape.diffuseRendererTextureAddress,
        .sourceViewAddress = shape.diffuseShaderResourceAddress,
        .componentShapeName = component.shapeName,
        .componentDiffuseTexture = component.diffuseTexture,
        .componentScenePath = component.scenePath,
        .color = {.red = a_red, .green = a_green, .blue = a_blue},
        .context = a_context,
        .tintedView = std::move(tintedView)});
  }
  {
    std::scoped_lock lock(g_worldTintMutex);
    std::erase_if(g_worldTintTargets, [&](const auto &a_entry) {
      const auto &target = a_entry.second;
      return target.actorFormID == a_actorFormID &&
             target.appearanceFormID == a_appearanceFormID &&
             target.componentShapeName == component.shapeName &&
             target.componentDiffuseTexture == component.diffuseTexture &&
             target.componentScenePath == component.scenePath;
    });
    for (auto &target : targets) {
      g_worldTintTargets[target.geometryAddress] = std::move(target);
    }
  }
  a_status = std::format(
      "World tint armed for {} linked renderer shape(s), including any verified first-person counterpart.",
      a_shapes.size());
  return true;
}

bool PreviewWorldTint(ID3D11Device *a_device, ID3D11DeviceContext *a_context,
                      const RE::FormID a_actorFormID,
                      const RE::FormID a_appearanceFormID,
                      const RenderedShapeInfo &a_shape,
                      std::string &a_status) {
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
  if (!actor || !actor->Is3DLoaded() || a_shape.firstPerson ||
      !IsRegisteredAppearanceForActor(a_actorFormID, a_appearanceFormID) ||
      !IsDyeableRendererShape(a_shape) ||
      !SceneBelongsToAppearance(a_shape, a_appearanceFormID)) {
    a_status = "The selected component has no dyeable diffuse renderer binding.";
    return false;
  }
  const auto currentShapes = ScanLoadedActorShapes(actor);
  if (std::ranges::count_if(currentShapes, [&](const auto &a_current) {
        return HasSameRuntimeBinding(a_current, a_shape);
      }) != 1) {
    a_status = "The selected component is no longer present in this actor's current 3D.";
    return false;
  }
  if (!InstallDrawHooks(a_context, a_status)) {
    return false;
  }
  ComPtr<ID3D11ShaderResourceView> previewView;
  // Bright amber deliberately differs from normal dye choices and lasts only
  // long enough to identify the selected geometry in the world.
  if (!CreateTintedView(
          a_device, a_context,
          reinterpret_cast<ID3D11ShaderResourceView *>(
              a_shape.diffuseShaderResourceAddress),
          2.0f, 1.65f, 0.18f, previewView, a_status)) {
    return false;
  }
  const auto now = std::chrono::steady_clock::now();
  {
    std::scoped_lock lock(g_worldTintMutex);
    g_worldTintPreview = {
        .target = {.geometryAddress = a_shape.geometryAddress,
                   .shaderPropertyAddress = a_shape.shaderPropertyAddress,
                   .rendererTextureAddress =
                       a_shape.diffuseRendererTextureAddress,
                   .sourceViewAddress = a_shape.diffuseShaderResourceAddress,
                   .color = {.red = 2.0f, .green = 1.65f, .blue = 0.18f},
                   .context = a_context,
                   .tintedView = std::move(previewView)},
        .beganAt = now,
        .expiresAt = now + std::chrono::milliseconds(1200)};
  }
  a_status = "Selected component is flashing in the world.";
  return true;
}

void ClearWorldTintPreview() {
  std::scoped_lock lock(g_worldTintMutex);
  g_worldTintPreview.reset();
}

void ClearWorldTints(const RE::FormID a_actorFormID,
                     const RE::FormID a_appearanceFormID,
                     const RenderedShapeInfo &a_component) {
  std::scoped_lock lock(g_worldTintMutex);
  std::erase_if(g_worldTintTargets, [&](const auto &a_entry) {
    const auto &target = a_entry.second;
    return target.actorFormID == a_actorFormID &&
           target.appearanceFormID == a_appearanceFormID &&
           target.componentShapeName == a_component.shapeName &&
           target.componentDiffuseTexture == a_component.diffuseTexture &&
           target.componentScenePath == a_component.scenePath;
  });
  g_worldTintPreview.reset();
}

void ClearWorldTint() {
  std::scoped_lock lock(g_worldTintMutex);
  g_worldTintTargets.clear();
  g_worldTintPreview.reset();
}

std::optional<WorldTintColor>
GetWorldTintColor(const RenderedShapeInfo &a_shape) {
  if (a_shape.geometryAddress == 0) {
    return std::nullopt;
  }
  std::scoped_lock lock(g_worldTintMutex);
  const auto target = g_worldTintTargets.find(a_shape.geometryAddress);
  return target != g_worldTintTargets.end() &&
                 target->second.shaderPropertyAddress ==
                     a_shape.shaderPropertyAddress &&
                 target->second.rendererTextureAddress ==
                     a_shape.diffuseRendererTextureAddress &&
                 target->second.sourceViewAddress ==
                     a_shape.diffuseShaderResourceAddress
             ? std::optional<WorldTintColor>{target->second.color}
             : std::nullopt;
}

void SaveWorldTintForComponent(const RE::FormID a_actorFormID,
                               const RE::FormID a_appearanceFormID,
                               const RenderedShapeInfo &a_shape,
                               const WorldTintColor a_color) {
  const SavedWorldTintComponent component{
      .shapeName = a_shape.shapeName,
      .diffuseTexture = a_shape.diffuseTexture,
      .scenePath = a_shape.scenePath,
      .color = a_color,
  };
  if (a_actorFormID == 0 || a_appearanceFormID == 0 ||
      !IsSavedTintComponentValid(component) ||
      !SceneBelongsToAppearance(a_shape, a_appearanceFormID) ||
      !IsRegisteredAppearanceForActor(a_actorFormID, a_appearanceFormID)) {
    logger::warn("Fitting Dye rejected an invalid saved component key");
    return;
  }
  std::scoped_lock lock(g_savedWorldTintMutex);
  auto &components = g_savedWorldTints[a_actorFormID][a_appearanceFormID];
  const auto existing = std::ranges::find_if(
      components, [&](const SavedWorldTintComponent &a_existing) {
        return a_existing.shapeName == component.shapeName &&
               a_existing.diffuseTexture == component.diffuseTexture &&
               a_existing.scenePath == component.scenePath;
      });
  if (existing != components.end()) {
    existing->color = component.color;
  } else {
    components.push_back(component);
  }
}

void RemoveSavedWorldTintForComponent(
    const RE::FormID a_actorFormID, const RE::FormID a_appearanceFormID,
    const RenderedShapeInfo &a_shape) {
  std::scoped_lock lock(g_savedWorldTintMutex);
  const auto actor = g_savedWorldTints.find(a_actorFormID);
  if (actor == g_savedWorldTints.end()) {
    return;
  }
  const auto appearance = actor->second.find(a_appearanceFormID);
  if (appearance == actor->second.end()) {
    return;
  }
  auto &components = appearance->second;
  std::erase_if(components, [&](const SavedWorldTintComponent &a_component) {
    return a_component.shapeName == a_shape.shapeName &&
           a_component.diffuseTexture == a_shape.diffuseTexture &&
           a_component.scenePath == a_shape.scenePath;
  });
  if (components.empty()) {
    actor->second.erase(appearance);
  }
  if (actor->second.empty()) {
    g_savedWorldTints.erase(actor);
  }
}

std::optional<WorldTintColor> GetSavedWorldTintColor(
    const RE::FormID a_actorFormID, const RE::FormID a_appearanceFormID,
    const RenderedShapeInfo &a_shape) {
  std::scoped_lock lock(g_savedWorldTintMutex);
  const auto actor = g_savedWorldTints.find(a_actorFormID);
  if (actor == g_savedWorldTints.end()) {
    return std::nullopt;
  }
  const auto appearance = actor->second.find(a_appearanceFormID);
  if (appearance == actor->second.end()) {
    return std::nullopt;
  }
  const auto component = std::ranges::find_if(
      appearance->second, [&](const SavedWorldTintComponent &a_component) {
        return a_component.shapeName == a_shape.shapeName &&
               a_component.diffuseTexture == a_shape.diffuseTexture &&
               a_component.scenePath == a_shape.scenePath;
      });
  return component != appearance->second.end()
             ? std::optional<WorldTintColor>{component->color}
             : std::nullopt;
}

void SerializeSavedWorldTints(SKSE::SerializationInterface *a_skse) {
  if (!a_skse) {
    return;
  }
  nlohmann::json root;
  root["actors"] = nlohmann::json::array();
  {
    std::scoped_lock lock(g_savedWorldTintMutex);
    std::vector<RE::FormID> actorFormIDs;
    actorFormIDs.reserve(g_savedWorldTints.size());
    for (const auto &[actorFormID, _] : g_savedWorldTints) {
      actorFormIDs.push_back(actorFormID);
    }
    std::ranges::sort(actorFormIDs);
    for (const auto actorFormID : actorFormIDs) {
      const auto &appearances = g_savedWorldTints.at(actorFormID);
      nlohmann::json serializedAppearances = nlohmann::json::array();
      std::vector<RE::FormID> appearanceFormIDs;
      appearanceFormIDs.reserve(appearances.size());
      for (const auto &[appearanceFormID, _] : appearances) {
        appearanceFormIDs.push_back(appearanceFormID);
      }
      std::ranges::sort(appearanceFormIDs);
      for (const auto appearanceFormID : appearanceFormIDs) {
        nlohmann::json serializedComponents = nlohmann::json::array();
        for (const auto &component : appearances.at(appearanceFormID)) {
          if (!IsSavedTintComponentValid(component)) {
            continue;
          }
          serializedComponents.push_back(
              {{"shapeName", component.shapeName},
               {"diffuseTexture", component.diffuseTexture},
               {"scenePath", component.scenePath},
               {"color", {component.color.red, component.color.green,
                           component.color.blue}}});
        }
        if (!serializedComponents.empty()) {
          serializedAppearances.push_back(
              {{"formID", appearanceFormID},
               {"components", std::move(serializedComponents)}});
        }
      }
      if (!serializedAppearances.empty()) {
        root["actors"].push_back(
            {{"formID", actorFormID},
             {"appearances", std::move(serializedAppearances)}});
      }
    }
  }
  const auto payload = root.dump();
  a_skse->WriteRecord(kSavedWorldTintRecordType, kSavedWorldTintRecordVersion,
                      payload.data(), static_cast<std::uint32_t>(payload.size()));
}

void RevertSavedWorldTints() {
  g_savedWorldTintRestoreGeneration.fetch_add(1, std::memory_order_acq_rel);
  {
    std::scoped_lock lock(g_savedWorldTintMutex);
    g_savedWorldTints.clear();
  }
  std::scoped_lock queueLock(g_savedWorldTintRestoreQueueMutex);
  g_queuedSavedWorldTintRestores.clear();
}

void DeserializeSavedWorldTints(SKSE::SerializationInterface *a_skse) {
  RevertSavedWorldTints();
  if (!a_skse) {
    return;
  }
  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    return;
  }
  if (type != kSavedWorldTintRecordType || version != kSavedWorldTintRecordVersion) {
    logger::warn("Skipped unsupported Fitting Dye record type={:X} version={}",
                 type, version);
    return;
  }
  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::warn("Failed to read Fitting Dye saved state");
    return;
  }
  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object() || !root["actors"].is_array()) {
    logger::warn("Failed to parse Fitting Dye saved state");
    return;
  }
  std::unordered_map<RE::FormID, SavedWorldTintAppearances> loaded;
  for (const auto &serializedActor : root["actors"]) {
    if (!serializedActor.is_object()) {
      continue;
    }
    const auto savedActorFormID = serializedActor.value("formID", RE::FormID{0});
    RE::FormID actorFormID = 0;
    if (savedActorFormID == 0 ||
        !a_skse->ResolveFormID(savedActorFormID, actorFormID) ||
        actorFormID == 0 || !RE::TESForm::LookupByID<RE::Actor>(actorFormID) ||
        !serializedActor["appearances"].is_array()) {
      continue;
    }
    for (const auto &serializedAppearance : serializedActor["appearances"]) {
      if (!serializedAppearance.is_object()) {
        continue;
      }
      const auto savedAppearanceFormID =
          serializedAppearance.value("formID", RE::FormID{0});
      RE::FormID appearanceFormID = 0;
      if (savedAppearanceFormID == 0 ||
          !a_skse->ResolveFormID(savedAppearanceFormID, appearanceFormID) ||
          appearanceFormID == 0 ||
          !RE::TESForm::LookupByID<RE::TESObjectARMO>(appearanceFormID) ||
          !IsRegisteredAppearanceForActor(actorFormID, appearanceFormID) ||
          !serializedAppearance["components"].is_array()) {
        continue;
      }
      auto &components = loaded[actorFormID][appearanceFormID];
      for (const auto &serializedComponent : serializedAppearance["components"]) {
        if (!serializedComponent.is_object()) {
          continue;
        }
        const auto color = serializedComponent.value(
            "color", nlohmann::json::array());
        if (!color.is_array() || color.size() != 3 ||
            !color[0].is_number() || !color[1].is_number() ||
            !color[2].is_number()) {
          continue;
        }
        SavedWorldTintComponent component{
            .shapeName = serializedComponent.value("shapeName", ""),
            .diffuseTexture = serializedComponent.value("diffuseTexture", ""),
            .scenePath = serializedComponent.value("scenePath", ""),
            .color = {.red = color[0].get<float>(), .green = color[1].get<float>(),
                      .blue = color[2].get<float>()},
        };
        if (!IsSavedTintComponentValid(component)) {
          continue;
        }
        const auto duplicate = std::ranges::any_of(
            components, [&](const SavedWorldTintComponent &a_existing) {
              return a_existing.shapeName == component.shapeName &&
                     a_existing.diffuseTexture == component.diffuseTexture &&
                     a_existing.scenePath == component.scenePath;
            });
        if (!duplicate) {
          components.push_back(std::move(component));
        }
      }
      if (components.empty()) {
        loaded[actorFormID].erase(appearanceFormID);
      }
    }
    if (loaded[actorFormID].empty()) {
      loaded.erase(actorFormID);
    }
  }
  {
    std::scoped_lock lock(g_savedWorldTintMutex);
    g_savedWorldTints = std::move(loaded);
  }
}

void QueueSavedWorldTintRestore(RE::Actor *a_actor) {
  if (!a_actor) {
    return;
  }
  const auto actorFormID = a_actor->GetFormID();
  if (actorFormID == 0) {
    return;
  }
  // Every backend rebuild invalidates this actor's process-local geometry
  // addresses. Remove only that actor's targets before considering durable
  // restoration; other actors remain armed and isolated.
  ClearRuntimeWorldTintsForActor(actorFormID);
  if (!HasSavedWorldTintForActor(actorFormID)) {
    return;
  }
  const auto generation =
      g_savedWorldTintRestoreGeneration.load(std::memory_order_acquire);
  {
    std::scoped_lock lock(g_savedWorldTintRestoreQueueMutex);
    if (g_savedWorldTintRestoreGeneration.load(std::memory_order_acquire) !=
        generation) {
      return;
    }
    if (!g_queuedSavedWorldTintRestores.insert(actorFormID).second) {
      return;
    }
  }
  // Two actor-local frames cover Update3D/UpdateEquipment handoff without a
  // persistent poll. A scene that still is not ready simply remains neutral.
  QueueSavedWorldTintRestoreTask(actorFormID, generation, 2);
}

} // namespace sfs::native::dye
