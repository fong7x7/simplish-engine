#include <algorithm>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-font-discovery.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/text-pipeline.h>
#include <game/ui/ui-screen-render.h>
#include <game/ui/ui-screen-view.h>
#include <utility>

namespace eng::game {

namespace {

  /// What stands in for the game behind the screen: a plain dark grey.
  constexpr uint32_t STAND_IN_GAME = 0xFF342C28U;

  /// The glyph atlas's texture, when there is no device to make one: the
  /// CPU rasterizer samples atlas pixels, and any handle but 0 lets the
  /// glyph quads be emitted.
  constexpr RhiTextureHandle STAND_IN_TEXTURE = 1;

  /// A text pipeline on the CPU, with the system's UI font loaded when
  /// there is one.
  struct CpuFont {
    /// The pipeline.
    TextPipelineContext pipeline{};
    /// The font, when one loaded.
    std::optional<uint32_t> face{};

    CpuFont() {
      const auto chosen = pipeline.init() ? selectGuiUiFont({}) : std::nullopt;
      face = chosen ? pipeline.loadFontFamily(chosen->file_path) : std::nullopt;
      if (face) {
        pipeline.atlases.at(0).texture = STAND_IN_TEXTURE;
      }
    }
    ~CpuFont() { pipeline.shutdown(); }
    CpuFont(const CpuFont&) = delete;
    CpuFont& operator=(const CpuFont&) = delete;
    CpuFont(CpuFont&&) = delete;
    CpuFont& operator=(CpuFont&&) = delete;
  };

  /// A draw context over @p renderer and, when it has one, @p font.
  GuiDrawContext drawContext(GuiRendererContext& renderer, CpuFont& font) {
    GuiDrawContext ctx;
    ctx.renderer = &renderer;
    if (font.face) {
      ctx.text_pipeline = &font.pipeline;
      ctx.face_id = *font.face;
    }
    return ctx;
  }

  /// @p tree's quads drawn over the stand-in game, with @p font's atlas.
  ImageData rasterize(const GuiRendererContext& renderer, const CpuFont& font,
                      const Rect& view) {
    if (!font.face) {
      return GuiSoftwareRasterizer::rasterizeQuads(renderer.vertices, view,
                                                   STAND_IN_GAME);
    }
    const auto& page = font.pipeline.atlases.at(0);
    return GuiSoftwareRasterizer::rasterizeQuads(
        renderer.vertices, view, STAND_IN_GAME,
        {page.rgba_pixels, page.width, page.height});
  }

  /// @p size, kept within 1 and `UI_RENDER_MAX_SIDE`, as a view.
  Rect viewOf(UiRenderSize size) {
    return makeRect(0.0F, 0.0F,
                    static_cast<float>(std::clamp<uint32_t>(
                        size.width, 1, UI_RENDER_MAX_SIDE)),
                    static_cast<float>(std::clamp<uint32_t>(
                        size.height, 1, UI_RENDER_MAX_SIDE)));
  }

  /// Build @p built over a clear root in @p tree, showing @p values, lay
  /// it out in @p view, and draw it through @p ctx.
  void buildAndDraw(GuiWidgetTree& tree, UiScreenView& built,
                    const UiValues& values,
                    const std::pair<Rect, GuiDrawContext>& target) {
    const GuiWidgetId root =
        tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
    dynamic_cast<GuiPanel&>(*tree.findWidget(root)).fill_color = {0, 0, 0, 0};
    (void)built.build(tree, root);
    (void)built.apply(tree, values);
    tree.computeLayout(target.first, target.second);
    tree.visitDrawOrder([&ctx = target.second](const GuiWidget& widget) {
      if (widget.visible) {
        widget.render(ctx);
      }
    });
  }

}  // namespace

UiScreenRender renderUiScreen(const UiScreen& screen, const UiValues& values,
                              UiRenderSize size) {
  const Rect view = viewOf(size);
  CpuFont font;
  GuiRendererContext renderer;
  (void)renderer.init(nullptr);
  renderer.viewport_width = static_cast<uint32_t>(view.w);
  renderer.viewport_height = static_cast<uint32_t>(view.h);
  GuiWidgetTree tree;
  UiScreenView built(screen, [](std::string_view) {});
  buildAndDraw(tree, built, values, {view, drawContext(renderer, font)});
  UiScreenRender out{rasterize(renderer, font, view), built.buttons(tree),
                     font.face.has_value()};
  renderer.shutdown();
  return out;
}

}  // namespace eng::game
