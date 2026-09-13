#pragma once

/// @file editor-emitter-ops.h
/// @brief Make, read, write and start particle emitters from presets.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-effect-choices.h>
#include <editor/shell/editor-emitter.h>
#include <editor/shell/editor-placement-bounds.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/iso-projection.h>
#include <engine/render-fx/fx-emit.h>
#include <string>
#include <string_view>

namespace eng::editor {

/// The definition an emitter is saved under, in a level file's `entities`
/// list.
inline constexpr std::string_view EDITOR_EMITTER_DEFINITION =
    "entity:fx_emitter";

/// What the browser and the panel call an emitter.
inline constexpr std::string_view EDITOR_EMITTER_NAME = "Particle Emitter";

/// The preset a dropped emitter starts from: sparks, which show what an
/// emitter is at once — they fly, fall, bounce and flash.
inline constexpr std::string_view EDITOR_EMITTER_DEFAULT_EFFECT = "wall_sparks";

/// How high above the floor a dropped emitter stands, in tiles: chest
/// height, where a shot's sparks fly from.
inline constexpr float EDITOR_EMITTER_DROP_HEIGHT = 0.9f;

/// Half the width of the box an emitter is drawn and picked as.
inline constexpr float EDITOR_EMITTER_MARKER_RADIUS = 0.15f;

/// The shortest interval an emitter bursts at, in seconds — a few frames'
/// worth, so an interval dragged to zero is a stream, not a hang.
inline constexpr float EDITOR_EMITTER_MIN_INTERVAL = 0.02f;

/// A new emitter standing at @p position, started from the preset
/// @p effect — or from `EDITOR_EMITTER_DEFAULT_EFFECT` when there is none
/// by that id.
[[nodiscard]] EditorEmitter makeEditorEmitter(std::string_view effect,
                                              WorldPoint position);

/// Start @p emitter from the preset @p effect: its burst and flash become
/// the preset's, and its position, direction and interval are kept. False,
/// with nothing changed, when there is no preset by that id.
bool applyEditorEmitterEffect(EditorEmitter& emitter, std::string_view effect);

/// Whether @p emitter's burst and flash are exactly its preset's — false
/// once any number of them has been changed, or when it names no preset.
[[nodiscard]] bool editorEmitterIsPreset(const EditorEmitter& emitter);

/// The name line the panel shows: `Particle Emitter · Wall Sparks`.
[[nodiscard]] std::string editorEmitterName(const EditorEmitter& emitter);

/// What the Effect row offers @p emitter: every preset, marking the one it
/// was started from `(edited)` once it has been changed. One naming a
/// preset there is none of — a hand-edited file — is offered last, as
/// `<id> (unknown)`.
[[nodiscard]] EditorEffectChoices
editorEffectChoices(const EditorEmitter& emitter);

/// Current value of one of an emitter's properties; zero for a field it
/// does not have.
[[nodiscard]] float editorEmitterValue(const EditorEmitter& emitter,
                                       EditorPropertyField field);

/// Write one of an emitter's properties, normalised as
/// `normalizeEditorPropertyValue` defines. A field it does not have is
/// ignored.
void setEditorEmitterValue(EditorEmitter& emitter, EditorPropertyField field,
                           float value);

/// Whether @p field is one an emitter has: `EDITOR_EMITTER_FIELDS`.
[[nodiscard]] bool editorEmitterHasField(EditorPropertyField field);

/// Whether two emitters are the same to the last bit — the test for "this
/// edit changed nothing", which then records nothing.
[[nodiscard]] bool sameEditorEmitter(const EditorEmitter& a,
                                     const EditorEmitter& b);

/// The box an emitter is drawn and picked as, centred on its position.
[[nodiscard]] PlacementBounds editorEmitterBounds(const EditorEmitter& emitter);

/// Where and which way @p emitter's bursts go off.
[[nodiscard]] FxEmit editorEmitterEmit(const EditorEmitter& emitter);

}  // namespace eng::editor
