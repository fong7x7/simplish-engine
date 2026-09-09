#pragma once

/// @file agent-http-parse.h
/// @brief Reading a request out of a socket buffer.
/// @par Threading Thread-safe (pure functions over strings).

#include <cstddef>
#include <engine/agent/agent-http-request.h>
#include <optional>
#include <string_view>

namespace eng::agent {

/// The largest request this server will hold in a buffer.
///
/// A caller that never sends a blank line would otherwise grow the buffer
/// until something else on the machine noticed. Far larger than any tool
/// call — a placement is a few hundred bytes — and far smaller than a
/// problem.
inline constexpr size_t AGENT_HTTP_MAX_REQUEST = 1 << 20;

/// How many bytes of @p buffer make up one complete request, or nothing
/// when the rest of it has not arrived yet.
///
/// Headers end at the blank line; the body is however many bytes
/// `Content-Length` claims. A request with no such header has no body,
/// which is what a GET is.
[[nodiscard]] std::optional<size_t>
agentHttpRequestLength(std::string_view buffer);

/// Take apart one complete request. The caller has already established
/// that it is complete, with `agentHttpRequestLength`.
[[nodiscard]] AgentHttpRequest parseAgentHttpRequest(std::string_view text);

/// @p response as the bytes to write back, headers and all.
[[nodiscard]] std::string formatAgentHttpResponse(int status,
                                                  std::string_view body);

}  // namespace eng::agent
