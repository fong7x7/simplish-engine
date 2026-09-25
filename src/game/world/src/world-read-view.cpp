#include "world-read-view.h"

namespace eng::game {

namespace {

  /// The scene over @p sources, writing into @p scratch.
  WorldLogicScene sceneOver(const WorldReadSources& sources,
                            WorldReadScratch& scratch) {
    return {.context = scratch.context,
            .players = sources.players,
            .actors = sources.actors,
            .brains = sources.brains,
            .actor_ids = sources.actor_ids,
            .writes = {scratch.commands, scratch.spawns, scratch.combat},
            .content = sources.content,
            .grid = sources.grid,
            .obstacles = sources.obstacles,
            .events = sources.events,
            .rng = scratch.rng,
            .run = {scratch.outcome, scratch.steps, scratch.paused,
                    sources.play_tick},
            .output = {scratch.log, scratch.cues, scratch.ui, sources.screens}};
  }

  /// The scratch a view of @p sources starts with.
  WorldReadScratch scratchFor(const WorldReadSources& sources) {
    WorldReadScratch scratch{.rng = sources.rng};
    scratch.context.tick = sources.tick;
    scratch.outcome = sources.outcome;
    scratch.ui = sources.ui;
    scratch.paused = sources.paused;
    return scratch;
  }

}  // namespace

WorldReadView::WorldReadView(const WorldReadSources& sources)
  : WorldReadScratch(scratchFor(sources)),
    WorldLogicView(sceneOver(sources, *this)) {}

}  // namespace eng::game
