# Game Screens — Menus and a HUD from the GUI Widgets

**Status:** built — `src/game/ui/` (the format, bindings and the builder), the logic's side in `src/game/logic/` and `src/game/world/`, the playtest's in `src/editor/shell/`, the agent tools in `src/editor/agent/src/agent-ui.cpp`.
**Decision:** [ADR-012](../decisions/ADR-012-game-menus-as-data.md). **Read with:** [sdk.md](sdk.md), [layout-engine.md](../engine/gui/technical/layout-engine.md) (the flexbox every screen is laid out by).

A game's own screens — a pause menu, "you died — retry?", an upgrade pick, a score and health readout — are **data**: one JSON file each in the project's `content/ui/`. Whoever presents the game builds them out of the engine's own GUI widgets (`GuiPanel`, `GuiLabel`, `GuiButton`, `GuiCheckbox`, `GuiToggle`), laid out by the GUI's CSS flexbox and drawn in the project's theme, `content/ui/theme.json`. The game logic never touches a widget. It shows and hides screens by id, sets named values their text shows and their flags follow, and hears a button pressed as an event.

```
content/ui/pause.ui.json ──read──► UiScreen ──UiScreenView──► GuiWidgetTree (panels, labels, buttons)
                                                                   │ a press: click, Return, pad's A
game logic: showScreen("pause")                                    ▼
            setUiValue("score", "12")          PlayerInput::ui_action on the next tick
            onUiAction(world, event) ◄──── UI_ACTION event ◄── the world, one tick later
```

---

## 1. A screen file

`content/ui/<id>.ui.json`. The file's name is the screen's id: lowercase letters, digits, `_` and `-`.

```json
{
  "schema": "simplish/ui_screen/1.0",
  "layer": "menu",
  "anchor": "center",
  "root": {
    "type": "panel", "width": 320, "padding": [20, 24], "gap": 12,
    "fill": "#181c26e6", "radius": 8, "elevation": "mid",
    "children": [
      {"type": "label", "text": "Paused", "role": "title"},
      {"type": "label", "text": "Wave {wave} — score {score}"},
      {"type": "bar", "value": "health", "max": "health_max", "height": 10},
      {"type": "toggle", "text": "Music", "action": "music", "checked": "music_on"},
      {"type": "button", "text": "Resume", "action": "resume", "id": "resume",
       "variant": "primary"},
      {"type": "button", "text": "Quit", "action": "quit", "variant": "ghost"}
    ]
  }
}
```

| Key | Meaning | Default |
|---|---|---|
| `layer` | `menu`: modal — drawn over a dimmed game, above every HUD, and the keyboard and pad move between its buttons. `hud`: drawn over play, never taking input; the pointer passes through it to the game | `menu` |
| `anchor` | Where the root sits on the view: `center`, `top`, `bottom`, `left`, `right`, `top_left`, `top_right`, `bottom_left`, `bottom_right`, or `fill` to cover it | `center` |
| `inset` | Pixels the root is kept from the view's edges | `24` |
| `root` | The root node | required |

### Nodes

| `type` | What | Its own keys |
|---|---|---|
| `panel` | A box laying its `children` out by flexbox | `children`: a list of nodes |
| `label` | Text: one line, or wrapped with `"wrap": true` | `text` |
| `button` | A button; pressed, it chooses its `action` | `text`, `action` (required) |
| `bar` | A bar filled by `value`'s share of `max` | `value` (a key), `max` (a key, or a number) |
| `spacer` | Empty space; grows to fill its line | — |
| `checkbox` | A box and its text; pressed, it chooses its `action` | `text`, `action` (required), `checked` (a binding, §1.3) |
| `toggle` | A switch and its text, the same way | `text`, `action` (required), `checked` |

Every node may have an `id` — a name agents and tests find it by.

**Text shows values.** `{key}` in a node's text is the value `key` the logic set, or nothing while it has set none; `{{` is a brace.

### 1.1 Layout

The part of the GUI's `LayoutStyle` a screen needs ([layout-engine.md §3](../engine/gui/technical/layout-engine.md#3-layoutstyle-reference)). Sizes are pixels, border-box; `-1` is automatic.

