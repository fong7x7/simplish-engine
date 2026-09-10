#include "engine/client/rendered-game-client.h"

#include <algorithm>
#include <engine/core/logger.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-font-discovery.h>
#include <engine/gui/gui-renderer.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <engine/render/rhi-types.h>

namespace eng::client {
namespace {


  /// Font directory searched under the engine data directory.
  constexpr const char* GUI_FONT_SUBDIR = "fonts";
  /// Regular weight, in the CSS-style scale `loadFont` takes.
  constexpr uint16_t GUI_FONT_WEIGHT = 400;
  /// Layout height of GUI text, in logical pixels. Chrome offsets across the
  /// editor are tuned against this size.
  constexpr uint32_t GUI_TEXT_PIXEL_H = 14;

  /// Byte to unit range, for alpha, which carries no transfer function.
  constexpr float BYTE_TO_FLOAT = 1.0f / 255.0f;

  void setClearColor(RhiRenderPassBeginInfo& rp, const GuiColor& color) {
    // Clear values are linear. The colour attachment is sRGB and encodes on
    // write, so passing the sRGB byte straight through would encode it a
    // second time and clear to a visibly lighter shade.
    rp.clear_color[0] = srgbByteToLinear(color.r);
    rp.clear_color[1] = srgbByteToLinear(color.g);
    rp.clear_color[2] = srgbByteToLinear(color.b);
    rp.clear_color[3] = static_cast<float>(color.a) * BYTE_TO_FLOAT;
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

void RenderedGameClient::beginScenePass(RhiCommandList& cmd, RhiDevice& device,
                                        RhiTextureHandle depth) {
  RhiTextureHandle backbuffer = device.backbufferTexture();
  RhiRenderPassBeginInfo rp{};
  rp.color_targets = &backbuffer;
  rp.color_target_count = 1;
  rp.depth_target = depth;
  rp.depth_load_op = RhiLoadOp::CLEAR;
  rp.clear_depth = 1.0f;
  setClearColor(rp, frameClearColor());
  // The pass under this one already cleared and painted the ground-plane
  // overlays; the scene draws over them, not over a fresh surface.
  rp.color_load_op = RhiLoadOp::LOAD;
  cmd.beginRenderPass(rp);
}

void RenderedGameClient::beginGuiPass(RhiCommandList& cmd, RhiDevice& device,
                                      RhiLoadOp color_load) {
  RhiTextureHandle backbuffer = device.backbufferTexture();
  RhiRenderPassBeginInfo rp{};
  rp.color_targets = &backbuffer;
  rp.color_target_count = 1;
  // No depth: the GUI is painted in draw order, and sharing the scene's
  // depth buffer would let 3D geometry reject interface pixels.
  rp.depth_target = RHI_TEXTURE_INVALID;
  setClearColor(rp, frameClearColor());
  rp.color_load_op = color_load;
  cmd.beginRenderPass(rp);
}

void RenderedGameClient::recordLayeredFrame(RhiCommandList& cmd,
                                            RhiDevice& device,
                                            RhiTextureHandle depth) {
  // Three passes, in paint order: what goes under the scene, the scene, and
  // what goes over it. The split is wherever the GUI marked it. The last
  // pass opens with whatever reads the scene's depth, which can only happen
  // once the pass that had it attached has ended.
  const size_t split = gui_.renderer->sceneSplit();
  const size_t total = gui_.renderer->commands.size();
  gui_.renderer->uploadFrame();

  beginGuiPass(cmd, device, RhiLoadOp::CLEAR);
  gui_.renderer->bindFrame(cmd);
  gui_.renderer->submitCommandRange(cmd, 0, split);
  cmd.endRenderPass();

  beginScenePass(cmd, device, depth);
  recordScene(cmd);
  cmd.endRenderPass();

  beginGuiPass(cmd, device, RhiLoadOp::LOAD);
  recordSceneOverlay(cmd);
  gui_.renderer->bindFrame(cmd);
  gui_.renderer->submitCommandRange(cmd, split, total - split);
  cmd.endRenderPass();
}

void RenderedGameClient::recordFrame(RhiCommandList& cmd, RhiDevice& device) {
  cmd.begin();
  const RhiTextureHandle depth = sceneDepthTarget();
  if (depth == RHI_TEXTURE_INVALID || gui_.renderer->commands.empty()) {
    // Nothing to layer around: one pass, exactly as before any of this.
    beginGuiPass(cmd, device, RhiLoadOp::CLEAR);
    gui_.renderer->endFrame(cmd);
    cmd.endRenderPass();
    cmd.end();
    return;
  }
  recordLayeredFrame(cmd, device, depth);
  cmd.end();
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
    submitFrame(*dev);
  }
}

void RenderedGameClient::submitFrame(RhiDevice& device) {
  if (!device.beginFrame()) {
    return;
  }
  auto cmd = device.createCommandList();
  if (cmd == nullptr) {
    return;
  }
  recordFrame(*cmd, device);
  device.submit(*cmd);
  device.endFrame();
  (void)device.present();
}

}  // namespace eng::client
