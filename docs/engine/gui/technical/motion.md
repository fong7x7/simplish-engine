# Simplish — Motion: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Library:** engine (`src/engine/gui/`)
**Date:** 2026-09-25

The interface moves the way a modern web page does. A state change blends
instead of snapping. Things arrive and leave with a short fade, scale or
slide. A widget that layout moves glides to its new place instead of
jumping. All of it follows the user's **reduced-motion** preference.

Animations draw the interface. They never write layout, never decide hit
testing, and never read the simulation.

| File | What is in it |
|---|---|
| `gui-easing.h` | `GuiEasing`: LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT, EASE, EMPHASIZED, BACK_OUT, SPRING; `applyEasing` |
| `gui-cubic-bezier.h` | `GuiCubicBezier`, CSS's `cubic-bezier()`, and `GUI_BEZIER_EASE`, `_EMPHASIZED`, `_BACK_OUT` |
| `gui-spring.h` | `GuiSpring`: stiffness and damping, `at(seconds)`, `settleSeconds()`, `eased(t)` |
| `gui-presence.h` | `GuiPresence`, where a widget arrives from or leaves to, and `GUI_PRESENCE_FADE`, `_POP`, `_DROP`, `_RISE` |
| `gui-motion.h` | `GuiMotion` (FULL, REDUCED) and `guiMotionStep` |
| `gui-animation.h`, `gui-widget-animator.h` | One property animation, and a widget's running set |
| `gui-widget-presence.cpp` | `enter`, `leave`, `glideFrom`, `GuiWidgetTree::dismiss`, and the glide pass after layout |
| `gui-motion-curves.cpp` | The bézier solver and the spring |
| `test/test_gui_motion.cpp`, `test_gui_motion_curves.cpp`, `test_gui_modal.cpp` | The executable spec |

---

## 1. Curves

```cpp
applyEasing(GuiEasing::EMPHASIZED, t);           // 0..1 in, 0..1 out
GUI_BEZIER_EASE.at(0.5f);                        // 0.80: CSS's `ease`
GuiCubicBezier{0.2f, 0.0f, 0.0f, 1.0f}.at(t);    // any cubic-bezier()
GuiSpring{.stiffness = 300, .damping = 20}.eased(t);
```

| Easing | Use it for |
|---|---|
| `EASE_OUT` | A state change: hover, press, select. The theme's transitions use it |
| `EMPHASIZED` | Something **arriving**: most of the move at once, then a soft landing |
| `EASE_IN` | Something **leaving**: it accelerates away. `GuiPresence::exiting()` uses it |
| `EASE`, `EASE_IN_OUT` | Something moving from one resting place to another |
| `BACK_OUT` | A small overshoot, for a popover landing or a badge appearing |
| `SPRING` | A physical feel with a little bounce. `GuiSpring` for a custom spring |

A spring ignores duration in physics terms. As a `GuiEasing` it is sampled
over whatever duration the animation has, and lands exactly at 1 at the end.

## 2. Arriving and leaving

```cpp
dialog_card->enter(GUI_PRESENCE_POP);            // fades up from 96% scale
menu->enter(GUI_PRESENCE_DROP);                  // fades down from 6 px above
panel->leave(GUI_PRESENCE_FADE.exiting(), [panel] { panel->visible = false; });
tree.dismiss(sheet_id, GUI_PRESENCE_RISE.exiting());  // leaves, then destroyed
```

A `GuiPresence` is the place a widget is drawn **away** from rest: its
opacity, scale and offset, how long the move takes, and its curve. `enter`
jumps there and animates to rest, meaning fully opaque, at scale 1 and not
offset. `leave` animates from wherever the widget is drawn now (even
mid-entrance) to the presence, then calls `done`.

- Only drawing moves. Layout places the widget at rest, and hit testing
  uses that rect, so a widget can be clicked while it arrives.
- `tree.dismiss(id, to)` makes the widget let the pointer through at once,
  plays the leaving, and destroys the widget after the frame's
  `updateAll`. Never destroy a widget from inside its own animation's
  callback: the update loop is iterating the tree.
- `exiting()` gives the same place, quicker and accelerating away. Exits
  should not linger.

Where it is used:

| Widget | In | Out |
|---|---|---|
| `GuiModal` | Backdrop fades; each card `GUI_PRESENCE_POP` | Fades, then hidden. Pointer and focus scope are released at once |
| `GuiMenuBar`, the editor's menu bar | `GUI_PRESENCE_DROP` when the bar opens | At once. Moving across titles swaps menus instantly, as a desktop menu bar does |
| `GuiToasts` | Rises `GUI_PRESENCE_RISE.offset_y`, `EMPHASIZED` | Fades; the stack closes up |
| `game/ui` screens | A menu `GUI_PRESENCE_POP` over a fading scrim; a HUD fades | — |

## 3. Gliding when layout moves something

```cpp
row->layout_glide = 0.18f;                        // seconds; 0 jumps (default)
row->layout_glide_easing = GuiEasing::EMPHASIZED; // the default
```

After every `computeLayout`, the tree compares each gliding widget's place
**within its parent** with where it was last time. If it moved, the widget
is drawn at the old place and glides to the new one. This is the FLIP
technique: layout is final at once, and only `render_offset` animates.

- **Relative to the parent's `contentOrigin()`.** A widget that moves only
  because its parent moved does not glide; it rides along with the
  parent's drawing. A child of a `GuiScrollPanel` does not glide when you
  scroll, because the panel's content origin moves with the scroll.
- **Position only.** A size change lands at once.
- **Mid-glide, it retargets** from where it is drawn, so rapid changes
  chain smoothly.
- The first layout never glides: there is no "before".

A `game/ui` menu gives all its nodes a glide of `UI_MENU_GLIDE_SECONDS`, so
a node shown or hidden by a binding slides its siblings aside. A HUD does
not: its values change every tick.

## 4. Reduced motion

```cpp
gui.motion = GuiMotion::REDUCED;          // GuiContext: the user's preference
ctx.motion                                // every draw context carries it
```

Under `REDUCED`, `GuiWidget::update` steps every animation and state
transition by `guiMotionStep(REDUCED, dt)`, which is long enough to finish
any animation. Glides, entrances and exits land in the frame they start,
and `leave`'s callback still runs, so a dismissed widget is still
destroyed. Timers keep real time: a toast stays up for its whole life, and
a tooltip still waits.

A widget with motion of its own takes its step from `guiMotionStep` (see
`GuiToggle`'s knob). A widget with a timer uses `dt` itself.

The editor's View › Reduce Motion sets it. It is saved as `reduce_motion`
in the user's graphics file, and `get_state` reports it.

## 5. Capturing a still

A capture drawn with no frame loop would catch every entrance at its start,
at opacity 0. Land them first:

```cpp
GuiDrawContext still = ctx;
still.motion = GuiMotion::REDUCED;
tree.updateAll(still, 0.0f);   // after computeLayout, if a widget reads its rect
tree.renderAll(ctx);
```

`game::renderUiScreen`, the editor's menu captures and the widget gallery
all do this.

## 6. Rules

- **Animate drawing, never layout.** Use `render_scale`, `render_offset`
  and `opacity`, via `enter`, `leave` and `layout_glide`. Do not animate
  `rect` or `tree_layout` values to move something; `slideTo` exists for
  hand-placed (MANUAL) widgets only.
- **Keep it short.** 120–200 ms for most things, and exits faster than
  entrances.
- **Respect `ctx.motion`.** Anything that moves on its own clock uses
  `guiMotionStep`.
