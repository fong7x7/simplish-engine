# ADR-008: Levels own the space, scenarios sequence stages, groups select what varies

**Status:** Proposed
**Date:** 2026-09-09
**Scope:** Editor | Engine | Game

## Context

A project needs more structure than the one level it holds today. Three separate needs are pushing on it at once, and they are easy to mistake for one feature.

**Content needs somewhere to say "this level, but at this point in the game."** The same space is used more than once — barricaded on the second visit, lit differently after the power comes back, holding a different set of props once an objective has been completed. Nothing in [project-format.md §4](../editor/project-format.md#4-level-files) can express that; a level file is a single flat list of placements.

**The editor needs to start a playtest at an arbitrary point in the progression.** [Editor §7](../editor/REQUIREMENTS.md#7-playtest) is written entirely around "play from the current edit state" — from the beginning, with nothing carried in. A designer tuning the fourth encounter of a run cannot reach it in under a minute, and cannot reach it with the build a player would actually have by then.

**Asset residency needs a boundary.** Two points in a run that share a space share most of their assets; two that do not must be able to unload. Something has to say where unloading is permitted, without saying so in a way that costs the "< 1 s from edit mode to simulating" budget in [Editor §9](../editor/REQUIREMENTS.md).

Four existing decisions constrain the answer before it is asked.

**The word "scenario" is already spent.** [project-format.md §6](../editor/project-format.md#6-scenario-files) defines a scenario as the sequence a run moves through — stages, each naming a level, an encounter, and objectives — and [Game §7](../game/REQUIREMENTS.md#7-run-structure) agrees that a run is a sequence of levels. A scenario sits *above* levels. The intuitive reading of the need above, that a level holds several scenarios, points the other way, and both directions cannot own the name.

**Determinism is first ([ADR-002](ADR-002-fixed-timestep-determinism.md), Principle 1).** Whatever selects content for a stage is an input to the simulation. Two peers in a lockstep session must resolve a stage to the same placements in the same order, and a stage's starting state must be exactly reproducible or replays taken from it do not reproduce.

**Every reference resolves at generation time ([ADR-007](ADR-007-json-authored-cpp-baked-content.md)).** Anything this ADR adds to the format has to survive being emitted as C++ with no dynamic shapes and no runtime resolution.

**The run structure is an open question** ([REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions), #5: roguelite or campaign). A design that only works if the answer is "campaign" would have to be redone.

## Decision

**Four concepts, each owning one thing, with references pointing in one direction.**

| Concept | Owns | File |
|---|---|---|
| **Level** | The space: tile layers, structures, every placement, every region | `content/levels/<id>.level.json` |
| **Group** | A named subset of a level's placements, in authored order | Declared in the level file |
| **Scenario** | An ordered sequence of stages — the run | `content/scenarios/<id>.scenario.json` |
| **Stage** | One point in a run: a level, a group mask, an encounter, objectives, and an entry state | Declared in the scenario file |

**Scenario keeps its §6 meaning.** It is the sequence above levels, not a variant below one. The intra-level concept is a **group**, because `layers` is already the level's tile layers and a second meaning for it would be read wrong every time.

**A level owns all of its content, for every point it is used at.** There is one authoritative list of placements. A group is a label on them: the level declares an ordered `groups` list, every placement carries a `group` id, and a stage names the groups that are active. Resolution is a mask over authored order — no merging, no overriding, no per-stage placement data anywhere.

**A stage carries a deterministic entry state**: seed, player count, build and loadout, and the flags that are already set on arrival. "Play from stage 4" is: load the level, apply the mask, bind the encounter, install the entry state, start the tick. The entry state is the primitive; the authored scenario chain is one producer of it, and a designer's ad-hoc "what if they got here with this weapon" is another.

**References point uphill only.** A stage names a level; a level never lists the stages that use it. The reverse index is built in memory by the editor from a scan of `content/scenarios/`. A level file must not change because an unrelated scenario was added — that is [§1's](../editor/project-format.md#1-what-the-format-has-to-do) diff requirement applied to the hierarchy.

**Asset residency is derived, and only the boundary is authored.** A stage declares whether it is a load boundary — whether unloading is permitted on the way in. What is resident is the union of assets reachable by reference from the active level, mask, and encounter, computed at load; the loader loads the difference from what is already resident. No file lists assets.

## Alternatives Considered

### Alternative A: Scenarios below levels, each owning its own layout

- **How it works:** The hierarchy the need suggests on first reading — a level names a space, and each of its scenarios holds a complete authored layout for that space.
- **Pros:** Conceptually flat. No mask, no resolution rule, nothing shared and therefore nothing to break by sharing. A scenario is loadable on its own with no reference to a base.
- **Cons:** The shared majority of a level is duplicated per scenario, so a wall moved once is moved five times, and the fifth is missed. It also inverts the name against §6 and Game §7, so either the run sequence loses the word or the project carries two meanings for it. The duplication is what the diff and hand-editing requirements in §1 exist to prevent.

### Alternative B: Scenarios as deltas over a base level

- **How it works:** Unity's additive scenes or Unreal's sublevels — the level is a base, and each scenario is a patch: placements added, removed, or with properties overridden.
- **Pros:** No duplication and no ceiling on what can vary. Familiar to anyone arriving from either engine.
- **Cons:** It introduces resolution semantics the project has nowhere else — order of application, what happens when two deltas touch one placement, what an override means once the base placement it targets has been deleted. Dangling overrides are silent by nature: the base moves, the patch still applies, and the result is wrong rather than broken. Undo and diff both have to reason across two files. Emitting it under ADR-007 means resolving arbitrary patches at generation time rather than folding a constant mask. It buys flexibility no stated requirement asks for, at the cost of the one thing Principle 4 says to protect.

### Alternative C: No intra-level variation — a stage is a whole separate level file

- **How it works:** Drop groups. Two points in the same space are two level files, copied and edited.
- **Pros:** The simplest thing that works, and the only one requiring no format change at all beyond multiple levels. Every stage is independently loadable, diffable, and generatable.
- **Cons:** The same duplication as Alternative A without even a hierarchy to organise it, and it makes every revisited space a copy that decays. It also forces a full asset reload between two stages that share almost everything, because nothing tells the loader they are the same space.

### Alternative D: Author the asset set per stage

- **How it works:** Each stage lists the assets it needs; the loader loads exactly that list.
- **Pros:** Explicit residency, trivially inspectable, and a memory ceiling that can be read off the file rather than measured.
- **Cons:** It is derived data with a chance to disagree with its source, which [§11](../editor/project-format.md#11-deliberately-not-in-the-format) rules out for exactly this reason: a prop added to a group and not to the list is a missing mesh at runtime. It also guarantees the full reload it appears to be optimising, because a hand-written list has no way to express "the same 95% as the last stage". Deriving the set gets the union for free and makes the diff between two stages the thing that is actually loaded.

### Alternative E: Reach a stage by simulating the run forward

- **How it works:** No entry state. To test stage 4, run stages 1 through 3 from the seed at high speed, or replay a recorded session up to that point ([Engine §4.4](../engine/REQUIREMENTS.md#44-replay)).
- **Pros:** Nothing new is authored, and the state reached is guaranteed to be a state the game can actually produce — no fixture can drift from reality because there is no fixture.
- **Cons:** The cost of reaching a point grows with how far in it is, against a requirement measured in seconds. It cannot express "arrive here with this build" unless a run that produces that build is found first, which is the case a designer tuning a weapon most needs. And it makes a headless CI test of a late encounter cost the whole run ahead of it. Simulating forward remains the right way to *validate* an entry state (see Consequences); it is the wrong way to reach one.

## Design Principle References

- **Principle 1: Determinism Always** — a group mask over authored order resolves to the same placements in the same sequence on every peer, and an entry state makes a stage's starting conditions exact rather than approximately reconstructed. Both are the deterministic form of what Alternatives B and E do inexactly.
- **Principle 4: Simplicity Over Flexibility** — a mask is a boolean per group; there is no override resolution, no precedence, and no merge. The variation the requirements ask for is "these things are here and those are not", and that is all the mechanism does.
- **Principle 5: Explicit Over Implicit** — references point uphill only, so ownership of a level file is unambiguous; the load boundary is authored where the policy decision belongs, while residency is computed where the facts are.
- **Principle 6: Testability by Construction** — an entry state is a headless fixture. The same declaration that starts an editor playtest at stage 4 starts a determinism test at stage 4 in CI, with no window and no GPU.
- **Principle 2: Budgets Are Requirements** — deriving residency and loading only the difference is what keeps stage switching inside the playtest-entry budget; the boundary flag is what keeps memory bounded when the difference is genuinely everything.

## Consequences

### Positive

- One authoritative copy of a space. A wall is moved once, and every stage using that level sees it.
- A stage is a mask plus references, so it generates as a constant: a `constexpr` bitmask over a dense group enum, folded by the compiler, with no resolution at runtime.
- "Play from any point" and "test any point headlessly" are the same feature, because both are an entry state. The CI determinism job gains coverage of late-run states it currently cannot reach.
- Switching stages within a level costs the difference in referenced assets, which for a revisited space is usually nothing.
- The design survives [open question #5](../../REQUIREMENTS.md#8-open-questions) either way. If the run is a campaign, the authored scenario chain is the progression. If it is a roguelite, the chain is one of several producers of entry states and the seeded assembler is another; nothing below the stage changes.

### Negative

- **A mask cannot express a moved prop.** "The crate is here in stage 1 and there in stage 3" is two placements in two groups, not one placement with an override. This is the flexibility Alternative B has and this decision declines; if it turns out to be needed constantly rather than occasionally, that is the argument for revisiting.
- **Level files grow with the number of stages using them**, because every variant's placements live in one file. RLE tile layers and per-group ordering keep the diff local, but a heavily reused space is a large file, and this is the most likely reason a future ADR splits a group into its own file.
- **An entry state is authored state that can drift** from what the run actually produces. It needs a test that simulates a scenario chain forward and asserts each stage's authored entry state matches the state the previous stage ends in — for the chains where that is meant to hold. Without it, designers tune against states the game never reaches.
- **Two more things the editor must show**: group visibility and membership, and a stage selector that switches level, mask, and entry state together. Per [CLAUDE.md](../../CLAUDE.md), both ship with their agent tools in `src/editor/agent/` in the same change and their entries in [capabilities.md](../editor/capabilities.md).
- **Residency is computed at every stage switch.** The scan is over references already resolved at load, so it is cheap, but it is not free and it sits inside the playtest-entry budget where it has to be measured.

### Implications for Future Work

- **A level browser comes first.** The editor authors one level whose id is the constant `main` ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)). Nothing in this ADR means anything until a project can hold several levels and the editor can open one; that is the first step and it is useful on its own.
- **Format changes this implies:** §4 gains a `groups` list in level content and a `group` field on each placement, defaulting to a single implicit group so every level written before this exists still loads; §6's stage gains `groups` and `entry_state`. Both are §10 migrations, and the default is what keeps them silent.
- **Where an entry state lives:** authored ones are content, inline in the stage that starts from them. A snapshot captured mid-playtest for debugging is editor scratch in `data/` and is neither content nor generated ([§11](../editor/project-format.md#11-deliberately-not-in-the-format)). A snapshot promoted to a regression test is committed beside the test that uses it.
- **Generation:** groups become a dense enum per level and a stage's mask a `constexpr` integer; an entry state becomes a `constexpr` initialiser. A stage naming a group the level does not declare is a build failure naming both files, on the same rule as any other dangling reference.
- **The load boundary is a policy knob, not a mechanism.** If measurement shows unloading at a boundary costs more than it saves, the flag is what changes, not the residency computation underneath it.
- **This ADR does not decide how a roguelite would assemble a run.** It decides that whatever assembles one hands the same entry state to the same loader.
