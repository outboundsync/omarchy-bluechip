# Bluechip ship backlog

Harris authorized ship **2026-09-15**. This file is the ranked honesty layer —
what Wave 1 actually delivered vs what is still parked. `bluechip-v1` is the
active build tip. `main` may still hold the parked vision until a later promote;
do not silently rewrite `main`.

MIT cockpit ≠ paid OutboundSync hygiene SKU. The local probe stays read-only
measured / unknown (H1–H10). No letter-grade marketing. No marketplace listing.
No Connected App required while `sf` CLI + Tooling-via-`sf` covers
the calls.

---

## Wave 1 — shipped on this tip

| Item | Status | Notes |
| --- | --- | --- |
| Local Bluechip MCP (read-only) | **shipped** | `mcp/server.py` stdio; tools wrap `bin/bluechip`. No deploy / FLS / TraceFlag create. |
| Named Cred / External Cred inspector | **shipped** | `bluechip named-creds`. Tooling first; Metadata *list* (names only) fallback. Secrets never printed. |
| TraceFlag + log tail | **shipped** | `bluechip trace start\|status\|stop` with confirm matrix. `bluechip logs [--follow]`. MCP lists flags/logs only. |
| Unpark / backlog honesty | **shipped** | This file + `PARKED.md` / README on this branch. |
| H1–H10 + pin/neutralize chrome | **kept** | Last-good / unknown ≠ fail. Three-valued `sandboxState`. |

**Wave 1 MCP tools:** `get_context` / `get_incident`, `get_limits`,
`list_flow_faults`, `get_hygiene`, `list_orgs` / `get_pin`, plus display-only
`get_named_creds`, `list_trace_flags`, `list_apex_logs`. **No FLS write.**

**Wave 1 CLI writes:** TraceFlag create/delete only, confirm-gated
(sandbox-first; type `PROD` on prod; `--yes` banned on prod / unknown).

---

## Wave 2 — this PR

| Item | Status | Notes |
| --- | --- | --- |
| FLS / perm-set matrix | **shipped** | `bluechip fls --user --fields`. Read vs Edit gaps. Unknown on SOQL miss — no invented Setup menus. |
| FLS proposal (not apply) | **shipped** | `bluechip fls-propose`. Diff only. Confirm matrix documented for a future sandbox-first apply. Not an MCP tool. |
| Limit offenders | **shipped** | `bluechip offenders [--since 9am]`. Event Log File when visible. Flex Credits `unknown`. Miss → unknown, never fake 0. LogFile bodies not cached. |
| Multi-org bar / desk | **shipped** | `bluechip desk` + `bluechip bar --all`. Per-org chrome. Pin confirm still gates crossing. |
| Clipboard → context | **shipped** | `bluechip clipboard` / `bluechip-clipboard`. Hyprland bind is documented, not installed. |
| Confirm-gated write kit | **TraceFlag shipped; FLS apply deferred** | TraceFlag remains the only org write. FLS apply would reuse that matrix (sandbox-first; type `PROD`; `--yes` banned on prod / unknown; no MCP write). |

**Wave 2 MCP tools (display-only):** Wave 1 set plus `get_fls`, `list_offenders`, `get_desk`.
No FLS apply, no TraceFlag create.

**Kept:** H1–H10, neutralize, three-valued `sandboxState`, last-good on FLS (not Event Log bodies).

---

## Later

| Item | Why |
| --- | --- |
| Apex-defined type explorer | IN_/OUT_2XX HTTP-callout soup — Tooling Apex Class / Flow retrieve. CLI stub: `bluechip types`. |
| DX drift | Sandbox vs prod vs git when DX is in play. |
| Sharing forensics | “Why can’t X see Y?” — profile / PS / group / OWD. |
| Hosted / Connected App | Mandatory only when we leave local `sf` (marketplace, long-lived plugin without CLI, hosted MCP). |
| QML / marketplace listing | Still parked. CLI + Waybar + MCP is enough to operate on Omarchy. |
| Paid OutboundSync attach API | Out of scope for this MIT repo. Optional deep-link only. |

---

## Post-beachhead gaps (Janus chat)

Captured so they cannot be “forgotten into shipped”:

1. **MCP beyond hosted SObject CRUD** — local Bluechip MCP over the same `sf` session (Wave 1). Hosted Salesforce MCP still cannot see Metadata / custom fields.
2. **Named Credential inspector** — header *flags* and principal names, never Consumer Secret / password / Authorization values (Wave 1).
3. **TraceFlag + log tail** — debug-before-activate without Setup archaeology (Wave 1 CLI; MCP create deferred).
4. **Apex-defined type explorer** — later.
5. **Limit offenders** — Event Log when available (Wave 2). Flex Credits remain unknown (no stable CLI object).
6. **FLS matrix** — probe before inventing fields (Wave 2). Apply deferred; `fls-propose` is the write-kit stand-in.
7. **Write kit** — confirm-gated FLS patch / Flow activate (later). TraceFlag already gated. Proposal matrix is documented.
8. **Multi-org** — bar + pin for more than one org (Wave 2: `desk` / `bar --all`).
9. **Clipboard hand-off** — hotkey → context pack (Wave 2).
10. **Offline last-good everywhere** — snapshot + hygiene + named-creds + FLS persist last-good. Logs/trace/Event Log bodies are **not** cached (PII).

---

## Connected App — when it becomes mandatory

Wave 1 rides `sf org login` + `sf data query --use-tooling-api`. No Connected App
to register.

A Connected App / External Client App becomes mandatory when any of these land:

- a host that cannot see the user’s local `sf` (remote MCP, marketplace package)
- OAuth scopes we cannot inherit from the CLI session
- writes that Salesforce will not allow via the CLI user’s session policy

Until then, do not scaffold one.

---

## What we do not do

- Marketplace listing
- QML this wave
- OpenRouter / Clay sprawl
- Secrets in the repo
- Pretend the MIT hygiene probe is the paid OutboundSync SKU
- Claim Tooling / Metadata retrieve/deploy unless a call exists (see README API table)
