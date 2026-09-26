# Networking — Co-op Sessions, Server and Client

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §4, M6
**Packages:** `src/engine/net/` (`eng::net`: the protocol, both ends of a session, the transport interface, a loopback transport), `src/platform/net/` (ENet over UDP), and the deployed game's session modes in `src/editor/deploy/`
**Governed by:** [ADR-005](../decisions/ADR-005-deterministic-lockstep-coop.md) (lockstep, inputs not state), [ADR-013](../decisions/ADR-013-server-relayed-lockstep.md) (one server relays; listen or dedicated)
**Status:** Built and tested, headless. `simplish-game` serves, hosts and joins over UDP, with every local player a stand-in; the delay is measured, stalls name who they wait on and drop who never answers, desyncs are traced to their first tick, every run can be recorded and verified, and sessions can be found on the LAN and closed with a password. The rendered client and the editor's playtest do not drive a session yet (§7).

A co-op session is deterministic lockstep in a star: every client sends its input to one server, the server turns each tick's inputs into one *frame* once all of them are in, and every client simulates frames — never its own unconfirmed input. The server can run inside a player's game (a listen server, `--host`) or on its own (a dedicated server, `--serve`). It is the same server either way.

---

## 1. The Shape

```
     client A                         server                          client B
  sample input ──NetInput(t+d)──►  InputQueue (sim)  ◄──NetInput(t+d)── sample input
                                   all in for t? ──────┐
  step(frame t) ◄──NetFrame(t)──── broadcast ──────────┴─NetFrame(t)──► step(frame t)
  hash every 60 ──NetHashReport──► compare first report ◄──NetHashReport── hash every 60
                                   differ? ──NetDesync──► everyone halts
```

| Piece | Header | What it owns |
|---|---|---|
| `NetTransport` | `engine/net/net-transport.h` | Whole messages to numbered peers, reliably and in order; polled, never threaded |
| `LoopbackNetwork` | `engine/net/loopback-network.h` | A server and its clients in one process, as queues. Every engine test runs sessions over it |
| `listenUdp`, `connectUdp` | `engine/net/udp-listen.h`, `udp-connect.h` (in `platform/net`) | ENet 1.3.18 over UDP: one reliable channel, flushed on send, peers dropped after 2–8 s of silence |
| `NetMessage` and its ten messages | `engine/net/net-message.h` … | The protocol (§3) |
| `encodeNetMessage`, `decodeNetMessage` | `engine/net/net-codec.h` | The bytes, and hostile-input-safe decoding |
| `LockstepServer` | `engine/net/lockstep-server.h` | Seats, runs, the input queue, frames, hash comparison |
| `LockstepClient` | `engine/net/lockstep-client.h` | A seat, the run's start, frames in order, the input-delay bound |
| `standInForAbsent` | `game/world/stand-in-input.h` | A frame's absent seats played by the game's stand-in |
| `runDeployedSession` | `editor/deploy/deployed-session.h` | `simplish-game --serve`, `--host`, `--join` |

The server reuses `sim::InputQueue` unchanged: *a tick is taken only when every player's input for it has arrived* is the whole lockstep stall condition, and it was already written for this. The codec reuses the replay's `ByteWriter` and `ByteReader`, which moved from `sim`'s private headers to its public ones for it.

Nothing in `engine/net` reads a clock. The caller decides when to sample input; frames arrive when they arrive.

---

## 2. A Session, Start to End

1. **Connect.** A client's transport polls `CONNECTED`, and the client sends `NetHello`: its protocol version, its content hash and the character it wants.
2. **Seat or refuse.** The server refuses, with a `NetRefusal` and a disconnect: another protocol; another *build* — the engine revision the client was compiled from, which it sends as `NetHello::build`; other *content* — which includes the project's game logic, since the deploy manifest records a hash of its `src/`; the wrong *password*; or a full table. Two builds, or two sets of rules, simulate two different runs, so this is caught before the first tick rather than at the first desync. A build id is a hash of `git describe --always --dirty`, baked in on every build (`cmake/SimplishRevision.cmake`), not of the executable, so the same commit built for macOS and for Windows still plays together. Otherwise it gives the lowest free seat in a `NetWelcome`, and tells every seated client who is in with a `NetRoster`.
3. **Start.** `LockstepServer::start(level, seed)` sends `NetStart` to every seated client: a run number, the run's `sim::ReplayHeader` (level, content hash, seed, player count, each seat's character) and the input delay. The players are the seats up to the highest filled one; an empty seat below that plays absent from tick 0. Every peer builds the same world from the same header.
4. **The first `delay` ticks run on no input.** Nobody can have sampled input for them, so the server queues empty input for every seat and sends those frames at once.
5. **Play.** Each client, each tick of its own clock: takes and steps every frame that has come, then sends its input for `nextInputTick()`. The server takes each input into its queue and sends a `NetFrame` — every seat's input, and which seats are absent — as soon as a tick is complete.
6. **Hashes.** A client reports its hash of every tick divisible by `NET_HASH_INTERVAL` (60). The server keeps the first report of each tick in a ring of 16 and compares later ones with it. A dedicated server that simulates reports its own hash too, first, so it is the reference.
7. **End.** `LockstepServer::end` sends `NetEnd` after the last frame. Clients keep the frames already sent and go back to their seats for the next start. `stopAt(tick)` makes the server send no frame at or past a tick, so every peer ends the run on the same one.

