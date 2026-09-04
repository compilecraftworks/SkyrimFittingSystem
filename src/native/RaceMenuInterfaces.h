#pragma once

#include "native/RaceMenuActorUpdateManager.h"
#include <cstddef>

namespace RE { class NiTransform; }

// Public ABI prefixes verified against RaceMenu upstream
// 87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc (BodyMorph v4) and
// 9ebcb733e17be695f994cd2e9cc383043446bc02 (BodyMorph v5 / NiTransform v3).
// These declarations are shared with independent slot-dispatch regression tests.
// Interface versions are independent; gate each cast with its own version.
namespace sfs::native::racemenu::abi {

// SE/AE x64 only. Public prefix v4/v5; the private task prefix is separately
// verified in both pinned BodyMorphInterface.h revisions and installed DLLs.
inline constexpr std::size_t kApplyBodyMorphsVtableIndex = 13;
inline constexpr std::size_t kUpdateModelWeightRunSlot = 0;
inline constexpr std::size_t kUpdateModelWeightFormIDOffset = 8;

class IInterfaceMap {
public:
  virtual IPluginInterface *QueryInterface(const char *a_name) = 0;
  virtual bool AddInterface(const char *a_name,
                            IPluginInterface *a_pluginInterface) = 0;
  virtual IPluginInterface *RemoveInterface(const char *a_name) = 0;
};

struct InterfaceExchangeMessage {
  enum : std::uint32_t { kExchangeInterface = 0x9E3779B9 };

  IInterfaceMap *interfaceMap{nullptr};
};

class IBodyMorphInterface : public IPluginInterface {
public:
  class MorphKeyVisitor {
  public:
    virtual void Visit(const char *, float) = 0;
  };

  class StringVisitor {
  public:
    virtual void Visit(const char *) = 0;
  };

  class ActorVisitor {
  public:
    virtual void Visit(RE::TESObjectREFR *) = 0;
  };

  class MorphValueVisitor {
  public:
    virtual void Visit(RE::TESObjectREFR *, const char *, const char *,
                       float) = 0;
  };

  class MorphVisitor {
  public:
    virtual void Visit(RE::TESObjectREFR *, const char *) = 0;
  };

  virtual void SetMorph(RE::TESObjectREFR *, const char *, const char *,
                        float) = 0;
  virtual float GetMorph(RE::TESObjectREFR *, const char *, const char *) = 0;
  virtual void ClearMorph(RE::TESObjectREFR *, const char *, const char *) = 0;
  virtual float GetBodyMorphs(RE::TESObjectREFR *, const char *) = 0;
  virtual void ClearBodyMorphNames(RE::TESObjectREFR *, const char *) = 0;
  virtual void VisitMorphs(RE::TESObjectREFR *, MorphVisitor &) = 0;
  virtual void VisitKeys(RE::TESObjectREFR *, const char *,
                         MorphKeyVisitor &) = 0;
  virtual void VisitMorphValues(RE::TESObjectREFR *, MorphValueVisitor &) = 0;
  virtual void ClearMorphs(RE::TESObjectREFR *) = 0;
  virtual void ApplyVertexDiff(RE::TESObjectREFR *, RE::NiAVObject *,
                               bool = false) = 0;
  virtual void ApplyBodyMorphs(RE::TESObjectREFR *, bool = true) = 0;
  virtual void UpdateModelWeight(RE::TESObjectREFR *, bool = false) = 0;
  virtual void SetCacheLimit(std::size_t) = 0;
  virtual bool HasMorphs(RE::TESObjectREFR *) = 0;
  virtual std::uint32_t EvaluateBodyMorphs(RE::TESObjectREFR *) = 0;
  virtual bool HasBodyMorph(RE::TESObjectREFR *, const char *,
                            const char *) = 0;
  virtual bool HasBodyMorphName(RE::TESObjectREFR *, const char *) = 0;
  virtual bool HasBodyMorphKey(RE::TESObjectREFR *, const char *) = 0;
  virtual void ClearBodyMorphKeys(RE::TESObjectREFR *, const char *) = 0;
  virtual void VisitStrings(StringVisitor &) = 0;
  virtual void VisitActors(ActorVisitor &) = 0;
  virtual std::size_t ClearMorphCache() = 0;
};

class INiTransformInterface : public IPluginInterface {
public:
  struct Position {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
  };

