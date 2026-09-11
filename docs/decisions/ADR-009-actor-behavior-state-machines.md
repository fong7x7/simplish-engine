# ADR-009: Actor behavior as data-driven state machines over closed sets

**Status:** Proposed
**Date:** 2026-09-11
**Scope:** Game | Editor | Engine

## Context

Enemies and NPCs need intelligence: they have to notice the players, find a way to them round the level's props, turn to face what they want, and do different things in different situations — a guard watches, gives chase, searches where it lost its quarry and walks back to its post; a charger winds up, rushes in a straight line that overshoots, and recovers. And a designer has to be able to pick that intelligence for a prop in the editor, without writing C++, and see it in a playtest the moment they press Play.

Four existing decisions constrain how that intelligence is written down before the question is asked.

**Determinism is first ([ADR-002](ADR-002-fixed-timestep-determinism.md), Principle 1).** Whatever an actor decides is simulation state. Two lockstep peers must make the same decision on the same tick for every actor, and a replay must remake it. Anything evaluated in the decision has to be ordered, integer or strictly IEEE, and free of hidden iteration order.

**Budgets are requirements (Principle 2).** Engine §7 gives enemy AI and steering 2.5 ms for 2,000 actors — about a microsecond each per tick. There is no room for per-actor virtual dispatch, dynamic allocation, or a tree walk that grows with the behavior's size.

**Content is JSON, baked to C++ for shipping ([ADR-007](ADR-007-json-authored-cpp-baked-content.md)).** A behavior is content: a designer tunes it without recompiling, and a shipping build generates it. So its shape has to be data the generator can emit as a `constexpr` table, with no dynamic shapes and every reference resolved at generation time. ADR-007's Alternative C already rejected an embedded scripting language for this reason.

