#include "Menu.h"

#include "ArmorUtils.h"
#include "ThemeConfig.h"
#include "native/ExternalEquipmentTransactions.h"
#include "poc/DeviousDevicesHiderPoC.h"
#include "poc/VirtualWornTokenPoC.h"
#include "ui/Localization.h"
#include "workbench/AppearanceSlotProtection.h"

#include <array>
#include <unordered_set>
#include <wincodec.h>
#include <wrl/client.h>

namespace {
using Mode = sfs::workbench::ExternalModStripLinkMode;
using Mappings = sfs::workbench::AutomaticEquipmentSlotMappings;
using Overrides = sfs::workbench::AutomaticEquipmentSlotOverrides;
using Microsoft::WRL::ComPtr;

constexpr auto kStripLinkSilhouettePath =
    L"Data\\Interface\\SkyrimFittingSystem\\images\\body-slot-silhouette.png";

constexpr std::array<std::uint32_t, 17> kLeftSlots{
    30, 31, 41, 42, 43, 50, 51, 35, 32, 33, 34, 36, 39, 40, 61, 37, 38};
constexpr std::array<std::uint32_t, 15> kRightSlots{
    44, 55, 45, 46, 47, 56, 57, 58, 59, 48, 60, 49, 52, 53, 54};

struct SlotAppearance {
  RE::FormID formID{0};
  std::string name;
  const sfs::workbench::EquipmentWidgetItem *item{nullptr};
};

using SlotAppearances =
    std::array<SlotAppearance, sfs::workbench::kAutomaticEquipmentSlotCount>;

[[nodiscard]] std::size_t SlotIndex(const std::uint32_t a_slotNumber) {
  return static_cast<std::size_t>(a_slotNumber -
                                  sfs::workbench::kAutomaticEquipmentFirstSlot);
}

[[nodiscard]] std::uint8_t FirstSlotNumber(const std::uint64_t a_slotMask) {
  for (std::uint32_t slot = sfs::workbench::kAutomaticEquipmentFirstSlot;
       slot <= sfs::workbench::kAutomaticEquipmentLastSlot; ++slot) {
    if ((a_slotMask & sfs::armor::GetArmorSlotMask(slot)) != 0) {
      return static_cast<std::uint8_t>(slot);
    }
  }
  return 0;
}

[[nodiscard]] std::string SlotLabel(const std::uint8_t a_slotNumber,
                                    const std::string_view a_fallback) {
  if (a_slotNumber == 0) {
    return std::string(a_fallback);
  }
  const auto labels = sfs::armor::GetArmorSlotLabels(
      sfs::armor::GetArmorSlotMask(a_slotNumber));
  return labels.empty() ? std::to_string(a_slotNumber) : labels.front();
}

[[nodiscard]] std::string AutomaticMappingLabel(
    const std::uint8_t a_slotNumber, const std::string_view a_fallback) {
  const auto *localization = sfs::ui::Localization::GetSingleton();
  auto slotLabel = SlotLabel(a_slotNumber, a_fallback);
  return sfs::strings::SafeVFormat(
      std::string(localization->Get("strip_link.slot.vanilla")),
      std::make_format_args(slotLabel));
}

[[nodiscard]] bool ExcludesBodyConnection(const std::uint32_t a_slotNumber) {
  return a_slotNumber == 50 || a_slotNumber == 51 || a_slotNumber == 61;
}

[[nodiscard]] bool IsSlotCardVisible(const std::uint32_t a_slotNumber) {
  if (a_slotNumber == 39 && !sfs::workbench::IsShieldAppearanceSlotEnabled()) {
    return false;
  }
  return (sfs::workbench::GetSpecialEffectProtectedSlotMask() &
          sfs::armor::GetArmorSlotMask(a_slotNumber)) == 0;
}

[[nodiscard]] bool IsVanillaDirectTargetSlot(
    const std::uint32_t a_slotNumber) {
  return std::ranges::find(
             sfs::workbench::kAutomaticEquipmentVanillaAnchorSlots,
             a_slotNumber) !=
         sfs::workbench::kAutomaticEquipmentVanillaAnchorSlots.end();
}

[[nodiscard]] bool IsDirectTargetSlotVisible(
    const std::uint32_t a_slotNumber, const Mode a_automaticBaseMode) {
  return IsSlotCardVisible(a_slotNumber) &&
         (a_automaticBaseMode == Mode::ModSettingsSlots ||
          IsVanillaDirectTargetSlot(a_slotNumber));
}

[[nodiscard]] bool LoadPngTexture(ID3D11Device *a_device, const wchar_t *a_path,
                                  ID3D11ShaderResourceView **a_textureView,
                                  std::uint32_t &a_width,
                                  std::uint32_t &a_height) {
  if (!a_device || !a_path || !a_textureView) {
    return false;
  }

  const auto comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool uninitializeCom = SUCCEEDED(comResult);
  if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
    return false;
  }

  ComPtr<IWICImagingFactory> factory;
  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frame;
  ComPtr<IWICFormatConverter> converter;
  ComPtr<ID3D11Texture2D> texture;
  bool loaded = false;
  do {
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.GetAddressOf()))) ||
        FAILED(factory->CreateDecoderFromFilename(a_path, nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnLoad,
                                                  decoder.GetAddressOf())) ||
        FAILED(decoder->GetFrame(0, frame.GetAddressOf())) ||
        FAILED(factory->CreateFormatConverter(converter.GetAddressOf())) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom))) {
      break;
    }

    UINT width = 0;
    UINT height = 0;
    if (FAILED(converter->GetSize(&width, &height)) || width == 0 ||
        height == 0) {
      break;
    }
    const auto stride = width * 4u;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * height);
    if (FAILED(converter->CopyPixels(nullptr, stride,
                                     static_cast<UINT>(pixels.size()),
                                     pixels.data()))) {
      break;
    }

    D3D11_TEXTURE2D_DESC textureDescription{};
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.MipLevels = 1;
    textureDescription.ArraySize = 1;
    textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Usage = D3D11_USAGE_IMMUTABLE;
    textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA textureData{};
    textureData.pSysMem = pixels.data();
    textureData.SysMemPitch = stride;
    if (FAILED(a_device->CreateTexture2D(&textureDescription, &textureData,
                                         texture.GetAddressOf())) ||
        FAILED(a_device->CreateShaderResourceView(texture.Get(), nullptr,
                                                  a_textureView))) {
      break;
    }
    a_width = width;
    a_height = height;
    loaded = true;
  } while (false);

  if (uninitializeCom) {
    CoUninitialize();
  }
  return loaded;
}

