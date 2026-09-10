#include "agent-call.h"

#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  /// The value at @p key, or a null when the params are not an object or
  /// hold no such key.
  const nlohmann::json& paramAt(const nlohmann::json& params,
                                std::string_view key) {
    static const nlohmann::json MISSING;
    if (!params.is_object()) {
      return MISSING;
    }
    const auto found = params.find(std::string(key));
    return found == params.end() ? MISSING : *found;
  }

}  // namespace

std::optional<double> agentNumberParam(const nlohmann::json& params,
                                       std::string_view key) {
  const nlohmann::json& value = paramAt(params, key);
  if (!value.is_number()) {
    return std::nullopt;
  }
  return value.get<double>();
}

std::optional<size_t> agentIndexParam(const nlohmann::json& params,
                                      std::string_view key) {
  const nlohmann::json& value = paramAt(params, key);
  if (!value.is_number_unsigned()) {
    return std::nullopt;
  }
  return value.get<size_t>();
}

std::optional<std::string> agentStringParam(const nlohmann::json& params,
                                            std::string_view key) {
  const nlohmann::json& value = paramAt(params, key);
  if (!value.is_string()) {
    return std::nullopt;
  }
  return value.get<std::string>();
}

float agentFloatParam(const nlohmann::json& params, std::string_view key,
                      float fallback) {
  const std::optional<double> value = agentNumberParam(params, key);
  return value ? static_cast<float>(*value) : fallback;
}

std::optional<bool> agentBoolParam(const nlohmann::json& params,
                                   std::string_view key) {
  const nlohmann::json& value = paramAt(params, key);
  if (!value.is_boolean()) {
    return std::nullopt;
  }
  return value.get<bool>();
}

AgentResult agentOk(std::string payload) {
  return {AgentStatus::OK, std::move(payload), {}, false};
}

AgentResult agentEdited(std::string payload) {
  return {AgentStatus::OK, std::move(payload), {}, true};
}

AgentResult agentFailure(AgentStatus status, std::string_view message) {
  return {status, nlohmann::json{{"message", message}}.dump(2), {}, false};
}

}  // namespace eng::editor
