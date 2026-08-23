#pragma once

#include <RE/Skyrim.h>

#include <unordered_set>

namespace sfs::native::racemenu {
struct AttachmentSceneSnapshot {
  std::unordered_set<RE::NiAVObject *> thirdPersonObjects;
  std::unordered_set<RE::NiAVObject *> firstPersonObjects;
};

void InitializeBodyMorphInterface();
[[nodiscard]] bool IsBodyMorphInterfaceReady();
[[nodiscard]] AttachmentSceneSnapshot
CaptureAttachmentScene(RE::Actor *a_actor);
void MorphNewRegisteredAppearanceNodes(
    RE::Actor *a_actor, const AttachmentSceneSnapshot &a_before,
    RE::FormID a_armorFormID);
void SetRegisteredAppearanceDisplayActive(RE::Actor *a_actor, bool a_active);
void ForgetRegisteredAppearanceNodes(RE::Actor *a_actor);
void ForgetAllRegisteredAppearanceNodes();
} // namespace sfs::native::racemenu
