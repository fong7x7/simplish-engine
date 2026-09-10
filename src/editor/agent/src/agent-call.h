#pragma once

/// @file agent-call.h
/// @brief Reading a tool's parameters, and shaping what it answers with.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/agent/agent-result.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace eng::editor {

/// The number at @p key, or nothing when it is absent or not a number.
///
/// Parameters arrive off a socket, so nothing here trusts a type: a missing
/// key and a key holding a string are the same answer, and the caller turns
/// either into `BAD_PARAMS`.
[[nodiscard]] std::optional<double>
agentNumberParam(const nlohmann::json& params, std::string_view key);

/// The list index at @p key: a whole number, and never negative.
[[nodiscard]] std::optional<size_t>
agentIndexParam(const nlohmann::json& params, std::string_view key);

/// The string at @p key, or nothing when it is absent or not a string.
[[nodiscard]] std::optional<std::string>
agentStringParam(const nlohmann::json& params, std::string_view key);

/// The number at @p key, or @p fallback when there is none. For the
/// parameters the schema marks optional and documents a default for.
[[nodiscard]] float agentFloatParam(const nlohmann::json& params,
                                    std::string_view key, float fallback);

/// The boolean at @p key, or nothing when it is absent or not one.
[[nodiscard]] std::optional<bool> agentBoolParam(const nlohmann::json& params,
                                                 std::string_view key);

/// A successful read: @p payload, and nothing for the editor to do.
[[nodiscard]] AgentResult agentOk(std::string payload);

/// A successful write: @p payload, and the chrome to rebuild from state.
[[nodiscard]] AgentResult agentEdited(std::string payload);

/// A failure, carrying a message written in terms of the call that was
/// made rather than of the code that refused it.
[[nodiscard]] AgentResult agentFailure(AgentStatus status,
                                       std::string_view message);

}  // namespace eng::editor
