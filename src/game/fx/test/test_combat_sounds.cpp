#include <catch2/catch_test_macros.hpp>
#include <game/fx/combat-sounds.h>
#include <set>
#include <string>

using eng::Vec3;
using eng::audio::AudioClipBank;
using eng::audio::SoundDucking;
using eng::audio::SoundPlacement;
using eng::game::COMBAT_CUE_KIND_COUNT;
using eng::game::COMBAT_SOUNDS_PER_KIND;
using eng::game::CombatCue;
using eng::game::CombatCueKind;
using eng::game::combatCueSound;
using eng::game::CombatSoundClips;
using eng::game::combatSoundName;
using eng::game::Faction;
using eng::game::hearCombatCues;
using eng::game::loadCombatSounds;

namespace {

CombatCue cueAt(CombatCueKind kind, float x) {
  return {.kind = kind, .at = {x, 0.0F, 0.0F}};
}

CombatSoundClips loaded() {
  AudioClipBank bank;
  return loadCombatSounds(bank, 48000);
}

}  // namespace

TEST_CASE("every kind of cue has a sound of its own in the bank") {
  AudioClipBank bank;
  const CombatSoundClips clips = loadCombatSounds(bank, 44100);
  std::set<std::string> names;
  for (uint8_t i = 0; i < COMBAT_CUE_KIND_COUNT; ++i) {
    const auto kind = static_cast<CombatCueKind>(i);
    names.emplace(combatSoundName(kind));
    REQUIRE(bank.find(combatSoundName(kind)) == clips[i]);
    REQUIRE(bank.clip(clips[i])->sample_rate == 44100);
    REQUIRE_FALSE(bank.clip(clips[i])->samples.empty());
  }
  REQUIRE(names.size() == COMBAT_CUE_KIND_COUNT);
}

TEST_CASE("loading again replaces the clips rather than adding more") {
  AudioClipBank bank;
  const CombatSoundClips first = loadCombatSounds(bank, 48000);
  REQUIRE(loadCombatSounds(bank, 48000) == first);
  REQUIRE(bank.size() == COMBAT_CUE_KIND_COUNT);
}

TEST_CASE("a cue is heard from where it happened") {
  const CombatSoundClips clips = loaded();
  const CombatCue cue{.kind = CombatCueKind::SHOT_HIT_WALL,
                      .at = {3.0F, 4.0F, 0.5F}};
  const auto play = combatCueSound(cue, clips);
  REQUIRE(play.clip ==
          clips[static_cast<size_t>(CombatCueKind::SHOT_HIT_WALL)]);
  REQUIRE(play.placement == SoundPlacement::IN_WORLD);
  REQUIRE(play.at.x == 3.0F);
  REQUIRE(play.at.y == 4.0F);
}

TEST_CASE("a blast ducks the music and outranks every shot") {
  const CombatSoundClips clips = loaded();
  const auto blast = combatCueSound(cueAt(CombatCueKind::BLAST, 0), clips);
  REQUIRE(blast.ducking == SoundDucking::DUCKS_MUSIC);
  for (const auto kind :
       {CombatCueKind::SHOT_FIRED, CombatCueKind::SHOT_HIT_BODY,
        CombatCueKind::SHOT_HIT_WALL}) {
    const auto shot = combatCueSound(cueAt(kind, 0), clips);
    REQUIRE(shot.ducking == SoundDucking::NONE);
    REQUIRE(shot.priority < blast.priority);
  }
}

TEST_CASE("a player's own shot is louder and matters more than an actor's") {
  const CombatSoundClips clips = loaded();
  CombatCue theirs = cueAt(CombatCueKind::SHOT_FIRED, 1);
  CombatCue ours = theirs;
  ours.side = Faction::FRIENDLY;
  REQUIRE(combatCueSound(ours, clips).gain >
          combatCueSound(theirs, clips).gain);
  REQUIRE(combatCueSound(ours, clips).priority >
          combatCueSound(theirs, clips).priority);
}

TEST_CASE("pitch varies by place, a little, and the same way every time") {
  const CombatSoundClips clips = loaded();
  const auto a = combatCueSound(cueAt(CombatCueKind::SHOT_FIRED, 1.0F), clips);
  const auto b = combatCueSound(cueAt(CombatCueKind::SHOT_FIRED, 2.5F), clips);
  REQUIRE(a.pitch != b.pitch);
  REQUIRE(a.pitch ==
          combatCueSound(cueAt(CombatCueKind::SHOT_FIRED, 1.0F), clips).pitch);
  REQUIRE(a.pitch > 0.9F);
  REQUIRE(a.pitch < 1.1F);
}

TEST_CASE("of each kind, only the nearest few are heard, in order") {
  std::vector<CombatCue> cues;
  cues.reserve(11);
  for (int i = 0; i < 10; ++i) {
    cues.push_back(
        cueAt(CombatCueKind::SHOT_FIRED, static_cast<float>(10 - i)));
  }
  cues.push_back(cueAt(CombatCueKind::BLAST, 50.0F));
  std::vector<CombatCue> heard;
  hearCombatCues(cues, Vec3{}, heard);
  REQUIRE(heard.size() == COMBAT_SOUNDS_PER_KIND + 1);
  // The nearest four shots were the last four fired; order is kept.
  REQUIRE(heard[0].at.x == 4.0F);
  REQUIRE(heard[3].at.x == 1.0F);
  // A far blast is still heard: it is the only one of its kind.
  REQUIRE(heard[4].kind == CombatCueKind::BLAST);
}

TEST_CASE("heard cues are added to what is already heard") {
  std::vector<CombatCue> heard{cueAt(CombatCueKind::BLAST, 1)};
  hearCombatCues({}, Vec3{}, heard);
  REQUIRE(heard.size() == 1);
  const std::vector<CombatCue> two{cueAt(CombatCueKind::SHOT_FIRED, 1),
                                   cueAt(CombatCueKind::SHOT_FIRED, 1)};
  hearCombatCues(two, Vec3{}, heard);
  REQUIRE(heard.size() == 3);
}