| Key | Meaning | Default |
|---|---|---|
| `direction` | `row` or `column`: how a panel lays its children out | `column` |
| `gap` | Between a panel's children | `0` |
| `padding` | A number, `[vertical, horizontal]`, or `[top, right, bottom, left]` | `0`; a button's `[8, 18]` |
| `margin` | The same, and any side may be `"auto"`: it takes the free space, pushing the node over — `[0, 0, 0, "auto"]` sends it right, `[0, "auto"]` centres it | `0` |
| `width`, `height` | Pixels, or a percentage of the parent's content box: `"50%"` | automatic; a bar `10` high, and with no `width` as wide as its line |
| `min_width`, `min_height`, `max_width`, `max_height` | Limits | `0`; no maximum |
| `grow` | Share of the free space on its line | `0`; a spacer's `1` |
| `shrink` | How much of an overflow it gives up; `0` never shrinks | `1` |
| `align` | A panel's children across its axis: `start`, `center`, `end`, `stretch` | `stretch` |
| `justify` | Along its axis: `start`, `center`, `end`, `space_between`, `space_around`, `space_evenly` | `start` |
| `align_self` | This node, overriding its parent's `align` | — |

A column stretches its children across by default, so a button spans it. Give the panel `"align": "center"` (or the button `"align_self": "start"`) to keep buttons at their natural width.

### 1.2 Look

Every screen is drawn in the project's **theme** (§1.4), so a node names a role and the theme gives the colours. `fill` and `color` override it for one node.

| Key | Applies to | Meaning | Default |
|---|---|---|---|
| `variant` | button | `neutral`, `primary` (the accent), `danger`, `ghost` (no box until hovered) — the theme's look for it in every state | `neutral` |
| `role` | label, button | `caption`, `label`, `body`, `heading`, `title`, `display`: the theme's size and weight for it | a label's `body`, a button's `label` |
| `size`, `weight` | label | Text size in pixels, weight 100–900, over the role's | the role's |
| `wrap` | label | `true` to wrap at the label's width, so a column of text flows | `false` |
| `text_align` | label | `left`, `center`, `right` | `left` |
| `elevation` | panel | `none`, `low`, `mid`, `high`: the theme's drop shadow for that step | `none` |
| `border`, `border_color` | panel | Border width, and its colour | none; the theme's `border` |
| `radius` | panel, button, bar | Corner roundness | `0`; a button's theme radius |
| `opacity` | any | 0 to 1; fades the node and everything in it | `1` |
| `fill` | panel, button, bar | Background, `#rrggbb` or `#rrggbbaa`: a panel's, a button's face (lighter when hovered), a bar's filled part | none; the theme's button look, the theme's `success` |
| `color` | label, button, bar | Text colour, or a bar's empty part | the theme's `text`; `surface_sunken` |

### 1.3 Bindings

A node's flags follow values the logic sets. Each is a key — on while that value is set to anything but empty, `0` or `false` — or `!key`, on while it is not:

| Key | While on | Unbound |
|---|---|---|
| `visible` | Shown; hidden, it takes no room and its siblings close up | shown |
| `disabled` | Dimmed, not pressable, skipped by the keyboard and pad | enabled |
| `selected` | Drawn chosen: the current tab, the upgrade picked | not |
| `checked` | A checkbox or toggle is on | as last pressed |

```json
{"type": "panel", "direction": "row", "gap": 8, "children": [
  {"type": "label", "text": "New record!", "role": "heading", "visible": "record"},
  {"type": "button", "text": "Buy ({cost})", "action": "buy", "variant": "primary",
   "disabled": "!can_afford"},
  {"type": "toggle", "text": "Music", "action": "music", "checked": "music_on"}]}
```

**A bound checkbox or toggle shows its value, never its own guess.** Pressed, it chooses its action and stays as it was until the logic sets the value — the same rule as every choice: the logic decides (§4). An unbound one flips when pressed, and is only a way to send the action.

```cpp
void onUiAction(GameLogicWorld& world, const LogicEvent& choice) override {
  if (sdk::chose(choice, "music")) {
    music_ = !music_;
    world.setUiValue("music_on", music_ ? "1" : "0");
  }
}
```

### 1.4 The theme

`content/ui/theme.json` is the engine's theme file ([theming.md §4.6](../engine/gui/technical/theming.md#46-a-theme-file)): a `base` preset (`dark` or `light`), palette colours by role, spacing, radii and text sizes. Every key is optional; with no file, screens are drawn in the dark preset.

```json
{ "name": "Ember", "base": "dark",
  "palette": { "primary": "#e8703a", "primary_hover": "#f08a52",
               "surface_raised": "#2a2220", "scrim": "#0a0604b0" },
  "radii": [0, 4, 8, 12] }
```

The screens' covering panel carries it as its `subtree_theme`, so everything in a screen — buttons, text, checkboxes, the focus ring — is measured and drawn in it, even inside the editor, whose own chrome keeps the editor's theme. A menu's dimming is the theme's `scrim`. A theme file that does not read is a problem like a screen's, and the screens fall back to the dark preset.

