#pragma once

/// @file editor-playtest-session.h
/// @brief The game running inside the editor, from Play until Stop.
/// @par Threading Main-thread-only.

#include <array>
#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-actor-overlay.h>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-character-gait.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-playtest-run.h>
#include <editor/shell/editor-playtest-state.h>
#include <editor/shell/editor-scripted-input.h>
#include <editor/shell/iso-projection.h>
#include <engine/core/fixed-step-advance.h>
#include <engine/core/fixed-step-clock.h>
#include <engine/input/gamepad-rumble.h>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <engine/render-fx/fx-world.h>
#include <engine/sim/player-input.h>
#include <engine/sim/replay-recorder.h>
#include <engine/sim/replay.h>
#include <engine/sim/simulation.h>
#include <engine/sim/tick-hash.h>
#include <engine/sim/tick-input.h>
#include <filesystem>
#include <game/actors/actor-pool.h>
#include <game/combat/combat-cue-kind.h>
#include <game/combat/combat-cue.h>
#include <game/content/behavior-state.h>
#include <game/content/character-definition.h>
#include <game/content/game-content.h>
#include <game/content/step-set.h>
#include <game/fx/footstep-cue.h>
#include <game/fx/footstep-surfaces.h>
#include <game/fx/footstep-tracker.h>
#include <game/fx/footstep-walker.h>
#include <game/logic/game-logic-instance.h>
#include <game/player/player-pool.h>
#include <game/world/game-setup.h>
#include <game/world/game-world.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The seed every playtest runs with. Fixed, so two playtests of the same
/// level on the same inputs are the same run — which is what makes one
/// driven by the agent API's `send_input` a test rather than a demo.
inline constexpr uint64_t EDITOR_PLAYTEST_SEED = 0;

/// The most cues a playtest keeps waiting to be heard: a few frames' worth
/// of the nearest few of each kind. Past it, the oldest are dropped.
inline constexpr size_t EDITOR_PLAYTEST_HEARD_CUES = 64;

/// The most cues a playtest keeps between one `takeCues` and the next:
/// several frames of a crowded fight. Past it, later cues are not kept.
inline constexpr size_t EDITOR_PLAYTEST_KEPT_CUES = 1024;

/// What a playtest of @p document starts from: one player, standing on the
/// first start for player 1 in the document, or on @p fallback — the tile
/// under the camera — when it has none (Editor REQUIREMENTS §7: "starting at
/// the camera position or a chosen spawn point"); each player as the
/// character their first start names, which the selector's pick then
/// replaces for player 1; a collision box for every placement that
/// collides, measured against @p assets; and an actor for every placement
/// with a behavior, in document order (`editorActorSpawn`). An actor is
/// never also a collision box: its body is the actor's.
///
/// The box is the one the viewport outlines and picks: the placement's
/// asset, turned and set where it stands, enclosed in an axis-aligned box.
/// A prop turned 45° therefore blocks a little more than its mesh covers;
/// that is the price of boxes, and what a collision shape authored per
/// asset would fix.
[[nodiscard]] game::GameSetup
makeEditorPlaytestSetup(const EditorDocument& document,
                        const std::vector<EditorAsset>& assets,
                        WorldPoint fallback);

/// Add @p stand_ins players to @p setup after player 1 — up to three — for
/// the multi-player preview (Editor §7): each at the first start for their
/// player in @p document, or beside player 1 when there is none.
void addEditorStandIns(game::GameSetup& setup, const EditorDocument& document,
                       uint8_t stand_ins);

/// Who player 1 plays as unless they pick someone else: the character the
/// first start for player 1 names, when @p characters has it; otherwise the
/// first character; otherwise nobody — empty, the default character. What
/// the selector opens on, and what a playtest started with no pick uses.
[[nodiscard]] std::string editorPlaytestDefaultCharacter(
    const EditorDocument& document,
    const std::vector<game::CharacterDefinition>& characters);

/// Where the replay of the last playtest of @p level_id is kept:
/// `<root>/data/playtests/<level_id>.replay`. Editor scratch rather than
/// content ([project-format.md §11]): overwritten by the next playtest of
/// that level, and never generated or shipped.
[[nodiscard]] std::filesystem::path
editorPlaytestReplayPath(const std::filesystem::path& root,
                         std::string_view level_id);

