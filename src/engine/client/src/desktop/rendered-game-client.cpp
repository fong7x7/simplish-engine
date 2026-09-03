#include "engine/client/rendered-game-client.h"

#include <algorithm>
#include <engine/core/logger.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-font-discovery.h>
#include <engine/gui/gui-renderer.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <engine/render/rhi-types.h>

namespace eng::client {
namespace {

  /// Dark background clear color (byte values, converted to 0–1 float range).
  constexpr float BYTE_TO_FLOAT = 1.0f / 255.0f;
  constexpr float CLEAR_RED = 30.0f * BYTE_TO_FLOAT;
  constexpr float CLEAR_GREEN = 30.0f * BYTE_TO_FLOAT;
  constexpr float CLEAR_BLUE = 34.0f * BYTE_TO_FLOAT;
  constexpr float CLEAR_ALPHA = 1.0f;

  /// Font directory searched under the engine data directory.
  constexpr const char* GUI_FONT_SUBDIR = "fonts";
  /// Regular weight, in the CSS-style scale `loadFont` takes.
  constexpr uint16_t GUI_FONT_WEIGHT = 400;
  /// Layout height of GUI text, in logical pixels. Chrome offsets across the
  /// editor are tuned against this size.
  constexpr uint32_t GUI_TEXT_PIXEL_H = 14;

  void setDarkClearColors(RhiRenderPassBeginInfo& rp) {
    rp.clear_color[0] = CLEAR_RED;
    rp.clear_color[1] = CLEAR_GREEN;
    rp.clear_color[2] = CLEAR_BLUE;
    rp.clear_color[3] = CLEAR_ALPHA;
    rp.color_load_op = RhiLoadOp::CLEAR;
  }

  void syncGuiRendererFromContext(GuiContext& gui, RhiDevice* dev) {
    if (gui.renderer == nullptr) {
      return;
    }
    gui.renderer->viewport_width = std::max(1u, gui.viewport_width);
    gui.renderer->viewport_height = std::max(1u, gui.viewport_height);
    if (dev != nullptr) {
      gui.renderer->surface_width = dev->backbufferWidth();
      gui.renderer->surface_height = dev->backbufferHeight();
    }
  }

  void recordGuiRenderPass(GuiRendererContext& gui_r, RhiDevice& dev,
                           RhiCommandList& cmd) {
    cmd.begin();
    RhiTextureHandle bb = dev.backbufferTexture();
    RhiRenderPassBeginInfo rp{};
    rp.color_targets = &bb;
    rp.color_target_count = 1;
    rp.depth_target = RHI_TEXTURE_INVALID;
    setDarkClearColors(rp);
    cmd.beginRenderPass(rp);
    gui_r.endFrame(cmd);
    cmd.endRenderPass();
    cmd.end();
  }

