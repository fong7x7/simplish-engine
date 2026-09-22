#include "editor-vector-field.h"

#include <algorithm>
#include <cmath>
#include <editor/shell/editor-emitter-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-property-traits.h>
#include <game/fx/combat-fx-preset.h>
#include <iterator>
#include <type_traits>

namespace eng::editor {

namespace {

  using Field = EditorPropertyField;

  /// One property that is a float member of @p Owner.
  template <typename Owner> struct Member {
    /// The property.
    Field field;
    /// The member it names.
    float Owner::* member;
  };

  /// The burst's own numbers.
  constexpr Member<FxBurst> BURST_NUMBERS[] = {
      {Field::SPREAD, &FxBurst::spread_degrees},
      {Field::SPEED_MIN, &FxBurst::speed_min},
      {Field::SPEED_MAX, &FxBurst::speed_max},
      {Field::LIFE_MIN, &FxBurst::life_min},
      {Field::LIFE_MAX, &FxBurst::life_max},
  };

  /// How each of its particles looks and moves, past its colours.
  constexpr Member<FxParticleLook> LOOK_NUMBERS[] = {
      {Field::SIZE_START, &FxParticleLook::size_start},
      {Field::SIZE_END, &FxParticleLook::size_end},
      {Field::GRAVITY, &FxParticleLook::gravity},
      {Field::DRAG, &FxParticleLook::drag},
      {Field::STRETCH, &FxParticleLook::stretch},
      {Field::SPIN, &FxParticleLook::spin},
  };

  /// The flash each burst lights, past its tint.
  constexpr Member<FxFlash> FLASH_NUMBERS[] = {
      {Field::FLASH, &FxFlash::intensity},
      {Field::FLASH_RANGE, &FxFlash::range},
      {Field::FLASH_TIME, &FxFlash::life},
  };

  /// One channel of one of a particle's two colours.
  struct ColorMember {
    /// The property.
    Field field;
    /// Which colour: at birth, or at death.
    FxColor FxParticleLook::* color;
    /// Which channel of it.
    float FxColor::* channel;
  };

  /// Both colours, channel by channel.
  constexpr ColorMember COLOR_NUMBERS[] = {
      {Field::START_R, &FxParticleLook::color_start, &FxColor::r},
      {Field::START_G, &FxParticleLook::color_start, &FxColor::g},
      {Field::START_B, &FxParticleLook::color_start, &FxColor::b},
      {Field::START_HIDE, &FxParticleLook::color_start, &FxColor::a},
      {Field::END_R, &FxParticleLook::color_end, &FxColor::r},
      {Field::END_G, &FxParticleLook::color_end, &FxColor::g},
      {Field::END_B, &FxParticleLook::color_end, &FxColor::b},
      {Field::END_HIDE, &FxParticleLook::color_end, &FxColor::a},
  };

  /// A float of @p Owner, const when @p Owner is.
  template <typename Owner>
  using FloatOf =
      std::conditional_t<std::is_const_v<Owner>, const float, float>;

  /// The member of @p owner that @p table says @p field names, or null.
  template <typename Owner, size_t N>
  FloatOf<Owner>* memberOf(const Member<std::remove_const_t<Owner>> (&table)[N],
                           Owner& owner, Field field) {
    const auto* const found = std::ranges::find(
        table, field, &Member<std::remove_const_t<Owner>>::field);
    return found == std::end(table) ? nullptr : &(owner.*found->member);
  }

  /// The colour channel of @p look that @p field names, or null.
  template <typename Look> FloatOf<Look>* colorOf(Look& look, Field field) {
    const auto* const found =
        std::ranges::find(COLOR_NUMBERS, field, &ColorMember::field);
    return found == std::end(COLOR_NUMBERS)
               ? nullptr
               : &((look.*found->color).*found->channel);
  }

  /// The float @p field names in @p emitter, or null for a field that is
  /// not one float of an emitter's — its position and direction are
  /// triples, reached by component, and its particle count a whole number.
  template <typename Emitter>
  FloatOf<Emitter>* scalarOf(Emitter& emitter, Field field) {
    if (field == Field::EMIT_INTERVAL) {
      return &emitter.interval;
    }
    if (auto* number = memberOf(BURST_NUMBERS, emitter.burst, field)) {
      return number;
    }
    if (auto* number = memberOf(LOOK_NUMBERS, emitter.burst.look, field)) {
      return number;
    }
    if (auto* number = memberOf(FLASH_NUMBERS, emitter.flash, field)) {
      return number;
    }
    return colorOf(emitter.burst.look, field);
  }

  bool isPositionField(Field field) {
    return editorFieldInTriple(field, Field::POSITION_X);
  }

  bool isDirectionField(Field field) {
    return editorFieldInTriple(field, Field::DIRECTION_X);
  }

  /// Whether @p field names one of the two settings a look carries as a
  /// named choice rather than as a number, both shown as toggles.
  bool isLookToggleField(Field field) {
    return field == Field::TEXTURED || field == Field::LIT;
  }

  /// Which way @p look has @p field set, as a toggle reads it.
  float lookToggle(const FxParticleLook& look, Field field) {
    if (field == Field::TEXTURED) {
      return look.shape == FxParticleShape::PUFF ? 1.0f : 0.0f;
    }
    return look.lighting == FxParticleLighting::LIT ? 1.0f : 0.0f;
  }

  /// Set @p field of @p look from a toggle's @p on.
  void setLookToggle(FxParticleLook& look, Field field, float on) {
    if (field == Field::TEXTURED) {
      look.shape = on != 0.0f ? FxParticleShape::PUFF : FxParticleShape::DISC;
    } else {
      look.lighting =
          on != 0.0f ? FxParticleLighting::LIT : FxParticleLighting::EMISSIVE;
    }
  }

