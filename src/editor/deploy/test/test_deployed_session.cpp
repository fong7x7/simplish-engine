#include "deployed-client.h"
#include "deployed-replay.h"
#include "deployed-server.h"
#include "support/deployed-content-fixture.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <editor/deploy/deployed-game.h>
#include <editor/project/project-text-file.h>
#include <engine/net/loopback-network.h>
#include <memory>
#include <sstream>
#include <vector>

using namespace eng;
using namespace eng::editor;
using eng::editor::test::DeployedContentFixture;

namespace {

/// Polls before a session test gives up on it finishing.
constexpr int MOST_POLLS = 100000;

/// Loses the run on tick 9.
class GivesUp final : public game::GameLogic {
public:
  void tick(game::GameLogicWorld& world) override {
    if (world.tick() == 9) {
      world.endRun(game::RunOutcome::LOST);
    }
  }
};

/// Does nothing.
class Quiet final : public game::GameLogic {
public:
  void tick([[maybe_unused]] game::GameLogicWorld& world) override {}
};

/// Draws from the run's random stream on tick 1 — which a peer running
/// `Quiet` does not, so the two diverge.
class Draws final : public game::GameLogic {
public:
  void tick(game::GameLogicWorld& world) override {
    if (world.tick() == 1) {
      (void)world.random(100);
    }
  }
};

game::GameLogic* makeGivesUp() {
  return std::make_unique<GivesUp>().release();
}

game::GameLogic* makeQuiet() {
  return std::make_unique<Quiet>().release();
}

game::GameLogic* makeDraws() {
  return std::make_unique<Draws>().release();
}

/// Options that play back the replay at @p replay against @p content.
DeployedGameOptions verifying(const DeployedContentFixture& content,
                              const std::filesystem::path& replay) {
  DeployedGameOptions options = content.options(0);
  options.verify = replay;
  return options;
}

void unmake(game::GameLogic* logic) {
  const std::unique_ptr<game::GameLogic> owned(logic);
}

/// @p options as @p mode, waiting for @p players, sampling input as fast
/// as the session takes it.
DeployedGameOptions as(DeployedGameOptions options, DeployedGameMode mode,
                       uint8_t players) {
  options.mode = mode;
  options.players = players;
  options.pace = DeployedPace::FAST;
  return options;
}

/// A dedicated server and its clients on one loopback network.
class ServedSession {
public:
  ServedSession(const DeployedGameOptions& options,
                game::GameLogicFactory logic)
    : server_(network_.listen(),
              as(options, DeployedGameMode::SERVE, options.players), logic) {}

  /// A client of @p content running @p logic.
  DeployedClient& join(const DeployedGameOptions& options,
                       game::GameLogicFactory logic) {
    clients_.push_back(std::make_unique<DeployedClient>(
        network_.connect(), as(options, DeployedGameMode::JOIN, 1), logic));
    return *clients_.back();
  }

  /// Poll everyone until the server and every client are done.
  void play(std::ostream& out) {
    for (int poll = 0; poll < MOST_POLLS && !done(); ++poll) {
      server_.poll(out);
      for (const std::unique_ptr<DeployedClient>& client : clients_) {
        client->poll(1, out);
      }
    }
    for (const std::unique_ptr<DeployedClient>& client : clients_) {
      client->poll(1, out);  // Hear the end the server sent last.
    }
  }

  /// Poll everyone @p polls times.
  void step(int polls, std::ostream& out) {
    for (int poll = 0; poll < polls; ++poll) {
      server_.poll(out);
      for (const std::unique_ptr<DeployedClient>& client : clients_) {
        client->poll(1, out);
      }
    }
  }

  /// Whether every client's run ended on the server's tick and hash.
  [[nodiscard]] bool clientsAgree() const {
    bool agree = true;
    for (const std::unique_ptr<DeployedClient>& client : clients_) {
      agree = agree && client->run().error.empty() &&
              client->run().ticks == server_.run().ticks &&
              client->run().hash == server_.run().hash;
    }
    return agree;
  }

  /// Drop client @p index, as a player quitting would.
  void drop(std::size_t index) {
    clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
  }

  [[nodiscard]] DeployedServer& server() { return server_; }
  [[nodiscard]] DeployedClient& client(std::size_t index) {
    return *clients_[index];
  }

private:
  [[nodiscard]] bool done() const {
    bool all = server_.finished();
    for (const std::unique_ptr<DeployedClient>& client : clients_) {
      all = all && client->finished();
    }
    return all;
  }