Every message after `NetStart` carries the run's number, and anything from an earlier run is ignored — an input sent before a client heard the next start is never taken for the new run's tick.

### 2.1 Input delay, and the stall

A client's input runs exactly `input_delay` ticks ahead of the frames it has taken and no further: `sendInput` refuses once `nextInputTick() >= nextFrameTick() + delay`. While the session waits on someone, a client cannot send, and its clock's ticks are simply spent waiting — that refusal is the local stall. The bound also keeps every client far inside the server queue's 64-tick reach (`sim::INPUT_QUEUE_TICKS`), so no input is ever refused as too far ahead.

**The delay is the server's**, chosen at each start. `NetDelayChoice::FIXED` uses `LockstepServerConfig::input_delay` (3 ticks by default). `NetDelayChoice::MEASURED` — what `simplish-game` does unless given `--delay N` — asks the transport for each seated peer's round trip (`NetTransport::roundTripMs`; ENet's smoothed RTT plus its variance) and takes `inputDelayForRoundTrip` of the worst: the ticks that round trip spans, rounded up, plus one for where in a tick things land, between 2 and `NET_MAX_INPUT_DELAY` (30). An input travels half a round trip to the server and its frame half a round trip back, so a delay covering the worst whole round trip stalls nobody. On one machine or a LAN that is 2 ticks; at 120 ms it is 9. The start carries the chosen delay, so every peer uses the same one.

**Who a stall is waiting on.** `LockstepServer::waitingOn()` names the seats whose input for the next frame has not come. `announceWaiting()` sends that to every client in the run as a `NetWaiting`, and a client holds it as `waiting()` until the frame arrives. The engine reads no clock, so whoever runs the server decides when a wait is worth saying: `simplish-game`'s server announces once a frame has been held up for 250 ms (`DEPLOYED_STALL_NOTICE`), printing `Waiting for player 2 at tick 180`, and each client prints `Waiting for player 2` — leaving itself out when it was the hold-up.

### 2.2 Dropping and rejoining

A seat whose client disconnects stops *playing* the run but stays in it: the server marks it in every later frame's `absent` mask with zero input, and every peer fills that seat with `game::standInForAbsent` — the same stand-in the editor's multi-player preview plays with — from its own copy of the world, before stepping. Since each peer fills from identical state, each steps identical input, and the filled input is what gets recorded. The server needs nothing of the game to keep a dropped character playing.

A client that connects mid-run is seated — in a dropped seat or a free one — but does not play until the next start (ADR-005: joins at level boundaries only). When every playing seat has gone, the server sends no more frames.

A client that vanishes without disconnecting stalls the session until ENet notices, within 2–8 seconds; then it is dropped as above. A client that stays connected but stops sending input — hung, or stuck somewhere its game loop does not run — ENet cannot notice; the server does. `LockstepServer::removeSeat(seat, STALLED)` tells a client why and frees its seat, and `simplish-game`'s server calls it for every seat a frame has waited on for `--stall-drop` seconds (10 by default), printing `Dropped player 2: no input for 10000 ms`. The dropped player's client ends with `The server dropped this player: it stopped sending input`.

**Stepping evenly.** Frames bunch up: after a stall, the late input releases a burst. `framesToStep(due, waiting)` is how many a client steps now — what its clock owes, and an eighth of any backlog beyond that, rounded up — so a rendered client moves evenly while it catches up, and a second's backlog is gone in about a fifth of a second. `simplish-game` paces this way in real time; `--pace fast` steps everything that has arrived.

### 2.3 Desync

On the first report that differs from the first report of its tick, the server stops sending frames and sends `NetDesync` — the tick, the first differing section's index (or `NET_SECTION_COUNT_DIFFERS`), and who disagreed — to everyone in the run. Section names do not travel; each peer names the section from its own hash of that tick, which is why a deployed peer keeps its last 16 checkpoint hashes. `simplish-game` prints, for instance, `Desync at tick 60 in section logic, reported by player 2` — the test that makes one peer's logic draw a random number the other's does not.

**Traces pin the tick the checkpoint only caught.** A checkpoint catches a divergence up to 60 ticks after it happened. So every peer keeps its hash of each of its last `NET_TRACE_TICKS` (240) ticks, every section, and a client told of a desync sends them at once as a `NetTrace`; a server that simulates keeps its own. `findTraceDivergence` then finds the earliest tick any two traces disagree on, and the first section there. `simplish-game`'s server waits for every playing seat's trace (at most 2 s, `DEPLOYED_TRACE_WAIT`), writes `simplish-desync-<level>-tick<N>.txt` — in `--desync-dir`, or the working directory — and ends with, for instance:

```
Desync at tick 60 in section logic, reported by player 2; the run first diverged at tick 1 in section logic, between player 1 and player 2. Report: …/simplish-desync-arena-tick60.txt
```

The report lists which ticks each peer's trace covers, every peer's combined hash of the ticks either side of the first divergence, and each section's hash on it. A desynced client keeps its connection up for a moment so its trace arrives. With `--replay`, the peers' replays of the run are the other half of the capture: play one back with `--verify` under a debugger, to the tick the report names.

---

## 3. The Protocol

One byte of kind — the alternative's index in `NetMessage`, so its order is the protocol — then the fields: fixed-width integers little-endian, ticks and counts as LEB128 varints, stick axes zigzagged. `NET_PROTOCOL_VERSION` is 3 (the build id, the password and the new refusals); any change to a message's bytes bumps it. No message may exceed `NET_MAX_MESSAGE_BYTES` (64 KB; the largest, a full trace, is under 40 KB): the decoder refuses anything longer, and the ENet transport neither accepts nor buffers more.

| Kind | Message | Direction | Fields |
|---|---|---|---|
| 0 | `NetHello` | client → server | protocol u16, content hash u64, build u64, password digest u64, character string |
| 1 | `NetWelcome` | server → client | seat u8 |
| 2 | `NetRefusal` | server → client | reason u8 (`PROTOCOL`, `CONTENT`, `FULL`, `BUILD`, `PASSWORD`, `STALLED`) |
| 3 | `NetRoster` | server → seated | seated mask u8 |
| 4 | `NetStart` | server → seated | run u16, level string, content hash u64, seed u64, players u8, a character string per player, delay u8 |
| 5 | `NetInput` | client → server | run u16, tick varint, a `PlayerInput` |
| 6 | `NetFrame` | server → run | run u16, tick varint, absent mask u8, four `PlayerInput`s |
| 7 | `NetHashReport` | client → server | run u16, tick varint, combined u64, section count u8, section hashes u64 |
| 8 | `NetDesync` | server → run | run u16, tick varint, section u8, seat u8 |
| 9 | `NetEnd` | server → run | run u16 |
| 10 | `NetWaiting` | server → run | run u16, tick varint, waiting mask u8 |
| 11 | `NetTrace` | client → server | run u16, section count u8, section names, tick count varint (at most 240), then per tick: tick varint, combined u64, section count u8, section hashes u64 |

A `PlayerInput` is four zigzag-varint axes, then the buttons and the screen choice as varints bounded to 32 bits. Strings are a varint length of at most `NET_MAX_ID_BYTES` (256) and the bytes.

**Decoding is hostile-input safe**, as the replay decoder is: messages come from other machines. Nothing is read past the end, every count is bounded, a byte left over is refused, and the tests decode every strict prefix and every single-byte corruption of every message.

**Passwords** travel as `netPasswordDigest`: a 64-bit hash, so the word itself is not on the wire. It is unsalted and unencrypted — anyone who can read the traffic can replay it — so it keeps strangers out of a co-op session and is not security (ADR-005: co-op trusts its peers).

**Bandwidth** is a frame per tick to each client — about 20 bytes of held input plus ENet's header — and an input per tick from each: a few kilobytes a second per player, whatever the horde is doing (ADR-005).

---

## 4. The Transport

`NetTransport` is three calls: `poll` for the next `CONNECTED`, `RECEIVED` or `DISCONNECTED`; `send` to a peer; `disconnect` a peer once what was sent to it has gone. A server's transport has a peer per client; a client's has one, the server. **Nothing happens between polls** — ENet's handshake, acknowledgements and resends all run inside `poll`, so both ends must keep polling even when they expect nothing.

| Transport | Where | For |
|---|---|---|
| `LoopbackNetwork` | `engine/net` | Tests; a server and its clients in one process |
| `listenUdp` / `connectUdp` | `platform/net`, desktop | Every real session. ENet, IPv4, one reliable channel. `listenUdp(0, …)` binds any free port and says which |
| A distributor relay | not written | Steam, Epic and console sessions, through the same interface (Platform §4.3, PLT-DST-5) |

Each ENet host accepts messages up to `NET_MAX_MESSAGE_BYTES` and buffers at most four of them's worth of partly arrived data (ENet's defaults are 32 MB each), so a hostile peer cannot make a server hold megabytes.