/// Write @p replay to `editorPlaytestReplayPath`, making the directory if
/// it is not there. False when it could not be written.
bool writeEditorPlaytestReplay(const std::filesystem::path& root,
                               std::string_view level_id,
                               const sim::Replay& replay);

/// One playtest: the game's world, the simulation stepping it, the clock
/// pacing it, and the replay recording it.
///
/// Built from a copy of the level when Play is pressed and thrown away when
/// it is stopped, so a playtest cannot change the document and nothing
/// needs restoring afterwards. The editor owns one while playing and none
/// otherwise; everything about the running game goes through here, which
/// is what keeps it testable with no window, no GPU and no clock.
/// @thread_safety Main-thread-only.
class EditorPlaytestSession {
public:
  /// A playtest at tick 0 of the level @p run names, set up as @p setup
  /// and played with @p content, which the session keeps a copy of — and
  /// with a fresh instance of the project's game logic, when @p run has
  /// one loaded.
  EditorPlaytestSession(const game::GameSetup& setup,
                        const game::GameContent& content,
                        const EditorPlaytestRun& run);

  /// Runs every tick @p elapsed_ns of real time pays for, at most four,
  /// each on player 1's next input: the front of @p scripted while any is
  /// queued, @p live otherwise.
  FixedStepAdvance advance(uint64_t elapsed_ns, const sim::PlayerInput& live,
                           std::vector<EditorScriptedInput>& scripted);

  /// Runs exactly one tick, as `advance` does for each tick it runs, and
  /// plays the effects of whatever that tick's combat cued.
  void step(const sim::PlayerInput& live,
            std::vector<EditorScriptedInput>& scripted);

  /// Move every effect on by @p seconds of the frame's time. Presentation:
  /// the simulation never sees it, so paused, the effects can simply be
  /// left where they are.
  void stepEffects(float seconds);

  /// Have a pad play input slot @p slot, 1 to 3, on @p input from the next
  /// tick on, in place of its stand-in; nothing hands it back to the
  /// stand-in. Held until changed, so every tick a frame runs uses it.
  void setPadInput(uint8_t slot, std::optional<sim::PlayerInput> input);

  /// What player 1 has felt since the last call — their own shots, blasts
  /// near them, hits they took — as one rumble, and forget it. Nothing
  /// when nothing happened. Presentation; the simulation never sees it.
  [[nodiscard]] input::GamepadRumble takeRumble();

  /// Every cue the ticks have left since the last call, in the order they
  /// happened, and forget them — what the water is splashed by. At most
  /// `EDITOR_PLAYTEST_KEPT_CUES`; a frame of more keeps the first.
  [[nodiscard]] std::vector<game::CombatCue> takeCues();

  /// The cues worth hearing from player 1 since the last call — of each
  /// kind a tick left, the nearest few (`game::hearCombatCues`) — in the
  /// order they happened, and forget them. Presentation, as rumble is:
  /// whoever owns the speakers turns them into sounds.
  [[nodiscard]] std::vector<game::CombatCue> takeHeardCues();

  /// The steps worth hearing from player 1 since the last call — of the
  /// steps each tick's walkers took, the nearest few (`game::hearFootsteps`)
  /// — in the order they fell, and forget them. Presentation, as the cues
  /// are.
  [[nodiscard]] std::vector<game::FootstepCue> takeHeardSteps();

  /// Hear steps land on @p surfaces from the next tick on: the level's
  /// floor, as `makeEditorFootstepSurfaces` builds it. Bare ground until
  /// then.
  void setFootstepSurfaces(game::FootstepSurfaces surfaces);

  /// What each of the setup's actors' feet sound like, in its order; an
  /// actor past the end of the list has the default's.
  void setActorFootsteps(std::vector<game::StepSet> footsteps);

  /// The walkers — by the key `walkers` gives them — whose steps their
  /// clips' footstep events time, from the next tick on: the stride no
  /// longer steps for them, so a step is not heard twice.
  void setAnimatedWalkers(std::vector<uint32_t> keys);

  /// Hear a step for each of @p steps — footstep events a clip or a sheet
  /// played this frame — on the surface under it, keeping the nearest few
  /// to player 1 as the stride's steps are kept.
  void addAnimatedSteps(std::span<const game::FootstepWalker> steps);

  /// Count @p played sounds animation events played, for `publish`.
  void countAnimationSounds(size_t played);

