# Simplish — Input

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md)
**Last Updated:** 2026-09-22

Keys and pads, turned into the `PlayerInput` a tick runs on. This covers the
engine's device-neutral vocabulary (`src/engine/input/`), the platform's pad
backends (`src/platform/input/`), and the file a player remaps their controls
in.

---

## 1. The split

```
 platform/input  ──  which pads exist, and reading them
   Gamepads         SDL3 on desktop; DualSense on PS5; GameInput on Xbox; none
       │  GamepadReading { device, GamepadState }  (canonical buttons and axes)
       ▼
 engine/input    ──  what the controls mean
   GamepadSet       every pad, the one in use, presses since last frame
   InputBindings    action ← keys, pad buttons, pad axis directions; deadzones
   ActionValues     each action's strength this frame, 0 to 1
   makePlayerInput  → sim::PlayerInput, quantised
```

**Which pads a build supports is the platform's decision, made at build
time.** Exactly one pad backend is compiled into `simplish-platform-input`,
chosen by `ENGINE_GAMEPAD_BACKEND` ([its CMakeLists](../../src/platform/input/CMakeLists.txt)):

| Backend | Default on | Pads |
|---|---|---|
| `SDL` | Desktop (macOS, Windows, Linux) | Everything SDL3 knows: Xbox 360/One/Series, DualShock 4, DualSense (and Edge), Switch Pro, Joy-Cons alone or as a pair, and any generic pad with an SDL mapping |
| `DUALSENSE` | PS5 | DualSense only — from the private console overlay ([Platform §5](../platform/REQUIREMENTS.md#5-console-targets)) |
| `GAMEINPUT` | Xbox | Xbox pads only — from the private console overlay |
| `NONE` | Anything else | No pads. A console build without its overlay lands here and still builds |

Everything above the platform layer sees the same thing whatever backend it
got: `GamepadState`s in the engine's canonical buttons and axes. No game or
editor code branches on which pad is plugged in (PLT-CLI-5).

## 2. Canonical buttons and axes

`GamepadButton` names buttons **by position**, not label, so a binding
means the same thumb movement on every pad: `SOUTH` is Xbox A, PlayStation
Cross and Nintendo B. The d-pad, shoulders, stick clicks, Back/Guide/Start,
the misc button (Share, mute, Capture), four back paddles and the touchpad
click make up the rest. `GamepadAxis` is the two sticks (+Y down, as the
screen is) and two triggers (0 to 1). The order of both is SDL3's, and the
SDL backend `static_assert`s it, so its mapping is a cast.

A console backend maps its own pad onto the same names.

**Families and labels.** A backend also reports each pad's `GamepadFamily` —
Xbox, PlayStation, Nintendo, or generic — which SDL gives from the pad's type
(`GamepadSet::activeFamily()` for the pad in use). Bindings never read it. It is
for what to *call* a button: `gamepadButtonLabel(button, family)` gives "A",
"Cross" or "B" for `SOUTH`, and `gamepadAxisLabel` gives "RT", "R2" or "ZR".
Menus also use it to confirm with A on a Nintendo pad, where A is on the right
([gui technical/input.md §5.7](gui/technical/input.md#57-prompts)). A generic
pad is labelled as an Xbox pad.

## 3. Bindings

An `InputBindings` maps each `InputAction` to any number of controls: a key
(by the platform's key symbol, opaque to the engine), a pad button, or one
direction of a pad axis. A control may feed several actions. The actions are
the four move directions, fire, and the four aim directions — the aim ones
are what let the right stick aim, and what let a player move aiming onto
keys or the other stick.

`defaultGamepadBindings()` is twin-stick on every pad: the left stick and
d-pad move, the right stick aims, the right trigger or right shoulder fires.
Keys are added by whoever knows the platform's keys; the editor adds WASD
and the arrows.

**Deadzones.** Each stick's deadzone is radial — measured on the stick's
length, so diagonals are not dead — and the travel past it is rescaled to
start from zero. Triggers have their own. Defaults are 0.2 for each stick
and 0.1 for the triggers, and are part of the bindings, so a player tunes
them in the same file.

**Strength.** `ActionValues` holds each action at 0 to 1: a held key is 1, a
stick part way is part way. Several controls on one action take the
strongest, not the sum. `makePlayerInput(values, fallback_aim, basis)` makes
the move actions a screen-space stick capped at unit length, the aim actions
the same way, turns both through the camera's `MoveBasis`, and quantises.
When no aim action is asked for, `fallback_aim` aims — on a desktop, the
cursor — so a resting right stick leaves the mouse in charge. Fire presses
past half strength, so a trigger fires at half a pull.

Floats end at quantisation, as before; the simulation, the lockstep wire and
replays only see the integers ([ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md)).

## 4. The pad in use

`GamepadSet` takes one frame's readings and keeps the bookkeeping every
backend would otherwise repeat. The pad in use is the last one *touched* — a
button pressed, or a stick or trigger pushed past halfway — so a second pad
on the desk, or one that drifts, does not take over. When the pad in use is
unplugged the first remaining pad stands in. `pressed(button)` reports a
button that went down since the last frame, for menus.

While the window is unfocused the backend reports every pad as resting
(`WindowFocus`). Pads are shared by every window on the machine, and one the
player has left should not keep walking their character.

## 5. The bindings file

`writeInputBindings` and `parseInputBindings` turn a scheme into JSON a
player can edit, and back:

```json
{
  "version": 1,
  "deadzones": { "left_stick": 0.2, "right_stick": 0.2, "trigger": 0.1 },
  "actions": {
    "move_up":   ["pad:-left_y", "pad:dpad_up", "key:w", "key:up"],
    "fire":      ["pad:right_trigger", "pad:right_shoulder"],
    "aim_right": ["pad:+right_x"]
  }
}
```

- `key:` is followed by a word from the platform's key-name table (`up`,
  `space`, `lshift`, `f5` — [`desktop-key-names.h`](../../src/platform/client/include/engine/client/desktop-key-names.h)),
  by a printable character (`w`, `1`, `/`), or by a hex key symbol.
- `pad:` is followed by a button (`south`, `dpad_up`, `left_shoulder`,
  `start`, `touchpad`, …) or an axis direction: `+left_x`, `-right_y`. A bare
  axis is the positive way, which is how triggers are written.
- An action the file lists gets exactly what it lists, and `[]` unbinds it.
  An action left out keeps its default, so an action added after the file
  was written still has a binding.
- An entry that cannot be read is skipped and reported; the rest still
  apply. Text that is not JSON gives the defaults.

The editor keeps the file in the user's application data,
`input-bindings.json` beside `recent-projects.json`. It writes the defaults
there the first time it runs, so there is a complete file to edit. It reads
the file at startup, so the editor has to be restarted for a change to apply.

## 6. Not yet

- **A rebinding screen.** The model supports one (`bind`, `unbind`,
  `clear`, `actionsFor`, then write the file); nothing draws one yet.
- **The `method_changed` event** [gui.md](gui/gui.md) describes. GUI focus
  navigation by pad and keyboard, with prompts and scrolling, is built
  ([gui technical/input.md §5](gui/technical/input.md#5-gamepad-navigation)).
- **More than one local player.** `GamepadSet` holds every pad, but player 1
  plays the pad in use; assigning pads to players waits on local co-op.
- **Haptics, gyro, the touchpad surface, adaptive triggers.**
- **Steam Input.** A Steam build could let Steam Input own the pads instead
  of SDL, as another backend.
