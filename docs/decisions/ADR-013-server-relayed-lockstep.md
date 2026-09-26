# ADR-013: A co-op session runs through a server that relays lockstep inputs — listen or dedicated

**Status:** Proposed
**Date:** 2026-09-25
**Scope:** Engine | Platform | Editor

## Context

[ADR-005](ADR-005-deterministic-lockstep-coop.md) settled *what* crosses the wire — inputs, never state — and left *how* peers are connected open: [Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions) question 4 asks whether co-op is peer-to-peer lockstep or always a listen server. The answer decides NAT traversal, how distributor relays plug in ([Platform §4.3](../platform/REQUIREMENTS.md#43-distributor-services), PLT-DST-5), who decides a player has dropped, and who arbitrates a desync.

Two requirements pull on it. A player must be able to host from their own machine with no infrastructure. And a session must be able to run on a machine with no player at all — a dedicated host, and the CI run that plays several clients against one ([§5](../../REQUIREMENTS.md#5-architecture) names `bin/server/` for exactly that).

Everything ADR-005 decided stays: input delay, the stall condition, periodic hash exchange, joining at level boundaries only, dropped characters played on, no prediction and no rollback.

## Decision

**Every session has exactly one server, and it relays lockstep inputs in a star.** A player hosting runs the server in their own process (a *listen* server) and joins it as a client like anyone else; a *dedicated* server is the same server with no local player.

- A client sends its input for tick `N + delay` to the server. The server holds every seat's input in the engine's `InputQueue` and, once a tick is complete, sends the whole `TickInput` — a *frame* — to every client. Clients simulate frames, never their own unconfirmed input. The stall condition is the queue's: a frame is not sent until every seated player's input for it is in.
- **The server orders everything.** Which seats are filled, when a run starts, what a run starts from (level, seed, characters: a replay header), which seats have dropped and from which tick — all arrive in the one ordered stream of the server's messages, so no two clients can disagree about them.
- **A dropped seat is marked, not filled, by the server.** A frame carries a mask of absent seats; every client fills those seats with the game's stand-in input from its own copy of the world. The server needs no game to keep a dropped character playing, and the filled input is what every peer steps and records.
- **Hashes go to the server, which compares them.** Every client reports its hash every `NET_HASH_INTERVAL` ticks; the first report of a tick is the reference and any other that differs halts the session for everyone, naming the tick and the first differing section. A dedicated server that simulates reports its own hash too, and being first, becomes the reference.
- **The transport is an interface in the engine** (`eng::net::NetTransport`): messages to and from numbered peers, polled. UDP through ENet, as Engine §3 names, is in `platform/net`; an in-memory loopback is in the engine for tests and single-process sessions; a distributor relay is one more implementation.
- **The dedicated server is the deployed game.** `simplish-game --serve PORT` runs it, `--host PORT` hosts and plays, `--join ADDR` joins. One executable, so the server always has exactly the client's logic and content.

## Alternatives Considered

### Alternative A: Peer-to-peer mesh lockstep

- **How it works:** Every peer sends its input to every other peer; each peer runs its own `InputQueue`. One peer is elected to order joins and starts.
- **Pros:** One hop between any two players instead of two, so the input delay can be a tick or so shorter at the same RTT. No machine is special.
- **Cons:** Every pair of players needs a working connection, which is up to six NAT traversals for four players instead of three connections to one reachable address. Drops need agreement — a peer that loses one link but not the others is dropped for some peers and not others, and the session desyncs on the mask rather than the simulation. A dedicated host has no place in a mesh; it would be a fifth peer that never sends input. Distributor relays still work, but each pair needs one.

### Alternative B: Always a listen server, no dedicated server

- **How it works:** As decided, but the server only ever runs inside a player's game.
- **Pros:** One mode fewer.
- **Cons:** The dedicated mode is the same code with no local client, so dropping it saves nothing; and without it CI cannot run several clients against one server with no window, and a group cannot run a session on a machine none of them plays on.

### Alternative C: TCP rather than ENet

- **How it works:** One reliable-ordered TCP stream per client.
- **Pros:** No dependency; sockets code already exists in `platform/agent`.
- **Cons:** One lost segment holds up every message behind it for a retransmission timeout — hundreds of milliseconds of stall in a protocol whose whole budget is two or three ticks. ENet's reliable channel resends on its own measured RTT, detects a dead peer on its own timer, and is the transport Engine §3 already names.

## Design Principle References

- **Principle 1: Determinism Always** — one ordered stream of server messages means every client sees the same starts, drops and frames in the same order; a mesh would have to reach agreement to get the same guarantee.
- **Principle 4: Simplicity Over Flexibility** — the server is an `InputQueue`, a seat table and a hash ring. The listen and dedicated modes are the same server; the host's own client is an ordinary client.
- **Principle 6: Testability by Construction** — the loopback transport lets a server and four clients run in one test with no sockets, and the dedicated mode lets CI run separate processes against each other.
- **Principle 7: Platform-Agnostic by Default** — the protocol and both endpoints are engine code over an interface; only the socket library is in `platform/`.

## Consequences

### Positive

- Hosting needs one reachable address — the server's — and every other player makes one outbound connection.
- Distributor relays plug in as a transport at one place.
- A desync is arbitrated by one party and reported to everyone at once, with the tick and the section.
- A dedicated server is free: the same executable, the same logic, no local player.

### Negative

- Two hops between clients: the input delay has to cover the round trip to the server, not to the nearest peer.
- The host's machine carries every message; with four players that is a few kilobytes a second, which does not matter, but a host that quits ends the session for everyone. Host migration is not planned.
- A server trusts its clients' inputs and hashes. Acceptable for co-op; see ADR-005's note on PvP.

### Implications for Future Work

- The rendered client, and the editor's playtest when it plays a co-op session, drive a `LockstepClient` exactly as `simplish-game --join` does: sample input on the local clock, step on frames.
- The server chooses the input delay at each start, from the worst round trip its transport has measured (`NetDelayChoice::MEASURED`), since it is the one party that sees every client's.
- Join and rejoin are at run boundaries: a player who connects mid-run is seated in a free or dropped seat and plays from the next `start`.
- Question 4 of [Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions) is closed by this record.
