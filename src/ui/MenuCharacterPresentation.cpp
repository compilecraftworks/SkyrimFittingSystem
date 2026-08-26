#include "ui/MenuCharacterPresentation.h"

#include "imgui.h"
#include "native/SmoothCamIntegration.h"
#include "ui/MenuHost.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace sfs::ui {
namespace {
// Use direct, symmetric third-person camera offsets for the SFS layout.  The
// Show Player In Menus-style `-requestedX - 75` conversion is intentionally
// not used here: applying the same requested magnitude to both sides produces
// asymmetric camera values (+90 / -240) because of the shoulder baseline.
constexpr float kLeftCameraHorizontalOffset = 78.0f;
constexpr float kRightCameraHorizontalOffset = -78.0f;
constexpr float kCameraVerticalOffset = -30.0f;
constexpr float kCameraDistance = 150.0f;
constexpr float kMenuWorldFov = 90.0f;
constexpr float kPlayerPitch = 0.27f;
constexpr float kLeftFacingCorrection = 0.35f;
constexpr float kRightFacingCorrection = -0.35f;
constexpr float kMouseRotationRadiansPerPixel = 0.003f;
constexpr float kMaxMouseRotationRadiansPerFrame = 0.060f;

float NormalizeAngle(float a_angle) {
  constexpr auto twoPi = std::numbers::pi_v<float> * 2.0f;
  while (a_angle > std::numbers::pi_v<float>) {
    a_angle -= twoPi;
  }
  while (a_angle < -std::numbers::pi_v<float>) {
    a_angle += twoPi;
  }
  return a_angle;
}

float VectorLength(const RE::NiPoint3 &a_vector) {
  return std::sqrt(a_vector.x * a_vector.x + a_vector.y * a_vector.y +
                   a_vector.z * a_vector.z);
}

RE::NiPoint3 ProjectVector(const RE::NiPoint3 &a_vector,
                           const RE::NiPoint3 &a_axis) {
  const auto denominator = a_axis.x * a_axis.x + a_axis.y * a_axis.y +
                           a_axis.z * a_axis.z;
  if (denominator <= 0.0001f) {
    return {};
  }

  const auto scale = (a_vector.x * a_axis.x + a_vector.y * a_axis.y +
                      a_vector.z * a_axis.z) /
                     denominator;
  return a_axis * scale;
}

RE::NiPoint2 RotateVector(const RE::NiPoint2 &a_vector, const float a_angle) {
  const auto sine = std::sin(a_angle);
  const auto cosine = std::cos(a_angle);
  return {a_vector.x * cosine - a_vector.y * sine,
          a_vector.x * sine + a_vector.y * cosine};
}

float GetAngle(const RE::NiPoint2 &a_from, const RE::NiPoint2 &a_to) {
  const auto cross = a_from.x * a_to.y - a_from.y * a_to.x;
  const auto dot = a_from.x * a_to.x + a_from.y * a_to.y;
  return std::atan2(cross, dot);
}

float GetCameraAlignedActorYaw(RE::Actor *a_actor,
                               RE::PlayerCamera *a_camera) {
  if (a_actor == nullptr || a_camera == nullptr ||
      a_camera->cameraRoot == nullptr) {
    return a_actor != nullptr ? a_actor->data.angle.z : 0.0f;
  }

  const auto actorPosition = a_actor->GetPosition();
  const auto cameraPosition = a_camera->cameraRoot->world.translate;
  const auto targetPosition = a_actor->GetLookingAtLocation();

  auto actorDirectionToTarget = targetPosition - actorPosition;
  if (VectorLength(actorDirectionToTarget) <= 0.0001f) {
    return a_actor->data.angle.z;
  }
  actorDirectionToTarget.Unitize();

  const auto cameraToActor = actorPosition - cameraPosition;
  const auto projected = ProjectVector(cameraToActor, actorDirectionToTarget);
  const auto projectedPosition = cameraPosition + projected;
  auto projectedDirectionToTarget = targetPosition - projectedPosition;
  if (VectorLength(projectedDirectionToTarget) <= 0.0001f) {
    return a_actor->data.angle.z;
  }
  projectedDirectionToTarget.Unitize();

  const auto currentCameraDirection =
      RotateVector({0.0f, 1.0f}, a_actor->data.angle.z);
  const RE::NiPoint2 projectedDirection{-projectedDirectionToTarget.x,
                                        projectedDirectionToTarget.y};
  return NormalizeAngle(a_actor->data.angle.z +
                        GetAngle(currentCameraDirection, projectedDirection));
}

RE::ThirdPersonState *GetThirdPersonState(RE::PlayerCamera *a_camera) {
  if (a_camera == nullptr) {
    return nullptr;
  }

  const auto &state =
      a_camera->GetRuntimeData().cameraStates[RE::CameraState::kThirdPerson];
  return state != nullptr ? static_cast<RE::ThirdPersonState *>(state.get())
                          : nullptr;
}

bool CanPresentActor(RE::PlayerCharacter *a_player, RE::Actor *a_actor,
                     RE::PlayerCamera *a_camera,
                     RE::ThirdPersonState *a_thirdPersonState) {
  if (a_player == nullptr || a_actor == nullptr || a_camera == nullptr ||
      a_thirdPersonState == nullptr || !a_player->Is3DLoaded() ||
      !a_actor->Is3DLoaded() ||
      a_player->IsOnMount() || a_camera->IsInFreeCameraMode()) {
    return false;
  }

  const auto sitSleepState = a_player->AsActorState()->GetSitSleepState();
  if (sitSleepState >= RE::SIT_SLEEP_STATE::kWantToSit) {
    return false;
  }

  return a_camera->currentState.get() == a_thirdPersonState;
}
} // namespace