[[nodiscard]] std::uint8_t
DefaultVanillaMapping(const std::uint32_t a_slotNumber) {
  switch (a_slotNumber) {
  case 30:
  case 41:
    return 30;
  case 31:
    return 31;
  case 42:
  case 43:
  case 44:
  case 55:
    return 42;
  case 35:
  case 45:
    return 35;
  case 33:
  case 58:
  case 59:
    return 33;
  case 34:
    return 34;
  case 36:
    return 36;
  case 37:
  case 53:
  case 54:
    return 37;
  case 38:
    return 38;
  case 39:
    return 39;
  case 50:
  case 51:
  case 61:
    return 0;
  default:
    return 32;
  }
}

[[nodiscard]] SlotAppearances
BuildSlotAppearances(const sfs::workbench::VariantWorkbench &a_workbench,
                     const RE::FormID a_actorFormID) {
  SlotAppearances appearances{};
  // Prefer the actor's base appearances. A conditional appearance fills a
  // card only when that slot has no base item, keeping the compact one-item
  // card deterministic while still exposing condition-only slots.
  for (const bool conditionalPass : {false, true}) {
    for (const auto &row : a_workbench.GetRows()) {
      if (row.ownerActorFormID != a_actorFormID ||
          row.conditionId.has_value() != conditionalPass) {
        continue;
      }
      for (const auto &item : row.overrides) {
        const auto displayMask = row.GetOverrideDisplaySlotMask(item);
        for (std::uint32_t slot = sfs::workbench::kAutomaticEquipmentFirstSlot;
             slot <= sfs::workbench::kAutomaticEquipmentLastSlot; ++slot) {
          auto &target = appearances[SlotIndex(slot)];
          if (target.formID != 0 ||
              (displayMask & sfs::armor::GetArmorSlotMask(slot)) == 0) {
            continue;
          }
          target.formID = item.formID;
          target.name = item.name;
          target.item = std::addressof(item);
        }
      }
    }
  }
  return appearances;
}

[[nodiscard]] std::uint64_t
BuildOccupiedActualSlotMask(const sfs::workbench::VariantWorkbench &a_workbench,
                            const RE::FormID a_actorFormID) {
  std::uint64_t result = 0;
  std::unordered_set<RE::FormID> seen;
  for (const auto &row : a_workbench.GetRows()) {
    if (row.ownerActorFormID != a_actorFormID || !row.isEquipped ||
        row.IsSlotRow() || !seen.insert(row.equipped.formID).second) {
      continue;
    }
    const auto *armor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
    const auto armorMask =
        sfs::workbench::GetAutomaticEquipmentControlSlotMask(armor);
    result |= armorMask != 0 ? armorMask : row.equipped.slotMask;
  }
  return result;
}

[[nodiscard]] std::uint8_t
ResolveVanillaMapping(const RE::TESObjectARMO *a_appearance,
                      const std::uint64_t a_occupiedActualSlotMask) {
  const auto priority = sfs::workbench::GetVanillaAnchorPriority(a_appearance);
  if (priority.empty()) {
    return 0;
  }
  const auto protectedSlotMask =
      sfs::workbench::GetEffectiveAppearanceProtectedSlotMask();
  for (std::size_t index = 0; index < priority.count; ++index) {
    const auto candidateSlotMask = priority.slotMasks[index];
    if ((candidateSlotMask & protectedSlotMask) == 0 &&
        (a_occupiedActualSlotMask & candidateSlotMask) != 0) {
      return FirstSlotNumber(candidateSlotMask);
    }
  }
  for (std::size_t index = 0; index < priority.count; ++index) {
    if ((priority.slotMasks[index] & protectedSlotMask) == 0) {
      return FirstSlotNumber(priority.slotMasks[index]);
    }
  }
  return 0;
}

[[nodiscard]] Mappings
BuildDisplayedMappings(const Mode a_mode, const SlotAppearances &a_appearances,
                       const std::uint64_t a_occupiedActualSlotMask,
                       const Mappings &a_directMappings,
                       const Overrides &a_directOverrides,
                       const Mode /*a_directAutomaticBaseMode*/) {
  Mappings result{};

  if (a_mode == Mode::DirectSlots) {
    // Direct editing is fully explicit. Untouched cards default to a same-slot
    // 1:1 mapping; there is no automatic-matching entry or fallback here.
    for (std::uint32_t slot = sfs::workbench::kAutomaticEquipmentFirstSlot;
         slot <= sfs::workbench::kAutomaticEquipmentLastSlot; ++slot) {
      result[SlotIndex(slot)] = static_cast<std::uint8_t>(slot);
      if (!IsSlotCardVisible(slot)) {
        result[SlotIndex(slot)] = 0;
      }
    }
    for (std::size_t index = 0; index < result.size(); ++index) {
      if (!a_directOverrides[index]) {
        continue;
      }
      auto &mappedSlot = result[index];
      mappedSlot = a_directMappings[index];
      if (mappedSlot != 0 && !IsSlotCardVisible(mappedSlot)) {
        // Keep the saved value in a_directMappings. A newly protected target
        // is only inactive while protection is enabled.
        mappedSlot = 0;
      }
    }
    return result;
  }

  for (std::uint32_t slot = sfs::workbench::kAutomaticEquipmentFirstSlot;
       slot <= sfs::workbench::kAutomaticEquipmentLastSlot; ++slot) {
    if (a_mode == Mode::VanillaSlots) {
      result[SlotIndex(slot)] = DefaultVanillaMapping(slot);
    } else {
      result[SlotIndex(slot)] = static_cast<std::uint8_t>(slot);
    }
    if (result[SlotIndex(slot)] != 0 &&
        !IsSlotCardVisible(result[SlotIndex(slot)])) {
      result[SlotIndex(slot)] = 0;
    }
  }
  if (a_mode == Mode::VanillaSlots) {
    std::unordered_set<RE::FormID> resolvedForms;
    for (std::uint32_t slot = sfs::workbench::kAutomaticEquipmentFirstSlot;
         slot <= sfs::workbench::kAutomaticEquipmentLastSlot; ++slot) {
      const auto &appearance = a_appearances[SlotIndex(slot)];
      if (appearance.formID == 0 ||
          !resolvedForms.insert(appearance.formID).second) {
        continue;
      }

      const auto mappedSlot = ResolveVanillaMapping(
          RE::TESForm::LookupByID<RE::TESObjectARMO>(appearance.formID),
          a_occupiedActualSlotMask);
      for (std::size_t targetIndex = 0; targetIndex < a_appearances.size();
           ++targetIndex) {
        if (a_appearances[targetIndex].formID == appearance.formID) {
          result[targetIndex] = mappedSlot;
        }
      }
    }
  }

  // Keep the selected automatic policy active and overlay only explicitly
  // edited appearance groups. Untouched cards retain the tested calculation
  // above and continue to react to equipment/appearance changes.
  for (std::size_t index = 0; index < result.size(); ++index) {
    if (!a_directOverrides[index]) {
      continue;
    }
    auto &mappedSlot = result[index];
    mappedSlot = a_directMappings[index];
    if (mappedSlot != 0 && !IsSlotCardVisible(mappedSlot)) {
      mappedSlot = 0;
    }
  }
  return result;
}