  /// The network everyone is on.
  net::LoopbackNetwork network_;
  /// The server.
  DeployedServer server_;
  /// Its clients.
  std::vector<std::unique_ptr<DeployedClient>> clients_;
};

}  // namespace

TEST_CASE("a dedicated server and two clients end the same run on the same "
          "hash") {
  const DeployedContentFixture content;
  std::ostringstream out;
  DeployedGameOptions options = as(content.options(200), {}, 2);
  ServedSession session(options, {});
  session.join(options, {});
  session.join(options, {});

  session.play(out);

  const DeployedGameRun& reference = session.server().run();
  CHECK(reference.error.empty());
  CHECK(reference.players == 2);
  CHECK(reference.ticks == 200);
  CHECK(reference.hash != 0);
  CHECK(session.clientsAgree());
  CHECK(out.str().contains("Starting arena for players 1 and 2, input delay 2 "
                           "ticks (measured)"));
}

TEST_CASE("a served run a client's logic ends is over for everyone on the "
          "same tick") {
  const DeployedContentFixture content;
  std::ostringstream out;
  DeployedGameOptions options = as(content.options(500), {}, 2);
  ServedSession session(options, {makeGivesUp, unmake});
  session.join(options, {makeGivesUp, unmake});
  session.join(options, {makeGivesUp, unmake});

  session.play(out);

  CHECK(session.server().run().ticks == 10);
  CHECK(session.server().run().outcome == game::RunOutcome::LOST);
  CHECK(session.server().run().logic);
  CHECK(session.client(0).run().ticks == 10);
  CHECK(session.client(1).run().outcome == game::RunOutcome::LOST);
  CHECK(session.client(1).run().hash == session.server().run().hash);
}

TEST_CASE("a client who leaves mid-run is played by a stand-in, and the run "
          "goes on") {
  const DeployedContentFixture content;
  std::ostringstream out;
  DeployedGameOptions options = as(content.options(300), {}, 2);
  ServedSession session(options, {});
  session.join(options, {});
  session.join(options, {});
  session.step(50, out);
  REQUIRE(session.server().session().playing() == 0b11);
  session.drop(1);

  session.play(out);

  CHECK(session.server().run().error.empty());
  CHECK(session.server().run().ticks == 300);
  CHECK(session.clientsAgree());
}

TEST_CASE("a client with other content is refused before it plays") {
  const DeployedContentFixture content;
  const DeployedContentFixture modded("a changed table");
  std::ostringstream out;
  DeployedGameOptions options = as(content.options(10), {}, 1);
  ServedSession session(options, {});
  DeployedClient& stranger = session.join(modded.options(10), {});

  for (int poll = 0; poll < 20; ++poll) {
    session.server().poll(out);
    stranger.poll(1, out);
  }

  CHECK(stranger.finished());
  CHECK(stranger.run().error == "The server's game content is not this game's");
  CHECK_FALSE(session.server().finished());
}

TEST_CASE("peers that diverge are halted, and told the tick") {
  const DeployedContentFixture content;
  std::ostringstream out;
  DeployedGameOptions options = as(content.options(300), {}, 2);
  ServedSession session(options, {makeQuiet, unmake});
  session.join(options, {makeQuiet, unmake});
  session.join(options, {makeDraws, unmake});

  session.play(out);

  const std::string& error = session.client(1).run().error;
  CHECK(error.starts_with("Desync at tick 60 in section "));
  CHECK(error.ends_with("reported by player 2"));
  CHECK(session.client(0).run().error == error);
  CHECK(session.server().run().error.starts_with("Desync at tick 60"));
}

TEST_CASE("a desync's traces pin the first diverging tick, and a report is "
          "written") {
  const DeployedContentFixture content;
  std::ostringstream out;
  DeployedGameOptions options = as(content.options(300), {}, 2);
  options.desync_dir = content.path();
  ServedSession session(options, {makeQuiet, unmake});
  session.join(options, {makeQuiet, unmake});
  session.join(options, {makeDraws, unmake});

  session.play(out);

  const std::string& error = session.server().run().error;
  CHECK(error.contains("first diverged at tick 1 in section logic"));
  const auto report = content.path() / "simplish-desync-arena-tick60.txt";
  CHECK(error.ends_with("Report: " + report.string()));
  CHECK(readProjectTextFile(report)->starts_with("Simplish desync report"));
}