### 4.1 Finding sessions on the LAN

Connectionless, beside the session: a player sends `encodeLanQuery()` (`SMPL` `Q`) to UDP port `NET_LAN_PORT` (47016), and every server answering there replies with a `NetLanGame` — protocol, build, content hash, session port, seats filled and in all, whether a run is on, whether it wants a password, its name, and a random session number. `UdpLanBeacon` (`platform/net`) is the server's end: a socket bound with address reuse, answered from each poll. `findLanGames` is the player's: one query, answers gathered for a second, one per session number — a server on this machine answers at its loopback and its network address, and is listed once.

A query for the whole network goes to every interface's own broadcast address, to 255.255.255.255, and to 127.0.0.1. The limited broadcast alone is not enough: macOS will not route it without a default route ("No route to host"), and a machine does not hear its own broadcasts, so a session on the same machine is asked directly. On Windows only the limited broadcast and loopback are asked for now. Measured on one Mac; finding a session on *another* machine is the interface broadcast's job and has not been tried across two machines.

The answers are datagrams from anyone on the network, so decoding them is hostile-input safe like the session's.

ENet is fetched by CMake (`cmake/SimplishDependencies.cmake`) and its nine C files built as `enet_static`, without its own CMakeLists.txt, which predates CMake 3.5. `UDP_DEFAULT_PORT` is 47015.