**Logic is event, conditions, actions from closed sets ([project-format.md §7](../editor/project-format.md#7-logic-and-expressions)).** The level's trigger logic already made this choice for the same reasons: a fixed shape, closed vocabularies, and no loops or user functions, so the set of things content can do to the simulation stays enumerable and reviewable.

## Decision

**A behavior is a small state machine written as data. Each state does one action from a closed set, and leaves by exits whose conditions come from another closed set.**

- A `BehaviorDefinition` holds senses (sight range, view cone, hearing, memory), movement (speed, turn rate), an initial state, a list of **interrupts**, and at most 32 **states**.
- A state names one **action** — `idle`, `hold`, `wander`, `pursue`, `keep_distance`, `flee`, `follow`, `search`, `return_home`, `charge` — its distances (how short a pursuit stops, how far a wander strays), a **facing** (`movement`, `target`, `locked`), a speed multiplier in permille, and an optional presentation-only animation clip.
- An **exit** names one **condition** — `always`, `sees_target`, `hears_target`, `lost_target_for`, `target_within`, `target_beyond`, `in_state_for`, `arrived`, `no_path`, `blocked`, `far_from_home`, `chance` — with the one number it compares against (tiles, ticks, or permille), and the state it leads to.
- Each tick, interrupts are tested before the current state's own exits, all in authored order, and the first that holds is taken. An exit leading to the state the actor is already in is skipped. One transition per tick at most.
- Conditions read only what the actor perceived this tick and what its movement did last tick. `chance` draws from the simulation's named AI stream; nothing else is random.
- Every action and condition is a case in C++ — an entry in a function table indexed by the enum, so dispatch is a load and a call, not a virtual lookup. Adding one is adding code, a name, and a test, deliberately.
- The game ships **built-in presets** (`idle`, `wander`, `guard`, `chase`, `skirmisher`, `coward`, `follower`, `charger`) so the feature works in a project with no behaviors file. A project row with a preset's id replaces it.
- A prop in the editor picks a behavior by reference (`behavior:guard`) and a faction; a prop with a behavior is an **actor** in a playtest.

## Alternatives Considered

### Alternative A: Behavior trees

- **How it works:** A tree of selector, sequence, decorator and leaf nodes, ticked from the root each frame; the industry default for game AI.
- **Pros:** Composes well, reuses subtrees, familiar to designers who have used Unreal or most commercial tools. Handles "try this, else that" cleanly.
- **Cons:** A tick is a tree walk whose length depends on the tree, per actor per tick — cost that grows with authoring ambition, against a microsecond budget. Running nodes carry state across ticks in the tree, which is per-actor memory shaped by the content, not by the pool. Decorators and parallel nodes reintroduce the evaluation-order subtleties ADR-002 exists to avoid. The behaviors Game §5.1 actually asks for — pursue, telegraph, rush, recover, keep distance — are sequences of modes, which a state machine states directly. Principle 4: the flexibility is real, and nothing asks for it yet.

### Alternative B: Utility AI

- **How it works:** Every candidate action scores itself from curves over the actor's situation; the highest score wins each tick.
- **Pros:** Smooth, emergent-looking choices; good at "which of many things matters most right now".
- **Cons:** Scores are floats compared against each other, so two peers that round one curve differently choose differently — a desync that appears only in some situations. Scoring every action every tick costs in proportion to the action count. Tuning is by curve shape, which is hard to reason about and hard to review in a diff. The horde's behaviors are deliberate and telegraphed by design (Game §2, "readable at all costs"); utility AI's strength is the opposite.

### Alternative C: Goal-oriented action planning

- **How it works:** Actions declare preconditions and effects; a planner searches for a sequence reaching a goal.
- **Pros:** Rich, adaptive plans from small authored pieces.
- **Cons:** A search per actor per replan, over a space the content defines — unbounded cost and a determinism surface as large as the planner. Built for a few smart agents, not two thousand simple ones.

### Alternative D: Hard-coded behavior per archetype

- **How it works:** A C++ function per enemy kind, with its tuning numbers in the data table.
- **Pros:** Fastest possible, simplest possible, trivially deterministic.
- **Cons:** Every new behavior — a guard that also flees when hurt, a follower that waits — is a code change and a rebuild, which Game §10 rules out ("a tuning change requires no recompile") and which defeats picking intelligence per prop in the editor. The state machine keeps this approach's speed by making each action exactly such a function, and puts only the sequencing in data.

### Alternative E: Conditions as expressions

- **How it works:** Exits carry expression strings evaluated by `ExpressionEvaluator`, as trigger conditions do.
- **Pros:** Open-ended; one grammar for triggers and behaviors.
- **Cons:** §7 restricts expressions to integers and booleans so the interpreted and compiled paths agree; distances and view cones are floats. Either the grammar widens — which §7 says needs its golden test to prove safe — or every distance becomes a fixed-point integer in content. A closed set of typed conditions keeps float comparisons inside C++, where one function is the only evaluator on both paths. If a condition is ever wanted that the set cannot express, that is the case for widening the grammar, made then, against that test.

## Design Principle References

- **Principle 1: Determinism Always** — authored order decides every tie; the only randomness is a named, hashed stream; floats are compared in one C++ function on every path, never in an interpreter that could round differently.
- **Principle 2: Budgets Are Requirements** — a tick costs one condition table lookup per exit tested and one action call per actor, whatever the behavior's size, and no allocation.
- **Principle 3: Data-Oriented Over Object-Oriented** — an actor is a row of a structure-of-arrays pool holding a brain index and a state byte; the machine is a table shared by every actor running it.
- **Principle 4: Simplicity Over Flexibility** — the requirements ask for modes and transitions between them, and that is all the mechanism does.
- **Principle 5: Explicit Over Implicit** — exits are tested in the order written, so which situation matters most is visible in the file.
- **Principle 6: Testability by Construction** — every action and condition is a free function over the pool, tested headless on its own; a behavior is a value a test builds in a few lines.

## Consequences

### Positive

- Designers write new intelligence in JSON, pick it per prop in the editor, and play it at once — the loop ADR-007 protects.
- A behavior generates as a `constexpr` table under ADR-007: dense enums for actions and conditions, state indices resolved at generation time.
- The cost of AI is independent of how elaborate a behavior is, which is what lets the budget be reasoned about per actor.
- Interrupts give "whatever else is happening, when this happens, do that" without duplicating an exit into every state.

### Negative

- **Expressiveness is bounded by the closed sets.** A behavior needing an action that does not exist waits for someone to write it. That is the point, but it is a real cost when a designer wants something new today.
- **Large behaviors are verbose.** A machine with many states and shared exits repeats itself; interrupts cover the commonest repetition, but there is no subtree reuse.
- **No hierarchy.** A state cannot contain a machine. If behaviors grow to want nested modes (a boss's phases each with their own states), that is the argument for revisiting — more likely by giving bosses the phase script Game §5.1 already specifies than by nesting these.

### Implications for Future Work

- **Damage-driven conditions** (`damaged`, `health_below`) and **attack actions** (`melee`, `fire`, `detonate`) join the sets with the damage phase; an attack appends to an effects buffer the `damage` phase applies, never writing state directly ([project-format.md §9](../editor/project-format.md#9-what-it-becomes)).
- **`patrol`** has joined, with route markers (`entity:waypoint`) authored in the editor. It kept the format's shape: the action names no route — the prop does, as it names its behavior — and the one thing a patrol state adds is how it walks one (`route`: `loop` or `ping_pong`). A route is a list of points handed in with the spawn, so the simulation never sees a waypoint entity.
- **Actors targeting actors** — a guard fighting a raider — needs the spatial hash to find neighbours; until then hostile and friendly actors both take players as targets, and a neutral one takes none.
- **Flow fields** replace per-actor A* for pursuing a player once the horde arrives, behind the same `pursue` action; nothing in the behavior format changes.
- The editor's data-editing panel (Editor §6) edits behaviors like any other table, by schema.