**Mistakes are named, never fatal.** An unknown `type` or key, a colour that is not one, a button with no `action`, a `weight` of 50: each is a problem named by where it is — `root/children[2]: 'variant' should be one of neutral, primary, danger, ghost` — and that node is left out or keeps its default. The editor logs them, and `get_ui_screens` lists them. A file that is not a JSON object, or has no `root`, is no screen.

---

## 2. From the logic

| Call | Does |
|---|---|
| `world.showScreen(id)` | Shows the screen, on top of those shown. A name no file has is warned of in the log |
| `world.hideScreen(id)` | Stops showing it |
| `world.showing(id)` | Whether it is shown |
| `world.setUiValue(key, text)` | Sets the value `{key}` shows, a bar reads and a binding follows; `sdk::setUiNumber(world, key, n)` for a number |
| `onUiAction(world, event)` | A player chose `event.id` — a button's action — on the tick before; `event.target` is who. `sdk::chose(event, "retry")` asks which |

```cpp
class Arena final : public sdk::Game {
protected:
  void onStart(GameLogicWorld& world) override { world.showScreen("hud"); }
  void onActorDied(GameLogicWorld& world, const LogicEvent&) override {
    sdk::setUiNumber(world, "score", ++score_);
  }
  void onPlayerOut(GameLogicWorld& world, const LogicEvent&) override {
    world.showScreen("game_over");
  }
  void onUiAction(GameLogicWorld& world, const LogicEvent& choice) override {
    if (sdk::chose(choice, "retry") && world.showing("game_over")) {
      world.hideScreen("game_over");
      // ...and start the next try: this game's own rules.
    }
  }
  void onHash(GameLogicHash& hash) const override { hash.add(score_); }
private:
  uint32_t score_ = 0;
};
```

**What is shown is presentation.** The screens shown and their values are kept by the world beside the cues and never hashed: showing a screen changes nothing a tick reads. **A choice is simulation.** It arrives as input, on one tick, recorded in the replay and exchanged between lockstep peers, and it is the logic that decides what it does — so a press on a screen the logic has already hidden is the logic's to ignore (`world.showing`).

**Pausing is the logic's.** Showing a menu stops nothing; the logic pauses the game (§3), and a pause menu is a screen it shows while it does.

---

## 3. Pausing

A pause stops the **game loop, not the UI loop**. It is simulation state the logic sets, so ticks go on: each still reads input, hears the pause button and the choices made on screens, and runs the logic, while players, actors, projectiles, hazards, damage and a downed player's window all stand still.

| Call | Does |
|---|---|
| `world.pause()`, `world.resume()` | Pause, or play on, from the next tick |
| `world.paused()` | Whether the game is paused |
| `world.playTick()` | Ticks played unpaused: the clock gameplay runs on. `world.tick()` counts every tick, paused or not |
| `onPausePressed(world, event)` | A player started pressing the pause button — P, or a pad's Start, unless rebound (Edit › Controls). Heard paused or not; nothing pauses unless the logic says so |
| `sdk::togglePause(world, "pause")` | Pause and show the screen, or hide it and play on |
| `onPausedTick(world)` | `sdk::Game`'s tick while paused, in place of `onTick` |

```cpp
void onPausePressed(GameLogicWorld& world, const LogicEvent&) override {
  sdk::togglePause(world, "pause");            // content/ui/pause.ui.json
}
void onUiAction(GameLogicWorld& world, const LogicEvent& choice) override {
  if (sdk::chose(choice, "resume") && world.paused()) {
    sdk::togglePause(world, "pause");
  }
}
```

**Time gameplay by `playTick()`.** The engine's own systems run on it — cooldowns, wind-ups, grace, the revive and bleed-out windows — so a pause runs none of them down, and the SDK's timers should too: `WAVES.due(world.playTick())`. `sdk::Schedule` and `sdk::fireWeapon` already do, and nobody fires while paused. `sdk::Game` calls `onTick` only on ticks played — whether a tick is paused is settled before its hooks, so each play tick reaches `onTick` exactly once, the tick a menu resumes on included — and `onPausedTick` on the others.

**Every peer pauses together.** Pausing is in the tick, so in co-op the whole session pauses on the same tick, whoever pressed; which players may pause, or resume, is the logic's to decide from `event.target`.

**In the editor**, a paused game holds its scene still — effects, water, animation — while its screens, their values and buttons go on. The editor's own clock pause, F6, is separate: it stops every tick, for looking at one frame, and a game menu cannot be answered through it. `get_playtest` reports `game_paused` and `play_tick`; `send_input`'s `pause` holds the button.