[[nodiscard]] ImU32 SlotColor(const std::uint32_t a_slotNumber) {
  switch (a_slotNumber) {
  case 31:
  case 41:
    return IM_COL32(143, 114, 216, 235);
  case 35:
  case 45:
    return IM_COL32(223, 146, 91, 235);
  case 33:
  case 36:
    return IM_COL32(228, 168, 78, 235);
  case 34:
  case 57:
  case 58:
  case 59:
    return IM_COL32(92, 145, 204, 235);
  case 37:
    return IM_COL32(85, 136, 209, 235);
  case 38:
  case 53:
  case 54:
    return IM_COL32(76, 168, 168, 235);
  case 42:
    return IM_COL32(228, 186, 85, 235);
  case 43:
  case 44:
  case 55:
    return IM_COL32(214, 111, 105, 235);
  default:
    return IM_COL32(93, 165, 120, 235);
  }
}

[[nodiscard]] ImVec2 BodyAnchor(const std::uint32_t a_slotNumber,
                                const ImVec2 &a_min, const ImVec2 &a_size,
                                const bool a_leftCard) {
  float x = 0.50f;
  float y = 0.31f;
  switch (a_slotNumber) {
  case 30:
  case 31:
  case 41:
  case 42:
  case 43:
  case 44:
  case 50:
  case 51:
  case 55:
    y = 0.075f;
    break;
  case 35:
  case 45:
    y = 0.15f;
    break;
  case 33:
  case 36:
  case 39:
    x = a_leftCard ? 0.20f : 0.80f;
    y = 0.40f;
    break;
  case 34:
  case 57:
    x = a_leftCard ? 0.24f : 0.76f;
    y = 0.29f;
    break;
  case 58:
    x = 0.24f;
    y = 0.29f;
    break;
  case 59:
    x = 0.76f;
    y = 0.29f;
    break;
  case 40:
  case 49:
  case 52:
    y = 0.47f;
    break;
  case 53:
    x = 0.58f;
    y = 0.62f;
    break;
  case 54:
    x = 0.42f;
    y = 0.62f;
    break;
  case 38:
    x = a_leftCard ? 0.42f : 0.58f;
    y = 0.78f;
    break;
  case 37:
    x = a_leftCard ? 0.38f : 0.62f;
    y = 0.94f;
    break;
  default:
    y = 0.31f;
    break;
  }
  return ImVec2(a_min.x + a_size.x * x, a_min.y + a_size.y * y);
}

void DrawBodySilhouette(ImDrawList *a_drawList, const ImVec2 &a_min,
                        const ImVec2 &a_size) {
  const auto px = [&](const float x) { return a_min.x + a_size.x * x; };
  const auto py = [&](const float y) { return a_min.y + a_size.y * y; };
  const ImU32 head = IM_COL32(214, 111, 105, 235);
  const ImU32 hair = IM_COL32(143, 114, 216, 235);
  const ImU32 neck = IM_COL32(223, 146, 91, 235);
  const ImU32 body = IM_COL32(93, 165, 120, 235);
  const ImU32 arm = IM_COL32(92, 145, 204, 235);
  const ImU32 hand = IM_COL32(228, 168, 78, 235);
  const ImU32 leg = IM_COL32(76, 168, 168, 235);
  const ImU32 calf = IM_COL32(61, 145, 154, 235);
  const ImU32 feet = IM_COL32(85, 136, 209, 235);
  const ImU32 outline = IM_COL32(205, 221, 231, 125);

  // Legs are drawn first with generous overlaps so no background seam can
  // appear where the curved pelvis and knee regions meet.
  a_drawList->AddBezierCubic(ImVec2(px(.43f), py(.46f)),
                             ImVec2(px(.40f), py(.60f)),
                             ImVec2(px(.39f), py(.69f)),
                             ImVec2(px(.40f), py(.78f)), leg, a_size.x * .18f);
  a_drawList->AddBezierCubic(ImVec2(px(.57f), py(.46f)),
                             ImVec2(px(.60f), py(.60f)),
                             ImVec2(px(.61f), py(.69f)),
                             ImVec2(px(.60f), py(.78f)), leg, a_size.x * .18f);
  a_drawList->AddBezierCubic(ImVec2(px(.40f), py(.73f)),
                             ImVec2(px(.39f), py(.82f)),
                             ImVec2(px(.40f), py(.88f)),
                             ImVec2(px(.41f), py(.94f)), calf, a_size.x * .14f);
  a_drawList->AddBezierCubic(ImVec2(px(.60f), py(.73f)),
                             ImVec2(px(.61f), py(.82f)),
                             ImVec2(px(.60f), py(.88f)),
                             ImVec2(px(.59f), py(.94f)), calf, a_size.x * .14f);
  a_drawList->AddEllipseFilled(ImVec2(px(.38f), py(.965f)),
                               ImVec2(a_size.x * .10f, a_size.y * .035f), feet);
  a_drawList->AddEllipseFilled(ImVec2(px(.62f), py(.965f)),
                               ImVec2(a_size.x * .10f, a_size.y * .035f), feet);

  // Curved arms with hand shapes overlapping the wrists.
  a_drawList->AddBezierCubic(ImVec2(px(.31f), py(.23f)),
                             ImVec2(px(.20f), py(.30f)),
                             ImVec2(px(.18f), py(.39f)),
                             ImVec2(px(.13f), py(.46f)), arm, a_size.x * .13f);
  a_drawList->AddBezierCubic(ImVec2(px(.69f), py(.23f)),
                             ImVec2(px(.80f), py(.30f)),
                             ImVec2(px(.82f), py(.39f)),
                             ImVec2(px(.87f), py(.46f)), arm, a_size.x * .13f);
  a_drawList->AddEllipseFilled(ImVec2(px(.11f), py(.48f)),
                               ImVec2(a_size.x * .07f, a_size.y * .038f), hand,
                               -.35f);
  a_drawList->AddEllipseFilled(ImVec2(px(.89f), py(.48f)),
                               ImVec2(a_size.x * .07f, a_size.y * .038f), hand,
                               .35f);

  // Torso and high-leg pelvis are one continuous curved path.
  a_drawList->PathClear();
  a_drawList->PathLineTo(ImVec2(px(.35f), py(.19f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.29f), py(.22f)),
                                     ImVec2(px(.31f), py(.32f)),
                                     ImVec2(px(.34f), py(.38f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.36f), py(.43f)),
                                     ImVec2(px(.34f), py(.49f)),
                                     ImVec2(px(.43f), py(.54f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.47f), py(.57f)),
                                     ImVec2(px(.49f), py(.59f)),
                                     ImVec2(px(.50f), py(.61f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.51f), py(.59f)),
                                     ImVec2(px(.53f), py(.57f)),
                                     ImVec2(px(.57f), py(.54f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.66f), py(.49f)),
                                     ImVec2(px(.64f), py(.43f)),
                                     ImVec2(px(.66f), py(.38f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.69f), py(.32f)),
                                     ImVec2(px(.71f), py(.22f)),
                                     ImVec2(px(.65f), py(.19f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.59f), py(.17f)),
                                     ImVec2(px(.55f), py(.18f)),
                                     ImVec2(px(.50f), py(.18f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.45f), py(.18f)),
                                     ImVec2(px(.41f), py(.17f)),
                                     ImVec2(px(.35f), py(.19f)));
  a_drawList->PathFillConcave(body);

  // Neck is a short U-shaped collar and never spills into the shoulders.
  a_drawList->PathClear();
  a_drawList->PathLineTo(ImVec2(px(.43f), py(.12f)));
  a_drawList->PathLineTo(ImVec2(px(.57f), py(.12f)));
  a_drawList->PathLineTo(ImVec2(px(.58f), py(.19f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.55f), py(.205f)),
                                     ImVec2(px(.45f), py(.205f)),
                                     ImVec2(px(.42f), py(.19f)));
  a_drawList->PathFillConvex(neck);
  a_drawList->AddEllipseFilled(ImVec2(px(.50f), py(.085f)),
                               ImVec2(a_size.x * .12f, a_size.y * .065f), head);
  a_drawList->PathClear();
  a_drawList->PathLineTo(ImVec2(px(.39f), py(.075f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.40f), py(.015f)),
                                     ImVec2(px(.60f), py(.015f)),
                                     ImVec2(px(.61f), py(.075f)));
  a_drawList->PathBezierCubicCurveTo(ImVec2(px(.56f), py(.065f)),
                                     ImVec2(px(.44f), py(.065f)),
                                     ImVec2(px(.39f), py(.075f)));
  a_drawList->PathFillConvex(hair);

  a_drawList->AddBezierCubic(
      ImVec2(px(.40f), py(.02f)), ImVec2(px(.35f), py(.10f)),
      ImVec2(px(.41f), py(.14f)), ImVec2(px(.43f), py(.19f)), outline, 1.0f);
  a_drawList->AddBezierCubic(
      ImVec2(px(.60f), py(.02f)), ImVec2(px(.65f), py(.10f)),
      ImVec2(px(.59f), py(.14f)), ImVec2(px(.57f), py(.19f)), outline, 1.0f);
}
} // namespace