TEST_CASE("a served run's replays, the server's and a client's, play back "
          "to its end") {
  const DeployedContentFixture content;
  std::ostringstream out;
  DeployedGameOptions served = as(content.options(150), {}, 1);
  served.replay = content.path() / "server.replay";
  DeployedGameOptions joined = served;
  joined.replay = content.path() / "client.replay";
  ServedSession session(served, {});
  session.join(joined, {});
  session.play(out);

  for (const auto& path : {served.replay, joined.replay}) {
    const DeployedGameRun run =
        verifyDeployedReplay(verifying(content, path), {});
    CHECK(run.error.empty());
    CHECK(run.ticks == 150);
    CHECK(run.hash == session.server().run().hash);
  }
}

TEST_CASE("a client stalled on another says who it is waiting for") {
  const DeployedContentFixture content;
  std::ostringstream out;
  DeployedGameOptions options = as(content.options(300), {}, 2);
  ServedSession session(options, {});
  session.join(options, {});
  session.join(options, {});
  session.step(20, out);
  const auto until = std::chrono::steady_clock::now() + DEPLOYED_STALL_NOTICE +
                     std::chrono::milliseconds(200);
  while (std::chrono::steady_clock::now() < until) {
    session.server().poll(out);
    session.client(0).poll(1, out);
  }
  CHECK(out.str().contains("Waiting for player 2 at tick"));
  CHECK(out.str().contains("Waiting for player 2\n"));
}

TEST_CASE("a host plays on its own server, a joiner with it, to the same "
          "end") {
  const DeployedContentFixture content;
  std::ostringstream out;
  const DeployedGameOptions options = as(content.options(150), {}, 2);
  net::LoopbackNetwork network;
  DeployedServer server(network.listen(),
                        as(options, DeployedGameMode::HOST, 2), {});
  DeployedClient host(network.connect(), options, {});
  DeployedClient guest(network.connect(), options, {});

  for (int poll = 0; poll < MOST_POLLS && !guest.finished(); ++poll) {
    server.poll(out);
    host.poll(1, out);
    guest.poll(1, out);
  }

  CHECK(host.run().ticks == 150);
  CHECK(guest.run().ticks == 150);
  CHECK(guest.run().hash == host.run().hash);
}

TEST_CASE("simplish-game reads a server's flags") {
  const std::string_view serve[] = {"--serve", "7000", "--players", "3",
                                    "--delay", "4",    "--pace",    "fast"};
  const std::string_view host[] = {"--host", "0"};

  const auto served = parseDeployedGameArgs(serve);

  REQUIRE(served);
  CHECK(served->mode == DeployedGameMode::SERVE);
  CHECK(served->port == 7000);
  CHECK(served->players == 3);
  CHECK(served->input_delay == 4);
  CHECK(served->pace == DeployedPace::FAST);
  CHECK(parseDeployedGameArgs(host)->mode == DeployedGameMode::HOST);
}

TEST_CASE("simplish-game reads a measured delay, and where replays and "
          "reports go") {
  const std::string_view args[] = {"--delay", "auto",         "--replay",
                                   "a.rpl",   "--desync-dir", "out"};
  const std::string_view verify[] = {"--verify", "b.rpl"};

  const auto options = parseDeployedGameArgs(args);

  REQUIRE(options);
  CHECK_FALSE(options->input_delay);
  CHECK(options->replay == "a.rpl");
  CHECK(options->desync_dir == "out");
  CHECK(parseDeployedGameArgs(verify)->mode == DeployedGameMode::VERIFY);
  CHECK(parseDeployedGameArgs(verify)->verify == "b.rpl");
}

TEST_CASE("simplish-game reads where to join, with or without a port") {
  const std::string_view join[] = {"--join", "10.0.0.5:7001"};
  const std::string_view join_default[] = {"--join", "example.org"};

  const auto joined = parseDeployedGameArgs(join);

  REQUIRE(joined);
  CHECK(joined->mode == DeployedGameMode::JOIN);
  CHECK(joined->address == "10.0.0.5");
  CHECK(joined->port == 7001);
  CHECK(parseDeployedGameArgs(join_default)->port == net::UDP_DEFAULT_PORT);
}

TEST_CASE("simplish-game refuses session flags that do not fit") {
  const std::string_view bad_port[] = {"--serve", "70000"};
  const std::string_view bad_join[] = {"--join", "host:port"};
  const std::string_view no_host[] = {"--join", ":7000"};
  const std::string_view bad_delay[] = {"--delay", "0"};
  const std::string_view bad_pace[] = {"--pace", "slow"};

  CHECK_FALSE(parseDeployedGameArgs(bad_port));
  CHECK_FALSE(parseDeployedGameArgs(bad_join));
  CHECK_FALSE(parseDeployedGameArgs(no_host));
  CHECK_FALSE(parseDeployedGameArgs(bad_delay));
  CHECK_FALSE(parseDeployedGameArgs(bad_pace));
}
