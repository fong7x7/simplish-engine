#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-mixer.h>

using Catch::Approx;
using eng::audio::AudioBus;
using eng::audio::AudioClip;
using eng::audio::AudioListener;
using eng::audio::AudioMixer;
using eng::audio::AudioMixerConfig;
using eng::audio::SoundDucking;
using eng::audio::SoundId;
using eng::audio::SoundLoop;
using eng::audio::SoundPlacement;
using eng::audio::SoundPlay;
using eng::audio::VoiceStart;

namespace {

constexpr uint32_t RATE = 48000;

/// A mono clip of @p frames samples, every one @p value.
AudioClip flat(float value, size_t frames, uint32_t rate = RATE) {
  return {.samples = std::vector<float>(frames, value),
          .sample_rate = rate,
          .channels = 1};
}

VoiceStart startOf(uint32_t id, const AudioClip& clip, SoundPlay play = {}) {
  return {.sound = SoundId{id}, .clip = &clip, .play = play};
}

/// Mix @p frames frames and give them back, interleaved.
std::vector<float> mixFrames(AudioMixer& mixer, size_t frames) {
  std::vector<float> out(frames * 2, 99.0F);
  mixer.mix(out);
  return out;
}

AudioMixer mixerOf(uint16_t voices) {
  return AudioMixer(AudioMixerConfig{.sample_rate = RATE, .voices = voices});
}

}  // namespace

TEST_CASE("nothing playing mixes to silence") {
  AudioMixer mixer = mixerOf(4);
  REQUIRE(mixFrames(mixer, 64) == std::vector<float>(128, 0.0F));
  REQUIRE(mixer.liveVoices() == 0);
}

TEST_CASE("a flat mono sound is heard as recorded in both ears, then ends") {
  AudioMixer mixer = mixerOf(4);
  const AudioClip clip = flat(0.5F, 10);
  mixer.start(startOf(1, clip));
  const std::vector<float> out = mixFrames(mixer, 16);
  REQUIRE(out[0] == 0.5F);
  REQUIRE(out[1] == 0.5F);
  REQUIRE(out[18] == 0.5F);
  REQUIRE(out[20] == 0.0F);
  REQUIRE(mixer.liveVoices() == 0);
}

TEST_CASE("a stereo clip keeps its two channels") {
  AudioMixer mixer = mixerOf(4);
  const AudioClip clip{.samples = {0.25F, -0.25F, 0.25F, -0.25F},
                       .sample_rate = RATE,
                       .channels = 2};
  mixer.start(startOf(1, clip));
  const std::vector<float> out = mixFrames(mixer, 2);
  REQUIRE(out == std::vector<float>{0.25F, -0.25F, 0.25F, -0.25F});
}

TEST_CASE("pitch and the clip's own rate set how fast it plays") {
  AudioMixer mixer = mixerOf(4);
  const AudioClip clip = flat(0.5F, 100);
  const AudioClip slow = flat(0.5F, 100, RATE / 2);
  mixer.start(startOf(1, clip, {.pitch = 2.0F}));
  mixer.start(startOf(2, slow));
  const std::vector<float> out = mixFrames(mixer, 300);
  REQUIRE(out[2 * 49] == Approx(1.0F));   // both
  REQUIRE(out[2 * 50] == Approx(0.5F));   // the fast one has ended
  REQUIRE(out[2 * 199] == Approx(0.5F));  // the slow one plays twice as long
  REQUIRE(out[2 * 200] == 0.0F);
}

TEST_CASE("a looping sound goes round until it is stopped") {
  AudioMixer mixer = mixerOf(4);
  const AudioClip clip = flat(0.5F, 7);
  mixer.start(startOf(1, clip, {.loop = SoundLoop::LOOP}));
  REQUIRE(mixFrames(mixer, 100)[199] == 0.5F);
  REQUIRE(mixer.liveVoices() == 1);
  mixer.stop(SoundId{1});
  const std::vector<float> fading = mixFrames(mixer, RATE / 50);
  REQUIRE(fading[2] < 0.5F);  // the fade starts after the first frame
  REQUIRE(fading[0] > fading[200]);
  REQUIRE(mixer.liveVoices() == 0);
}