namespace sfs {
void Menu::EnsureStripLinkSilhouetteTexture() {
  if (stripLinkSilhouetteLoadAttempted_) {
    return;
  }
  stripLinkSilhouetteLoadAttempted_ = true;

  if (LoadPngTexture(device_, kStripLinkSilhouettePath,
                     &stripLinkSilhouetteTexture_, stripLinkSilhouetteWidth_,
                     stripLinkSilhouetteHeight_)) {
    logger::info("Loaded strip-link silhouette texture {}x{}",
                 stripLinkSilhouetteWidth_, stripLinkSilhouetteHeight_);
    return;
  }

  logger::warn("Failed to load strip-link silhouette texture; using vector "
               "fallback");
}

void Menu::OpenStripLinkDialog() {
  stripLinkDialog_ = {};
  stripLinkDialog_.openRequested = true;
  const auto configuredMode = workbench::GetExternalModStripLinkMode();
  if (configuredMode == Mode::Custom) {
    stripLinkDialog_.baseMode =
        workbench::GetCustomExternalModStripLinkBaseMode();
    stripLinkDialog_.directAutomaticBaseMode =
        workbench::GetCustomDirectStripLinkAutomaticBaseMode();
    stripLinkDialog_.directMappings =
        workbench::GetCustomDirectStripLinkMappings();
    stripLinkDialog_.directOverrides =
        workbench::GetCustomDirectStripLinkOverrides();
    stripLinkDialog_.disabledAppearanceSlotMask =
        workbench::GetCustomStripLinkDisabledAppearanceSlotMask();
    if (stripLinkDialog_.baseMode == Mode::DirectSlots) {
      // Fully direct was available only in development builds and is no
      // longer exposed as a popup-wide policy. Preserve its exact 1:1 result
      // as explicit per-slot overrides on the mod-settings base, which is the
      // only automatic base that accepts every 30-61 target. The compatibility
      // branch remains in the runtime until the user applies this migration.
      stripLinkDialog_.baseMode = Mode::ModSettingsSlots;
      stripLinkDialog_.directAutomaticBaseMode = Mode::ModSettingsSlots;
      for (std::uint32_t slot = workbench::kAutomaticEquipmentFirstSlot;
           slot <= workbench::kAutomaticEquipmentLastSlot; ++slot) {
        const auto index = SlotIndex(slot);
        if (!stripLinkDialog_.directOverrides[index]) {
          stripLinkDialog_.directMappings[index] =
              static_cast<std::uint8_t>(slot);
          stripLinkDialog_.directOverrides[index] = true;
        }
        if ((stripLinkDialog_.disabledAppearanceSlotMask &
             sfs::armor::GetArmorSlotMask(slot)) != 0) {
          stripLinkDialog_.directMappings[index] = 0;
          stripLinkDialog_.directOverrides[index] = true;
        }
      }
    }
  } else if (configuredMode == Mode::VanillaSlots) {
    stripLinkDialog_.baseMode = Mode::VanillaSlots;
    stripLinkDialog_.directAutomaticBaseMode = Mode::VanillaSlots;
  } else {
    stripLinkDialog_.baseMode = Mode::ModSettingsSlots;
    stripLinkDialog_.directAutomaticBaseMode = Mode::ModSettingsSlots;
  }
}

void Menu::DrawStripLinkDialog() {
  auto *localization = ui::Localization::GetSingleton();
  std::string popupTitle{localization->Get("strip_link.title")};

  if (stripLinkDialog_.openRequested) {
    const auto &io = ImGui::GetIO();
    ImGui::SetNextWindowSize(
        ImVec2((std::min)(io.DisplaySize.x * .96f, 1760.0f),
               (std::min)(io.DisplaySize.y * .96f, 1600.0f)),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * .5f, io.DisplaySize.y * .5f),
        ImGuiCond_Appearing, ImVec2(.5f, .5f));
    ImGui::SetNextWindowFocus();
    stripLinkDialog_.open = true;
    stripLinkDialog_.openRequested = false;
  }