  /// The key `walkers` gives the player in input slot @p slot.
  [[nodiscard]] static uint32_t playerWalkerKey(uint8_t slot);
  /// The key `walkers` gives the setup's @p actor-th actor.
  [[nodiscard]] static uint32_t actorWalkerKey(size_t actor);

  /// The effects playing: what the viewport draws and lights the scene by.
  [[nodiscard]] const FxWorld& effects() const { return fx_; }

  /// The same, for the level's emitters to burst into: presentation, which
  /// the simulation never reads, so nothing outside a tick is changing it.
  [[nodiscard]] FxWorld& effects() { return fx_; }

  /// Copies what the rest of the editor may know — tick, hash, where the
  /// players are and what every actor is doing — into @p state.
  void publish(EditorPlaytestState& state) const;

  /// Where the player at dense index @p index is drawn, @p alpha of the way
  /// from where the last tick found it to where it left it. Render-side
  /// only: the simulation never sees it.
  [[nodiscard]] Vec3 renderPosition(size_t index, float alpha) const;

  /// The players, as the simulation holds them.
  [[nodiscard]] const game::PlayerPool& players() const;

  /// The character the player at dense index @p index is playing as: the
  /// one their slot picked, or the default when it picked none the content
  /// has.
  [[nodiscard]] const game::CharacterDefinition& character(size_t index) const;

  /// Whether the player at dense index @p index moved on the last tick,
  /// which is what picks the clip a rigged character plays.
  [[nodiscard]] EditorCharacterGait gait(size_t index) const;

  /// Name the setup's actors, in its order: the prop each came from. What
  /// `publish` reports each actor as.
  void setActorIds(std::vector<std::string> ids);

  /// Actors the setup asked for, whether or not each is still in the game.
  [[nodiscard]] size_t actorCount() const;

  /// The dense index of the setup's @p actor-th actor, or nothing when it
  /// is not in the game.
  [[nodiscard]] std::optional<uint32_t> actorIndex(size_t actor) const;

  /// The actors, as the simulation holds them.
  [[nodiscard]] const game::ActorPool& actors() const;

  /// Where the actor at dense index @p index is drawn, @p alpha of the way
  /// from the last tick's position to this one's. Render-side only.
  [[nodiscard]] Vec3 actorRenderPosition(uint32_t index, float alpha) const;

  /// Whether the actor at dense index @p index moved on the last tick.
  [[nodiscard]] EditorCharacterGait actorGait(uint32_t index) const;

  /// The state of its behavior the actor at dense index @p index is in.
  [[nodiscard]] const game::BehaviorState& actorState(uint32_t index) const;

  /// The actor at dense index @p index as the AI overlay draws it, @p alpha
  /// of the way between the last two ticks.
  [[nodiscard]] EditorActorOverlay actorOverlay(uint32_t index,
                                                float alpha) const;

  /// Ticks simulated so far.
  [[nodiscard]] uint64_t tick() const { return simulation_.nextTick(); }

  /// The run so far, as a replay (Editor REQUIREMENTS §7: "every playtest
  /// records a replay").
  [[nodiscard]] sim::Replay replay() const { return recorder_.finish(); }

private:
  /// The actors' part of `publish`.
  void publishActors(EditorPlaytestState& state) const;
  /// The players' part of `publish`.
  void publishPlayers(EditorPlaytestState& state) const;
  /// Fill in whom the actor at dense index @p index targets, into
  /// @p report: a player's number, or another actor's id.
  void reportTarget(EditorPlaytestActor& report, uint32_t index) const;
  /// What the AI overlay labels the actor at dense index @p index: its
  /// state and its health.
  [[nodiscard]] std::string overlayLabel(uint32_t index) const;
  /// The projectiles', the hazards' and the run's part of `publish`.
  void publishCombat(EditorPlaytestState& state) const;
  /// The effects' part of `publish`.
  void publishEffects(EditorPlaytestState& state) const;
  /// The run's part of `publish`: whether it is over and how, and what
  /// the game logic has said.
  void publishRun(EditorPlaytestState& state) const;
  /// Take what the game logic said on the last tick into the log, and the
  /// editor's log.
  void keepLogicLog();
  /// Play the effects of every cue the last tick left, count them, and
  /// keep the ones worth hearing.
  void playCues();
  /// Keep the cues the last tick left that player 1 would hear.
  void hearCues();
  /// Follow every walker to where the last tick left it, and keep the
  /// steps player 1 would hear.
  void hearSteps();
  /// Keep, of @p steps, the ones player 1 would hear.
  void keepHeardSteps(std::span<const game::FootstepCue> steps);
  /// Every player and actor in the game, as the footstep tracker follows
  /// them.
  [[nodiscard]] std::vector<game::FootstepWalker> walkers() const;
  /// Where player 1 hears from: where they stand, or the origin with no
  /// player 1.
  [[nodiscard]] Vec3 listenerAt() const;
  /// Add what the last tick did to player 1 to the pending rumble, given
  /// their health before it, @p health_before.
  void feelTick(std::optional<uint16_t> health_before);
  /// Player 1's dense index in the player pool, if they are in it.
  [[nodiscard]] std::optional<uint32_t> playerOneIndex() const;
  /// Fill every stand-in's slot of @p input with what their stand-in does.
  void addStandInInput(sim::TickInput& input) const;
  /// The id of the prop the actor @p handle names became, or empty.
  [[nodiscard]] std::string actorIdOf(sim::EntityHandle handle) const;
  /// The report for the setup's @p actor-th actor, at dense index @p index.
  [[nodiscard]] EditorPlaytestActor actorReport(size_t actor,
                                                uint32_t index) const;

