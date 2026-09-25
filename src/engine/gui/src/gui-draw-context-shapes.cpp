/// @file gui-draw-context-shapes.cpp
/// @brief `GuiDrawContext`'s CSS-style shapes: boxes, gradients, shadows
/// and nine-slice images, built on `GuiRendererContext::emitShape`.

#include <algorithm>
#include <array>
#include <cmath>
#include <engine/gui/gui-draw-context.h>
#include <numbers>

namespace eng {

namespace {

  /// A gradient's angle as `GuiVertex::param` takes it: radians from
  /// pointing right, where the gradient's degrees are from pointing up.
  float gradientRadians(float css_degrees) {
    return (css_degrees - 90.0f) * std::numbers::pi_v<float> / 180.0f;
  }

  /// @p radii, each no more than half @p rect's shorter side.
  GuiCorners clampRadii(const GuiCorners& radii, const Rect& rect) {
    const float most = std::max(0.0f, std::min(rect.w, rect.h) * 0.5f);
    return {std::min(radii.top_left, most), std::min(radii.top_right, most),
            std::min(radii.bottom_right, most),
            std::min(radii.bottom_left, most)};
  }

  /// @p radii grown by @p spread, as a spread shadow's corners are.
  GuiCorners grownRadii(const GuiCorners& radii, float spread) {
    return {std::max(radii.top_left + spread, 0.0f),
            std::max(radii.top_right + spread, 0.0f),
            std::max(radii.bottom_right + spread, 0.0f),
            std::max(radii.bottom_left + spread, 0.0f)};
  }

  /// A shape vertex's shared fields for @p radii.
  GuiVertex shapeStyle(const GuiCorners& radii) {
    return {.radii = {radii.top_left, radii.top_right, radii.bottom_right,
                      radii.bottom_left},
            .flags = GUI_VERTEX_SHAPE};
  }

  /// Colour @p style with @p paint's fill or gradient.
  void applyFill(GuiVertex& style, const GuiRectPaint& paint) {
    if (!paint.gradient) {
      style.color = style.color2 = paint.fill.pack();
      return;
    }
    const GuiGradient& g = *paint.gradient;
    style.color = g.from.pack();
    style.color2 = g.to.pack();
    style.flags |= g.kind == GuiGradientKind::LINEAR
                       ? GUI_VERTEX_LINEAR_GRADIENT
                       : GUI_VERTEX_RADIAL_GRADIENT;
    style.param = gradientRadians(g.angle_degrees);
  }

  bool hasBorder(const Edges& border) {
    return border.top > 0.0f || border.right > 0.0f || border.bottom > 0.0f ||
           border.left > 0.0f;
  }

  bool paintsFill(const GuiRectPaint& paint) {
    return paint.gradient.has_value() || paint.fill.a > 0;
  }

  /// One band of a nine-slice along an axis: where it is drawn and which
  /// part of the texture it takes, both as start and length.
  struct SliceBand {
    /// Start in layout pixels.
    float at = 0.0f;
    /// Length in layout pixels.
    float size = 0.0f;
    /// Start in texture coordinates.
    float uv = 0.0f;
    /// Length in texture coordinates.
    float uv_size = 0.0f;
  };

  /// The three bands along one axis: @p start and @p length laid out, a
  /// texture @p texels long with insets @p lead and @p trail texels, the
  /// fixed bands drawn @p scale pixels a texel and shrunk to fit.
  std::array<SliceBand, 3> sliceBands(float start, float length,
                                      std::array<float, 3> texels,
                                      float scale) {
    const float total = std::max(texels[0], 1.0f);
    float lead = texels[1] * scale;
    float trail = texels[2] * scale;
    const float fit = std::min(1.0f, length / std::max(lead + trail, 1e-4f));
    lead *= fit;
    trail *= fit;
    const float u_lead = texels[1] / total;
    const float u_trail = texels[2] / total;
    return {
        {{start, lead, 0.0f, u_lead},
         {start + lead, length - lead - trail, u_lead, 1.0f - u_lead - u_trail},
         {start + length - trail, trail, 1.0f - u_trail, u_trail}}};
  }

