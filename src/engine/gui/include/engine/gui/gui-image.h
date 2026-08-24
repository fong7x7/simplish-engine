#pragma once

#include "gui-color.h"
#include "gui-widget-type.h"
#include "gui-widget.h"

#include <engine/render/rhi-device.h>
#include <optional>
#include <string_view>

namespace eng {

/// A widget that displays a raster image (PNG, JPG, BMP, TGA, GIF).
/// The image is loaded from disk into an RHI texture and rendered as a
/// textured quad spanning the widget's rect.
/// @thread_safety Main thread only.
class GuiImage : public GuiWidget {
public:
  GuiImage() { widget_type = GuiWidgetType::IMAGE; }

  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render this image (textured quad with optional tint).
  void render(const GuiDrawContext& ctx) const override;

  /// Load an image file and upload it as an RHI texture.
  /// Returns the texture handle, or std::nullopt on failure.
  static std::optional<RhiTextureHandle> loadTexture(RhiDevice& device,
                                                     std::string_view path);

  /// GPU texture handle for the loaded image (0 = no image loaded).
  RhiTextureHandle texture_handle = 0;
  /// Tint color multiplied with the texture (white = no tint).
  GuiColor tint_color{255, 255, 255, 255};
};

}  // namespace eng
