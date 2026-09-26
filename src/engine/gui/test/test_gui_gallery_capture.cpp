// The widget gallery: every widget, in the dark and the light theme, drawn
// by the software rasterizer to gui-gallery-dark-capture.png and
// gui-gallery-light-capture.png at the repository root — gitignored
// pictures to look at after changing how anything is drawn.

#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-card.h>
#include <engine/gui/gui-checkbox.h>
#include <engine/gui/gui-font-discovery.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-number-field.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-progress-bar.h>
#include <engine/gui/gui-radio-group.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-slider.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/gui-tabs.h>
#include <engine/gui/gui-text-input.h>
#include <engine/gui/gui-toggle.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/text-pipeline.h>
#include <memory>
#include <optional>
#include <string>

using namespace eng;

namespace {

/// The picture's size.
constexpr float GALLERY_W = 760.0f;
constexpr float GALLERY_H = 560.0f;

/// A text pipeline with the machine's UI font, when it has one.
struct GalleryFont {
  TextPipelineContext pipeline{};
  std::optional<uint32_t> face{};

  GalleryFont() {
    const auto chosen = pipeline.init() ? selectGuiUiFont({}) : std::nullopt;
    face = chosen ? pipeline.loadFontFamily(chosen->file_path) : std::nullopt;
    if (face) {
      // Any handle but 0 lets glyph quads be emitted with no device.
      pipeline.atlases.at(0).texture = 1;
    }
  }
  ~GalleryFont() { pipeline.shutdown(); }
  GalleryFont(const GalleryFont&) = delete;
  GalleryFont& operator=(const GalleryFont&) = delete;
  GalleryFont(GalleryFont&&) = delete;
  GalleryFont& operator=(GalleryFont&&) = delete;
};

/// A tree of every widget, drawn in one theme.
class Gallery {
public:
  explicit Gallery(const GuiTheme& theme) : theme_(theme) {
    auto& page = panel(GUI_WIDGET_ID_INVALID);
    page.fill_color = theme.palette.background;
    page.subtree_theme = std::make_shared<GuiTheme>(theme);
    page.tree_layout.padding = {24, 24, 24, 24};
    page.tree_layout.gap = 18;
    root_ = page.widget_id;
    addType();
    addButtons();
    addChoices();
    addFields();
    addProgress();
    addCard();
  }

  /// Turn the layout overlay on, inspecting the card as if the pointer
  /// were over it.
  void inspectCard() {
    tree_.layout_overlay = GuiLayoutOverlay::BOXES;
    tree_.findWidget(root_)->hovered = true;
    tree_.findWidget(card_)->hovered = true;
  }

  /// The gallery drawn, laid out in its picture's size.
  ImageData draw(GalleryFont& font) {
    GuiRendererContext renderer;
    (void)renderer.init(nullptr);
    renderer.viewport_width = static_cast<uint32_t>(GALLERY_W);
    renderer.viewport_height = static_cast<uint32_t>(GALLERY_H);
    const GuiDrawContext ctx = contextFor(renderer, font);
    tree_.computeLayout({0, 0, GALLERY_W, GALLERY_H}, ctx);
    // After layout: tabs place their indicator from where they are laid out.
    tree_.updateAll(ctx, 0.0f);
    tree_.renderAll(ctx);
    ImageData image = rasterize(renderer, font);
    renderer.shutdown();
    return image;
  }

private:
  /// A draw context over @p renderer, with @p font's face when it has one,
  /// every animation landed.
  GuiDrawContext contextFor(GuiRendererContext& renderer, GalleryFont& font) {
    GuiDrawContext ctx;
    ctx.renderer = &renderer;
    ctx.motion = GuiMotion::REDUCED;
    if (font.face) {
      ctx.text_pipeline = &font.pipeline;
      ctx.face_id = *font.face;
    }
    return ctx;
  }

  /// @p renderer's quads as a picture, glyphs from @p font's atlas.
  static ImageData rasterize(const GuiRendererContext& renderer,
                             const GalleryFont& font) {
    const Rect view{0, 0, GALLERY_W, GALLERY_H};
    if (!font.face) {
      return GuiSoftwareRasterizer::rasterizeQuads(renderer.vertices, view,
                                                   GUI_RASTER_DEFAULT_BG);
    }
    const auto& atlas = font.pipeline.atlases.at(0);
    return GuiSoftwareRasterizer::rasterizeQuads(
        renderer.vertices, view, GUI_RASTER_DEFAULT_BG,
        {atlas.rgba_pixels, atlas.width, atlas.height});
  }

  /// A new clear panel under @p parent.
  GuiPanel& panel(GuiWidgetId parent) {
    auto& made = dynamic_cast<GuiPanel&>(
        *tree_.findWidget(tree_.createWidget(GuiWidgetType::PANEL, parent)));
    made.fill_color = {0, 0, 0, 0};
    return made;
  }

  /// A new row under the page, its children centred across it.
  GuiWidgetId row() {
    auto& line = panel(root_);
    line.tree_layout.direction = FlexDirection::ROW;
    line.tree_layout.gap = 12;
    line.tree_layout.align_items = Align::CENTER;
    return line.widget_id;
  }

  /// A new widget of type @p T under @p parent.
  template <typename T> T& add(GuiWidgetId parent) {
    return dynamic_cast<T&>(*tree_.findWidget(
        tree_.insertExternalWidget(std::make_unique<T>(), parent)));
  }

  /// A label under @p parent saying @p text in @p role.
  void label(GuiWidgetId parent, std::string_view text, GuiTextRole role) {
    auto& made = dynamic_cast<GuiLabel&>(
        *tree_.findWidget(tree_.createWidget(GuiWidgetType::TEXT, parent)));
    made.text = text;
    made.role = role;
  }

