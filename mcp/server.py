#!/usr/bin/env python3
"""Bluechip local MCP server — display-only. Shells out to bin/bluechip.

No metadata deploy, no FLS write, no TraceFlag create. Reuses CLI business rules.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

PROTOCOL = "2024-11-05"
VERSION = "1.1.0"
NAME = "bluechip"

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BIN = ROOT / "bin" / "bluechip"
BLUECHIP = os.environ.get("BLUECHIP_BIN", str(DEFAULT_BIN))

ORG_ALIAS_RE = re.compile(r"^[A-Za-z0-9._-]{1,80}$")


def _bluechip(*args: str, timeout: int = 120) -> tuple[int, str, str]:
    cmd = [BLUECHIP, "--no-color", *args]
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=os.environ.copy(),
        )
    except FileNotFoundError:
        return 127, "", "bluechip executable not found"
    except subprocess.TimeoutExpired:
        return 124, "", "bluechip timed out"
    return proc.returncode, proc.stdout, proc.stderr


def _org_args(arguments: dict) -> list[str]:
    org = arguments.get("org")
    if org is None or org == "":
        return []
    if not isinstance(org, str) or not ORG_ALIAS_RE.match(org):
        raise ValueError("org must be a Salesforce alias (letters, digits, . _ -)")
    return ["-o", org]


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
        "description": "Alias of get_context — paste-ready incident bundle.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "org": {"type": "string"},
                "json": {"type": "boolean"},
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
]


def _text_result(text: str, is_error: bool = False) -> dict:
    return {"content": [{"type": "text", "text": text}], "isError": is_error}


def call_tool(name: str, arguments: dict | None) -> dict:
    arguments = arguments or {}
    try:
        org = _org_args(arguments)
    except ValueError as exc:
        return _text_result(str(exc), is_error=True)

    if name in ("get_context", "get_incident"):
        extra = ["--json"] if arguments.get("json") else []
        code, out, err = _bluechip(*org, *extra, "context")
        text = out if out.strip() else err
        return _text_result(text or "empty context", is_error=code != 0)

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
                    "Do not treat output as write authority. TraceFlag create is CLI-only."
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
        result = call_tool(name, arguments)
        return {"jsonrpc": "2.0", "id": msg_id, "result": result}

    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "error": {"code": -32601, "message": f"method not found: {method}"},
    }


def _read_message() -> dict | None:
    """Newline-delimited JSON, or LSP Content-Length framing."""
    line = sys.stdin.buffer.readline()
    if not line:
        return None
    if line.lower().startswith(b"content-length:"):
        try:
            length = int(line.split(b":", 1)[1].strip())
        except ValueError:
            return None
        while True:
            header = sys.stdin.buffer.readline()
            if header in (b"\r\n", b"\n", b""):
                break
        body = sys.stdin.buffer.read(length)
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