---

## 5. `simplish-game` as Server and Client

The deployed game is every end a session can have, so the server always runs exactly the client's logic and content ([ADR-013](../decisions/ADR-013-server-relayed-lockstep.md)). `src/bin/server/` of [REQUIREMENTS §5](../../REQUIREMENTS.md#5-architecture) is therefore this mode rather than a second executable.

```bash
# A dedicated server: waits for two players, simulates the run itself as the reference
build/deploy/simplish-game --serve 47015 --players 2 --ticks 3600

# A player hosting on their own machine: a listen server, and themselves as player 1
build/deploy/simplish-game --host 47015 --players 2

# Joining either
build/deploy/simplish-game --join 192.168.1.20:47015
```

| Flag | Meaning |
|---|---|
| `--serve PORT` | Dedicated server. Starts the level once `--players` are seated; simulates the run from the frames it sends and ends it when its world says the run is over, after `--ticks`, or when everyone has left |
| `--host PORT` | The same server, relaying only, with this process's player joined to it through 127.0.0.1. `--players` counts the host. The run ends when the host's world is over |
| `--join HOST[:PORT]` | A player in someone's session, until the server ends the run |
| `--delay TICKS\|auto` | The session's input delay, 1–30, or `auto` — the default — to measure it at each start (a server's flag) |
| `--replay FILE` | Record the run: a server its reference run, a client its own, a solo game its run |
| `--verify FILE` | Play a recording back against this game's content and logic, and say whether it reproduces — or the tick and section it first diverges on |
| `--desync-dir DIR` | Where a server writes a desync's report; the working directory by default |
| `--pace real\|fast` | Sample input at 60 Hz of real time (the default), or whenever the session will take one — for tests and CI |
| `--password WORD` | A server admits only clients that give it; a client gives it |
| `--stall-drop SECONDS` | How long a server waits on a seat before dropping it to a stand-in; 10 by default |
| `--find` | List the sessions on the local network, and what would keep this game out of each |
| `--join lan` | Join the first session on the local network this game can: same build and content, a free seat, waiting for players |
| `--name NAME` | What a server calls its session on the LAN; the game's name by default |
| `--lan-port PORT` | The UDP port servers answer LAN queries on and players ask on; 47016 by default |

