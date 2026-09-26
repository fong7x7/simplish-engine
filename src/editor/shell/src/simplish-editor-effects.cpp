// SimplishEditor's particle emitters: placing one from general > effects,
// the Effect row and the burst's numbers in the properties panel, and the
// bursts they throw — into the editor's own effects while the level is
// edited, and into the playtest's while it is played.

#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-emitter-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/simplish-editor.h>
#include <utility>

namespace eng::editor {

void SimplishEditor::placeEmitter(WorldPoint tile) {
  // Over the middle of the tile it was dropped on, at chest height, where
  // a shot's sparks would fly from.
  const WorldPoint position{tile.x + 0.5f, tile.y + 0.5f,
                            EDITOR_EMITTER_DROP_HEIGHT};
  const size_t added = state_.document.emitters.size();
  EditorEmitter emitter =
      makeEditorEmitter(EDITOR_EMITTER_DEFAULT_EFFECT, position);
  emitter.id = mintEditorEmitterId(state_.document);
  recordAction({.kind = EditorActionKind::ADD_EMITTER,
                .index = added,
                .emitter = emitter});
  select({EditorSelectionKind::EMITTER, added});
}

void SimplishEditor::showEmitterSelection(EditorPropertiesWidget& panel) {
  const EditorEmitter& emitter =
      state_.document.emitters[state_.selection.index];
  panel.setSelection(editorEmitterName(emitter), emitter);
  EditorEffectChoices choices = editorEffectChoices(emitter);
  panel.addChoices(EditorChoiceKind::EFFECT, std::move(choices.names),
                   choices.current);
}

void SimplishEditor::applyEmitterEdit(EditorPropertyField field, float value,
                                      EditorPropertyEdit edit) {
  EditorEmitter& emitter = state_.document.emitters[state_.selection.index];
  if (!emitter_prior_.has_value()) {
    emitter_prior_ = emitter;
  }
  setEditorEmitterValue(emitter, field, value);
  if (edit == EditorPropertyEdit::COMMIT) {
    commitEmitterEdit();
  }
}

void SimplishEditor::commitEmitterEdit() {
  if (!emitter_prior_.has_value()) {
    return;
  }
  const auto prior = *std::exchange(emitter_prior_, std::nullopt);
  if (!editSubjectSelected(EditorSelectionKind::EMITTER)) {
    return;
  }
  const auto& emitter = state_.document.emitters[state_.selection.index];
  if (sameEditorEmitter(prior, emitter)) {
    return;
  }
  recordAction({.kind = EditorActionKind::TRANSFORM_EMITTER,
                .index = state_.selection.index,
                .emitter = emitter,
                .emitter_prior = prior});
}

void SimplishEditor::applyEffectChoice(size_t index) {
  if (isPlaying() || !editSubjectSelected(EditorSelectionKind::EMITTER)) {
    return;
  }
  EditorEmitter& emitter = state_.document.emitters[state_.selection.index];
  // Worked out again rather than remembered from when the panel was shown,
  // as the other rows' choices are.
  const EditorEffectChoices choices = editorEffectChoices(emitter);
  if (index < choices.ids.size()) {
    if (!emitter_prior_.has_value()) {
      emitter_prior_ = emitter;
    }
    (void)applyEditorEmitterEffect(emitter, choices.ids[index]);
    commitEmitterEdit();
  }
}

EditorPlacementMarker SimplishEditor::emitterMarker(size_t index) {
  return {editorEmitterBounds(state_.document.emitters[index]),
          isSelected(EditorSelectionKind::EMITTER, index),
          EditorMarkerStyle::EMITTER};
}

const FxWorld& SimplishEditor::activeEffects() const {
  return isPlaying() ? playtest_->effects() : edit_fx_;
}

FxWorld& SimplishEditor::activeEffects() {
  return isPlaying() ? playtest_->effects() : edit_fx_;
}

void SimplishEditor::playEffectShot(const EditorEffectShot& shot) {
  playFxEffect(activeEffects(), {shot.bursts, shot.volumes, shot.flash},
               shot.emit);
  ++state_.effects.shots_played;
}

void SimplishEditor::publishEffects() {
  const FxWorld& effects = activeEffects();
  state_.effects.particles = effects.particles.live;
  state_.effects.volumes = effects.volumes.live;
  state_.effects.lights = effects.lights.live;
  const std::span<const uint64_t> bursts = emitter_player_.bursts();
  state_.effects.emitter_bursts.assign(bursts.begin(), bursts.end());
}

void SimplishEditor::tickEditEffects(float seconds) {
  if (isPlaying()) {
    return;
  }
  stepFxWorld(edit_fx_, seconds);
  emitter_player_.advance(state_.document.emitters, seconds, edit_fx_);
}

void SimplishEditor::advancePlaytestEffects(float seconds) {
  if (playtest_->gamePaused()) {
    return;
  }
  playtest_->stepEffects(seconds);
  emitter_player_.advance(state_.document.emitters, seconds,
                          playtest_->effects());
}

void SimplishEditor::resetEditEffects() {
  clearFxWorld(edit_fx_);
  emitter_player_.reset();
}

}  // namespace eng::editor
