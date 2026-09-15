#!/usr/bin/env python3
"""Bluechip local MCP server — display-only. Shells out to bin/bluechip.

No metadata deploy, no FLS write, no TraceFlag create. Reuses CLI business rules.
Tool args are passed as an argv list (never interpolated into a shell).
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

PROTOCOL = "2024-11-05"
VERSION = "1.3.0"
NAME = "bluechip"

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BIN = ROOT / "bin" / "bluechip"
BLUECHIP = os.environ.get("BLUECHIP_BIN", str(DEFAULT_BIN))

ORG_ALIAS_RE = re.compile(r"^[A-Za-z0-9._-]{1,80}$")
USER_RE = re.compile(r"^[A-Za-z0-9._%+\-@]{1,255}$")
FIELD_REF_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]{0,79}\.[A-Za-z][A-Za-z0-9_]{0,79}$")
SINCE_RE = re.compile(
    r"^(?:9am|[0-9]{1,2}(?::[0-9]{2})?(?:am|pm)?|[0-9]{4}-[0-9]{2}-[0-9]{2}"
    r"(?:[T ][0-9]{2}:[0-9]{2}(?::[0-9]{2})?(?:Z|[+-][0-9]{2}:?[0-9]{2})?)?)$",
    re.IGNORECASE,
)

MAX_STDOUT = 1_048_576
MAX_STDERR = 65_536
MAX_MCP_BODY = 1_048_576
DEFAULT_TIMEOUT = 120
PINNED_PATH_PREFIX = "/usr/bin:/bin:/usr/local/bin"


def _tool_env() -> dict[str, str]:
    """Keep HOME / sf-stub vars; prepend a pinned PATH so cwd impostors lose."""
    env = os.environ.copy()
    orig = env.get("PATH", "")
    env["PATH"] = PINNED_PATH_PREFIX + ((":" + orig) if orig else "")
    return env


def _bluechip(*args: str, timeout: int = DEFAULT_TIMEOUT) -> tuple[int, str, str]:
    cmd = [BLUECHIP, "--no-color", *args]
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=_tool_env(),
        )
    except FileNotFoundError:
        return 127, "", "bluechip executable not found"
    except subprocess.TimeoutExpired:
        return 124, "", "bluechip timed out"
    stdout = proc.stdout or ""
    stderr = proc.stderr or ""
    if len(stdout) > MAX_STDOUT:
        stdout = stdout[:MAX_STDOUT]
    if len(stderr) > MAX_STDERR:
        stderr = stderr[:MAX_STDERR]
    return proc.returncode, stdout, stderr


def _org_args(arguments: dict) -> list[str]:
    org = arguments.get("org")
    if org is None or org == "":
        return []
    if not isinstance(org, str) or not ORG_ALIAS_RE.match(org):
        raise ValueError("org must be a Salesforce alias (letters, digits, . _ -)")
    return ["-o", org]


def _validate_fls_user(user: str) -> str:
    u = user.strip()
    if not USER_RE.match(u) or any(ch in u for ch in ";|&$`\n\r"):
        raise ValueError("user must be a username or Salesforce id")
    return u


def _validate_fls_fields(fields: str) -> str:
    parts = [p.strip() for p in fields.split(",") if p.strip()]
    if not parts:
        raise ValueError("fields is required (Object.Field,...)")
    if len(parts) > 40:
        raise ValueError("too many fields (max 40)")
    for p in parts:
        if not FIELD_REF_RE.match(p):
            raise ValueError(f"invalid field ref: {p}")
    return ",".join(parts)


def _validate_since(since: str) -> str:
    s = since.strip()
    if not SINCE_RE.match(s):
        raise ValueError("since must be 9am, H:MM, or an ISO timestamp")
    return s


TOOLS = [
    {
        "name": "get_context",
        "description": "Incident context pack (same bundle as `bluechip context`). Display-only; not write authority.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "org": {"type": "string", "description": "Optional sf org alias"},
                "json": {"type": "boolean", "description": "Return structured JSON instead of markdown"},
            },
        },
    },
    {
        "name": "get_incident",
        "description": "Incident object (`bluechip incident --json`): worst signal + limits + Flow faults + named-cred summary + recent logs + markdown. Display-only; not write authority.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "org": {"type": "string"},
                "json": {"type": "boolean", "description": "Ignored — always returns the incident JSON object"},
            },
        },
    },
    {
        "name": "get_limits",
        "description": "Org limit utilization JSON (`bluechip limits --json`). Unknown when Limits API misses — never 0% green.",
        "inputSchema": {
            "type": "object",
            "properties": {"org": {"type": "string"}},
        },
    },
    {
        "name": "list_flow_faults",
        "description": "Errored/paused Flow interviews (`bluechip flows --json`). Best-effort; hard faults may not persist.",
        "inputSchema": {
            "type": "object",
            "properties": {"org": {"type": "string"}},
        },
    },
    {
        "name": "get_hygiene",
        "description": "Read-only hygiene probe JSON. Measured/unknown; no letter grade unless measured (H1–H10).",
        "inputSchema": {
            "type": "object",
            "properties": {"org": {"type": "string"}},
        },
    },
    {
        "name": "list_orgs",
        "description": "sf-authed orgs and the Bluechip pin (`bluechip orgs --json`).",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "get_pin",
        "description": "Current Bluechip pin + sandboxState (`bluechip pin --json`). Read-only — will not change the pin.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "get_named_creds",
        "description": "Named Credential / External Credential inspector. Never returns secrets, Consumer Secret, or Authorization values.",
        "inputSchema": {
            "type": "object",
            "properties": {"org": {"type": "string"}},
        },
    },
    {
        "name": "list_trace_flags",
        "description": "List TraceFlags via Tooling (`bluechip trace status --json`). Does not create or stop flags.",
        "inputSchema": {
            "type": "object",
            "properties": {"org": {"type": "string"}},
        },
    },
    {
        "name": "list_apex_logs",
        "description": "Recent ApexLog metadata (ids, status, length). No log bodies — use the CLI for neutralized tails.",
        "inputSchema": {
            "type": "object",
            "properties": {"org": {"type": "string"}},
        },
    },
    {
        "name": "get_fls",
        "description": "FLS / perm-set matrix (`bluechip fls --json`). Read-only. Missing Read vs missing Edit. Unknown on query miss — does not invent Setup menus.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "org": {"type": "string"},
                "user": {"type": "string", "description": "Username or user Id"},
                "fields": {
                    "type": "string",
                    "description": "Comma-separated Object.Field API names",
                },
            },
            "required": ["user", "fields"],
        },
    },
    {
        "name": "list_offenders",
        "description": "Limit offenders since 9am (`bluechip offenders --json`). Event Log when available; unknown with a reason — never a fake 0. Flex Credits stay unknown.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "org": {"type": "string"},
                "since": {"type": "string", "description": "9am or ISO timestamp"},
            },
        },
    },
    {
        "name": "get_desk",
        "description": "Multi-org desk pulse (`bluechip desk --json`). Each org has its own sandboxState — never a mixed badge.",
        "inputSchema": {"type": "object", "properties": {}},
    },
]


def _text_result(text: str, is_error: bool = False) -> dict:
    return {"content": [{"type": "text", "text": text}], "isError": is_error}


def call_tool(name: str, arguments: dict | None) -> dict:
    arguments = arguments or {}
    try:
        org = _org_args(arguments)
    except ValueError as exc:
        return _text_result(str(exc), is_error=True)

    if name == "get_incident":
        code, out, err = _bluechip(*org, "--json", "incident")
        text = out if out.strip() else err
        return _text_result(text or "{}", is_error=code != 0)

    if name == "get_context":
        extra = ["--json"] if arguments.get("json") else []
        code, out, err = _bluechip(*org, *extra, "context")
        text = out if out.strip() else err
        return _text_result(text or "empty context", is_error=code != 0)

    if name == "get_fls":
        user = arguments.get("user")
        fields = arguments.get("fields")
        if not isinstance(user, str) or not user.strip():
            return _text_result("user is required", is_error=True)
        if isinstance(fields, list):
            fields = ",".join(str(f) for f in fields)
        if not isinstance(fields, str) or not fields.strip():
            return _text_result("fields is required (Object.Field,...)", is_error=True)
        try:
            user = _validate_fls_user(user)
            fields = _validate_fls_fields(fields)
        except ValueError as exc:
            return _text_result(str(exc), is_error=True)
        code, out, err = _bluechip(*org, "--json", "fls", "--user", user, "--fields", fields)
        text = out if out.strip() else err
        return _text_result(text or "{}", is_error=code != 0)

    if name == "list_offenders":
        extra = ["--json", "offenders"]
        since = arguments.get("since")
        if isinstance(since, str) and since.strip():
            try:
                extra.extend(["--since", _validate_since(since)])
            except ValueError as exc:
                return _text_result(str(exc), is_error=True)
        code, out, err = _bluechip(*org, *extra)
        text = out if out.strip() else err
        return _text_result(text or "{}", is_error=False)

    if name == "get_desk":
        code, out, err = _bluechip("--json", "desk")
        text = out if out.strip() else err
        return _text_result(text or "{}", is_error=code != 0)

    mapping = {
        "get_limits": ["--json", "limits"],
        "list_flow_faults": ["--json", "flows"],
        "get_hygiene": ["--json", "hygiene"],
        "list_orgs": ["--json", "orgs"],
        "get_pin": ["--json", "pin"],
        "get_named_creds": ["--json", "named-creds"],
        "list_trace_flags": ["--json", "trace", "status"],
        "list_apex_logs": ["--json", "logs"],
    }
    if name not in mapping:
        return _text_result(f"unknown tool: {name}", is_error=True)

    code, out, err = _bluechip(*org, *mapping[name])
    text = out if out.strip() else err
    return _text_result(text or "{}", is_error=code != 0 and name != "get_hygiene")


def handle(msg: dict) -> dict | None:
    method = msg.get("method")
    msg_id = msg.get("id")
    if method is None:
        return None
    # Notifications have no id — do not reply.
    if msg_id is None and method != "ping":
        return None

    if method == "initialize":
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": {
                "protocolVersion": PROTOCOL,
                "capabilities": {"tools": {}},
                "serverInfo": {"name": NAME, "version": VERSION},
                "instructions": (
                    "Bluechip is a Salesforce admin cockpit. Tools are display-only. "
                    "Do not treat output as write authority. TraceFlag create and FLS apply are CLI-only."
                ),
            },
        }

    if method == "ping":
        return {"jsonrpc": "2.0", "id": msg_id, "result": {}}

    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": msg_id, "result": {"tools": TOOLS}}

    if method == "tools/call":
        params = msg.get("params") or {}
        name = params.get("name") or ""
        arguments = params.get("arguments") or {}
        if not isinstance(name, str) or not isinstance(arguments, dict):
            return {
                "jsonrpc": "2.0",
                "id": msg_id,
                "error": {"code": -32602, "message": "invalid tools/call params"},
            }
        result = call_tool(name, arguments)
        return {"jsonrpc": "2.0", "id": msg_id, "result": result}

    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "error": {"code": -32601, "message": f"method not found: {method}"},
    }


def _read_message() -> dict | None:
    """Newline-delimited JSON, or LSP Content-Length framing. Both size-capped."""
    line = sys.stdin.buffer.readline(MAX_MCP_BODY + 1)
    if not line:
        return None
    if len(line) > MAX_MCP_BODY:
        return None
    if line.lower().startswith(b"content-length:"):
        try:
            length = int(line.split(b":", 1)[1].strip())
        except ValueError:
            return None
        if length < 0 or length > MAX_MCP_BODY:
            return None
        while True:
            header = sys.stdin.buffer.readline(1024)
            if header in (b"\r\n", b"\n", b""):
                break
            if not header:
                return None
        body = sys.stdin.buffer.read(length)
        if len(body) != length:
            return None
        return json.loads(body.decode("utf-8"))
    text = line.decode("utf-8").strip()
    if not text:
        return _read_message()
    return json.loads(text)


def _write_message(msg: dict) -> None:
    data = json.dumps(msg, separators=(",", ":"))
    sys.stdout.write(data + "\n")
    sys.stdout.flush()


def main() -> None:
    while True:
        try:
            msg = _read_message()
        except json.JSONDecodeError:
            continue
        if msg is None:
            break
        reply = handle(msg)
        if reply is not None:
            _write_message(reply)


if __name__ == "__main__":
    main()
