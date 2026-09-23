#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-synth.h>
#include <game/fx/footstep-sounds.h>
#include <string>
#include <vector>

using namespace eng;
using namespace eng::game;

namespace {

constexpr uint32_t RATE = 48000;

/// A bank with every built-in step in it.
audio::AudioClipBank builtIns() {
  audio::AudioClipBank bank;
  loadFootstepSounds(bank, RATE);
  return bank;
}

/// The clip @p steps plays on @p surface.
const FootstepClip& clipFor(const FootstepSoundClips& clips, StepSet steps,
                            FootstepSurface surface) {
  return clips[static_cast<size_t>(steps)][static_cast<size_t>(surface)];
}

}  // namespace

TEST_CASE("a step's slot names its feet and its surface") {
  REQUIRE(footstepSoundName(StepSet::BOOTS, FootstepSurface::SAND) ==
          "step.boots.sand");
}

TEST_CASE("every surface has a built-in step, and it is not silence") {
  const audio::AudioClipBank bank = builtIns();
  for (const FootstepSurface surface : ALL_FOOTSTEP_SURFACES) {
    const auto id = bank.find(footstepSoundName(StepSet::DEFAULT, surface));
    REQUIRE(id.has_value());
    REQUIRE(bank.clip(*id) != nullptr);
  }
}

TEST_CASE("with nothing recorded every step set borrows the default's") {
  const FootstepSoundClips clips = resolveFootstepClips(builtIns(), {});
  const FootstepClip& boots =
      clipFor(clips, StepSet::BOOTS, FootstepSurface::WOOD);
  REQUIRE(boots.clip ==
          clipFor(clips, StepSet::DEFAULT, FootstepSurface::WOOD).clip);
  REQUIRE(boots.source == FootstepClipSource::BORROWED);
  REQUIRE(clipFor(clips, StepSet::DEFAULT, FootstepSurface::WOOD).source ==
          FootstepClipSource::OWN);
}

TEST_CASE("a step set's own recording beats the default's, and its ground "
          "recording covers surfaces it has none for") {
  audio::AudioClipBank bank = builtIns();
  const auto sand = bank.add("step.boots.sand", audio::synthesize({}, RATE));
  const auto ground =
      bank.add("step.boots.ground", audio::synthesize({}, RATE));
  const std::vector<std::string> recorded{"step.boots.sand",
                                          "step.boots.ground"};
  const FootstepSoundClips clips = resolveFootstepClips(bank, recorded);

  REQUIRE(clipFor(clips, StepSet::BOOTS, FootstepSurface::SAND).clip == sand);
  REQUIRE(clipFor(clips, StepSet::BOOTS, FootstepSurface::STONE).clip ==
          ground);
  REQUIRE(clipFor(clips, StepSet::BOOTS, FootstepSurface::STONE).source ==
          FootstepClipSource::OWN);
}

TEST_CASE("a borrowed step is pitched to its feet, an own one is not") {
  audio::AudioClipBank bank = builtIns();
  (void)bank.add("step.heavy.stone", audio::synthesize({}, RATE));
  const std::vector<std::string> recorded{"step.heavy.stone"};
  const FootstepSoundClips clips = resolveFootstepClips(bank, recorded);
  const FootstepCue on_sand{{0, 0, 0}, StepSet::HEAVY, FootstepSurface::SAND};
  const FootstepCue on_stone{{0, 0, 0}, StepSet::HEAVY, FootstepSurface::STONE};

  // Same place, so the same pitch spread: only the stand-in is lowered.
  REQUIRE(footstepSound(on_sand, clips).pitch <
          footstepSound(on_stone, clips).pitch * 0.8F);
  REQUIRE(footstepSound(on_sand, clips).placement ==
          audio::SoundPlacement::IN_WORLD);
}

TEST_CASE("heavy feet are louder than bare ones") {
  const FootstepSoundClips clips = resolveFootstepClips(builtIns(), {});
  const FootstepCue heavy{{0, 0, 0}, StepSet::HEAVY, FootstepSurface::GROUND};
  const FootstepCue bare{{0, 0, 0}, StepSet::BARE, FootstepSurface::GROUND};
  REQUIRE(footstepSound(heavy, clips).gain > footstepSound(bare, clips).gain);
}

TEST_CASE("only the nearest steps are heard, in the order they fell") {
  std::vector<FootstepCue> cues;
  for (int i = 0; i < 20; ++i) {
    cues.push_back({{static_cast<float>(20 - i), 0, 0}});
  }
  std::vector<FootstepCue> heard;
  hearFootsteps(cues, {0, 0, 0}, heard);

  REQUIRE(heard.size() == FOOTSTEPS_HEARD);
  // The last six fell nearest; kept in the order they fell.
  REQUIRE(heard.front().at.x == 6.0F);
  REQUIRE(heard.back().at.x == 1.0F);
}

TEST_CASE("a recording taken away stops playing, though the bank keeps it") {
  audio::AudioClipBank bank = builtIns();
  (void)bank.add("step.claws.metal", audio::synthesize({}, RATE));
  const FootstepSoundClips clips = resolveFootstepClips(bank, {});
  REQUIRE(clipFor(clips, StepSet::CLAWS, FootstepSurface::METAL).source ==
          FootstepClipSource::BORROWED);
}

TEST_CASE("a footstep slot is read back into its feet and surface") {
  REQUIRE(footstepSlotNamed("step.claws.metal") ==
          std::pair{StepSet::CLAWS, FootstepSurface::METAL});
  REQUIRE_FALSE(footstepSlotNamed("combat.blast").has_value());
}
