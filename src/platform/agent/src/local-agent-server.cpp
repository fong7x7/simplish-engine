#include <cstddef>
#include <cstdint>
#include <cstring>
#include <engine/agent/agent-http-parse.h>
#include <engine/agent/local-agent-server.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace eng::agent {

namespace {

#ifdef _WIN32
  using NativeSocket = SOCKET;
  /// What the platform's send and recv take for a buffer length.
  using IoLength = int;
#else
  using NativeSocket = int;
  /// What the platform's send and recv take for a buffer length.
  using IoLength = size_t;
#endif

  /// Whether a socket should wait for its work or report having none.
  ///
  /// A named pair rather than a bare `bool`, so the two calls that switch a
  /// socket over read as what they mean at the call site.
  enum class SocketMode : uint8_t { BLOCKING, NON_BLOCKING };

  /// How many connections one poll will accept before leaving the rest for
  /// the next frame. A caller opening sockets in a loop then costs a frame
  /// a bounded amount of work rather than all of it.
  constexpr int ACCEPTS_PER_POLL = 16;

  /// How much is read from one connection per poll.
  constexpr size_t READ_CHUNK = 4096;

  /// The stored socket as the platform's own type.
  NativeSocket native(intptr_t socket) {
    return static_cast<NativeSocket>(socket);
  }

  /// Bring the socket layer up. Winsock needs asking; everything else does
  /// not, and says so by succeeding.
  bool startSockets() {
#ifdef _WIN32
    static WSADATA data;
    static const int started = WSAStartup(MAKEWORD(2, 2), &data);
    return started == 0;
#else
    return true;
#endif
  }

  /// Whether the last call failed only because there was nothing to do.
  bool wouldBlock() {
#ifdef _WIN32
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
  }

  /// Close one socket.
  void closeSocket(intptr_t socket) {
#ifdef _WIN32
    closesocket(native(socket));
#else
    ::close(native(socket));
#endif
  }

  /// Put a socket into non-blocking mode, or back into blocking mode.
  bool setSocketMode(intptr_t socket, SocketMode wanted) {
    const bool blocking = wanted == SocketMode::BLOCKING;
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    return ioctlsocket(native(socket), FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(native(socket), F_GETFL, 0);
    if (flags < 0) {
      return false;
    }
    const int next = blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK;
    return fcntl(native(socket), F_SETFL, next) == 0;
#endif
  }

  /// A TCP socket that may be re-bound straight after a previous run of the
  /// editor let go of the port.
  intptr_t makeListenerSocket() {
    const intptr_t socket =
        static_cast<intptr_t>(::socket(AF_INET, SOCK_STREAM, 0));
    if (socket < 0) {
      return AGENT_SOCKET_NONE;
    }
    int on = 1;
    (void)setsockopt(native(socket), SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&on), sizeof(on));
    return socket;
  }

  /// Bind @p socket to @p port on the loopback interface and listen.
  bool bindLoopback(intptr_t socket, uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    // Loopback and nothing else: this is the server's entire access
    // control, so it is not a parameter.
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(native(socket), reinterpret_cast<const sockaddr*>(&address),
               sizeof(address)) != 0) {
      return false;
    }
    return ::listen(native(socket), 8) == 0;
  }

  /// The port a bound socket actually got, which is what matters when the
  /// caller asked for port zero and let the system choose.
  uint16_t boundPort(intptr_t socket) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (getsockname(native(socket), reinterpret_cast<sockaddr*>(&address),
                    &length) != 0) {
      return 0;
    }
    return ntohs(address.sin_port);
  }

  /// Write all of @p text, blocking until it is gone. The socket is closed
  /// straight after, so there is nothing left for this to hold up.
  void sendAll(intptr_t socket, std::string_view text) {
    (void)setSocketMode(socket, SocketMode::BLOCKING);
    size_t sent = 0;
    while (sent < text.size()) {
      const auto wrote = ::send(native(socket), text.data() + sent,
                                static_cast<IoLength>(text.size() - sent), 0);
      if (wrote <= 0) {
        return;
      }
      sent += static_cast<size_t>(wrote);
    }
  }

}  // namespace

LocalAgentServer::~LocalAgentServer() {
  close();
}

bool LocalAgentServer::open(uint16_t port) {
  close();
  if (!startSockets()) {
    return false;
  }
  const intptr_t socket = makeListenerSocket();
  if (socket == AGENT_SOCKET_NONE) {
    return false;
  }
  if (!setSocketMode(socket, SocketMode::NON_BLOCKING) ||
      !bindLoopback(socket, port)) {
    closeSocket(socket);
    return false;
  }
  listener_ = socket;
  port_ = boundPort(socket);
  return true;
}

void LocalAgentServer::close() {
  for (const AgentConnection& connection : connections_) {
    closeSocket(connection.socket);
  }
  connections_.clear();
  if (listener_ != AGENT_SOCKET_NONE) {
    closeSocket(listener_);
    listener_ = AGENT_SOCKET_NONE;
  }
  port_ = 0;
}

bool LocalAgentServer::isOpen() const {
  return listener_ != AGENT_SOCKET_NONE;
}

uint16_t LocalAgentServer::port() const {
  return port_;
}

void LocalAgentServer::acceptPending() {
  for (int i = 0; i < ACCEPTS_PER_POLL; ++i) {
    const intptr_t client =
        static_cast<intptr_t>(::accept(native(listener_), nullptr, nullptr));
    if (client < 0) {
      return;
    }
    (void)setSocketMode(client, SocketMode::NON_BLOCKING);
    connections_.push_back({client, {}});
  }
}

bool LocalAgentServer::respond(
    AgentConnection& connection, size_t length,
    const std::function<AgentHttpResponse(const AgentHttpRequest&)>& handler) {
  const AgentHttpRequest request = parseAgentHttpRequest(
      std::string_view(connection.buffer).substr(0, length));
  const AgentHttpResponse response = handler(request);
  sendAll(connection.socket,
          formatAgentHttpResponse(response.status, response.body));
  // One request per connection, which is what `Connection: close` said.
  return false;
}

bool LocalAgentServer::serve(
    AgentConnection& connection,
    const std::function<AgentHttpResponse(const AgentHttpRequest&)>& handler) {
  char chunk[READ_CHUNK];
  const auto got = ::recv(native(connection.socket), chunk, READ_CHUNK, 0);
  if (got == 0 || (got < 0 && !wouldBlock())) {
    return false;
  }
  if (got > 0) {
    connection.buffer.append(chunk, static_cast<size_t>(got));
  }
  if (connection.buffer.size() > AGENT_HTTP_MAX_REQUEST) {
    return false;
  }
  const std::optional<size_t> length =
      agentHttpRequestLength(connection.buffer);
  return length ? respond(connection, *length, handler) : true;
}

void LocalAgentServer::poll(
    const std::function<AgentHttpResponse(const AgentHttpRequest&)>& handler) {
  if (!isOpen()) {
    return;
  }
  acceptPending();
  for (AgentConnection& connection : connections_) {
    if (!serve(connection, handler)) {
      closeSocket(connection.socket);
      connection.socket = AGENT_SOCKET_NONE;
    }
  }
  std::erase_if(connections_, [](const AgentConnection& connection) {
    return connection.socket == AGENT_SOCKET_NONE;
  });
}

}  // namespace eng::agent
