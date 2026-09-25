#include "gui-quad-shading.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace eng {

namespace {

  constexpr float BYTE_TO_UNIT = 1.0f / 255.0f;
  /// Border widths below this are no border.
  constexpr float MIN_BORDER = 1.0e-5f;

  /// A point or size in the quad's pixels.
  struct Vec2 {
    /// Across.
    float x = 0.0f;
    /// Down.
    float y = 0.0f;
  };

  GuiShadedPixel unpack(uint32_t packed) {
    return {static_cast<float>(packed & 0xFFU) * BYTE_TO_UNIT,
            static_cast<float>((packed >> 8U) & 0xFFU) * BYTE_TO_UNIT,
            static_cast<float>((packed >> 16U) & 0xFFU) * BYTE_TO_UNIT,
            static_cast<float>((packed >> 24U) & 0xFFU) * BYTE_TO_UNIT};
  }

  GuiShadedPixel mix(const GuiShadedPixel& a, const GuiShadedPixel& b,
                     float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t};
  }

  /// `gui_sd_round_rect`: signed distance from @p p to the rounded rect of
  /// half-size @p half whose corner radii are @p radii (tl, tr, br, bl).
  float sdRoundRect(Vec2 p, Vec2 half, const std::array<float, 4>& radii) {
    const float corner = p.x < 0.0f ? (p.y < 0.0f ? radii[0] : radii[3])
                                    : (p.y < 0.0f ? radii[1] : radii[2]);
    const float r = std::clamp(corner, 0.0f, std::min(half.x, half.y));
    const float qx = std::abs(p.x) - half.x + r;
    const float qy = std::abs(p.y) - half.y + r;
    const float outside = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f));
    return outside + std::min(std::max(qx, qy), 0.0f) - r;
  }

  float smoothstep(float lo, float hi, float x) {
    const float t = std::clamp((x - lo) / (hi - lo), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
  }

  /// `gui_cover`, with one pixel standing in for `fwidth`.
  float cover(float d, float soft) {
    const float w = std::max(soft, 1.0f);
    return 1.0f - smoothstep(-w, w, d);
  }

  std::array<float, 4> radiiOf(const GuiVertex& v) {
    return {v.radii[0], v.radii[1], v.radii[2], v.radii[3]};
  }

  /// `gui_fill`.
  GuiShadedPixel fill(const GuiVertex& v, Vec2 p) {
    const GuiShadedPixel from = unpack(v.color);
    if ((v.flags & GUI_VERTEX_LINEAR_GRADIENT) != 0) {
      const Vec2 dir{std::cos(v.param), std::sin(v.param)};
      const float len = std::abs(v.rect_w * dir.x) + std::abs(v.rect_h * dir.y);
      const float t = (p.x * dir.x + p.y * dir.y) / std::max(len, 1e-4f) + 0.5f;
      return mix(from, unpack(v.color2), std::clamp(t, 0.0f, 1.0f));
    }
    if ((v.flags & GUI_VERTEX_RADIAL_GRADIENT) != 0) {
      const float t = std::hypot(p.x / std::max(v.rect_w * 0.5f, 1e-4f),
                                 p.y / std::max(v.rect_h * 0.5f, 1e-4f));
      return mix(from, unpack(v.color2), std::clamp(t, 0.0f, 1.0f));
    }
    return from;
  }

  bool hasBorder(const GuiVertex& v) {
    return std::ranges::any_of(v.border,
                               [](float w) { return w > MIN_BORDER; });
  }

  /// The inner radii of a border ring: each corner's radius less the
  /// wider of the two sides that meet there.
  std::array<float, 4> innerRadii(const GuiVertex& v) {
    const float t = v.border[0];
    const float r = v.border[1];
    const float b = v.border[2];
    const float l = v.border[3];
    return {std::max(v.radii[0] - std::max(t, l), 0.0f),
            std::max(v.radii[1] - std::max(t, r), 0.0f),
            std::max(v.radii[2] - std::max(b, r), 0.0f),
            std::max(v.radii[3] - std::max(b, l), 0.0f)};
  }

  /// `gui_border_cover`.
  float borderCover(const GuiVertex& v, Vec2 p, Vec2 half) {
    const float outer = cover(sdRoundRect(p, half, radiiOf(v)), 0.0f);
    const float t = v.border[0];
    const float r = v.border[1];
    const float b = v.border[2];
    const float l = v.border[3];
    const Vec2 inner_half{half.x - (l + r) * 0.5f, half.y - (t + b) * 0.5f};
    if (inner_half.x <= 0.0f || inner_half.y <= 0.0f) {
      return outer;
    }
    const Vec2 inner_p{p.x - (l - r) * 0.5f, p.y - (t - b) * 0.5f};
    return outer *
           (1.0f -
            cover(sdRoundRect(inner_p, inner_half, innerRadii(v)), 0.0f));
  }

  /// How much of the pixel at @p p the quad's shape covers.
  float coverage(const GuiVertex& v, Vec2 p) {
    const Vec2 half{v.rect_w * 0.5f, v.rect_h * 0.5f};
    if ((v.flags & GUI_VERTEX_SHADOW) != 0) {
      const Vec2 shape{std::max(half.x - v.param, 0.0f),
                       std::max(half.y - v.param, 0.0f)};
      return cover(sdRoundRect(p, shape, radiiOf(v)), v.param * 0.5f);
    }
    if (hasBorder(v)) {
      return borderCover(v, p, half);
    }
    if ((v.flags & GUI_VERTEX_SHAPE) != 0) {
      return cover(sdRoundRect(p, half, radiiOf(v)), 0.0f);
    }
    return 1.0f;
  }

}  // namespace

GuiShadedPixel shadeGuiShape(const GuiVertex& quad, float px, float py) {
  GuiShadedPixel out = fill(quad, {px, py});
  out.a *= coverage(quad, {px, py});
  return out;
}

}  // namespace eng
