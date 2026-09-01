#pragma once

#include <cmath>
#include <numbers>

namespace sfs::ui::camera_projection {

inline bool SetHorizontalFov(RE::NiFrustum &a_frustum,
                             const float a_degrees) noexcept {
  if (a_frustum.bOrtho || !std::isfinite(a_degrees) || a_degrees <= 0.0f ||
      a_degrees >= 179.0f) {
    return false;
  }

  const auto currentHalfWidth =
      std::abs(a_frustum.fRight - a_frustum.fLeft) * 0.5f;
  if (!std::isfinite(currentHalfWidth) || currentHalfWidth <= 0.0001f) {
    return false;
  }

  const auto desiredHalfWidth =
      std::tan(a_degrees * std::numbers::pi_v<float> / 360.0f);
  const auto scale = desiredHalfWidth / currentHalfWidth;
  if (!std::isfinite(scale)) {
    return false;
  }

  a_frustum.fLeft *= scale;
  a_frustum.fRight *= scale;
  a_frustum.fTop *= scale;
  a_frustum.fBottom *= scale;
  return true;
}

} // namespace sfs::ui::camera_projection