  /// The library the logic's code is in, kept open while it runs. Before
  /// the logic and the world, so it closes after both are gone.
  std::shared_ptr<EditorLogicLibrary> logic_library_;
  /// This run's instance of the project's game logic; none without. Before
  /// the world, which borrows it.
  game::GameLogicInstance logic_;
  /// The game being played. Held by pointer because `simulation_` holds
  /// its address, and the session must be movable without moving it.
  std::unique_ptr<game::GameWorld> world_;
  /// Steps `world_`, hashing every tick.
  sim::Simulation simulation_;
  /// Turns frame time into ticks.
  FixedStepClock clock_;
  /// Every tick's input and a checkpoint a second.
  sim::ReplayRecorder recorder_;
  /// The content the run is played with.
  game::GameContent content_;
  /// Which character each input slot picked, as the setup said.
  std::array<std::string, sim::MAX_PLAYERS> characters_{};
  /// Each player's position before the last tick, for interpolation.
  std::vector<Vec3> previous_;
  /// Each actor's position before the last tick, for interpolation.
  std::vector<Vec3> previous_actors_;
  /// The prop each of the setup's actors came from, in setup order.
  std::vector<std::string> actor_ids_;
  /// Ticks the clock has dropped, summed over the playtest.
  uint64_t dropped_ticks_ = 0;
  /// The hash the last tick ended on.
  std::optional<sim::TickHash> last_hash_;
  /// Every effect playing. Seeded like the run, so a playtest's sparks fly
  /// the same way each time — though nothing depends on it, since effects
  /// are never state.
  FxWorld fx_{EDITOR_PLAYTEST_SEED};
  /// Cues played since the playtest started, by kind.
  std::array<uint64_t, game::COMBAT_CUE_KIND_COUNT> cues_played_{};
  /// What player 1 has felt since `takeRumble` last ran.
  input::GamepadRumble pending_rumble_{};
  /// Cues to be heard since `takeHeardCues` last ran.
  std::vector<game::CombatCue> heard_cues_{};
  /// Every cue since `takeCues` last ran.
  std::vector<game::CombatCue> kept_cues_{};
  /// Cues handed out to be heard since the playtest started.
  uint64_t sounds_heard_ = 0;
  /// What the level sounds like underfoot.
  game::FootstepSurfaces surfaces_{};
  /// When each walker takes a step.
  game::FootstepTracker footsteps_{};
  /// What each of the setup's actors' feet sound like, in its order.
  std::vector<game::StepSet> actor_footsteps_{};
  /// Steps to be heard since `takeHeardSteps` last ran.
  std::vector<game::FootstepCue> heard_steps_{};
  /// Steps handed out to be heard since the playtest started.
  uint64_t steps_heard_ = 0;
  /// Walkers whose clips time their steps, sorted.
  std::vector<uint32_t> animated_walkers_{};
  /// Sounds animation events played since the playtest started.
  uint64_t animation_sounds_ = 0;
  /// The input each slot's pad gives, for the slots a pad plays.
  std::array<std::optional<sim::PlayerInput>, sim::MAX_PLAYERS> pad_input_{};
  /// The last `EDITOR_LOGIC_LOG_LINES` lines the game logic said.
  std::vector<std::string> logic_log_{};
};

}  // namespace eng::editor