  if (!stripLinkDialog_.open) {
    return;
  }

  bool keepOpen = stripLinkDialog_.open;
  if (!ImGui::Begin(popupTitle.c_str(), &keepOpen,
                    ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::End();
    if (!keepOpen) {
      stripLinkDialog_ = {};
    }
    return;
  }
  EnsureStripLinkSilhouetteTexture();
  const auto *actor = ResolveWorkbenchPreviewActor();
  const auto actorFormID = actor != nullptr ? actor->GetFormID() : 0;
  const auto appearances = actorFormID != 0
                               ? BuildSlotAppearances(workbench_, actorFormID)
                               : SlotAppearances{};
  const auto occupiedActualSlotMask =
      actorFormID != 0
          ? BuildOccupiedActualSlotMask(workbench_, actorFormID)
          : 0;
  stripLinkDialog_.mappings = BuildDisplayedMappings(
      stripLinkDialog_.baseMode, appearances, occupiedActualSlotMask,
      stripLinkDialog_.directMappings, stripLinkDialog_.directOverrides,
      stripLinkDialog_.directAutomaticBaseMode);
  const Mappings emptyDirectMappings{};
  const Overrides emptyDirectOverrides{};
  const auto automaticMappings =
      stripLinkDialog_.baseMode == Mode::DirectSlots
          ? Mappings{}
          : BuildDisplayedMappings(stripLinkDialog_.baseMode, appearances,
                                   occupiedActualSlotMask,
                                   emptyDirectMappings, emptyDirectOverrides,
                                   stripLinkDialog_.baseMode);
  const std::array modeLabels{
      localization->Get("strip_link.mode.modding"),
      localization->Get("strip_link.mode.vanilla")};
  constexpr std::array modes{Mode::ModSettingsSlots, Mode::VanillaSlots};

  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(localization->GetCStr("strip_link.mode.label"));
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
  for (int modeIndex = 0; modeIndex < static_cast<int>(modeLabels.size());
       ++modeIndex) {
    if (modeIndex > 0) {
      ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
    }
    const auto mode = modes[static_cast<std::size_t>(modeIndex)];
    const std::string label = std::string(modeLabels[modeIndex]) +
                              "##strip-link-mode-" + std::to_string(modeIndex);
    if (ImGui::RadioButton(label.c_str(), stripLinkDialog_.baseMode == mode)) {
      stripLinkDialog_.baseMode = mode;
      if (mode == Mode::ModSettingsSlots || mode == Mode::VanillaSlots) {
        stripLinkDialog_.directAutomaticBaseMode = mode;
      }
      stripLinkDialog_.mappings = BuildDisplayedMappings(
          mode, appearances, occupiedActualSlotMask,
          stripLinkDialog_.directMappings, stripLinkDialog_.directOverrides,
          stripLinkDialog_.directAutomaticBaseMode);
    }
  }
  ImGui::Separator();

  const float mapHeight =
      (std::max)(360.0f, ImGui::GetContentRegionAvail().y -
                             ImGui::GetFrameHeightWithSpacing() -
                             ImGui::GetStyle().ItemSpacing.y * 2.0f);
  if (ImGui::BeginChild("##strip-link-map", ImVec2(0.0f, mapHeight),
                        ImGuiChildFlags_Borders,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    const auto contentStart = ImGui::GetCursorScreenPos();
    const float viewportWidth = ImGui::GetContentRegionAvail().x;
    const float contentWidth = (std::max)(viewportWidth, 1320.0f);
    const float cardHeight = ImGui::GetFrameHeight() +
                             ImGui::GetTextLineHeight() +
                             ImGui::GetStyle().ItemSpacing.y + 10.0f;
    const float cardGap = 4.0f;
    const float centerWidth = std::clamp(contentWidth * .24f, 280.0f, 400.0f);
    const float sideGap = 24.0f;
    const float cardWidth = std::clamp(
        (contentWidth - centerWidth - sideGap * 2.0f) * .5f, 345.0f, 570.0f);
    const float leftX = contentStart.x;
    const float rightX = contentStart.x + contentWidth - cardWidth;
    const float centerX = contentStart.x + (contentWidth - centerWidth) * .5f;

    const auto slotZone = [](const std::uint32_t slotNumber) {
      if (slotNumber == 37 || slotNumber == 38 || slotNumber == 49 ||
          slotNumber == 52 || slotNumber == 53 || slotNumber == 54) {
        return 2;
      }
      if (slotNumber == 30 || slotNumber == 31 || slotNumber == 35 ||
          slotNumber == 41 || slotNumber == 42 || slotNumber == 43 ||
          slotNumber == 44 || slotNumber == 45 || slotNumber == 50 ||
          slotNumber == 51 || slotNumber == 55) {
        return 0;
      }
      return 1;
    };
    const auto countVisibleSlotsByZone = [&](const auto &slots) {
      std::array<int, 3> counts{};
      for (const auto slotNumber : slots) {
        if (IsSlotCardVisible(slotNumber)) {
          ++counts[slotZone(slotNumber)];
        }
      }
      return counts;
    };
    const auto leftZoneCounts = countVisibleSlotsByZone(kLeftSlots);
    const auto rightZoneCounts = countVisibleSlotsByZone(kRightSlots);
    const std::array zoneHeights{
        (std::max)(leftZoneCounts[0], rightZoneCounts[0]) *
                (cardHeight + cardGap) +
            24.0f,
        (std::max)(leftZoneCounts[1], rightZoneCounts[1]) *
                (cardHeight + cardGap) +
            24.0f,
        (std::max)(leftZoneCounts[2], rightZoneCounts[2]) *
                (cardHeight + cardGap) +
            24.0f};
    const float contentHeight =
        zoneHeights[0] + zoneHeights[1] + zoneHeights[2];
    const std::array zoneTop{0.0f, zoneHeights[0],
                             zoneHeights[0] + zoneHeights[1]};
    auto *drawList = ImGui::GetWindowDrawList();
    const auto separatorColor = ImGui::GetColorU32(ImGuiCol_Separator, .72f);
    drawList->AddLine(
        ImVec2(contentStart.x, contentStart.y + zoneTop[1]),
        ImVec2(contentStart.x + contentWidth, contentStart.y + zoneTop[1]),
        separatorColor);
    drawList->AddLine(
        ImVec2(contentStart.x, contentStart.y + zoneTop[2]),
        ImVec2(contentStart.x + contentWidth, contentStart.y + zoneTop[2]),
        separatorColor);

    const ImVec2 silhouetteBoundsMin(centerX + 8.0f, contentStart.y + 12.0f);
    const ImVec2 silhouetteBoundsSize(centerWidth - 16.0f,
                                      contentHeight - 24.0f);
    constexpr float kSilhouetteCropWidth = 808.0f - 213.0f;
    constexpr float kSilhouetteCropHeight = 1475.0f - 37.0f;
    const float silhouetteAspect = kSilhouetteCropWidth / kSilhouetteCropHeight;
    ImVec2 silhouetteSize(silhouetteBoundsSize.x,
                          silhouetteBoundsSize.x / silhouetteAspect);
    if (silhouetteSize.y > silhouetteBoundsSize.y) {
      silhouetteSize.y = silhouetteBoundsSize.y;
      silhouetteSize.x = silhouetteSize.y * silhouetteAspect;
    }
    const ImVec2 silhouetteMin(
        silhouetteBoundsMin.x +
            (silhouetteBoundsSize.x - silhouetteSize.x) * .5f,
        silhouetteBoundsMin.y +
            (silhouetteBoundsSize.y - silhouetteSize.y) * .5f);
    if (stripLinkSilhouetteTexture_ != nullptr) {
      const ImVec2 silhouetteMax(silhouetteMin.x + silhouetteSize.x,
                                 silhouetteMin.y + silhouetteSize.y);
      constexpr ImVec2 uvMin(213.0f / 1024.0f, 37.0f / 1536.0f);
      constexpr ImVec2 uvMax(808.0f / 1024.0f, 1475.0f / 1536.0f);
      drawList->AddImage(
          ImTextureRef(static_cast<ImTextureID>(
              reinterpret_cast<std::uintptr_t>(stripLinkSilhouetteTexture_))),
          silhouetteMin, silhouetteMax, uvMin, uvMax);
    } else {
      DrawBodySilhouette(drawList, silhouetteMin, silhouetteSize);
    }

    const auto slotY = [&](const std::uint32_t slotNumber, const bool left,
                           const int indexInZone) {
      const int zone = slotZone(slotNumber);
      const int count = left ? leftZoneCounts[zone] : rightZoneCounts[zone];
      const float groupHeight =
          count * cardHeight + (std::max)(0, count - 1) * cardGap;
      return contentStart.y + zoneTop[zone] +
             (zoneHeights[zone] - groupHeight) * .5f +
             indexInZone * (cardHeight + cardGap);
    };

    std::array<int, 3> leftIndices{};
    std::array<int, 3> rightIndices{};
    const auto drawConnections = [&](const auto &slots, const bool left) {
      auto indices = left ? leftIndices : rightIndices;
      for (const auto slotNumber : slots) {
        if (!IsSlotCardVisible(slotNumber)) {
          continue;
        }
        const int zone = slotZone(slotNumber);
        const auto y = slotY(slotNumber, left, indices[zone]++);
        const auto sourceSlotMask = sfs::armor::GetArmorSlotMask(slotNumber);
        const auto mappedSlot =
            stripLinkDialog_.mappings[SlotIndex(slotNumber)];
        if (mappedSlot == 0 || ExcludesBodyConnection(slotNumber) ||
            (stripLinkDialog_.baseMode != Mode::DirectSlots &&
             (stripLinkDialog_.disabledAppearanceSlotMask & sourceSlotMask) !=
                 0)) {
          continue;
        }
        const auto start =
            ImVec2(left ? leftX + cardWidth : rightX, y + cardHeight * .5f);
        const auto end =
            BodyAnchor(mappedSlot, silhouetteMin, silhouetteSize, left);
        const float direction = left ? 1.0f : -1.0f;
        drawList->AddBezierCubic(
            start, ImVec2(start.x + direction * sideGap * 2.3f, start.y),
            ImVec2(end.x - direction * sideGap * 1.8f, end.y), end,
            SlotColor(mappedSlot), 1.35f);
      }
    };
    drawConnections(kLeftSlots, true);
    drawConnections(kRightSlots, false);

    const auto helpText = localization->Get(
        stripLinkDialog_.baseMode == Mode::VanillaSlots
            ? "strip_link.help.vanilla"
            : stripLinkDialog_.baseMode == Mode::ModSettingsSlots
                ? "strip_link.help.modding"
                : "strip_link.help.direct");
    const float helpBoxWidth =
        (std::max)(240.0f, rightX - (leftX + cardWidth) - 16.0f);
    const float helpCenterX =
        ((leftX + cardWidth) + rightX) * .5f;
    std::vector<std::string> helpLines;
    std::size_t paragraphStart = 0;
    while (paragraphStart <= helpText.size()) {
      const auto paragraphEnd = helpText.find('\n', paragraphStart);
      const auto paragraph = helpText.substr(
          paragraphStart,
          paragraphEnd == std::string_view::npos
              ? std::string_view::npos
              : paragraphEnd - paragraphStart);
      std::string line;
      std::size_t wordStart = 0;
      while (wordStart < paragraph.size()) {
        const auto wordEnd = paragraph.find(' ', wordStart);
        const auto word = paragraph.substr(
            wordStart, wordEnd == std::string_view::npos
                           ? std::string_view::npos
                           : wordEnd - wordStart);
        std::string candidate = line;
        if (!candidate.empty()) {
          candidate.push_back(' ');
        }
        candidate.append(word);
        if (!line.empty() &&
            ImGui::CalcTextSize(candidate.c_str()).x > helpBoxWidth) {
          helpLines.push_back(std::move(line));
          line.assign(word);
        } else {
          line = std::move(candidate);
        }
        if (wordEnd == std::string_view::npos) {
          break;
        }
        wordStart = wordEnd + 1;
      }
      if (!line.empty()) {
        helpLines.push_back(std::move(line));
      }
      if (paragraphEnd == std::string_view::npos) {
        break;
      }
      paragraphStart = paragraphEnd + 1;
    }
    float helpY = contentStart.y + 18.0f;
    for (const auto &line : helpLines) {
      const auto lineSize = ImGui::CalcTextSize(line.c_str());
      drawList->AddText(
          ImGui::GetFont(), ImGui::GetFontSize(),
          ImVec2(helpCenterX - lineSize.x * .5f, helpY),
          ImGui::GetColorU32(ImGuiCol_Text), line.c_str());
      helpY += ImGui::GetTextLineHeightWithSpacing();
    }

    leftIndices = {};
    rightIndices = {};
    const auto emptyLabel = localization->Get("strip_link.slot.empty");
    const auto noLinkLabel = localization->Get("strip_link.slot.none");
    const auto sameAppearanceGroup = [&](const std::uint32_t sourceSlot,
                                         const std::uint32_t candidateSlot) {
      if (sourceSlot == candidateSlot) {
        return true;
      }
      const auto formID = appearances[SlotIndex(sourceSlot)].formID;
      return formID != 0 &&
             appearances[SlotIndex(candidateSlot)].formID == formID;
    };
    const auto setDirectMappingForAppearance =
        [&](const std::uint32_t sourceSlot, const std::uint8_t mappedSlot,
            const bool overridden) {
          for (std::uint32_t groupedSlot =
                   workbench::kAutomaticEquipmentFirstSlot;
               groupedSlot <= workbench::kAutomaticEquipmentLastSlot;
               ++groupedSlot) {
            if (!sameAppearanceGroup(sourceSlot, groupedSlot)) {
              continue;
            }
            const auto groupedIndex = SlotIndex(groupedSlot);
            stripLinkDialog_.directMappings[groupedIndex] = mappedSlot;
            stripLinkDialog_.directOverrides[groupedIndex] = overridden;
            stripLinkDialog_.mappings[groupedIndex] = mappedSlot;
            const auto groupedMask = sfs::armor::GetArmorSlotMask(groupedSlot);
            if (overridden && mappedSlot == 0) {
              stripLinkDialog_.disabledAppearanceSlotMask |= groupedMask;
            } else {
              stripLinkDialog_.disabledAppearanceSlotMask &= ~groupedMask;
            }
          }
        };
    const auto setAutomaticDisabledForAppearance =
        [&](const std::uint32_t sourceSlot, const bool disabled) {
          for (std::uint32_t groupedSlot =
                   workbench::kAutomaticEquipmentFirstSlot;
               groupedSlot <= workbench::kAutomaticEquipmentLastSlot;
               ++groupedSlot) {
            if (!sameAppearanceGroup(sourceSlot, groupedSlot)) {
              continue;
            }
            const auto groupedMask = sfs::armor::GetArmorSlotMask(groupedSlot);
            if (disabled) {
              stripLinkDialog_.disabledAppearanceSlotMask |= groupedMask;
            } else {
              stripLinkDialog_.disabledAppearanceSlotMask &= ~groupedMask;
            }
          }
        };
    const auto drawCards = [&](const auto &slots, const bool left) {
      auto &indices = left ? leftIndices : rightIndices;
      for (const auto slotNumber : slots) {
        if (!IsSlotCardVisible(slotNumber)) {
          continue;
        }
        const int zone = slotZone(slotNumber);
        const auto y = slotY(slotNumber, left, indices[zone]++);
        const auto x = left ? leftX : rightX;
        const ImVec2 cardMin(x, y);
        const ImVec2 cardMax(x + cardWidth, y + cardHeight);
        drawList->AddRectFilled(
            cardMin, cardMax,
            ThemeConfig::GetSingleton()->GetColorU32("BG_LIGHT", .92f), 4.0f);
        drawList->AddRect(cardMin, cardMax, ImGui::GetColorU32(ImGuiCol_Border),
                          4.0f);
        if (!ExcludesBodyConnection(slotNumber)) {
          if (left) {
            drawList->AddRectFilled(ImVec2(cardMax.x - 4.0f, cardMin.y),
                                    cardMax, SlotColor(slotNumber), 4.0f,
                                    ImDrawFlags_RoundCornersRight);
          } else {
            drawList->AddRectFilled(
                cardMin, ImVec2(cardMin.x + 4.0f, cardMax.y),
                SlotColor(slotNumber), 4.0f, ImDrawFlags_RoundCornersLeft);
          }
        }

        ImGui::PushID(static_cast<int>(slotNumber));
        const float insetLeft = left ? 7.0f : 11.0f;
        ImGui::SetCursorScreenPos(
            ImVec2(cardMin.x + insetLeft, cardMin.y + 5.0f));
        const auto slotLabels = sfs::armor::GetArmorSlotLabels(
            sfs::armor::GetArmorSlotMask(slotNumber));
        const auto slotLabel =
            slotLabels.empty() ? std::to_string(slotNumber) : slotLabels[0];
        const float mappingX = cardMin.x + cardWidth * .38f;
        const float textRight = mappingX - 5.0f;
        drawList->PushClipRect(ImVec2(cardMin.x + insetLeft, cardMin.y),
                               ImVec2(textRight, cardMax.y), true);
        ImGui::TextUnformatted(slotLabel.c_str());
        drawList->PopClipRect();

        const auto mappedSlot =
            stripLinkDialog_.mappings[SlotIndex(slotNumber)];
        const auto sourceSlotMask = sfs::armor::GetArmorSlotMask(slotNumber);
        bool stripLinkDisabled =
            (stripLinkDialog_.disabledAppearanceSlotMask & sourceSlotMask) != 0;
        const auto mappingText = SlotLabel(mappedSlot, noLinkLabel);
        if (stripLinkDialog_.baseMode == Mode::DirectSlots) {
          const auto preview = mappedSlot == 0 ? std::string(noLinkLabel)
                                               : mappingText;
          ImGui::SetCursorScreenPos(ImVec2(mappingX, cardMin.y + 3.0f));
          ImGui::SetNextItemWidth(cardMax.x - mappingX - 7.0f);
          if (ImGui::BeginCombo("##actual-slot", preview.c_str())) {
            if (ImGui::Selectable(noLinkLabel.data(), mappedSlot == 0)) {
              setDirectMappingForAppearance(slotNumber, 0, true);
            }
            for (std::uint32_t targetSlot =
                     workbench::kAutomaticEquipmentFirstSlot;
                 targetSlot <= workbench::kAutomaticEquipmentLastSlot;
                 ++targetSlot) {
              // Direct mode has only explicit targets plus No Linking.
              if (!IsSlotCardVisible(targetSlot)) {
                continue;
              }
              const auto labels = sfs::armor::GetArmorSlotLabels(
                  sfs::armor::GetArmorSlotMask(targetSlot));
              if (labels.empty()) {
                continue;
              }
              const auto targetLabel = labels[0];
              if (ImGui::Selectable(targetLabel.c_str(),
                                    mappedSlot == targetSlot)) {
                setDirectMappingForAppearance(
                    slotNumber, static_cast<std::uint8_t>(targetSlot), true);
              }
            }
            ImGui::EndCombo();
          }
        } else {
          const auto sourceIndex = SlotIndex(slotNumber);
          const bool directlyOverridden =
              stripLinkDialog_.directOverrides[sourceIndex];
          const auto automaticSlot = automaticMappings[sourceIndex];
          std::string previewText;
          if (stripLinkDisabled) {
            previewText = localization->Get("strip_link.slot.off");
          } else if (directlyOverridden) {
            previewText = mappingText;
          } else {
            previewText = AutomaticMappingLabel(automaticSlot, noLinkLabel);
          }
          ImGui::SetCursorScreenPos(ImVec2(mappingX, cardMin.y + 3.0f));
          ImGui::SetNextItemWidth(cardMax.x - mappingX - 7.0f);
          if (ImGui::BeginCombo("##actual-slot", previewText.c_str())) {
            if (ImGui::Selectable(noLinkLabel.data(), stripLinkDisabled)) {
              setDirectMappingForAppearance(slotNumber, 0, false);
              setAutomaticDisabledForAppearance(slotNumber, true);
            }
            if (automaticSlot != 0 &&
                ImGui::Selectable(
                    AutomaticMappingLabel(automaticSlot, noLinkLabel).c_str(),
                    !stripLinkDisabled && !directlyOverridden)) {
              setDirectMappingForAppearance(slotNumber, automaticSlot, false);
              setAutomaticDisabledForAppearance(slotNumber, false);
            }
            for (std::uint32_t targetSlot =
                     workbench::kAutomaticEquipmentFirstSlot;
                 targetSlot <= workbench::kAutomaticEquipmentLastSlot;
                 ++targetSlot) {
              const auto automaticBase =
                  stripLinkDialog_.baseMode == Mode::VanillaSlots
                      ? Mode::VanillaSlots
                      : Mode::ModSettingsSlots;
              if (!IsDirectTargetSlotVisible(targetSlot, automaticBase) ||
                  (!directlyOverridden && targetSlot == automaticSlot)) {
                continue;
              }
              const auto labels = sfs::armor::GetArmorSlotLabels(
                  sfs::armor::GetArmorSlotMask(targetSlot));
              if (labels.empty()) {
                continue;
              }
              const auto targetLabel = labels[0];
              if (ImGui::Selectable(targetLabel.c_str(),
                                    directlyOverridden &&
                                        mappedSlot == targetSlot)) {
                // A concrete edit remains an exception under the currently
                // selected automatic policy; it must not switch the radio.
                setDirectMappingForAppearance(
                    slotNumber, static_cast<std::uint8_t>(targetSlot), true);
              }
            }
            ImGui::EndCombo();
          }
        }

        ImGui::SetCursorScreenPos(ImVec2(
            cardMin.x + insetLeft, cardMin.y + ImGui::GetFrameHeight() + 7.0f));
        const auto &appearance = appearances[SlotIndex(slotNumber)];
        const float disableX = cardMax.x - 7.0f;
        drawList->PushClipRect(ImVec2(cardMin.x + insetLeft, cardMin.y),
                               ImVec2(disableX - 5.0f, cardMax.y), true);
        if (appearance.formID == 0) {
          ImGui::TextDisabled("%s", emptyLabel.data());
        } else {
          ImGui::TextUnformatted(appearance.name.c_str());
        }
        drawList->PopClipRect();
        ImGui::PopID();
      }
    };
    drawCards(kLeftSlots, true);
    drawCards(kRightSlots, false);

    ImGui::SetCursorScreenPos(
        ImVec2(contentStart.x, contentStart.y + contentHeight));
    ImGui::Dummy(ImVec2(contentWidth, 1.0f));
  }
  ImGui::EndChild();

  const auto cancelLabel = localization->Get("common.cancel");
  const auto applyLabel = localization->Get("common.apply");
  const float cancelWidth = ImGui::CalcTextSize(cancelLabel.data()).x +
                            ImGui::GetStyle().FramePadding.x * 2.0f;
  const float applyWidth = ImGui::CalcTextSize(applyLabel.data()).x +
                           ImGui::GetStyle().FramePadding.x * 2.0f;
  const float buttonsWidth =
      cancelWidth + applyWidth + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SetCursorPosX(
      ImGui::GetCursorPosX() +
      (std::max)(0.0f,
                 (ImGui::GetContentRegionAvail().x - buttonsWidth) * .5f));
  if (ImGui::Button(cancelLabel.data(), ImVec2(cancelWidth, 0.0f))) {
    stripLinkDialog_ = {};
  }
  ImGui::SameLine();
  if (ImGui::Button(applyLabel.data(), ImVec2(applyWidth, 0.0f))) {
    if (stripLinkDialog_.baseMode == Mode::DirectSlots) {
      // A No-Link exception created while either automatic base was selected
      // must remain a per-slot direct exception after switching to Direct.
      // Direct mode deliberately ignores the automatic disabled-mask layer.
      for (std::uint32_t slot = workbench::kAutomaticEquipmentFirstSlot;
           slot <= workbench::kAutomaticEquipmentLastSlot; ++slot) {
        const auto slotMask = sfs::armor::GetArmorSlotMask(slot);
        if ((stripLinkDialog_.disabledAppearanceSlotMask & slotMask) == 0) {
          continue;
        }
        const auto index = SlotIndex(slot);
        stripLinkDialog_.directMappings[index] = 0;
        stripLinkDialog_.directOverrides[index] = true;
      }
    }
    workbench::SetCustomExternalModStripLinkBaseMode(
        stripLinkDialog_.baseMode);
    workbench::SetCustomDirectStripLinkAutomaticBaseMode(
        stripLinkDialog_.directAutomaticBaseMode);
    workbench::SetCustomStripLinkDisabledAppearanceSlotMask(
        stripLinkDialog_.disabledAppearanceSlotMask);
    workbench::SetCustomDirectStripLinkMappings(
        stripLinkDialog_.directMappings);
    workbench::SetCustomDirectStripLinkOverrides(
        stripLinkDialog_.directOverrides);
    // The popup stores one custom policy. Its internal automatic/direct base
    // stays selected when editing cards, while the outer Options combo shows
    // the public Direct Slot Editing entry after Apply.
    workbench::SetExternalModStripLinkMode(Mode::Custom);
    native::external_equipment::ClearRuntimeState();
    poc::ResetVirtualWornTokenRuntimeState();
    poc::UpdateVirtualWornTokenCache();
    if (workbench::IsModSettingsStripLinkPolicyActive()) {
      static_cast<void>(poc::RefreshDeviousDevicesHiderSettings());
    }
    SaveUserSettings();
    workbench_.RefreshNativeArmorOverrides(ConditionDefinitions(),
                                           conditionStore_.revision, true);
    stripLinkDialog_ = {};
  }

  if (!keepOpen) {
    stripLinkDialog_ = {};
  }
  ImGui::End();
}
} // namespace sfs
