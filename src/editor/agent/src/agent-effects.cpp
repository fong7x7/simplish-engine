#include "agent-effects.h"

#include "agent-call.h"
#include "agent-json-values.h"

#include <editor/agent/agent-names.h>
#include <editor/shell/editor-emitter-ops.h>
#include <game/fx/combat-fx-preset.h>
#include <game/fx/combat-fx.h>
#include <optional>
#include <string>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// What `play_effect` says when it is called wrongly.
  constexpr std::string_view PLAY_EFFECT_USAGE =
      "give effect — a preset id list_emitters lists under effects, or "
      "shot_fired, shot_hit_body, shot_hit_wall or blast — with x and y; or "
      "emitter, the index of a placed emitter, to fire its own burst";

  /// Which way the call's dx, dy and dz point, or @p fallback when it gives
  /// none of them.
  Vec3 directionParam(const json& params, Vec3 fallback) {
    if (!params.contains("dx") && !params.contains("dy") &&
        !params.contains("dz")) {
      return fallback;
    }
    return {agentFloatParam(params, "dx", 0.0f),
            agentFloatParam(params, "dy", 0.0f),
            agentFloatParam(params, "dz", 0.0f)};
  }

  /// @p preset once at @p at, pointing where the call says — up unless it
  /// says — at the call's scale.
  EditorEffectShot presetShot(const game::CombatFxPreset& preset, WorldPoint at,
                              const json& params) {
    return {{preset.burst},
            {},
            preset.flash,
            {{at.x, at.y, at.z},
             directionParam(params, {0.0f, 0.0f, 1.0f}),
             agentFloatParam(params, "scale", 1.0f)}};
  }

  /// The whole combat effect a cue of @p kind plays, once at @p at: along
  /// the call's direction for a shot — +X unless it says — and a blast of
  /// `COMBAT_FX_BLAST_RADIUS` times the call's scale.
  EditorEffectShot cueShot(game::CombatCueKind kind, WorldPoint at,
                           const json& params) {
    const float scale = agentFloatParam(params, "scale", 1.0f);
    const Vec3 way = directionParam(params, {1.0f, 0.0f, 0.0f});
    const game::CombatCue cue{kind,
                              {at.x, at.y, at.z},
                              {way.x, way.y},
                              game::COMBAT_FX_BLAST_RADIUS * scale};
    const FxEffect& effect = game::combatCueEffect(kind);
    EditorEffectShot shot{{effect.bursts.begin(), effect.bursts.end()},
                          {effect.volumes.begin(), effect.volumes.end()},
                          effect.flash,
                          game::combatCueEmit(cue)};
    if (kind != game::CombatCueKind::BLAST) {
      shot.emit.scale = scale;
    }
    return shot;
  }

  /// Where the call puts an effect: its x and y, and its z — chest height
  /// unless it says, and the floor for a blast. Nothing without x and y.
  std::optional<WorldPoint> callPoint(const json& params,
                                      std::optional<game::CombatCueKind> cue) {
    const std::optional<double> x = agentNumberParam(params, "x");
    const std::optional<double> y = agentNumberParam(params, "y");
    if (!x || !y) {
      return std::nullopt;
    }
    const float z =
        cue == game::CombatCueKind::BLAST ? 0.0f : EDITOR_EMITTER_DROP_HEIGHT;
    return WorldPoint{static_cast<float>(*x), static_cast<float>(*y),
                      agentFloatParam(params, "z", z)};
  }

  /// The effect `effect` names, where the call puts it. Nothing for a name
  /// that is neither a preset nor a cue, or a call with no x or y.
  std::optional<EditorEffectShot> namedShot(const json& params) {
    const std::string effect = agentStringParam(params, "effect").value_or("");
    const std::optional<game::CombatCueKind> cue = findAgentCombatCue(effect);
    const std::optional<WorldPoint> at = callPoint(params, cue);
    if (!at) {
      return std::nullopt;
    }
    if (const game::CombatFxPreset* preset = game::findCombatFxPreset(effect)) {
      return presetShot(*preset, *at, params);
    }
    return cue ? std::optional{cueShot(*cue, *at, params)} : std::nullopt;
  }

  /// The burst the emitter the call's `emitter` names throws, once, where
  /// it stands. Nothing when there is no emitter at that index.
  std::optional<EditorEffectShot> emitterShot(const EditorShellState& state,
                                              const json& params) {
    const std::optional<size_t> index = agentIndexParam(params, "emitter");
    if (!index || *index >= state.document.emitters.size()) {
      return std::nullopt;
    }
    const EditorEmitter& emitter = state.document.emitters[*index];
    return EditorEffectShot{
        {emitter.burst}, {}, emitter.flash, editorEmitterEmit(emitter)};
  }

  /// What `play_effect` reports it has handed the editor.
  json shotJson(const EditorShellState& state, const EditorEffectShot& shot) {
    uint32_t particles = 0;
    for (const FxBurst& burst : shot.bursts) {
      particles += burst.count;
    }
    const Vec3& at = shot.emit.at;
    return {{"queued", "play_effect"},
            {"into", state.playtest.mode == EditorPlayMode::PLAYING ? "playtest"
                                                                    : "editor"},
            {"at", agentPointJson({at.x, at.y, at.z})},
            {"bursts", shot.bursts.size()},
            {"particles", particles},
            {"volumes", shot.volumes.size()},
            {"flash", shot.flash.intensity}};
  }

  /// Every emitter with the bursts it has thrown.
  json emitterBurstsJson(const EditorShellState& state) {
    json emitters = json::array();
    const auto& bursts = state.effects.emitter_bursts;
    for (size_t i = 0; i < state.document.emitters.size(); ++i) {
      const EditorEmitter& emitter = state.document.emitters[i];
      emitters.push_back({{"index", i},
                          {"id", emitter.id},
                          {"effect", emitter.effect},
                          {"bursts", i < bursts.size() ? bursts[i] : 0}});
    }
    return emitters;
  }

}  // namespace

AgentResult runAgentPlayEffect(EditorShellState& state, const json& params) {
  const std::optional<EditorEffectShot> shot = params.contains("emitter")
                                                   ? emitterShot(state, params)
                                                   : namedShot(params);
  if (!shot) {
    return agentFailure(AgentStatus::BAD_PARAMS, PLAY_EFFECT_USAGE);
  }
  AgentResult result = agentOk(shotJson(state, *shot).dump(2));
  result.host.kind = AgentHostRequestKind::PLAY_EFFECT;
  result.host.effect = *shot;
  return result;
}

std::string agentEffectsStateJson(const EditorShellState& state) {
  const EditorEffectsState& effects = state.effects;
  return json{{"source", state.playtest.mode == EditorPlayMode::PLAYING
                             ? "playtest"
                             : "editor"},
              {"particles", effects.particles},
              {"volumes", effects.volumes},
              {"lights", effects.lights},
              {"shots_played", effects.shots_played},
              {"emitters", emitterBurstsJson(state)}}
      .dump(2);
}

}  // namespace eng::editor
