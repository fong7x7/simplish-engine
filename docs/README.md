# Simplish Documentation

Start at the project hub — [REQUIREMENTS.md](../REQUIREMENTS.md) — then follow the pillar you're working in.

## Requirements

| Document | Read it when |
|---|---|
| [Project REQUIREMENTS](../REQUIREMENTS.md) | You need the overall shape: pillars, architecture, layout, current state |
| [Engine](engine/REQUIREMENTS.md) | Working on rendering, simulation, spatial queries, audio, input, netcode transport |
| [Game](game/REQUIREMENTS.md) | Working on weapons, enemies, the wave director, run structure, co-op rules |
| [Editor](editor/REQUIREMENTS.md) | Working on level authoring, encounter scripting, playtest, asset tooling. A first slice — project loading, toolbar, viewport — is built |
| [Platform](platform/REQUIREMENTS.md) | Working on RHI backends, windowing, or distributor services |
| [GUI](engine/gui/README.md) | Working on UI: widgets, layout, text, theming, docking, markdown |
| [Animation](engine/animation.md) | Working on skeletons, animation clips, glTF rigs, or skinned drawing |
| [Spatial](engine/spatial.md) | Working on the navigation grid, line of sight, or path planning |
| [Actors](game/actors.md) | Working on enemies and NPCs: perception, behaviors, steering, facing, attacks, damage and death, stand-in players, or the Behavior row in the editor |
| [Development](development/REQUIREMENTS.md) | Setting up a build, adding tests, touching CI, or hitting a lint gate |
| [Editor agent API](editor/agent-api.md) | Driving the editor from an agent, over MCP or HTTP — and the rule for adding a tool to it |
| [Editor capabilities](editor/capabilities.md) | What the editor can do today, and whether an agent can do it too |

## Cross-cutting

| Document | Purpose |
|---|---|
| [Compound engineering](development/compound-engineering.md) | How work is planned, reviewed, and compounded: the `ce-*` loop, review personas, review lanes, what to document |
| [Solutions library](solutions/README.md) | Accumulated problem/root-cause/fix write-ups, searched by agents during planning |
| [Code layout](development/code-layout.md) | Where code goes: `src/<package>/{include,src,test}`, include paths, per-package CMake. Read before adding a package |
| [Design principles](development/design-principles.md) | Eight ranked principles. When two designs both work, the higher-ranked principle decides |
| [Decisions (ADRs)](decisions/README.md) | What was already considered and rejected, and why. Read before proposing an architectural change |

## Conventions

- **Code layout:** all buildable code lives under `src/`, in packages that each carry `include/`, `src/`, and `test/`. Tests live with the code they test. Full rules in [code-layout.md](development/code-layout.md).
- **File naming:** kebab-case for sources and headers (`rhi-device-factory.h`), `ADR-NNN-short-title.md` for decisions.
- **Header design summaries:** every public header opens with a `DESIGN SUMMARY` block stating responsibilities, threading, and invariants — the pattern the copied platform headers already follow.
- **Requirements vs. design:** a `REQUIREMENTS.md` states *what must be true and by when*. Implementation strategy belongs in a design doc alongside it; a choice between competing architectures belongs in an ADR.
- **Milestone tags:** systems carry an `M0`–`M9` tag in the core-systems tables. The milestone definitions live in [Engine §7](engine/REQUIREMENTS.md#7-milestones).
