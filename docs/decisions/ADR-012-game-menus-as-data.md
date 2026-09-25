# ADR-012: A game's menus and HUD are data built from the engine's GUI widgets; a choice made in one is player input

**Status:** Proposed
**Date:** 2026-09-25
**Scope:** Engine | Game | Editor

## Context

A game needs screens of its own: a pause menu, a "you died — retry?" screen, an upgrade pick between waves, a score and health readout. The engine has a complete retained-mode GUI (`engine/gui`: widget tree, CSS flexbox layout, FreeType text, theming, pad navigation) that only the editor uses. A project's game logic — C++ in its own `src/` ([ADR-011](ADR-011-project-game-logic-in-cpp.md)) — cannot reach it: the logic compiles against `GameLogicWorld` alone and links nothing of the engine, and `engine/gui` depends on the renderer and FreeType.

Four forces decide how a game's screens are made.

**Determinism is first ([ADR-002](ADR-002-fixed-timestep-determinism.md)).** Showing a screen changes nothing a tick reads. *Choosing* in one does — the upgrade picked changes the run. Every peer of a lockstep session ([ADR-005](ADR-005-deterministic-lockstep-coop.md)) must see the same choice on the same tick, and a replay must remake it.

**Agents author most of it, and cannot look at a screen.** Whatever describes a menu must be something an agent can write, check and see without a window: a file it can read back, validated with named problems, rendered to an image it can open.

**Content is data ([ADR-007](ADR-007-json-authored-cpp-baked-content.md)).** Behaviors are data, not scripts ([ADR-009](ADR-009-actor-behavior-state-machines.md)); sounds, characters and enemies are tables. A screen is the same kind of thing.

**Presentation never writes the simulation** ([fx.md](../engine/fx.md)): effects and sounds read cues and never write back.

## Decision

**A screen is a JSON file in a project's `content/ui/`, describing a tree of the engine's own widgets — panels laid out by flexbox, labels, buttons and bars. Whoever presents the game builds real `engine/gui` widgets from it. The game logic never touches a widget: it shows and hides screens by id and sets named values the screens' text shows. A button names an action; pressing it is recorded in that player's input for the next tick, and the logic hears it as an event.**

- **Screens are data.** `content/ui/<id>.ui.json`: a root node and its children, each `panel`, `label`, `button`, `bar` or `spacer`, styled by the subset of `LayoutStyle` a menu needs (direction, gap, padding, sizes, grow, alignment) plus fill and text colours. A screen is a `menu` — modal, over a dimmed game, taking the pad — or a `hud`, drawn over play and never taking input. `game/ui` reads the file (with named problems, never an exception) and builds the widgets; the editor, a rendered client and a test all use that one builder.
- **Logic drives values, not widgets.** `GameLogicWorld` gains `showScreen(id)`, `hideScreen(id)` and `setUiValue(key, text)`. A label's `"Score: {score}"` and a bar's `value`/`max` keys read those values. What is open and what the values are is presentation state, kept by the world beside the cues and never hashed: a screen shown changes nothing a tick reads.
- **A choice is input.** `sim::PlayerInput` gains `ui_action`: 0 for none, else one plus the action's index in the project's action list — every action any screen names, sorted, so each peer and a replay number them alike. The host sets it for one tick when a button is pressed — a click, the pad's confirm, or an agent's `press_ui` — and the world turns it into a `UI_ACTION` event carrying the action's name. The logic decides what a choice does, so a stale press on a screen already hidden is the logic's to ignore. The replay format records the field (version 3).
- **Agents can see it.** A tool writes and validates a screen, and another renders it through the software rasterizer to a PNG with the rect of every button, so an agent checks a layout by looking at the image.

## Alternatives Considered

### Alternative A: Let the game logic build widgets itself

- **How it works:** Expose `GuiWidgetTree` — or a wrapper of it — through `GameLogicWorld`, and let the logic create, style and wire widgets in C++.
- **Pros:** Anything the GUI can do, the logic can do; no file format.
- **Cons:** The logic module links nothing of the engine by design (ADR-011): every call would be a new virtual across the boundary, for dozens of widget operations, versioned with the logic API. Widgets hold pointers, callbacks and focus state — the opposite of the plain values the logic's world trades in. A menu an agent writes in C++ can only be seen by building and running it. And a click delivered to a callback in the logic would reach the simulation outside the tick.

### Alternative B: Screens as data, choices as direct calls into the logic

- **How it works:** The same data, but the host calls the logic (or queues an event in the world) the moment a button is pressed.
- **Pros:** No engine change; no replay format change.
- **Cons:** A click happens between ticks on one machine. Nothing records it, so a replay diverges at the first menu choice, and a lockstep peer never hears it. Input is the only channel into the simulation that is exchanged and recorded; a choice has to use it.

### Alternative C: A button is a bit in `PlayerInput::buttons`

- **How it works:** Give each action one of the 32 button bits.
- **Pros:** No new field.
- **Cons:** Thirty-two actions across a whole game is too few, the bits are the controls' own, and a held bit means "held" — a choice is a single pulse. A field of its own costs four bytes and one change-mask bit, and is zero, costing nothing in a replay, on every tick nobody chooses.

## Design Principle References

- **Principle 1: Determinism** — a choice reaches the tick only as recorded input; showing, hiding and values never reach it.
- **Data over code** — screens are content an agent can write and read back, like behaviors and sounds.

## Consequences

### Positive

- Menus and HUDs are built from the engine's existing widgets and flexbox layout — no second UI system.
- An agent writes a screen as a file, validates it, and sees it as a PNG before any playtest.
- Replays and lockstep sessions keep menu choices, because they are input.
- The logic stays behind its narrow interface; the GUI stays presentation.

### Negative

- A screen can only use what the format names; a custom widget means a format change in `game/ui`.
- `PlayerInput` grows by four bytes and the replay format moves to version 3; version 2 replays are not read.
- A choice is heard a tick after it is made, like every input.
- Pausing a run is not a menu's to decide: in lockstep a run cannot stop for one player. The editor's playtest pauses its own clock; a game's pause screen stops nothing unless its logic does.

### Neutral

- The deployed game is headless until the rendered client exists; it keeps the screen state and drops it, as it drops cues.
