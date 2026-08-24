#include <engine/gui/gui-context.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-theme.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/text-pipeline.h>
#include <engine/render/rhi-device.h>
#include <memory>
#include <utility>

namespace eng {

/// Default opaque white color for widget background quads.
constexpr uint32_t DEFAULT_WIDGET_COLOR = 0xFFFFFFFF;

namespace {

  void allocateSubsystems(GuiContext& ctx) {
    ctx.tree = std::make_unique<GuiWidgetTree>();
    ctx.theme_stack = std::make_unique<ThemeScopeStack>();
    ctx.text_pipeline = std::make_unique<TextPipelineContext>();
    ctx.renderer = std::make_unique<GuiRendererContext>();
  }

  void initSubsystems(GuiContext& ctx, RhiDevice* device) {
    ctx.text_pipeline->init();
    ctx.text_pipeline->gpu_device = device;
    ctx.renderer->init(device);
  }

  void emitWidgetQuads(GuiContext& ctx) {
    if (ctx.tree == nullptr) {
      return;
    }
    ctx.tree->visitDrawOrder([&](const GuiWidget& node) {
      if (!node.visible) {
        return;
      }
      ctx.renderer->emitQuad({node.rect, DEFAULT_WIDGET_COLOR, 0.0f, 0.0f});
    });
  }

}  // namespace

GuiContext::~GuiContext() = default;

bool GuiContext::init(uint32_t width, uint32_t height, RhiDevice* device) {
  allocateSubsystems(*this);
  viewport_width = width;
  viewport_height = height;
  initSubsystems(*this, device);
  return true;
}

namespace {

  void shutdownSubsystems(GuiContext& ctx) {
    if (ctx.renderer != nullptr) {
      ctx.renderer->shutdown();
    }
    if (ctx.text_pipeline != nullptr) {
      ctx.text_pipeline->shutdown();
    }
    ctx.renderer.reset();
    ctx.text_pipeline.reset();
  }

}  // namespace

void GuiContext::shutdown() {
  shutdownSubsystems(*this);
  if (theme_stack != nullptr) {
    theme_stack->root_theme = nullptr;
  }
  root_theme_storage.reset();
  theme_stack.reset();
  tree.reset();
}

void GuiContext::resize(uint32_t width, uint32_t height) {
  viewport_width = width;
  viewport_height = height;
}

void GuiContext::update(float delta_time) {
  dt = delta_time;
}

void GuiContext::render(RhiCommandList& cmd_list) {
  if (!renderer) {
    return;
  }
  renderer->viewport_width = viewport_width;
  renderer->viewport_height = viewport_height;
  renderer->beginFrame();
  emitWidgetQuads(*this);
  renderer->endFrame(cmd_list);
}

bool GuiContext::applyRootTheme(Theme&& theme) {
  if (!theme_stack) {
    return false;
  }
  root_theme_storage = std::make_unique<Theme>(std::move(theme));
  theme_stack->root_theme = root_theme_storage.get();
  return true;
}

bool GuiContext::setRootTheme(std::string_view theme_path) {
  auto theme = loadTheme(theme_path);
  if (!theme.has_value()) {
    return false;
  }
  return applyRootTheme(std::move(*theme));
}

}  // namespace eng
