# Bluechip ship backlog

Harris authorized ship **2026-09-15**. This file is the ranked honesty layer —
what Wave 1 actually delivered vs what is still parked. `bluechip-v1` is the
active build tip. `main` may still hold the parked vision until a later promote;
do not silently rewrite `main`.

MIT cockpit ≠ paid OutboundSync hygiene SKU. The local probe stays read-only
measured / unknown (H1–H10). No letter-grade marketing. No marketplace listing.
No Connected App required for Wave 1 while `sf` CLI + Tooling-via-`sf` covers
the calls.

---

## Wave 1 — this PR (highest leverage: SF admin × Omarchy × agents)

| Item | Status | Notes |
| --- | --- | --- |
| Local Bluechip MCP (read-only) | **shipped** | `mcp/server.py` stdio; tools wrap `bin/bluechip`. No deploy / FLS / TraceFlag create. |
| Named Cred / External Cred inspector | **shipped** | `bluechip named-creds`. Tooling first; Metadata *list* (names only) fallback. Secrets never printed. |
| TraceFlag + log tail | **shipped** | `bluechip trace start\|status\|stop` with confirm matrix. `bluechip logs [--follow]`. MCP lists flags/logs only. |
| Unpark / backlog honesty | **shipped** | This file + `PARKED.md` / README on this branch. |
| H1–H10 + pin/neutralize chrome | **kept** | Last-good / unknown ≠ fail. Three-valued `sandboxState`. |

**Wave 1 MCP tools:** `get_context` / `get_incident`, `get_limits`,
`list_flow_faults`, `get_hygiene`, `list_orgs` / `get_pin`, plus display-only
`get_named_creds`, `list_trace_flags`, `list_apex_logs`.

**Wave 1 CLI writes:** TraceFlag create/delete only, confirm-gated
(sandbox-first; type `PROD` on prod; `--yes` banned on prod / unknown).

---

## Wave 2

| Item | Why | Notes |
| --- | --- | --- |
| FLS / perm-set matrix | Integration-user Read+Edit traps (SavvyCal / Harris × Brutus) | Confirm-gated *patch* later; Wave 2 can start read-only. CLI stub: `bluechip fls`. |
| Limit offenders | “What ate API since 9am?” | Event Log / Flex / Digital Wallet when the running user can see them. Unknown when not. CLI stub: `bluechip offenders`. |
| Multi-org bar | More than one hot org | Waybar / pin UX for several aliases without losing chrome. |
| Clipboard → context hotkey | Agent hand-off without typing | Omarchy bind: selection/clipboard → `bluechip context`. CLI stub: `bluechip clipboard`. |
| Confirm-gated write kit | FLS patch / Flow activate | TraceFlag is already gated. Extend the same matrix. **No MCP write tools** until the matrix is in-process (not `--yes` from an agent). |

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
5. **Limit offenders** — Event Log / Flex when available (Wave 2).
6. **FLS matrix** — probe before inventing fields (Wave 2).
7. **Write kit** — confirm-gated FLS patch / Flow activate (Wave 2+). TraceFlag already gated.
8. **Multi-org** — bar + pin for more than one org (Wave 2).
9. **Clipboard hand-off** — hotkey → context pack (Wave 2).
10. **Offline last-good everywhere** — snapshot + hygiene + named-creds persist last-good. Logs/trace bodies are **not** cached (PII). Remaining: propagate last-good chrome to every new probe the same way.

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