  void addType() {
    const GuiWidgetId line = row();
    label(line, "Display", GuiTextRole::DISPLAY);
    label(line, "Title", GuiTextRole::TITLE);
    label(line, "Heading", GuiTextRole::HEADING);
    label(line, "Body text", GuiTextRole::BODY);
    label(line, "Caption", GuiTextRole::CAPTION);
  }

  /// A button under @p parent saying @p text as @p variant.
  GuiButton& button(GuiWidgetId parent, std::string_view text,
                    GuiButtonVariant variant) {
    auto& made = dynamic_cast<GuiButton&>(
        *tree_.findWidget(tree_.createWidget(GuiWidgetType::BUTTON, parent)));
    made.label = text;
    made.variant = variant;
    made.tree_layout.padding = {8, 16, 8, 16};
    return made;
  }

  void addButtons() {
    const GuiWidgetId line = row();
    (void)button(line, "Neutral", GuiButtonVariant::NEUTRAL);
    (void)button(line, "Primary", GuiButtonVariant::PRIMARY);
    (void)button(line, "Danger", GuiButtonVariant::DANGER);
    (void)button(line, "Ghost", GuiButtonVariant::GHOST);
    button(line, "Selected", GuiButtonVariant::NEUTRAL).selected = true;
    button(line, "Disabled", GuiButtonVariant::PRIMARY).disabled = true;
  }

  void addChoices() {
    const GuiWidgetId line = row();
    auto& on = add<GuiCheckbox>(line);
    on.label = "Checked";
    on.state = GuiCheckState::CHECKED;
    add<GuiCheckbox>(line).label = "Unchecked";
    auto& flipped = add<GuiToggle>(line);
    flipped.label = "On";
    flipped.on = true;
    flipped.tree_layout.width = 90;
    auto& radio = add<GuiRadioGroup>(line);
    radio.options = {"Easy", "Normal", "Hard"};
    radio.selected = 1;
  }

  void addFields() {
    const GuiWidgetId line = row();
    auto& name = add<GuiTextInput>(line);
    name.setText("Ada of the Outpost");
    name.tree_layout.width = 200;
    auto& count = add<GuiNumberField>(line);
    count.value = 42;
    count.suffix = "%";
    count.tree_layout.width = 100;
    auto& volume = add<GuiSlider>(line);
    volume.value = 0.6f;
    volume.tree_layout.width = 180;
    volume.tree_layout.height = 20;
  }

  void addProgress() {
    const GuiWidgetId line = row();
    auto& tabs = add<GuiTabs>(line);
    tabs.tabs = {"Weapons", "Armour", "Perks"};
    tabs.selected = 1;
    tabs.tree_layout.width = 300;
    auto& bar = add<GuiProgressBar>(line);
    bar.value = 0.6f;
    bar.tree_layout.width = 220;
  }

  void addCard() {
    auto& card = add<GuiCard>(root_);
    card_ = card.widget_id;
    card.id = "card";
    card.tree_layout.width = 420;
    card.tree_layout.margin = {4, 0, 12, 0};
    card.tree_layout.padding = {16, 16, 16, 16};
    card.tree_layout.gap = 8;
    label(card.widget_id, "A card", GuiTextRole::HEADING);
    auto& body = dynamic_cast<GuiLabel&>(*tree_.findWidget(
        tree_.createWidget(GuiWidgetType::TEXT, card.widget_id)));
    body.text = "Raised off the page by the theme's low shadow, it lifts "
                "further under the pointer, and its text wraps at its width.";
    body.wrap = GuiTextWrap::WORD;
  }

  /// The theme it is drawn in.
  GuiTheme theme_;
  /// The tree.
  GuiWidgetTree tree_;
  /// The page.
  GuiWidgetId root_ = GUI_WIDGET_ID_INVALID;
  /// The card, which the layout overlay inspects.
  GuiWidgetId card_ = GUI_WIDGET_ID_INVALID;
};

/// Whether the pixel at (4, 4) of @p image is @p color.
bool cornerIs(const ImageData& image, GuiColor color) {
  return image.pixels[16] == color.r && image.pixels[17] == color.g &&
         image.pixels[18] == color.b;
}

}  // namespace

TEST_CASE("the widget gallery draws every widget in the dark theme") {
  GalleryFont font;
  const ImageData image = Gallery(GuiTheme::dark()).draw(font);
  (void)GuiSoftwareRasterizer::writePng(image, "gui-gallery-dark-capture.png");
  REQUIRE(image.width == static_cast<uint32_t>(GALLERY_W));
  CHECK(cornerIs(image, GuiTheme::dark().palette.background));
}

TEST_CASE("the widget gallery draws every widget in the light theme") {
  GalleryFont font;
  const ImageData image = Gallery(GuiTheme::light()).draw(font);
  (void)GuiSoftwareRasterizer::writePng(image, "gui-gallery-light-capture.png");
  REQUIRE(image.width == static_cast<uint32_t>(GALLERY_W));
  CHECK(cornerIs(image, GuiTheme::light().palette.background));
}

TEST_CASE("the layout overlay draws the inspected card's box model") {
  GalleryFont font;
  Gallery gallery(GuiTheme::dark());
  gallery.inspectCard();
  const ImageData image = gallery.draw(font);
  (void)GuiSoftwareRasterizer::writePng(image,
                                        "gui-layout-overlay-capture.png");
  REQUIRE(image.width == static_cast<uint32_t>(GALLERY_W));
}