  /// One of a nine-slice's nine quads.
  struct Slice {
    /// Its column.
    SliceBand col;
    /// Its row.
    SliceBand row;
    /// The texture.
    uint64_t texture = 0;
    /// Packed tint.
    uint32_t tint = 0;
  };

  void emitSlice(GuiRendererContext& renderer, const Slice& s) {
    if (s.col.size <= 0.0f || s.row.size <= 0.0f) {
      return;
    }
    renderer.emitTexturedQuad(
        {{s.col.at, s.row.at, s.col.size, s.row.size},
         {s.col.uv, s.row.uv, s.col.uv_size, s.row.uv_size},
         s.texture,
         s.tint});
  }

}  // namespace

void GuiDrawContext::drawRect(const GuiRectPaint& paint) const {
  if (renderer == nullptr) {
    return;
  }
  const GuiCorners radii = clampRadii(paint.radii, paint.rect);
  if (paintsFill(paint)) {
    GuiVertex style = shapeStyle(radii);
    applyFill(style, paint);
    renderer->emitShape(paint.rect, style);
  }
  if (hasBorder(paint.border) && paint.border_color.a > 0) {
    GuiVertex ring = shapeStyle(radii);
    ring.color = ring.color2 = paint.border_color.pack();
    const Edges& b = paint.border;
    std::copy_n(std::array<float, 4>{b.top, b.right, b.bottom, b.left}.begin(),
                4, std::begin(ring.border));
    renderer->emitShape(paint.rect, ring);
  }
}

void GuiDrawContext::drawShadow(const Rect& rect, const GuiCorners& radii,
                                const GuiShadow& shadow) const {
  if (renderer == nullptr || shadow.color.a == 0) {
    return;
  }
  const float grow = shadow.spread + shadow.blur;
  const Rect quad{rect.x + shadow.offset_x - grow,
                  rect.y + shadow.offset_y - grow, rect.w + grow * 2.0f,
                  rect.h + grow * 2.0f};
  GuiVertex style =
      shapeStyle(grownRadii(clampRadii(radii, rect), shadow.spread));
  style.color = style.color2 = shadow.color.pack();
  style.flags |= GUI_VERTEX_SHADOW;
  style.param = std::max(shadow.blur, 0.0f);
  renderer->emitShape(quad, style);
}

void GuiDrawContext::drawBox(const Rect& rect, const GuiStateStyle& style,
                             float opacity) const {
  const GuiCorners radii = GuiCorners::all(style.radius);
  GuiShadow shadow = activeTheme().shadow(style.elevation);
  if (style.elevation != GuiElevation::NONE && style.fill.a > 0) {
    shadow.color = GuiColor::applyOpacity(shadow.color, opacity);
    drawShadow(rect, radii, shadow);
  }
  const float bw = style.border.a > 0 ? style.border_width : 0.0f;
  drawRect({.rect = rect,
            .fill = GuiColor::applyOpacity(style.fill, opacity),
            .radii = radii,
            .border = {bw, bw, bw, bw},
            .border_color = GuiColor::applyOpacity(style.border, opacity)});
}

void GuiDrawContext::drawNineSlice(const Rect& rect, const GuiNineSlice& image,
                                   const GuiColor& tint) const {
  if (renderer == nullptr || image.texture == 0) {
    return;
  }
  const Edges& in = image.insets;
  const auto cols = sliceBands(
      rect.x, rect.w, {image.texture_w, in.left, in.right}, image.scale);
  const auto rows = sliceBands(
      rect.y, rect.h, {image.texture_h, in.top, in.bottom}, image.scale);
  for (const SliceBand& row : rows) {
    for (const SliceBand& col : cols) {
      emitSlice(*renderer, {col, row, image.texture, tint.pack()});
    }
  }
}

}  // namespace eng