Nobody holds the controls of a headless game, so every local player is a stand-in, playing its seat through the network like anyone else. Each process prints how its run ended and its last tick's hash; for one session they are all the same. A run that could not start or stopped short says why — `The server's game content is not this game's`, `Could not reach the server`, `Lost the server at tick 1234`, or a desync.

A joining client builds its world from the `NetStart` exactly as the server's reference does (`DeployedNetWorld`): the level's baked setup, then the header's seed, player count and characters. A solo run and a replay's playback build it the same way from a replay header — a solo run steps as a session whose every seat is absent — so all four paths make identical worlds.

**Replays.** A networked run is a replay (ADR-005): the header is the `NetStart`'s, and every input stepped — absent seats already filled with their stand-in's — is recorded. The file is the engine's replay format (`encodeReplay`), and `--verify` checks it with `verifyReplay`, refusing one recorded against other content.

The content hash a client sends — and a replay carries — is `deployedContentHash`: the files the simulation reads, which are `manifest.json`, `levels/` and `content/`, paths and bytes, in path order, hidden files skipped. A replay or a report written beside the game does not change it.

---

## 6. Testing

| Test | What it proves |
|---|---|
| `engine/net/test/test_net_codec.cpp` | Every message round-trips; every strict prefix, every trailing byte and out-of-range value is refused; single-byte corruption never reads out of bounds |
| `engine/net/test/test_loopback_network.cpp` | The loopback keeps the transport's contract |
| `engine/net/test/test_lockstep_server.cpp` | Seating and refusal, the start, the stall, frames, absent seats from a drop or from the start, rejoin at the next start, `stopAt`, hash comparison and the server's own reference, ending and restarting |
| `engine/net/test/test_lockstep_client.cpp` | The input-delay bound; two clients see identical frames; frames before an end survive it; a lost server |
| `platform/net/test/test_udp_transport.cpp` | ENet on 127.0.0.1: ports, whole ordered messages both ways, a disconnect heard, a lockstep session over real sockets, and a message over the cap from a hostile raw ENet peer dropped |
| `engine/net/test/test_net_input_delay.cpp`, `test_net_trace_divergence.cpp` | The delay a round trip calls for; the first diverging tick and section found across traces |
| `engine/net/test/test_frame_pacing.cpp`, `test_net_lan_codec.cpp` | Even stepping and catching up; LAN datagrams round-trip and survive truncation and corruption |
| `platform/net/test/test_udp_lan.cpp` | A beacon answers a query sent to it; nobody answering finds nothing |
| `editor/deploy/test/test_lan_text.cpp` | How a found session reads, and which one `--join lan` picks |
| `editor/deploy/test/test_deployed_session.cpp` | Whole deployed runs over loopback: a dedicated server and two clients end on the same tick and hash; logic ending the run ends it for all; a dropped client played by its stand-in; other content refused; divergent logic caught at tick 60, traced back to tick 1, and reported to a file; the server's and a client's replays verifying to the run's hash; a stalled client told who it waits for, then dropped; a wrong password refused; what a server says of itself on the LAN; a host with a joiner; the flags |

`test/support/loopback-session.h` is a server and any number of clients on one loopback network, pumped together — the fixture for any new session behavior.

---

## 7. Not Yet

| Gap | Waiting on |
|---|---|
| A person at the controls of a networked run | The rendered client, and the editor's playtest joining a session. Both drive a `LockstepClient` as `DeployedClient` does: sample on the local clock, step on frames — `framesToStep` is written for them |
| Sessions across the internet: NAT traversal, relays | Hosting past a home router needs its port forwarded today. The standard answers are the distributors' relays — the Steam and Epic packages are unwired stubs with no SDK in the tree, and their networking API has no way for a listener to accept a connection, which a relay transport needs — or a rendezvous service for UDP hole punching, which someone has to host and which cannot be tried without two networks behind real NATs |
| Finding sessions across machines, verified; subnet broadcast on Windows | LAN discovery is measured on one machine only. Windows asks the limited broadcast, not each interface's |
| Retuning the delay mid-run | Only at a start: changing it mid-run needs every peer to switch on the same tick, and a run that outgrows its delay already stalls visibly and drops who never answers |
| IPv6 | ENet 1.3 is IPv4-only; IPv6 means another ENet |
| Testing under loss, latency and jitter; the cross-platform hash in CI | A loopback that delays and drops; a CI matrix with more than macOS arm64 in it |
| Host migration | Not planned: a host that quits ends the session (ADR-013) |
