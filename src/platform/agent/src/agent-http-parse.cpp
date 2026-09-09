#include <algorithm>
#include <cctype>
#include <cstddef>
#include <engine/agent/agent-http-parse.h>
#include <string>
#include <string_view>

namespace eng::agent {

namespace {

  /// Where the headers stop and the body starts.
  constexpr std::string_view HEADER_END = "\r\n\r\n";

  /// The header the body's length is declared in, lower-cased for the
  /// comparison below: header names are case-insensitive, and clients
  /// disagree about which case to send this one in.
  constexpr std::string_view CONTENT_LENGTH = "content-length:";

  /// @p text lower-cased, for a case-insensitive search over headers.
  std::string lowered(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return out;
  }

  /// The digits at the front of @p text as a length, zero when there are
  /// none. Anything that is not a digit ends the number, which is what
  /// makes trailing whitespace and a stray carriage return harmless.
  size_t readLength(std::string_view text) {
    size_t value = 0;
    for (const char c : text) {
      if (c < '0' || c > '9') {
        break;
      }
      value = value * 10 + static_cast<size_t>(c - '0');
    }
    return value;
  }

  /// The body length @p headers declares, or zero when they declare none.
  size_t contentLength(std::string_view headers) {
    const std::string lower = lowered(headers);
    const size_t at = lower.find(CONTENT_LENGTH);
    if (at == std::string::npos) {
      return 0;
    }
    std::string_view value = headers.substr(at + CONTENT_LENGTH.size());
    while (!value.empty() && value.front() == ' ') {
      value.remove_prefix(1);
    }
    return readLength(value);
  }

  /// The word of @p text before @p from, and @p from advanced past it.
  std::string_view nextToken(std::string_view text, size_t& from) {
    const size_t start = from;
    const size_t space = text.find(' ', start);
    const size_t end = space == std::string_view::npos ? text.size() : space;
    from = end + 1;
    return text.substr(start, end - start);
  }

}  // namespace

std::optional<size_t> agentHttpRequestLength(std::string_view buffer) {
  const size_t head = buffer.find(HEADER_END);
  if (head == std::string_view::npos) {
    return std::nullopt;
  }
  const size_t total =
      head + HEADER_END.size() + contentLength(buffer.substr(0, head));
  if (buffer.size() < total) {
    return std::nullopt;
  }
  return total;
}

AgentHttpRequest parseAgentHttpRequest(std::string_view text) {
  AgentHttpRequest request;
  size_t at = 0;
  request.method = std::string(nextToken(text, at));
  std::string_view target = nextToken(text, at);
  // A query string is not part of the route, and nothing here reads one.
  request.path = std::string(target.substr(0, target.find('?')));
  const size_t head = text.find(HEADER_END);
  if (head != std::string_view::npos) {
    request.body = std::string(text.substr(head + HEADER_END.size()));
  }
  return request;
}

std::string formatAgentHttpResponse(int status, std::string_view body) {
  std::string out = "HTTP/1.1 " + std::to_string(status) + " ";
  out += status == 200 ? "OK" : "Error";
  out += "\r\nContent-Type: application/json\r\nContent-Length: ";
  out += std::to_string(body.size());
  // One request per connection, closed on the way out: the client is a
  // script or an agent making one call, not a browser holding a session.
  out += "\r\nConnection: close\r\n\r\n";
  out += body;
  return out;
}

}  // namespace eng::agent
