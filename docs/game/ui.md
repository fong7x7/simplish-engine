# Game Screens — Menus and a HUD from the GUI Widgets

**Status:** built — `src/game/ui/` (the format and the builder), the logic's side in `src/game/logic/` and `src/game/world/`, the playtest's in `src/editor/shell/`, the agent tools in `src/editor/agent/src/agent-ui.cpp`.
**Decision:** [ADR-012](../decisions/ADR-012-game-menus-as-data.md). **Read with:** [sdk.md](sdk.md), [layout-engine.md](../engine/gui/technical/layout-engine.md) (the flexbox every screen is laid out by).

A game's own screens — a pause menu, "you died — retry?", an upgrade pick, a score and health readout — are **data**: one JSON file each in the project's `content/ui/`. Whoever presents the game builds them out of the engine's own GUI widgets (`GuiPanel`, `GuiLabel`, `GuiButton`), laid out by the GUI's CSS flexbox. The game logic never touches a widget. It shows and hides screens by id, sets named values their text shows, and hears a button pressed as an event.

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
    "fill": "#181c26e6", "radius": 8, "align": "center",
    "children": [
      {"type": "label", "text": "Paused"},
      {"type": "label", "text": "Wave {wave} — score {score}"},
      {"type": "bar", "value": "health", "max": "health_max", "height": 10},
      {"type": "button", "text": "Resume", "action": "resume", "id": "resume"},
      {"type": "button", "text": "Quit", "action": "quit"}
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
| `label` | A line of text | `text` |
| `button` | A button; pressed, it chooses its `action` | `text`, `action` (required) |
| `bar` | A bar filled by `value`'s share of `max` | `value` (a key), `max` (a key, or a number) |
| `spacer` | Empty space; grows to fill its line | — |

Every node may have an `id` — a name agents and tests find it by.

**Text shows values.** `{key}` in a label's or a button's text is the value `key` the logic set, or nothing while it has set none; `{{` is a brace.

### Style

The part of the GUI's `LayoutStyle` a menu needs ([layout-engine.md §3](../engine/gui/technical/layout-engine.md#3-layoutstyle-reference)), and colours. Sizes are pixels, border-box; `-1` is automatic.

| Key | Meaning | Default |
|---|---|---|
| `direction` | `row` or `column`: how a panel lays its children out | `column` |
| `gap` | Between a panel's children | `0` |
| `padding`, `margin` | A number, `[vertical, horizontal]`, or `[top, right, bottom, left]` | `0`; a button's padding `[8, 18]` |
| `width`, `height`, `min_width`, `min_height` | Sizes | automatic, `0`; a bar `10` high, and with no `width` as wide as its line |
| `grow` | Share of the free space on its line | `0`; a spacer's `1` |
| `align` | A panel's children across its axis: `start`, `center`, `end`, `stretch` | `stretch` |
| `justify` | Along its axis: `start`, `center`, `end`, `space_between`, `space_around`, `space_evenly` | `start` |
| `align_self` | This node, overriding its parent's `align` | — |
| `fill` | Background: a panel's, a button's face, a bar's filled part — `#rrggbb` or `#rrggbbaa` | none; a button's slate, a bar's green |
| `color` | Text colour, or a bar's empty part | near-white; a bar's dark grey |
| `radius` | Corner roundness | `0`; a button's `6` |

A column stretches its children across by default, so a button spans it. Give the panel `"align": "center"` (or the button `"align_self": "start"`) to keep buttons at their natural width.

**Mistakes are named, never fatal.** An unknown `type` or key, a colour that is not one, a button with no `action`: each is a problem named by where it is — `root/children[2]: 'align' should be start, center, ...` — and that node is left out or keeps its default. The editor logs them, and `get_ui_screens` lists them. A file that is not a JSON object, or has no `root`, is no screen.

---

## 2. From the logic

| Call | Does |
|---|---|
| `world.showScreen(id)` | Shows the screen, on top of those shown. A name no file has is warned of in the log |
| `world.hideScreen(id)` | Stops showing it |
| `world.showing(id)` | Whether it is shown |
| `world.setUiValue(key, text)` | Sets the value `{key}` shows and a bar reads; `sdk::setUiNumber(world, key, n)` for a number |
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

**Pausing is not a menu's.** In lockstep a run cannot stop for one player, so showing a menu pauses nothing: a game whose pause screen should stop play stops it in its own logic. The editor's playtest pauses its own clock with F6.

---

## 3. A choice is input

`sim::PlayerInput::ui_action` is 0, or one plus the action's index in the **project's action list** — every action any screen's button names, sorted and once each (`game::uiActions`). Sorted, so every peer and every replay number them alike; adding a screen that names a new action renumbers the ones after it, which a replay recorded before the change will not match. The replay format records it (version 3).

The host sets it for one tick when a button is pressed; the world turns it into a `UI_ACTION` event carrying the action's name; the logic hears it on the next tick, like every event.

---

## 4. In the editor

Every table read — opening a project, a rescan — reads `content/ui/`, logging each problem. A playtest's content carries the screen ids and the action list, and a bake (the logic check, a deploy) copies `content/ui/` beside the data tables.

While playing, the game's screens are drawn over the viewport in a layer of their own, rebuilt when the logic changes which are shown and filled in as it sets values:

- **Mouse:** click a button.
- **Keyboard:** while a menu is on top, the arrows move between its buttons, Tab steps through them, and Return or Space presses the focused one. Other keys still reach the game.
- **Pad:** the D-pad moves, the confirm button presses, as in any menu.
- **HUD:** never takes the pointer — clicks pass through it to the game.

The deployed game is headless until the rendered client exists; it keeps the screen state and drops it, as it drops cues.

---

## 5. For agents

| Tool | Does |
|---|---|
| `get_ui_screens` | Every screen: id, layer and buttons (id, action, text); the action list; every file's problems |
| `set_ui_screen {id, screen}` | Checks a screen and writes it; one that is no screen is refused with its problems |
| `render_ui_screen {id, width, height, values}` | Draws it through the GUI's software rasterizer, with the system's UI font, to `build/ui/<id>.png` — open the image to see it — and answers with each button's rect |
| `press_ui {action}` | Chooses an action as player 1 on the next playtest tick, as a click would; follow with `step_playtest` |
| `get_playtest` → `ui` | The screens shown, the values, and every button with its rect |

The loop: write a screen with `set_ui_screen`, look at it with `render_ui_screen`, have the logic show it, playtest, `press_ui`, `step_playtest`, and read the outcome.

---

## 6. Testing

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

`test.choose(slot, action)` makes the choice on the next tick only; `test.uiValue(key)` reads a value; `test.world().showing(id)` asks what is shown.

In the engine, `game/ui`'s tests build screens into a bare `GuiWidgetTree` and lay them out with the one-argument `computeLayout`; `test_ui_screen_render.cpp` renders one and writes `ui-screen-capture.png` at the repository root, a gitignored artefact.

---

## 7. Not yet

- Text size and weight: every label uses the UI font at one size.
- Images, sliders, text fields: the format names panels, labels, buttons, bars and spacers.
- Transitions: a screen appears and goes at once.
- Per-player screens in split-screen co-op: every screen is shown to everyone, and a choice is player 1's in the editor.