  void submitGuiToDevice(GuiRendererContext& gui_r, RhiDevice& dev) {
    if (!dev.beginFrame()) {
      return;
    }
    auto cmd = dev.createCommandList();
    if (cmd == nullptr) {
      return;
    }
    recordGuiRenderPass(gui_r, dev, *cmd);
    dev.submit(*cmd);
    dev.endFrame();
    (void)dev.present();
  }

}  // namespace

bool RenderedGameClient::onInit() {
  eng::RhiDevice* dev = rhiDevice();
  if (dev == nullptr) {
    return false;
  }
  if (!gui_.init(guiLayoutWidth(), guiLayoutHeight(), dev)) {
    return false;
  }
  loadGuiFont();
  syncGuiRendererSurfaceFromDevice();
  return true;
}

std::filesystem::path RenderedGameClient::guiFontDirectory() {
  const EngineContext* ctx = engine();
  if (ctx == nullptr || ctx->config.data_dir.empty()) {
    return {};
  }
  return std::filesystem::path(ctx->config.data_dir) / GUI_FONT_SUBDIR;
}

void RenderedGameClient::loadGuiFont() {
  auto font = eng::selectGuiUiFont(guiFontDirectory());
  if (!font.has_value()) {
    LOG_WARN("gui", "No UI font found; text will draw as placeholder boxes");
    return;
  }
  auto face = gui_.text_pipeline->loadFont(font->file_path, GUI_FONT_WEIGHT,
                                           eng::FontLoadItalic::NORMAL);
  if (!face.has_value()) {
    LOG_WARN("gui", "Could not load UI font: " + font->file_path);
    return;
  }
  gui_text_face_id_ = *face;
  gui_.text_pipeline->setFontRasterHeight(gui_text_face_id_, GUI_TEXT_PIXEL_H,
                                          textRasterSupersample());
  LOG_INFO("gui", "UI font: " + font->family + " (" + font->file_path + ")");
}

void RenderedGameClient::onShutdown() {
  if (gui_.tree != nullptr) {
    gui_.tree->clearComponents();
  }
  gui_.shutdown();
}

void RenderedGameClient::resizeGuiToBackbuffer() {
  gui_.resize(guiLayoutWidth(), guiLayoutHeight());
  syncGuiRendererSurfaceFromDevice();
}

void RenderedGameClient::syncGuiRendererSurfaceFromDevice() {
  if (gui_.renderer == nullptr) {
    return;
  }
  eng::RhiDevice* dev = rhiDevice();
  if (dev == nullptr) {
    return;
  }
  gui_.renderer->surface_width = dev->backbufferWidth();
  gui_.renderer->surface_height = dev->backbufferHeight();
}

void RenderedGameClient::guiDispatchMouseDown(
    const eng::GuiMouseEvent& event) const {
  if (gui_.tree == nullptr) {
    return;
  }
  gui_.tree->dispatchMouseDown(event);
}

void RenderedGameClient::guiDispatchMouseUp(
    const eng::GuiMouseEvent& event) const {
  if (gui_.tree == nullptr) {
    return;
  }
  gui_.tree->dispatchMouseUp(event);
}

void RenderedGameClient::guiDispatchMouseMove(
    const eng::GuiMouseEvent& event) const {
  if (gui_.tree == nullptr) {
    return;
  }
  gui_.tree->dispatchMouseMove(event);
}

void RenderedGameClient::guiDispatchScroll(
    const eng::GuiScrollEvent& event) const {
  if (gui_.tree == nullptr) {
    return;
  }
  gui_.tree->dispatchScroll(event);
}

bool RenderedGameClient::guiDispatchText(std::string_view text) const {
  if (gui_.tree == nullptr) {
    return false;
  }
  return gui_.tree->dispatchText(text);
}

bool RenderedGameClient::guiDispatchKey(uint32_t keycode) const {
  if (gui_.tree == nullptr) {
    return false;
  }
  return gui_.tree->dispatchKey(keycode);
}

bool RenderedGameClient::guiDispatchKey(const eng::GuiKeyEvent& event) const {
  if (gui_.tree == nullptr) {
    return false;
  }
  return gui_.tree->dispatchKey(event);
}

GuiDrawContext RenderedGameClient::guiDrawContext() const {
  GuiDrawContext ctx{};
  ctx.renderer = gui_.renderer.get();
  ctx.text_pipeline = gui_.text_pipeline.get();
  ctx.face_id = guiTextFaceId();
  return ctx;
}

void RenderedGameClient::presentGuiFrame() {
  auto& gui = gui_;
  if (gui.renderer == nullptr || gui.tree == nullptr) {
    return;
  }
  eng::RhiDevice* dev = rhiDevice();
  syncGuiRendererFromContext(gui, dev);
  gui.renderer->beginFrame();
  gui.tree->renderAll(guiDrawContext());
  if (dev != nullptr) {
    submitGuiToDevice(*gui.renderer, *dev);
  }
}

}  // namespace eng::client
