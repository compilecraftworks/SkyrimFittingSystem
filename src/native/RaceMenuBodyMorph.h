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
    RE::FormID a_armorFormID, bool a_applyInitialMorphs);
// Re-evaluates RaceMenu's equippable transforms for one actor after SFS has
// attached or removed registered-appearance nodes. The task is bounded and
// actor-local; it never scans nearby actors or changes actual equipment.
void QueueRegisteredAppearanceHighHeelSync(RE::Actor *a_actor);
void SetRegisteredAppearanceDisplayActive(RE::Actor *a_actor, bool a_active,
                                          bool a_observeHighHeelAttachments);
// Only real object-unload/form-delete events. Never call for UI hiding or a
// backend refresh that can retain its attachments. No RaceMenu transform edits.
void ReleaseActorSceneResources(RE::FormID a_actorFormID, bool a_deleted);
void ForgetAllRegisteredAppearanceNodes();
} // namespace sfs::native::racemenu