  struct Rotation {
    float heading{0.0F};
    float attitude{0.0F};
    float bank{0.0F};
  };

  class NodeVisitor {
  public:
    virtual bool VisitPosition(const char *, const char *, Position &) = 0;
    virtual bool VisitRotation(const char *, const char *, Rotation &) = 0;
    virtual bool VisitScale(const char *, const char *, float) = 0;
    virtual bool VisitScaleMode(const char *, const char *, std::uint32_t) = 0;
  };

  // Keep the complete public v3 ABI in declaration order. SFS uses only the
  // position and update methods, but omitting an earlier virtual would shift
  // every later call into the wrong RaceMenu slot.
  virtual bool HasNodeTransformPosition(RE::TESObjectREFR *, bool, bool,
                                        const char *, const char *) = 0;
  virtual bool HasNodeTransformRotation(RE::TESObjectREFR *, bool, bool,
                                        const char *, const char *) = 0;
  virtual bool HasNodeTransformScale(RE::TESObjectREFR *, bool, bool,
                                     const char *, const char *) = 0;
  virtual bool HasNodeTransformScaleMode(RE::TESObjectREFR *, bool, bool,
                                         const char *, const char *) = 0;
  virtual void AddNodeTransformPosition(RE::TESObjectREFR *, bool, bool,
                                        const char *, const char *,
                                        Position &) = 0;
  virtual void AddNodeTransformRotation(RE::TESObjectREFR *, bool, bool,
                                        const char *, const char *,
                                        Rotation &) = 0;
  virtual void AddNodeTransformScale(RE::TESObjectREFR *, bool, bool,
                                     const char *, const char *, float) = 0;
  virtual void AddNodeTransformScaleMode(RE::TESObjectREFR *, bool, bool,
                                         const char *, const char *,
                                         std::uint32_t) = 0;
  virtual Position GetNodeTransformPosition(RE::TESObjectREFR *, bool, bool,
                                            const char *, const char *) = 0;
  virtual Rotation GetNodeTransformRotation(RE::TESObjectREFR *, bool, bool,
                                            const char *, const char *) = 0;
  virtual float GetNodeTransformScale(RE::TESObjectREFR *, bool, bool,
                                      const char *, const char *) = 0;
  virtual std::uint32_t GetNodeTransformScaleMode(RE::TESObjectREFR *, bool,
                                                  bool, const char *,
                                                  const char *) = 0;
  virtual bool RemoveNodeTransformPosition(RE::TESObjectREFR *, bool, bool,
                                           const char *, const char *) = 0;
  virtual bool RemoveNodeTransformRotation(RE::TESObjectREFR *, bool, bool,
                                           const char *, const char *) = 0;
  virtual bool RemoveNodeTransformScale(RE::TESObjectREFR *, bool, bool,
                                        const char *, const char *) = 0;
  virtual bool RemoveNodeTransformScaleMode(RE::TESObjectREFR *, bool, bool,
                                            const char *, const char *) = 0;
  virtual bool RemoveNodeTransform(RE::TESObjectREFR *, bool, bool,
                                   const char *, const char *) = 0;
  virtual void RemoveAllReferenceTransforms(RE::TESObjectREFR *) = 0;
  virtual bool GetOverrideNodeTransform(RE::TESObjectREFR *, bool, bool,
                                        const char *, const char *,
                                        std::uint16_t, RE::NiTransform *) = 0;
  virtual void UpdateNodeAllTransforms(RE::TESObjectREFR *) = 0;
  virtual void VisitNodes(RE::TESObjectREFR *, bool, bool, NodeVisitor &) = 0;
  virtual void UpdateNodeTransforms(RE::TESObjectREFR *, bool, bool,
                                    const char *) = 0;
};


} // namespace sfs::native::racemenu::abi
