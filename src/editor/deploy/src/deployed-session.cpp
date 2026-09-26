#include "deployed-client.h"
#include "deployed-pacer.h"
#include "deployed-server.h"

#include <chrono>
#include <editor/deploy/deployed-session.h>
#include <engine/net/udp-connect.h>
#include <engine/net/udp-listen.h>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  /// How long a server keeps answering after its run, so its last messages
  /// — the end of the run — reach clients before its sockets close.
  constexpr std::chrono::seconds SERVER_LINGER{2};

  /// A run that could not happen, for @p why.
  DeployedGameRun failed(std::string why) {
    DeployedGameRun run;
    run.error = std::move(why);
    return run;
  }

  /// Keep @p client answering for a moment after a desync, so its trace
  /// reaches the server before its connection closes.
  void linger(DeployedClient& client) {
    const auto until = std::chrono::steady_clock::now() + SERVER_LINGER;
    while (client.session().state() == net::NetClientState::DESYNCED &&
           std::chrono::steady_clock::now() < until) {
      client.drain();
      DeployedPacer(DeployedPace::REAL_TIME).rest();
    }
  }

  /// Keep @p server answering until its run is reported and its clients
  /// have gone, or for `SERVER_LINGER` past that.
  void linger(DeployedServer& server, std::ostream& out) {
    while (!server.finished()) {
      server.poll(out);
      DeployedPacer(DeployedPace::REAL_TIME).rest();
    }
    const auto until = std::chrono::steady_clock::now() + SERVER_LINGER;
    while (server.session().seated() != 0 &&
           std::chrono::steady_clock::now() < until) {
      server.poll(out);
      DeployedPacer(DeployedPace::REAL_TIME).rest();
    }
  }

  /// Say where a server is listening, and for how many.
  void announce(const DeployedGameOptions& options, uint16_t port,
                std::ostream& out) {
    out << (options.mode == DeployedGameMode::HOST ? "Hosting" : "Serving")
        << " on UDP port " << port << "; starting when "
        << static_cast<int>(options.players)
        << (options.players == 1 ? " player is in\n" : " players are in\n");
  }

  /// A listening transport on @p options' port, said to @p out; nothing
  /// when the port cannot be had.
  std::optional<net::UdpListen> listenFor(const DeployedGameOptions& options,
                                          std::ostream& out) {
    std::optional<net::UdpListen> listen =
        net::listenUdp(options.port, sim::MAX_PLAYERS);
    if (listen) {
      announce(options, listen->port, out);
    }
    return listen;
  }

  /// The run of a server that could not listen on @p options' port.
  DeployedGameRun cannotListen(const DeployedGameOptions& options) {
    return failed("Cannot listen on UDP port " + std::to_string(options.port));
  }

  DeployedGameRun serve(const DeployedGameOptions& options,
                        game::GameLogicFactory logic, std::ostream& out) {
    std::optional<net::UdpListen> listen = listenFor(options, out);
    if (!listen) {
      return cannotListen(options);
    }
    DeployedServer server(std::move(listen->transport), options, logic);
    const DeployedPacer pacer(options.pace);
    while (!server.finished()) {
      server.poll(out);
      pacer.rest();
    }
    linger(server, out);
    return server.run();
  }

  /// Poll @p server and @p client together until the client is done,
  /// ending the run when the client's world is over.
  void playHosted(DeployedServer& server, DeployedClient& client,
                  const DeployedGameOptions& options, std::ostream& out) {
    DeployedPacer pacer(options.pace);
    while (!client.finished()) {
      server.poll(out);
      client.poll(pacer.due(), out);
      if (client.worldOver() && !server.finished()) {
        server.endRun(out);
      }
      pacer.rest();
    }
    // A desync's trace from the host's own player goes through the server
    // polled here, before the client goes.
    for (int poll = 0; poll < 8 && !server.finished(); ++poll) {
      server.poll(out);
      client.drain();
    }
  }

  DeployedGameRun host(const DeployedGameOptions& options,
                       game::GameLogicFactory logic, std::ostream& out) {
    std::optional<net::UdpListen> listen = listenFor(options, out);
    if (!listen) {
      return cannotListen(options);
    }
    const uint16_t port = listen->port;
    DeployedServer server(std::move(listen->transport), options, logic);
    DeployedGameRun run;
    {
      // Gone before the linger, which waits for every client to leave.
      DeployedClient client(net::connectUdp("127.0.0.1", port), options, logic);
      playHosted(server, client, options, out);
      run = client.run();
    }
    linger(server, out);
    return run;
  }

  DeployedGameRun join(const DeployedGameOptions& options,
                       game::GameLogicFactory logic, std::ostream& out) {
    std::unique_ptr<net::NetTransport> transport =
        net::connectUdp(options.address, options.port);
    if (!transport) {
      return failed("Cannot find the server " + options.address);
    }
    out << "Joining " << options.address << ':' << options.port << '\n';
    DeployedClient client(std::move(transport), options, logic);
    DeployedPacer pacer(options.pace);
    while (!client.finished()) {
      client.poll(pacer.due(), out);
      pacer.rest();
    }
    linger(client);
    return client.run();
  }

}  // namespace

DeployedGameRun runDeployedSession(const DeployedGameOptions& options,
                                   game::GameLogicFactory logic,
                                   std::ostream& out) {
  if (options.mode == DeployedGameMode::SERVE) {
    return serve(options, logic, out);
  }
  if (options.mode == DeployedGameMode::HOST) {
    return host(options, logic, out);
  }
  if (options.mode == DeployedGameMode::JOIN) {
    return join(options, logic, out);
  }
  return failed("Not a networked game");
}

}  // namespace eng::editor