struct MenuCharacterPresentation::State {
  struct SavedSetting {
    RE::Setting *setting{nullptr};
    float originalValue{0.0f};
  };

  bool active{false};
  bool rotating{false};
  MenuCharacterSide side{MenuCharacterSide::Disabled};
  MenuCharacterSide requestedSide{MenuCharacterSide::Disabled};
  RE::ActorHandle presentedActorHandle{};
  RE::ActorHandle requestedActorHandle{};
  RE::ActorHandle originalCameraTarget{};
  RE::TESCameraState *originalCameraState{nullptr};
  RE::NiPoint3 posOffsetExpected{};
  RE::NiPoint3 posOffsetActual{};
  RE::NiPoint3 desiredPosOffset{};
  RE::NiPoint2 freeRotation{};
  float actorAngleX{0.0f};
  float actorAngleZ{0.0f};
  bool actorPitchModified{false};
  float targetZoomOffset{0.0f};
  float pitchZoomOffset{0.0f};
  float worldFov{0.0f};
  bool freeRotationEnabled{false};
  bool toggleAnimCam{false};
  bool headTrackingEnabled{false};
  bool headTrackingModified{false};
  std::array<SavedSetting, 9> cameraSettings{};
};

MenuCharacterPresentation *MenuCharacterPresentation::GetSingleton() {
  static MenuCharacterPresentation singleton;
  static State state;
  singleton.state_ = std::addressof(state);
  return std::addressof(singleton);
}

void MenuCharacterPresentation::Apply(const MenuCharacterSide a_side) {
  Apply(a_side, RE::PlayerCharacter::GetSingleton());
}