  /// Whether two flashes light alike, to the last bit.
  bool sameFlash(const FxFlash& a, const FxFlash& b) {
    return a.color.x == b.color.x && a.color.y == b.color.y &&
           a.color.z == b.color.z && a.intensity == b.intensity &&
           a.range == b.range && a.life == b.life;
  }

  /// The name the Effect row gives the preset @p preset, marked edited when
  /// it is @p emitter's own and has been changed.
  std::string choiceName(const game::CombatFxPreset& preset,
                         const EditorEmitter& emitter) {
    std::string name(preset.name);
    if (preset.id == emitter.effect && !editorEmitterIsPreset(emitter)) {
      name += " (edited)";
    }
    return name;
  }

  static_assert(editorPropertyTraits(Field::PARTICLES).maximum <= 65535.0f,
                "a burst's count is sixteen bits");

}  // namespace

EditorEmitter makeEditorEmitter(std::string_view effect, WorldPoint position) {
  EditorEmitter emitter;
  emitter.position = position;
  if (!applyEditorEmitterEffect(emitter, effect)) {
    (void)applyEditorEmitterEffect(emitter, EDITOR_EMITTER_DEFAULT_EFFECT);
  }
  return emitter;
}

bool applyEditorEmitterEffect(EditorEmitter& emitter, std::string_view effect) {
  const game::CombatFxPreset* preset = game::findCombatFxPreset(effect);
  if (preset == nullptr) {
    return false;
  }
  emitter.effect = std::string(preset->id);
  emitter.burst = preset->burst;
  emitter.flash = preset->flash;
  return true;
}

bool editorEmitterIsPreset(const EditorEmitter& emitter) {
  const game::CombatFxPreset* preset = game::findCombatFxPreset(emitter.effect);
  return preset != nullptr && emitter.burst == preset->burst &&
         sameFlash(emitter.flash, preset->flash);
}

std::string editorEmitterName(const EditorEmitter& emitter) {
  const game::CombatFxPreset* preset = game::findCombatFxPreset(emitter.effect);
  const std::string_view effect =
      preset != nullptr ? preset->name : std::string_view(emitter.effect);
  return std::string(EDITOR_EMITTER_NAME) +
         (effect.empty() ? "" : " · " + std::string(effect));
}

EditorEffectChoices editorEffectChoices(const EditorEmitter& emitter) {
  EditorEffectChoices choices;
  for (const game::CombatFxPreset& preset : game::combatFxPresets()) {
    if (preset.id == emitter.effect) {
      choices.current = choices.ids.size();
    }
    choices.names.push_back(choiceName(preset, emitter));
    choices.ids.emplace_back(preset.id);
  }
  if (game::findCombatFxPreset(emitter.effect) == nullptr) {
    choices.current = choices.ids.size();
    choices.names.push_back(emitter.effect + " (unknown)");
    choices.ids.push_back(emitter.effect);
  }
  return choices;
}

float editorEmitterValue(const EditorEmitter& emitter, Field field) {
  if (isPositionField(field)) {
    return editorVectorValue(emitter.position,
                             editorFieldAxis(field, Field::POSITION_X));
  }
  if (isDirectionField(field)) {
    return editorVectorValue(emitter.direction,
                             editorFieldAxis(field, Field::DIRECTION_X));
  }
  if (field == Field::PARTICLES) {
    return static_cast<float>(emitter.burst.count);
  }
  if (isLookToggleField(field)) {
    return lookToggle(emitter.burst.look, field);
  }
  const float* value = scalarOf(emitter, field);
  return value != nullptr ? *value : 0.0f;
}

void setEditorEmitterValue(EditorEmitter& emitter, Field field, float value) {
  const float written = normalizeEditorPropertyValue(field, value);
  if (isPositionField(field)) {
    editorVectorAxis(emitter.position,
                     editorFieldAxis(field, Field::POSITION_X)) = written;
  } else if (isDirectionField(field)) {
    editorVectorAxis(emitter.direction,
                     editorFieldAxis(field, Field::DIRECTION_X)) = written;
  } else if (field == Field::PARTICLES) {
    emitter.burst.count = static_cast<uint16_t>(written);
  } else if (isLookToggleField(field)) {
    setLookToggle(emitter.burst.look, field, written);
  } else if (float* scalar = scalarOf(emitter, field)) {
    *scalar = written;
  }
}

bool editorEmitterHasField(Field field) {
  return std::ranges::find(EDITOR_EMITTER_FIELDS, field) !=
         std::end(EDITOR_EMITTER_FIELDS);
}

bool sameEditorEmitter(const EditorEmitter& a, const EditorEmitter& b) {
  return a.id == b.id && a.effect == b.effect && a.position.x == b.position.x &&
         a.position.y == b.position.y && a.position.z == b.position.z &&
         a.direction.x == b.direction.x && a.direction.y == b.direction.y &&
         a.direction.z == b.direction.z && a.interval == b.interval &&
         a.burst == b.burst && sameFlash(a.flash, b.flash);
}

PlacementBounds editorEmitterBounds(const EditorEmitter& emitter) {
  const WorldPoint& at = emitter.position;
  const float r = EDITOR_EMITTER_MARKER_RADIUS;
  return {{at.x - r, at.y - r, at.z - r}, {at.x + r, at.y + r, at.z + r}};
}

FxEmit editorEmitterEmit(const EditorEmitter& emitter) {
  const WorldPoint& at = emitter.position;
  return {{at.x, at.y, at.z}, emitter.direction, 1.0f};
}

}  // namespace eng::editor