TEST_CASE("buses and the master scale the mix, which is clamped") {
  AudioMixer mixer = mixerOf(4);
  const AudioClip clip = flat(0.5F, 100);
  mixer.setBusGain(AudioBus::EFFECTS, 0.5F);
  mixer.start(startOf(1, clip));
  mixer.start(startOf(2, clip, {.bus = AudioBus::INTERFACE, .gain = 2.0F}));
  REQUIRE(mixFrames(mixer, 1)[0] == 1.0F);  // 0.25 + 1.0, clamped
  mixer.setMasterGain(0.5F);
  REQUIRE(mixFrames(mixer, 1)[0] == Approx(0.625F));
  mixer.setMasterGain(4.0F);
  REQUIRE(mixFrames(mixer, 1)[0] == 1.0F);
}

TEST_CASE("a full mixer steals the oldest of its lowest priority") {
  AudioMixer mixer = mixerOf(2);
  const AudioClip clip = flat(0.1F, 1000);
  mixer.start(startOf(1, clip, {.priority = 100}));
  mixer.start(startOf(2, clip, {.priority = 100}));
  mixer.start(startOf(3, clip, {.priority = 100}));
  REQUIRE(mixer.voices()[0].sound == SoundId{3});
  REQUIRE(mixer.stolenVoices() == 1);

  // Something that matters less than all of them is the one dropped.
  mixer.start(startOf(4, clip, {.priority = 50}));
  REQUIRE(mixer.droppedSounds() == 1);

  // Something that matters more takes the oldest of the lowest.
  mixer.start(startOf(5, clip, {.priority = 200}));
  REQUIRE(mixer.voices()[1].sound == SoundId{5});
  mixer.start(startOf(6, clip, {.priority = 150}));
  REQUIRE(mixer.voices()[0].sound == SoundId{6});
  REQUIRE(mixer.liveVoices() == 2);
}

TEST_CASE("nothing to play is dropped, not started") {
  AudioMixer mixer = mixerOf(2);
  const AudioClip empty{};
  mixer.start({.sound = SoundId{1}, .clip = nullptr});
  mixer.start(startOf(2, empty));
  REQUIRE(mixer.droppedSounds() == 2);
  REQUIRE(mixer.liveVoices() == 0);
}

TEST_CASE("a ducking sound pulls the music down, and it comes back") {
  AudioMixer mixer(AudioMixerConfig{.sample_rate = RATE, .duck_gain = 0.25F});
  const AudioClip music = flat(0.5F, RATE * 10);
  const AudioClip blast = flat(0.0F, RATE / 2);
  mixer.start(startOf(1, music, {.bus = AudioBus::MUSIC}));
  REQUIRE(mixFrames(mixer, 256)[0] == Approx(0.5F));
  mixer.start(startOf(2, blast, {.ducking = SoundDucking::DUCKS_MUSIC}));
  for (int i = 0; i < 40; ++i) {
    (void)mixFrames(mixer, 256);
  }
  REQUIRE(mixer.musicDuck() == Approx(0.25F).margin(0.01F));
  REQUIRE(mixFrames(mixer, 1)[0] == Approx(0.125F).margin(0.01F));
  for (int i = 0; i < 1000; ++i) {
    (void)mixFrames(mixer, 256);
  }
  REQUIRE(mixer.musicDuck() == Approx(1.0F).margin(0.01F));
}

TEST_CASE("a sound in the world is placed around the listener") {
  AudioMixer mixer = mixerOf(4);
  mixer.setListener(AudioListener{.at = {5.0F, 5.0F, 0.0F}});
  const AudioClip clip = flat(0.5F, 100);
  const SoundPlay right{.placement = SoundPlacement::IN_WORLD,
                        .at = {10.0F, 5.0F, 0.0F}};
  mixer.start(startOf(1, clip, right));
  const std::vector<float> out = mixFrames(mixer, 1);
  REQUIRE(out[1] > out[0]);
  REQUIRE(out[0] > 0.0F);

  SoundPlay far = right;
  far.at = {500.0F, 5.0F, 0.0F};
  mixer.stopAll();
  (void)mixFrames(mixer, RATE / 10);
  mixer.start(startOf(2, clip, far));
  REQUIRE(mixFrames(mixer, 1) == std::vector<float>{0.0F, 0.0F});
}