void MenuCharacterPresentation::Apply(const MenuCharacterSide a_side,
                                      RE::Actor *a_actor) {
  if (a_side == MenuCharacterSide::Disabled) {
    Restore();
    return;
  }

  auto *player = RE::PlayerCharacter::GetSingleton();
  auto *presentedActor = a_actor != nullptr ? a_actor : player;
  const auto requestedActorHandle =
      presentedActor != nullptr ? presentedActor->GetHandle() : RE::ActorHandle{};

  if (state_->active) {
    if (state_->side == a_side &&
        state_->presentedActorHandle == requestedActorHandle) {
      state_->requestedSide = a_side;
      state_->requestedActorHandle = requestedActorHandle;
      return;
    }
    Restore();
  }
  // On the first frame of a menu opening Skyrim can still be transitioning
  // camera states.  Keep the request so the ordinary per-frame menu update can
  // apply it as soon as third person becomes stable.
  state_->requestedSide = a_side;
  state_->requestedActorHandle = requestedActorHandle;

  auto *camera = RE::PlayerCamera::GetSingleton();
  auto *thirdPersonState = GetThirdPersonState(camera);
  if (!CanPresentActor(player, presentedActor, camera, thirdPersonState)) {
    logger::debug("Skipped SFS menu character presentation for the current "
                  "actor/camera state");
    return;
  }
  if (!native::smoothcam::AcquireCameraControl()) {
    return;
  }

  state_->originalCameraState = camera->currentState.get();
  state_->originalCameraTarget = camera->cameraTarget;
  state_->presentedActorHandle = requestedActorHandle;
  state_->posOffsetExpected = thirdPersonState->posOffsetExpected;
  state_->posOffsetActual = thirdPersonState->posOffsetActual;
  state_->freeRotation = thirdPersonState->freeRotation;
  state_->actorAngleX = presentedActor->data.angle.x;
  state_->actorAngleZ = presentedActor->data.angle.z;
  state_->actorPitchModified = presentedActor == player;
  state_->targetZoomOffset = thirdPersonState->targetZoomOffset;
  state_->pitchZoomOffset = thirdPersonState->pitchZoomOffset;
  state_->worldFov = camera->worldFOV;
  state_->freeRotationEnabled = thirdPersonState->freeRotationEnabled;
  state_->toggleAnimCam = thirdPersonState->toggleAnimCam;
  state_->side = a_side;
  state_->rotating = false;
  state_->active = true;

  camera->cameraTarget = requestedActorHandle;
  auto cameraTargetHandle = requestedActorHandle.native_handle();
  thirdPersonState->SetCameraHandle(cameraTargetHandle);

  // Match Show Player In Menus' RotatePlayer application order.  Reasserting
  // the already-active third-person state makes Skyrim rebuild its camera
  // offsets from the temporary menu settings instead of a stale tween frame.
  camera->SetState(thirdPersonState);
  thirdPersonState->freeRotationEnabled = true;
  thirdPersonState->toggleAnimCam = true;

  if (presentedActor == player &&
      player->GetGraphVariableBool("IsNPC", state_->headTrackingEnabled)) {
    player->SetGraphVariableBool("IsNPC", false);
    state_->headTrackingModified = true;
  }

  const auto cameraAlignedYaw =
      GetCameraAlignedActorYaw(presentedActor, camera);
  // Start from a near-frontal view for SFS's off-centre camera framing.
  // Rotation remains a user-controlled right-drag action after the menu opens.
  const auto facingCorrection =
      a_side == MenuCharacterSide::Left ? kLeftFacingCorrection
                                        : kRightFacingCorrection;
  const auto angleChange = std::numbers::pi_v<float> + facingCorrection;
  const auto menuFreeRotation = angleChange;
  presentedActor->SetHeading(NormalizeAngle(cameraAlignedYaw - angleChange));
  if (state_->actorPitchModified) {
    presentedActor->data.angle.x = kPlayerPitch;
  }
  thirdPersonState->freeRotation =
      {NormalizeAngle(menuFreeRotation), 0.0f};

  const auto horizontalOffset =
      a_side == MenuCharacterSide::Left ? kLeftCameraHorizontalOffset
                                        : kRightCameraHorizontalOffset;
  state_->desiredPosOffset =
      {horizontalOffset, 0.0f, kCameraVerticalOffset};
  if (auto *ini = RE::INISettingCollection::GetSingleton(); ini != nullptr) {
    const std::array settings{
        std::pair{"fOverShoulderCombatPosX:Camera", horizontalOffset},
        std::pair{"fOverShoulderCombatAddY:Camera", 0.0f},
        std::pair{"fOverShoulderCombatPosZ:Camera", kCameraVerticalOffset},
        std::pair{"fOverShoulderPosX:Camera", horizontalOffset},
        std::pair{"fOverShoulderPosZ:Camera", kCameraVerticalOffset},
        std::pair{"fAutoVanityModeDelay:Camera", 10800.0f},
        std::pair{"fVanityModeMinDist:Camera", kCameraDistance},
        std::pair{"fVanityModeMaxDist:Camera", kCameraDistance},
        std::pair{"fMouseWheelZoomSpeed:Camera", 10000.0f},
    };
    for (std::size_t index = 0; index < settings.size(); ++index) {
      auto *setting = ini->GetSetting(settings[index].first);
      if (setting == nullptr) {
        continue;
      }
      state_->cameraSettings[index] = {setting, setting->GetFloat()};
      setting->data.f = settings[index].second;
    }
  }
  thirdPersonState->pitchZoomOffset = 0.1f;
  thirdPersonState->posOffsetExpected = state_->desiredPosOffset;
  thirdPersonState->posOffsetActual = state_->desiredPosOffset;
  camera->worldFOV = kMenuWorldFov;

  camera->Update();
  presentedActor->Update3DPosition(true);
  logger::debug("Applied SFS menu character presentation: side={}, actor={:08X}",
                static_cast<std::uint8_t>(a_side),
                presentedActor->GetFormID());
}

