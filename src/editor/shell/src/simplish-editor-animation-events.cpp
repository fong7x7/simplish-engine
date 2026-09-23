// The animation events half of SimplishEditor: the table of sounds clips
// and sheets make, kept loaded and saved, and — while a level is played —
// the events each frame's animation reached, heard where they happened.

#include <editor/project/project-paths.h>
#include <editor/shell/editor-actor-placement.h>
#include <editor/shell/editor-animation-event-ops.h>
#include <editor/shell/editor-character-figure.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-event-hits.h>
#include <editor/shell/simplish-editor.h>
#include <engine/core/logger.h>
#include <game/fx/footstep-sounds.h>
#include <string>

namespace eng::editor {

namespace {

  /// How much an animation's own sound matters when voices run short:
  /// above a footstep, below a fight.
  constexpr uint8_t EVENT_PRIORITY = 90;

}  // namespace

void SimplishEditor::reloadAnimationEvents() {
  const uint64_t revision = state_.animation_events.revision;
  state_.animation_events =
      state_.project.loaded ? loadEditorAnimationEventTable(state_.project.root)
                            : EditorAnimationEventTable{};
  state_.animation_events.revision = revision;
  saved_animation_events_revision_ = revision;
  refreshClipEvents();
  loadAnimationEventSounds();
}

void SimplishEditor::loadAnimationEventSounds() {
  const std::vector<std::string> problems = loadEditorEventSounds(
      audio().clips(), state_.animation_events,
      state_.project.loaded ? projectAssetsPath(state_.project.root)
                            : std::filesystem::path{});
  auto& all = state_.animation_events.problems;
  all.insert(all.end(), problems.begin(), problems.end());
  for (const std::string& problem : all) {
    LOG_WARN("editor", "animation-events.data.json: " + problem);
  }
}

void SimplishEditor::refreshClipEvents() {
  for (EditorAsset& asset : state_.assets) {
    asset.clip_events =
        asset.rig != nullptr
            ? resolveEditorClipEvents(state_.animation_events,
                                      editorAssetRef(asset), *asset.rig)
            : std::vector<EditorClipEventSet>{};
  }
}

void SimplishEditor::tickTables() {
  tickSound();
  tickAnimationEvents();
}

void SimplishEditor::tickAnimationEvents() {
  if (saved_animation_events_revision_ == state_.animation_events.revision) {
    return;
  }
  if (state_.project.loaded &&
      !saveEditorAnimationEventTable(state_.project.root,
                                     state_.animation_events)) {
    showStatusMessage(
        "Could not save " +
        editorAnimationEventTablePath(state_.project.root).string());
  }
  reloadAnimationEvents();
}

void SimplishEditor::hearAnimationEvents() {
  const std::vector<EditorClipPass> passes = placement_animator_.takePasses();
  const animation::ClipWindow sprite_window{sprite_events_clock_,
                                            animation_clock_};
  sprite_events_clock_ = animation_clock_;
  // A paused playtest shows a frozen frame; its clips and sheets still run
  // on the frame clock, but nothing in it is moving to make a sound.
  if (!isPlaying() || state_.playtest.clock == EditorPlaytestClock::PAUSED) {
    return;
  }
  const auto walkers = frameWalkers();
  noteAnimatedWalkers(passes, walkers);
  playEventHits(eventHits(passes, sprite_window), walkers);
}

std::vector<EditorEventHit>
SimplishEditor::eventHits(const std::vector<EditorClipPass>& passes,
                          animation::ClipWindow sprite_window) const {
  std::vector<EditorEventHit> hits;
  for (const EditorClipPass& pass : passes) {
    appendClipEventHits(pass, state_.assets, hits);
  }
  for (const EditorSprite& sprite : state_.document.sprites) {
    appendSheetEventHits(sprite, state_.animation_events, sprite_window, hits);
  }
  return hits;
}

void SimplishEditor::noteAnimatedWalkers(
    const std::vector<EditorClipPass>& passes,
    const std::map<std::string, game::FootstepWalker>& walkers) {
  std::vector<uint32_t> keys;
  for (const EditorClipPass& pass : passes) {
    const auto walker = walkers.find(pass.key);
    if (walker != walkers.end() && pass.asset < state_.assets.size() &&
        pass.clip < state_.assets[pass.asset].clip_events.size() &&
        editorEventsStep(state_.assets[pass.asset].clip_events[pass.clip])) {
      keys.push_back(walker->second.key);
    }
  }
  playtest_->setAnimatedWalkers(std::move(keys));
}

void SimplishEditor::playEventHits(
    const std::vector<EditorEventHit>& hits,
    const std::map<std::string, game::FootstepWalker>& walkers) {
  std::vector<game::FootstepWalker> steps;
  size_t played = 0;
  for (const EditorEventHit& hit : hits) {
    if (hit.sound == EDITOR_FOOTSTEP_EVENT) {
      steps.push_back(eventWalker(hit, walkers));
    } else {
      played += playEventSound(hit) ? 1 : 0;
    }
  }
  playtest_->addAnimatedSteps(steps);
  playtest_->countAnimationSounds(played);
}

bool SimplishEditor::playEventSound(const EditorEventHit& hit) {
  // A footstep slot plays as a footstep does — the nearest recording its
  // feet have, pitched to them — rather than only a slot recorded as named.
  if (const auto slot = game::footstepSlotNamed(hit.sound)) {
    audio::SoundPlay step = game::footstepSound(
        {hit.at, slot->first, slot->second}, footstep_sounds_);
    step.gain *= hit.gain;
    return audio().play(step).value != 0;
  }
  const std::optional<audio::AudioClipId> clip =
      findEditorEventClip(audio().clips(), hit.sound);
  return clip && audio().play({.clip = *clip,
                               .gain = hit.gain,
                               .placement = audio::SoundPlacement::IN_WORLD,
                               .at = hit.at,
                               .priority = EVENT_PRIORITY})
                         .value != 0;
}

std::map<std::string, game::FootstepWalker>
SimplishEditor::frameWalkers() const {
  std::map<std::string, game::FootstepWalker> walkers;
  const game::PlayerPool& pool = playtest_->players();
  for (uint32_t i = 0; i < pool.slots.size(); ++i) {
    const uint8_t slot = pool.input_slot[i];
    walkers[editorPlayerFigureKey(static_cast<uint8_t>(slot + 1U))] = {
        EditorPlaytestSession::playerWalkerKey(slot),
        {},
        playtest_->character(i).footsteps};
  }
  addActorWalkers(walkers);
  return walkers;
}

void SimplishEditor::addActorWalkers(
    std::map<std::string, game::FootstepWalker>& walkers) const {
  const std::vector<size_t> actors = editorActorPlacements(state_.document);
  for (size_t actor = 0; actor < actors.size(); ++actor) {
    const EditorPlacement& placement =
        state_.document.placements[actors[actor]];
    walkers[placement.id] = {
        EditorPlaytestSession::actorWalkerKey(actor), {}, placement.footsteps};
  }
}

game::FootstepWalker SimplishEditor::eventWalker(
    const EditorEventHit& hit,
    const std::map<std::string, game::FootstepWalker>& walkers) const {
  if (const auto walker = walkers.find(hit.key); walker != walkers.end()) {
    return {walker->second.key, hit.at, walker->second.steps};
  }
  const auto prop = std::ranges::find(state_.document.placements, hit.key,
                                      &EditorPlacement::id);
  return {0, hit.at,
          prop != state_.document.placements.end() ? prop->footsteps
                                                   : game::StepSet::DEFAULT};
}

}  // namespace eng::editor
