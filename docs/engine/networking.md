# Networking — Co-op Sessions, Server and Client

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §4, M6
**Packages:** `src/engine/net/` (`eng::net`: the protocol, both ends of a session, the transport interface, a loopback transport), `src/platform/net/` (ENet over UDP), and the deployed game's session modes in `src/editor/deploy/`
**Governed by:** [ADR-005](../decisions/ADR-005-deterministic-lockstep-coop.md) (lockstep, inputs not state), [ADR-013](../decisions/ADR-013-server-relayed-lockstep.md) (one server relays; listen or dedicated)
**Status:** Built and tested, headless. `simplish-game` serves, hosts and joins over UDP, with every local player a stand-in. The rendered client and the editor's playtest do not drive a session yet (§7).

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
2. **Seat or refuse.** The server refuses another protocol, other content — two builds of the game simulate two different runs, so this is caught before the first tick rather than at the first desync — or a full table, with a `NetRefusal` and a disconnect. Otherwise it gives the lowest free seat in a `NetWelcome`, and tells every seated client who is in with a `NetRoster`.
3. **Start.** `LockstepServer::start(level, seed)` sends `NetStart` to every seated client: a run number, the run's `sim::ReplayHeader` (level, content hash, seed, player count, each seat's character) and the input delay. The players are the seats up to the highest filled one; an empty seat below that plays absent from tick 0. Every peer builds the same world from the same header.
4. **The first `delay` ticks run on no input.** Nobody can have sampled input for them, so the server queues empty input for every seat and sends those frames at once.
5. **Play.** Each client, each tick of its own clock: takes and steps every frame that has come, then sends its input for `nextInputTick()`. The server takes each input into its queue and sends a `NetFrame` — every seat's input, and which seats are absent — as soon as a tick is complete.
6. **Hashes.** A client reports its hash of every tick divisible by `NET_HASH_INTERVAL` (60). The server keeps the first report of each tick in a ring of 16 and compares later ones with it. A dedicated server that simulates reports its own hash too, first, so it is the reference.
7. **End.** `LockstepServer::end` sends `NetEnd` after the last frame. Clients keep the frames already sent and go back to their seats for the next start. `stopAt(tick)` makes the server send no frame at or past a tick, so every peer ends the run on the same one.

Every message after `NetStart` carries the run's number, and anything from an earlier run is ignored — an input sent before a client heard the next start is never taken for the new run's tick.

### 2.1 Input delay, and the stall

A client's input runs exactly `input_delay` ticks ahead of the frames it has taken and no further: `sendInput` refuses once `nextInputTick() >= nextFrameTick() + delay`. While the session waits on someone, a client cannot send, and its clock's ticks are simply spent waiting — that refusal is the local stall. The bound also keeps every client far inside the server queue's 64-tick reach (`sim::INPUT_QUEUE_TICKS`), so no input is ever refused as too far ahead.

The server's `waitingOn()` names the seats whose input for the next frame has not come — what a stall indicator shows. The delay is the server's (`LockstepServerConfig::input_delay`, `--delay`), 3 ticks by default.

### 2.2 Dropping and rejoining

A seat whose client disconnects stops *playing* the run but stays in it: the server marks it in every later frame's `absent` mask with zero input, and every peer fills that seat with `game::standInForAbsent` — the same stand-in the editor's multi-player preview plays with — from its own copy of the world, before stepping. Since each peer fills from identical state, each steps identical input, and the filled input is what gets recorded. The server needs nothing of the game to keep a dropped character playing.

A client that connects mid-run is seated — in a dropped seat or a free one — but does not play until the next start (ADR-005: joins at level boundaries only). When every playing seat has gone, the server sends no more frames.

A client that vanishes without disconnecting stalls the session until ENet notices, within 2–8 seconds; then it is dropped as above.

### 2.3 Desync

On the first report that differs from the first report of its tick, the server stops sending frames and sends `NetDesync` — the tick, the first differing section's index (or `NET_SECTION_COUNT_DIFFERS`), and who disagreed — to everyone in the run. Section names do not travel; each peer names the section from its own hash of that tick, which is why a deployed peer keeps its last 16 checkpoint hashes. `simplish-game` prints, for instance, `Desync at tick 60 in section logic, reported by player 2` — the test that makes one peer's logic draw a random number the other's does not.

---

## 3. The Protocol

One byte of kind — the alternative's index in `NetMessage`, so its order is the protocol — then the fields: fixed-width integers little-endian, ticks and counts as LEB128 varints, stick axes zigzagged. `NET_PROTOCOL_VERSION` is 1; any change to a message's bytes bumps it.

