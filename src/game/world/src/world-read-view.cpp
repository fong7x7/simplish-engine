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
            .commands = scratch.commands,
            .spawns = scratch.spawns,
            .combat = scratch.combat,
            .content = sources.content,
            .grid = sources.grid,
            .obstacles = sources.obstacles,
            .events = sources.events,
            .rng = scratch.rng,
            .run = {scratch.outcome, scratch.steps},
            .output = {scratch.log, scratch.cues, scratch.ui, sources.screens}};
  }

  /// The scratch a view of @p sources starts with.
  WorldReadScratch scratchFor(const WorldReadSources& sources) {
    WorldReadScratch scratch{.rng = sources.rng};
    scratch.context.tick = sources.tick;
    scratch.outcome = sources.outcome;
    scratch.ui = sources.ui;
    return scratch;
  }

}  // namespace

WorldReadView::WorldReadView(const WorldReadSources& sources)
  : WorldReadScratch(scratchFor(sources)),
    WorldLogicView(sceneOver(sources, *this)) {}

}  // namespace eng::game
