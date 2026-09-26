#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-number-field.h>
#include <engine/gui/gui-progress-bar.h>
#include <engine/gui/gui-renderer.h>

using namespace eng;

TEST_CASE("a number field steps, scrubs and stays in range") {
  GuiNumberField field;
  field.rect = {0, 0, 120, 28};
  field.min = 0;
  field.max = 10;
  field.step = 1;
  double reported = -1;
  field.on_change = [&](double v) {
    reported = v;
  };
  field.handleMouseDown({.x = 115, .y = 10});  // + stepper
  CHECK(field.value == 1.0);
  CHECK(reported == 1.0);
  field.handleMouseDown({.x = 60, .y = 10});  // scrub from the middle
  field.handleMouseMove({.x = 80, .y = 10});  // 20 px at 4 a step
  CHECK(field.value == 6.0);
  field.handleMouseMove({.x = 300, .y = 10});
  CHECK(field.value == 10.0);
  field.handleMouseUp({});
  field.handleNav(GuiNavCommand::LEFT);
  CHECK(field.value == 9.0);
  field.handleScroll({.delta_y = 1.0f});
  CHECK(field.value == 10.0);
}

TEST_CASE("a number field rounds to its places and shows its suffix") {
  GuiNumberField field;
  field.max = 1;
  field.decimals = 2;
  field.suffix = "%";
  field.setValue(0.4567);
  CHECK(field.value == 0.46);
  CHECK(field.text() == "0.46%");
}

TEST_CASE("a progress bar fills to its value; an indeterminate one sweeps") {
  GuiProgressBar bar;
  bar.rect = {0, 0, 200, 6};
  bar.value = 0.25f;
  GuiRendererContext renderer;
  renderer.beginFrame();
  GuiDrawContext ctx;
  ctx.renderer = &renderer;
  bar.render(ctx);
  REQUIRE(renderer.vertices.size() == 8);  // track, fill
  CHECK(renderer.vertices[4 + 1].pos[0] == 50.0f);
  bar.indeterminate = true;
  bar.update(ctx, 0.5f);
  renderer.beginFrame();
  bar.render(ctx);
  CHECK(renderer.vertices.size() == 8);
  CHECK(bar.measureContent(ctx, -1).h == 6.0f);
}
