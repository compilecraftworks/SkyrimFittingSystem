#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>

namespace sfs::input::character_rotation {
inline constexpr float kMaximumStep = 0.060f;
inline float MouseRadians(float pixels) {
  return std::isfinite(pixels) ? std::clamp(-pixels * 0.003f, -kMaximumStep, kMaximumStep) : 0.0f;
}
// Copied scalars, never retained Skyrim event pointers. No controller polling
// loop; input and presentation threads exchange only these atomic values.
class GamepadState {
public:
  void SetTrigger(float value) { trigger_.store(FiniteClamp(value, 0.0f, 1.0f)); }
  void SetRightX(float value) { rightX_.store(FiniteClamp(value, -1.0f, 1.0f)); }
  void Reset() { trigger_.store(0); rightX_.store(0); }
  bool Held() const { return trigger_.load() > 0.12f; }
  float Radians(float seconds) const {
    if (!Held() || !std::isfinite(seconds) || seconds <= 0.0f) { return 0.0f; }
    const auto axis = rightX_.load();
    const auto magnitude = std::abs(axis);
    constexpr float deadZone = 0.20f;
    if (magnitude <= deadZone) { return 0.0f; }
    const auto scaled = std::copysign((magnitude - deadZone) / (1.0f - deadZone), axis);
    // 120 degrees/second, frame-rate independent and bounded like right-drag.
    return std::clamp(-scaled * 2.0943951f * seconds, -kMaximumStep, kMaximumStep);
  }
private:
  static float FiniteClamp(float value, float minimum, float maximum) {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : 0.0f;
  }
  std::atomic<float> trigger_{0}, rightX_{0};
};
} // namespace sfs::input::character_rotation
