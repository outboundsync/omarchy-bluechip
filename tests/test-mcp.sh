#!/usr/bin/env python3
"""Local MCP server — display-only tools, wraps bluechip, no write tools."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SERVER = ROOT / "mcp" / "server.py"
STUB = ROOT / "tests" / "hygiene" / "sf"
GEN = ROOT / "tests" / "wave1" / "gen_fixtures.py"

PASS = 0
FAIL = 0


def ok(msg: str) -> None:
    global PASS
    PASS += 1
    print(f"  ✓ {msg}")


def bad(msg: str) -> None:
    global FAIL
    FAIL += 1
    print(f"  ✗ {msg}")


def rpc(proc: subprocess.Popen, payload: dict) -> dict:
    line = json.dumps(payload) + "\n"
    assert proc.stdin is not None
    proc.stdin.write(line)
    proc.stdin.flush()
    assert proc.stdout is not None
    raw = proc.stdout.readline()
    return json.loads(raw)


def main() -> int:
    fix = Path(tempfile.mkdtemp(prefix="bluechip-mcp-"))
    subprocess.check_call(["python3", str(GEN), str(fix)])
    home = Path(tempfile.mkdtemp(prefix="bluechip-mcp-home-"))
    (home / ".config" / "bluechip" / "cache").mkdir(parents=True)
    os.chmod(home / ".config" / "bluechip", 0o700)

    env = os.environ.copy()
    env["HOME"] = str(home)
    env["XDG_CONFIG_HOME"] = str(home / ".config")
    env["BLUECHIP_BIN"] = str(ROOT / "bin" / "bluechip")
    env["BLUECHIP_SF"] = str(STUB)
    env["BLUECHIP_FIXTURE"] = str(fix / "ok")

    proc = subprocess.Popen(
        ["python3", str(SERVER)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
    )
    try:
        init = rpc(
            proc,
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {},
                    "clientInfo": {"name": "test", "version": "0"},
                },
            },
        )
        if init.get("result", {}).get("serverInfo", {}).get("name") == "bluechip":
            ok("initialize serverInfo")
        else:
            bad(f"initialize: {init}")

        listed = rpc(proc, {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}})
        names = {t["name"] for t in listed.get("result", {}).get("tools", [])}
        expected = {
            "get_context",
            "get_incident",
            "get_limits",
            "list_flow_faults",
            "get_hygiene",
            "list_orgs",
            "get_pin",
            "get_named_creds",
            "list_trace_flags",
            "list_apex_logs",
            "get_fls",
            "list_offenders",
            "get_desk",
        }
        if expected <= names:
            ok("tools/list has Wave 1+2 read tools")
        else:
            bad(f"missing tools: {expected - names}")
        writes = {"trace_start", "create_trace", "start_trace", "deploy", "pin_org", "fls_apply", "fls_propose"}
        if names & writes:
            bad(f"write tools exposed: {names & writes}")
        else:
            ok("no write/create tools on MCP")

        pin = rpc(
            proc,
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {"name": "get_pin", "arguments": {}},
            },
        )
        text = pin["result"]["content"][0]["text"]
        blob = json.loads(text)
        if blob.get("ok") is True and blob.get("org", {}).get("sandboxState") == "sandbox":
            ok("get_pin returns sandboxState")
        else:
            bad(f"get_pin: {text[:200]}")

        lim = rpc(
            proc,
            {
                "jsonrpc": "2.0",
                "id": 4,
                "method": "tools/call",
                "params": {"name": "get_limits", "arguments": {}},
            },
        )
        lblob = json.loads(lim["result"]["content"][0]["text"])
        if lblob.get("availability") == "ok" and lblob.get("limits"):
            ok("get_limits JSON")
        else:
            bad(f"get_limits: {lim['result']['content'][0]['text'][:200]}")

        nc = rpc(
            proc,
            {
                "jsonrpc": "2.0",
                "id": 5,
                "method": "tools/call",
                "params": {"name": "get_named_creds", "arguments": {}},
            },
        )
        ntext = nc["result"]["content"][0]["text"]
        if "SUPER_SECRET" in ntext or "NEVER_PRINT" in ntext or "CONSUMER_SECRET" in ntext:
            bad("MCP named creds leaked a secret")
        else:
            ok("MCP named creds has no secrets")
        nblob = json.loads(ntext)
        if nblob.get("namedCredentials", {}).get("records", [{}])[0].get("apiName") == "ZoomInfo_NC":
            ok("MCP named creds apiName")
        else:
            bad(f"named creds payload: {ntext[:200]}")

        ctx = rpc(
            proc,
            {
                "jsonrpc": "2.0",
                "id": 6,
                "method": "tools/call",
                "params": {"name": "get_context", "arguments": {}},
            },
        )
        ctext = ctx["result"]["content"][0]["text"]
        if "**SANDBOX**" in ctext or "SANDBOX" in ctext:
            ok("get_context markdown mentions SANDBOX")
        else:
            bad(f"context missing SANDBOX: {ctext[:180]}")
        if "SHOULD_NEVER_LEAK" in ctext or "CONSUMER_SECRET" in ctext:
            bad("context leaked token/secret")
        else:
            ok("get_context has no access token")
    finally:
        proc.kill()
        proc.wait(timeout=5)

    print(f"\n{PASS} passed, {FAIL} failed")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
