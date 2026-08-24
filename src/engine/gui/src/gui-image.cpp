#include "engine/gui/gui-image.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/image-loader.h"

#include <engine/render/rhi-texture-desc.h>

namespace eng {
namespace {

  /// Build an RHI texture descriptor from decoded image data.
  RhiTextureDesc buildGuiTextureDesc(const ImageData& image) {
    RhiTextureDesc desc{};
    desc.width = image.width;
    desc.height = image.height;
    desc.format = RhiFormat::RGB_A8_SRGB;
    desc.usage = RhiTextureUsage::SAMPLED;
    desc.debug_name = "gui-image";
    desc.initial_pixels = image.pixels.data();
    return desc;
  }

}  // namespace

std::unique_ptr<GuiWidget> GuiImage::clone() const {
  return std::make_unique<GuiImage>(*this);
}

void GuiImage::render(const GuiDrawContext& ctx) const {
  if (ctx.renderer == nullptr || texture_handle == 0) {
    return;
  }
  ctx.drawTexturedRect({rect, texture_handle, tint_color});
}

std::optional<RhiTextureHandle> GuiImage::loadTexture(RhiDevice& device,
                                                      std::string_view path) {
  auto image = ImageLoader::loadFromFile(path);
  if (!image.has_value()) {
    return std::nullopt;
  }

  auto desc = buildGuiTextureDesc(*image);
  auto handle = device.createTexture(desc);
  return (handle != 0) ? std::optional{handle} : std::nullopt;
}

}  // namespace eng