void MenuCharacterPresentation::Restore() {
  // Covers every menu-closing path, including a window shutdown that happens
  // while the right mouse button is still held.
  MenuHost::EndCharacterRotationUnpause();
  if (state_ == nullptr) {
    return;
  }
  state_->requestedSide = MenuCharacterSide::Disabled;
  state_->requestedActorHandle.reset();
  if (!state_->active) {
    native::smoothcam::ReleaseCameraControl();
    return;
  }

  auto presentedActor = state_->presentedActorHandle.get();
  auto *camera = RE::PlayerCamera::GetSingleton();
  auto *thirdPersonState = GetThirdPersonState(camera);

  if (camera != nullptr) {
    camera->cameraTarget = state_->originalCameraTarget;
  }
  if (thirdPersonState != nullptr) {
    auto originalCameraTargetHandle =
        state_->originalCameraTarget.native_handle();
    thirdPersonState->SetCameraHandle(originalCameraTargetHandle);
  }

  if (camera != nullptr && state_->originalCameraState != nullptr &&
      camera->currentState.get() != state_->originalCameraState) {
    camera->SetState(state_->originalCameraState);
  }

  if (presentedActor != nullptr) {
    if (state_->actorPitchModified) {
      presentedActor->data.angle.x = state_->actorAngleX;
    }
    presentedActor->SetHeading(state_->actorAngleZ);
    if (state_->headTrackingModified) {
      presentedActor->SetGraphVariableBool("IsNPC",
                                           state_->headTrackingEnabled);
    }
    if (presentedActor->Is3DLoaded()) {
      presentedActor->Update3DPosition(true);
    }
  }

  if (thirdPersonState != nullptr) {
    thirdPersonState->posOffsetExpected = state_->posOffsetExpected;
    thirdPersonState->posOffsetActual = state_->posOffsetActual;
    thirdPersonState->freeRotation = state_->freeRotation;
    thirdPersonState->targetZoomOffset = state_->targetZoomOffset;
    thirdPersonState->pitchZoomOffset = state_->pitchZoomOffset;
    thirdPersonState->freeRotationEnabled = state_->freeRotationEnabled;
    thirdPersonState->toggleAnimCam = state_->toggleAnimCam;
  }

  for (const auto &[setting, originalValue] : state_->cameraSettings) {
    if (setting != nullptr) {
      setting->data.f = originalValue;
    }
  }

  if (camera != nullptr) {
    camera->worldFOV = state_->worldFov;
    camera->Update();
  }

  state_->originalCameraState = nullptr;
  state_->presentedActorHandle.reset();
  state_->originalCameraTarget.reset();
  state_->desiredPosOffset = {};
  state_->cameraSettings = {};
  state_->headTrackingModified = false;
  state_->actorPitchModified = false;
  state_->side = MenuCharacterSide::Disabled;
  state_->rotating = false;
  state_->active = false;
  native::smoothcam::ReleaseCameraControl();
  logger::debug("Restored SFS menu character presentation");
}