---

## 4. A choice is input

`sim::PlayerInput::ui_action` is 0, or one plus the action's index in the **project's action list** — every action any screen's button names, sorted and once each (`game::uiActions`). Sorted, so every peer and every replay number them alike; adding a screen that names a new action renumbers the ones after it, which a replay recorded before the change will not match. The replay format records it (version 3).

The host sets it for one tick when a button is pressed; the world turns it into a `UI_ACTION` event carrying the action's name; the logic hears it on the next tick, like every event.

---

## 5. In the editor

Every table read — opening a project, a rescan — reads `content/ui/`, logging each problem. A playtest's content carries the screen ids and the action list, and a bake (the logic check, a deploy) copies `content/ui/` beside the data tables.

While playing, the game's screens are drawn over the viewport in a layer of their own, rebuilt when the logic changes which are shown and filled in as it sets values. A menu pops up over its fading scrim and its nodes glide aside as a binding shows or hides one; a HUD fades in and keeps still ([motion.md](../engine/gui/technical/motion.md)) — all of it at once under View › Reduce Motion. A screen answers to:

- **Mouse:** click a button.
- **Keyboard:** while a menu is on top, the arrows move between its buttons, Tab steps through them, and Return or Space presses the focused one. Other keys still reach the game.
- **Pad:** the D-pad moves, the confirm button presses, as in any menu.
- **HUD:** never takes the pointer — clicks pass through it to the game.

The deployed game is headless until the rendered client exists; it keeps the screen state and drops it, as it drops cues.

---

## 6. For agents

| Tool | Does |
|---|---|
| `get_ui_screens` | Every screen: id, layer and buttons (id, action, text — checkboxes and toggles too); the action list; the theme's name, null for the dark preset; every file's problems |
| `set_ui_screen {id, screen}` | Checks a screen and writes it; one that is no screen is refused with its problems |
| `set_ui_theme {theme}` | Checks a theme and writes it to `content/ui/theme.json`; screens shown are built again in it. One that does not read is refused, saying why |
| `render_ui_screen {id, width, height, values}` | Draws it in the theme through the GUI's software rasterizer, with the system's UI font, to `build/ui/<id>.png` — open the image to see it — and answers with each button's rect and each named node's rect, type and bound flags (`visible`, `disabled`, `selected`) |
| `press_ui {action}` | Chooses an action as player 1 on the next playtest tick, as a click would; follow with `step_playtest` |
| `get_playtest` → `ui` | The screens shown, the values, every button with its rect, and every named node with its rect and flags |

The loop: set the theme with `set_ui_theme`, write a screen with `set_ui_screen`, look at it with `render_ui_screen` — passing `values` to see each binding's two states — have the logic show it, playtest, `press_ui`, `step_playtest`, and read the outcome: `get_playtest`'s `ui.nodes` says what is shown, disabled and selected now.

---

## 7. Testing

A logic test (`SIMPLISH_LOGIC_TEST`, [sdk.md §10](sdk.md#10-testing-it)) can choose and read:

```cpp
SIMPLISH_LOGIC_TEST(retry_brings_the_player_back, "main") {
  test.run(1);
  test.expect(test.choose(0, "retry"), "retry is an action");
  test.run(2);
  test.expect(!test.world().showing("game_over"), "the screen is gone");
  test.expect(test.uiValue("tries") == "1", "and a try counted");
}
```

`test.choose(slot, action)` makes the choice on the next tick only; `test.uiValue(key)` reads a value; `test.world().showing(id)` asks what is shown; `test.hold(0, {.pause = true})` holds the pause button, and `test.world().paused()` says whether it paused.

In the engine, `game/ui`'s tests build screens into a bare `GuiWidgetTree` and lay them out with the one-argument `computeLayout`; `test_ui_screen_render.cpp` renders two and writes `ui-screen-capture.png` and `ui-screen-themed-capture.png` — an upgrade pick in the light theme — at the repository root, gitignored artefacts.

---

## 8. Not yet

- A button's text size and weight past its `role`: only a label takes `size` and `weight`.
- Images, sliders, text fields, tabs, lists: the format names panels, labels, buttons, bars, spacers, checkboxes and toggles.
- Named styles shared between nodes, and a node repeated for each item of a list value.
- Exit transitions: a screen pops in, but goes at once when the logic hides it.
- Per-player screens in split-screen co-op: every screen is shown to everyone, and a choice is player 1's in the editor.
