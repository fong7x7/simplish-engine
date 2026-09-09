#!/usr/bin/env python3
"""MCP bridge to a running Simplish editor.

Speaks MCP over stdio to an agent, and HTTP to the editor's agent API on the
loopback interface. It holds no tool list of its own: `tools/list` fetches
the editor's manifest and converts it, so a tool added to
`src/editor/agent/agent-tool-info.h` shows up here with no change to this
file. That is the whole design — see docs/editor/agent-api.md.

Start the editor with the API open, then point an MCP client at this script:

    SIMPLISH_AGENT_PORT=default ./scripts/editor.sh ~/projects/level
    claude mcp add simplish-editor -- python3 scripts/simplish-editor-mcp.py

Environment:
    SIMPLISH_AGENT_PORT   port the editor is listening on (default 8787;
                          the word "default" also means 8787)
    SIMPLISH_AGENT_HOST   host to reach it on (default 127.0.0.1)
"""

import json
import os
import sys
import urllib.error
import urllib.request

DEFAULT_PORT = 8787
PROTOCOL_VERSION = "2024-11-05"
SERVER_INFO = {"name": "simplish-editor", "version": "1"}
TIMEOUT_SECONDS = 5

# How the editor's own parameter vocabulary becomes JSON Schema. The editor
# publishes one of these four words per parameter; anything else is a new
# word on that side and is reported rather than guessed at.
SCHEMA_TYPES = {
    "number": {"type": "number"},
    "integer": {"type": "integer", "minimum": 0},
    "string": {"type": "string"},
    # An asset is named either by its index or by its name, and the editor
    # accepts both, so the schema says both.
    "asset_ref": {"type": ["integer", "string"]},
}


def resolve_port():
    """The port the editor was told to listen on."""
    value = os.environ.get("SIMPLISH_AGENT_PORT", "").strip()
    if not value or value == "default":
        return DEFAULT_PORT
    try:
        return int(value)
    except ValueError:
        return DEFAULT_PORT


BASE_URL = "http://{}:{}".format(
    os.environ.get("SIMPLISH_AGENT_HOST", "127.0.0.1"), resolve_port()
)


class EditorUnreachable(Exception):
    """The editor is not listening, or not listening there."""


def request_editor(path, body=None):
    """One HTTP call to the editor, returning its parsed JSON."""
    url = BASE_URL + path
    data = None if body is None else json.dumps(body).encode("utf-8")
    request = urllib.request.Request(
        url, data=data, headers={"Content-Type": "application/json"}
    )
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as reply:
            return json.loads(reply.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        return json.loads(error.read().decode("utf-8"))
    except (urllib.error.URLError, OSError, TimeoutError) as error:
        raise EditorUnreachable(
            "cannot reach the Simplish editor at {} ({}). Start it with "
            "SIMPLISH_AGENT_PORT set, e.g. "
            "SIMPLISH_AGENT_PORT=default ./scripts/editor.sh <project>".format(
                BASE_URL, error
            )
        ) from error


def schema_for(params):
    """A JSON Schema object for one tool's published parameters."""
    properties = {}
    required = []
    for param in params:
        entry = dict(SCHEMA_TYPES.get(param["type"], {"type": "string"}))
        entry["description"] = param["description"]
        properties[param["name"]] = entry
        if param["required"]:
            required.append(param["name"])
    schema = {"type": "object", "properties": properties}
    if required:
        schema["required"] = required
    return schema


def describe_tool(tool):
    """One editor tool as MCP describes a tool.

    The name is the editor's own, unchanged. A tool an agent calls here is
    the same tool by the same name over HTTP, which is what keeps the two
    surfaces from drifting into two vocabularies.
    """
    summary = tool["summary"]
    if tool["effect"] != "read":
        summary += " (changes the editor)"
    return {
        "name": tool["name"],
        "description": summary,
        "inputSchema": schema_for(tool["params"]),
    }


def list_tools():
    """Every tool the running editor offers, as MCP tool definitions."""
    manifest = request_editor("/tools")
    return {"tools": [describe_tool(tool) for tool in manifest["tools"]]}


def call_tool(params):
    """Run one tool in the editor and hand back what it said."""
    name = params.get("name", "")
    arguments = params.get("arguments") or {}
    reply = request_editor("/tools/" + name, arguments)
    text = json.dumps(reply.get("result", reply), indent=2)
    return {
        "content": [{"type": "text", "text": text}],
        # A tool that refused says so in its status; the agent should see
        # that as a failed call rather than as a successful empty one.
        "isError": reply.get("status", "ok") != "ok",
    }


def handle(method, params):
    """Answer one JSON-RPC method, or raise for one nothing here serves."""
    if method == "initialize":
        return {
            "protocolVersion": PROTOCOL_VERSION,
            "capabilities": {"tools": {"listChanged": False}},
            "serverInfo": SERVER_INFO,
        }
    if method == "tools/list":
        return list_tools()
    if method == "tools/call":
        return call_tool(params)
    if method == "ping":
        return {}
    raise LookupError(method)


def respond(message_id, result=None, error=None):
    """Write one JSON-RPC response line to stdout."""
    reply = {"jsonrpc": "2.0", "id": message_id}
    if error is None:
        reply["result"] = result
    else:
        reply["error"] = error
    sys.stdout.write(json.dumps(reply) + "\n")
    sys.stdout.flush()


def dispatch(message):
    """Run one message, answering unless it was a notification."""
    method = message.get("method", "")
    message_id = message.get("id")
    if message_id is None:
        return
    try:
        respond(message_id, result=handle(method, message.get("params") or {}))
    except LookupError:
        respond(message_id, error={"code": -32601,
                                   "message": "no such method: " + method})
    except EditorUnreachable as error:
        respond(message_id, error={"code": -32000, "message": str(error)})
    except Exception as error:  # noqa: BLE001 - a bridge must not fall over
        respond(message_id, error={"code": -32603, "message": repr(error)})


def main():
    """Read one JSON-RPC message per line until stdin closes."""
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            message = json.loads(line)
        except json.JSONDecodeError:
            continue
        dispatch(message)
    return 0


if __name__ == "__main__":
    sys.exit(main())
