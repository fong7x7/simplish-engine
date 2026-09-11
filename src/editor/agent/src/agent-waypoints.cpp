#include "agent-waypoints.h"

#include "agent-call.h"
#include "agent-json-values.h"

#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <optional>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// What `add_waypoint` says when it is called wrongly.
  constexpr std::string_view ADD_WAYPOINT_USAGE =
      "x and y are required; route, when given, is a number from 1 to 9, "
      "and order a number from 1 to 99";

  /// Record a changed waypoint as one undoable edit, and select it.
  AgentResult recordWaypoint(EditorShellState& state, size_t index,
                             const EditorWaypoint& prior,
                             const EditorWaypoint& next) {
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::TRANSFORM_WAYPOINT,
                         .index = index,
                         .waypoint = next,
                         .waypoint_prior = prior});
    state.selection = {EditorSelectionKind::WAYPOINT, index};
    return agentEdited(agentWaypointPayload(state, index));
  }

  /// The route an `add_waypoint` call names: the one it gives, else the
  /// selected waypoint's, else route 1 — the one a drag would pick.
  std::optional<uint8_t> routeParam(const EditorShellState& state,
                                    const json& params) {
    if (params.contains("route")) {
      const std::optional<double> route = agentNumberParam(params, "route");
      return route ? std::optional{clampEditorRoute(static_cast<float>(*route))}
                   : std::nullopt;
    }
    const EditorSelection& selection = state.selection;
    const bool on_waypoint = selection.kind == EditorSelectionKind::WAYPOINT &&
                             selection.index < state.document.waypoints.size();
    return on_waypoint ? state.document.waypoints[selection.index].route
                       : uint8_t{1};
  }

  /// The place an `add_waypoint` call names in @p route, or the one after
  /// its last waypoint when it names none.
  std::optional<uint16_t> orderParam(const EditorShellState& state,
                                     const json& params, uint8_t route) {
    if (!params.contains("order")) {
      return nextEditorWaypointOrder(state.document, route);
    }
    const std::optional<double> order = agentNumberParam(params, "order");
    return order ? std::optional{clampEditorWaypointOrder(
                       static_cast<float>(*order))}
                 : std::nullopt;
  }

  /// Add @p waypoint to the document as one undoable edit, and select it.
  AgentResult addWaypoint(EditorShellState& state, EditorWaypoint waypoint) {
    const size_t index = state.document.waypoints.size();
    waypoint.id = mintEditorWaypointId(state.document);
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::ADD_WAYPOINT,
                         .index = index,
                         .waypoint = waypoint});
    state.selection = {EditorSelectionKind::WAYPOINT, index};
    return agentEdited(agentWaypointPayload(state, index));
  }

}  // namespace

AgentResult runAgentAddWaypoint(EditorShellState& state, const json& params) {
  const std::optional<double> x = agentNumberParam(params, "x");
  const std::optional<double> y = agentNumberParam(params, "y");
  const std::optional<uint8_t> route = routeParam(state, params);
  const std::optional<uint16_t> order =
      route ? orderParam(state, params, *route) : std::nullopt;
  if (!x || !y || !order) {
    return agentFailure(AgentStatus::BAD_PARAMS, ADD_WAYPOINT_USAGE);
  }
  const WorldPoint at{static_cast<float>(*x), static_cast<float>(*y),
                      agentFloatParam(params, "z", 0.0f)};
  return addWaypoint(state, makeEditorWaypoint(*route, *order, at));
}

AgentResult setAgentWaypointField(EditorShellState& state, size_t index,
                                  EditorPropertyField field, float value) {
  if (!editorWaypointHasField(field)) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "a waypoint holds a route, an order and a position, "
                        "and nothing else");
  }
  const EditorWaypoint prior = state.document.waypoints[index];
  EditorWaypoint next = prior;
  setEditorWaypointValue(next, field, value);
  if (editorWaypointValue(next, field) == editorWaypointValue(prior, field)) {
    return agentOk(agentWaypointPayload(state, index));
  }
  return recordWaypoint(state, index, prior, next);
}

AgentResult translateAgentWaypoint(EditorShellState& state, size_t index,
                                   const json& params) {
  const EditorWaypoint prior = state.document.waypoints[index];
  EditorWaypoint next = prior;
  next.position.x += agentFloatParam(params, "dx", 0.0f);
  next.position.y += agentFloatParam(params, "dy", 0.0f);
  next.position.z += agentFloatParam(params, "dz", 0.0f);
  if (next.position.x == prior.position.x &&
      next.position.y == prior.position.y &&
      next.position.z == prior.position.z) {
    return agentOk(agentWaypointPayload(state, index));
  }
  return recordWaypoint(state, index, prior, next);
}

std::string agentWaypointPayload(const EditorShellState& state, size_t index) {
  json out = agentWaypointValue(state.document.waypoints[index]);
  out["index"] = index;
  return out.dump(2);
}

}  // namespace eng::editor