void MenuCharacterPresentation::UpdateRotationInteraction() {
  if (state_ == nullptr || ImGui::GetCurrentContext() == nullptr) {
    MenuHost::EndCharacterRotationUnpause();
    return;
  }
  if (!state_->active &&
      state_->requestedSide != MenuCharacterSide::Disabled) {
    const auto requestedSide = state_->requestedSide;
    auto requestedActor = state_->requestedActorHandle.get();
    Apply(requestedSide, requestedActor.get());
  }
  if (!state_->active) {
    MenuHost::EndCharacterRotationUnpause();
    return;
  }

  auto &io = ImGui::GetIO();
  const bool popupOpen =
      ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  const bool overSfsWindow =
      ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow |
                             ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  const bool onCharacterSide = state_->side == MenuCharacterSide::Right
                                   ? io.MousePos.x >= io.DisplaySize.x * 0.55f
                                   : io.MousePos.x <= io.DisplaySize.x * 0.45f;

  // The game may only advance while a deliberate character rotation is in
  // progress.  Losing app focus is important here: Windows can swallow the
  // matching right-button-up event when the game is alt-tabbed.
  if (state_->rotating &&
      (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || popupOpen ||
       io.AppFocusLost)) {
    state_->rotating = false;
    MenuHost::EndCharacterRotationUnpause();
  }

  if (!state_->rotating && !popupOpen && !overSfsWindow && onCharacterSide &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    state_->rotating = true;
    // Fast SMP intentionally suspends all simulation while UI::GameIsPaused.
    // Temporarily release SFS's own pause count so its nodes can follow this
    // real player-heading rotation.  Gameplay controls stay blocked by SFS's
    // menu context; release/focus loss restores the pause immediately.
    MenuHost::BeginCharacterRotationUnpause();
  }

  auto presentedActor = state_->presentedActorHandle.get();
  auto *camera = RE::PlayerCamera::GetSingleton();
  auto *thirdPersonState = GetThirdPersonState(camera);
  if (presentedActor == nullptr || camera == nullptr ||
      thirdPersonState == nullptr ||
      camera->currentState.get() != thirdPersonState) {
    state_->rotating = false;
    MenuHost::EndCharacterRotationUnpause();
    return;
  }

  // Camera state updates can recompute offsets from the active camera mode.
  // Keep the SFS-owned menu framing stable while the menu is open.
  thirdPersonState->posOffsetExpected = state_->desiredPosOffset;
  thirdPersonState->posOffsetActual = state_->desiredPosOffset;
  if (camera->cameraTarget != state_->presentedActorHandle) {
    camera->cameraTarget = state_->presentedActorHandle;
    auto cameraTargetHandle = state_->presentedActorHandle.native_handle();
    thirdPersonState->SetCameraHandle(cameraTargetHandle);
  }

  if (!state_->rotating || io.MouseDelta.x == 0.0f) {
    return;
  }

  // Large cursor deltas used to teleport the actor by tens or hundreds of
  // degrees in one frame.  SMP bones then retained their previous world-space
  // transforms and appeared pinned to the background.  Preserve proportional
  // mouse control, but keep each physics-visible rotation step small enough
  // for the equipped nodes to follow the actor.
  const auto delta =
      std::clamp(-io.MouseDelta.x * kMouseRotationRadiansPerPixel,
                 -kMaxMouseRotationRadiansPerFrame,
                 kMaxMouseRotationRadiansPerFrame);
  presentedActor->SetHeading(
      NormalizeAngle(presentedActor->data.angle.z + delta));
  thirdPersonState->freeRotation.x =
      NormalizeAngle(thirdPersonState->freeRotation.x - delta);
  presentedActor->Update3DPosition(true);
  camera->Update();
}

} // namespace sfs::ui