| Kind | Message | Direction | Fields |
|---|---|---|---|
| 0 | `NetHello` | client → server | protocol u16, content hash u64, character string |
| 1 | `NetWelcome` | server → client | seat u8 |
| 2 | `NetRefusal` | server → client | reason u8 (`PROTOCOL`, `CONTENT`, `FULL`) |
| 3 | `NetRoster` | server → seated | seated mask u8 |
| 4 | `NetStart` | server → seated | run u16, level string, content hash u64, seed u64, players u8, a character string per player, delay u8 |
| 5 | `NetInput` | client → server | run u16, tick varint, a `PlayerInput` |
| 6 | `NetFrame` | server → run | run u16, tick varint, absent mask u8, four `PlayerInput`s |
| 7 | `NetHashReport` | client → server | run u16, tick varint, combined u64, section count u8, section hashes u64 |
| 8 | `NetDesync` | server → run | run u16, tick varint, section u8, seat u8 |
| 9 | `NetEnd` | server → run | run u16 |

A `PlayerInput` is four zigzag-varint axes, then the buttons and the screen choice as varints bounded to 32 bits. Strings are a varint length of at most `NET_MAX_ID_BYTES` (256) and the bytes.

**Decoding is hostile-input safe**, as the replay decoder is: messages come from other machines. Nothing is read past the end, every count is bounded, a byte left over is refused, and the tests decode every strict prefix and every single-byte corruption of every message.

**Bandwidth** is a frame per tick to each client — about 20 bytes of held input plus ENet's header — and an input per tick from each: a few kilobytes a second per player, whatever the horde is doing (ADR-005).

---

## 4. The Transport

`NetTransport` is three calls: `poll` for the next `CONNECTED`, `RECEIVED` or `DISCONNECTED`; `send` to a peer; `disconnect` a peer once what was sent to it has gone. A server's transport has a peer per client; a client's has one, the server. **Nothing happens between polls** — ENet's handshake, acknowledgements and resends all run inside `poll`, so both ends must keep polling even when they expect nothing.

| Transport | Where | For |
|---|---|---|
| `LoopbackNetwork` | `engine/net` | Tests; a server and its clients in one process |
| `listenUdp` / `connectUdp` | `platform/net`, desktop | Every real session. ENet, IPv4, one reliable channel. `listenUdp(0, …)` binds any free port and says which |
| A distributor relay | not written | Steam, Epic and console sessions, through the same interface (Platform §4.3, PLT-DST-5) |

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
| `--delay TICKS` | The session's input delay, 1–30 (a server's flag) |
| `--pace real\|fast` | Sample input at 60 Hz of real time (the default), or whenever the session will take one — for tests and CI |

Nobody holds the controls of a headless game, so every local player is a stand-in, playing its seat through the network like anyone else. Each process prints how its run ended and its last tick's hash; for one session they are all the same. A run that could not start or stopped short says why — `The server's game content is not this game's`, `Could not reach the server`, `Lost the server at tick 1234`, or a desync.

A joining client builds its world from the `NetStart` exactly as the server's reference does (`DeployedNetWorld`): the level's baked setup, then the header's seed, player count and characters. The content hash a client sends is `deployedContentHash` — every file under the deployed `game/` folder, paths and bytes, in path order, hidden files skipped.

---

## 6. Testing

| Test | What it proves |
|---|---|
| `engine/net/test/test_net_codec.cpp` | Every message round-trips; every strict prefix, every trailing byte and out-of-range value is refused; single-byte corruption never reads out of bounds |
| `engine/net/test/test_loopback_network.cpp` | The loopback keeps the transport's contract |
| `engine/net/test/test_lockstep_server.cpp` | Seating and refusal, the start, the stall, frames, absent seats from a drop or from the start, rejoin at the next start, `stopAt`, hash comparison and the server's own reference, ending and restarting |
| `engine/net/test/test_lockstep_client.cpp` | The input-delay bound; two clients see identical frames; frames before an end survive it; a lost server |
| `platform/net/test/test_udp_transport.cpp` | ENet on 127.0.0.1: ports, whole ordered messages both ways, a disconnect heard, a lockstep session over real sockets |
| `editor/deploy/test/test_deployed_session.cpp` | Whole deployed runs over loopback: a dedicated server and two clients end on the same tick and hash; logic ending the run ends it for all; a dropped client played by its stand-in; other content refused; divergent logic caught at tick 60 and named; a host with a joiner; the flags |

`test/support/loopback-session.h` is a server and any number of clients on one loopback network, pumped together — the fixture for any new session behavior.

---

## 7. Not Yet

| Gap | Waiting on |
|---|---|
| A person at the controls of a networked run | The rendered client, and the editor's playtest joining a session. Both drive a `LockstepClient` as `DeployedClient` does: sample on the local clock, step on frames |
| Recording a networked run as a replay | Nothing: the header is the start's, and every stepped input — absent seats filled — is what a `ReplayRecorder` takes. Not yet wired into `DeployedClient` |
| Input delay tuned from measured RTT | The server sees every client; ENet measures RTT per peer. Configured by hand today |
| A stall indicator that names who everyone is waiting on | The rendered client; `LockstepServer::waitingOn` has the answer, but clients are not told it |
| Diagnostic capture on desync (both peers' recent tick traces) | Clients keep their last 16 checkpoint hashes; per-tick traces and shipping them to the server are not written |
| Distributor relays, NAT traversal, IPv6 | Distributor packages being wired (Platform §4.3); ENet is IPv4 |
| Host migration | Not planned: a host that quits ends the session (ADR-013) |
