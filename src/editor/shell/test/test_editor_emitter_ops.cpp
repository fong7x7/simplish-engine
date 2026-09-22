#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-emitter-ops.h>
#include <editor/shell/editor-property-traits.h>
#include <game/fx/combat-fx-preset.h>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// An emitter standing at (2.5, 3.5, 0.9), started from the default preset.
EditorEmitter testEmitter() {
  return makeEditorEmitter(EDITOR_EMITTER_DEFAULT_EFFECT, {2.5f, 3.5f, 0.9f});
}

}  // namespace

TEST_CASE("a new emitter throws its preset's burst, straight up, each second") {
  const EditorEmitter emitter = testEmitter();
  const game::CombatFxPreset* sparks =
      game::findCombatFxPreset(EDITOR_EMITTER_DEFAULT_EFFECT);
  REQUIRE(sparks != nullptr);

  REQUIRE(emitter.effect == EDITOR_EMITTER_DEFAULT_EFFECT);
  REQUIRE(emitter.burst == sparks->burst);
  REQUIRE(emitter.flash.intensity == sparks->flash.intensity);
  REQUIRE(emitter.direction.z == 1.0f);
  REQUIRE(emitter.interval == 1.0f);
  REQUIRE(editorEmitterIsPreset(emitter));
}

TEST_CASE("an emitter asked for a preset there is none of takes the default") {
  const EditorEmitter emitter = makeEditorEmitter("no_such_effect", {});
  REQUIRE(emitter.effect == EDITOR_EMITTER_DEFAULT_EFFECT);
}

TEST_CASE("starting from a preset keeps where it stands and how often") {
  EditorEmitter emitter = testEmitter();
  emitter.interval = 0.25f;
  emitter.direction = {1.0f, 0.0f, 0.0f};

  REQUIRE(applyEditorEmitterEffect(emitter, "smoke"));
  REQUIRE(emitter.effect == "smoke");
  REQUIRE(emitter.burst == game::findCombatFxPreset("smoke")->burst);
  REQUIRE(emitter.interval == 0.25f);
  REQUIRE(emitter.direction.x == 1.0f);
  REQUIRE(emitter.position.x == 2.5f);
  REQUIRE_FALSE(applyEditorEmitterEffect(emitter, "no_such_effect"));
  REQUIRE(emitter.effect == "smoke");
}

TEST_CASE("the Effect row offers every preset, and marks the one edited") {
  EditorEmitter emitter = testEmitter();
  EditorEffectChoices choices = editorEffectChoices(emitter);
  REQUIRE(choices.ids.size() == game::combatFxPresets().size());
  REQUIRE(choices.ids[choices.current] == EDITOR_EMITTER_DEFAULT_EFFECT);
  REQUIRE(choices.names[choices.current] == "Wall Sparks");

  setEditorEmitterValue(emitter, EditorPropertyField::PARTICLES, 40.0f);
  REQUIRE_FALSE(editorEmitterIsPreset(emitter));
  choices = editorEffectChoices(emitter);
  REQUIRE(choices.names[choices.current] == "Wall Sparks (edited)");
  REQUIRE(editorEmitterName(emitter) == "Particle Emitter · Wall Sparks");
}

TEST_CASE("an emitter naming a preset the editor lacks is offered as unknown") {
  EditorEmitter emitter = testEmitter();
  emitter.effect = "plasma";
  const EditorEffectChoices choices = editorEffectChoices(emitter);

  REQUIRE(choices.current == choices.ids.size() - 1);
  REQUIRE(choices.ids.back() == "plasma");
  REQUIRE(choices.names.back() == "plasma (unknown)");
}

TEST_CASE("every emitter field reads back what was written to it") {
  for (const EditorPropertyField field : EDITOR_EMITTER_FIELDS) {
    EditorEmitter emitter = testEmitter();
    setEditorEmitterValue(emitter, field, 0.5f);
    INFO("field " << editorPropertyFieldLabel(field));
    // A half rounds up to one slot, and lands on a toggle's far side.
    const bool snapped = field == EditorPropertyField::PARTICLES ||
                         editorPropertyFieldIsToggle(field);
    const float expected = snapped ? 1.0f : 0.5f;
    REQUIRE(editorEmitterValue(emitter, field) == Approx(expected));
    REQUIRE(editorEmitterHasField(field));
  }
  REQUIRE_FALSE(editorEmitterHasField(EditorPropertyField::SCALE));
}

TEST_CASE("emitter fields are held to what each can mean") {
  EditorEmitter emitter = testEmitter();
  setEditorEmitterValue(emitter, EditorPropertyField::SPREAD, 400.0f);
  setEditorEmitterValue(emitter, EditorPropertyField::START_R, 3.0f);
  setEditorEmitterValue(emitter, EditorPropertyField::LIFE_MIN, -1.0f);
  setEditorEmitterValue(emitter, EditorPropertyField::PARTICLES, 999.0f);
  setEditorEmitterValue(emitter, EditorPropertyField::GRAVITY, -2.5f);

  REQUIRE(emitter.burst.spread_degrees == 180.0f);
  REQUIRE(emitter.burst.look.color_start.r == 1.0f);
  REQUIRE(emitter.burst.life_min == 0.0f);
  REQUIRE(emitter.burst.count == 200);
  // Negative gravity is smoke rising, not a mistake.
  REQUIRE(emitter.burst.look.gravity == -2.5f);
}

TEST_CASE("an emitter is a small box on its position, and bursts from it") {
  const EditorEmitter emitter = testEmitter();
  const PlacementBounds bounds = editorEmitterBounds(emitter);
  REQUIRE(bounds.min.x == Approx(2.5f - EDITOR_EMITTER_MARKER_RADIUS));
  REQUIRE(bounds.max.z == Approx(0.9f + EDITOR_EMITTER_MARKER_RADIUS));

  const FxEmit emit = editorEmitterEmit(emitter);
  REQUIRE(emit.at.y == 3.5f);
  REQUIRE(emit.direction.z == 1.0f);
  REQUIRE(emit.scale == 1.0f);
}

TEST_CASE("two emitters are the same only to the last bit") {
  const EditorEmitter a = testEmitter();
  EditorEmitter b = a;
  REQUIRE(sameEditorEmitter(a, b));
  b.burst.look.drag += 0.01f;
  REQUIRE_FALSE(sameEditorEmitter(a, b));
}
