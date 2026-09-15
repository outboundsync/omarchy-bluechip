# Bluechip ship backlog

Honesty layer for **`main`** (active tip). `bluechip-v1` is a historical branch.

Agent contract: [AGENTS.md](../AGENTS.md). Recipes: [AGENT-PLAYBOOK.md](AGENT-PLAYBOOK.md).
Confirm matrix: [VISION.md](../VISION.md#confirm-before-write-matrix). Salesforce
I/O actually called: [VISION.md](../VISION.md#api-honesty).

---

## Shipped

| Item | Notes |
| --- | --- |
| Display-only MCP | `mcp/server.py` wraps `bin/bluechip`. No deploy / FLS apply / TraceFlag create. |
| Named Cred inspector | `bluechip named-creds`. Secrets never printed. |
| TraceFlag + log tail | Only org write. Confirm-gated CLI (`--yes` banned on prod / unknown). MCP lists flags/logs only. |
| FLS / perm-set matrix | `bluechip fls --user --fields`. Read vs Edit gaps. Unknown on SOQL miss. |
| FLS proposal | `bluechip fls-propose`. Diff only. Apply not shipped. Not an MCP tool. |
| Limit offenders | `bluechip offenders [--since 9am]`. Event Log when visible. Flex Credits `unknown`. Miss → unknown, never fake 0. |
| Multi-org desk | `bluechip desk` + `bar --all`. Per-org chrome. Pin confirm still gates crossing. |
| Clipboard → context | `bluechip clipboard`. Hyprland bind documented, not installed. |
| Compact chip / incident / watch / scratchpad | [UX-PASS.md](UX-PASS.md) |
| H1–H10, neutralize, 0700/0600, I/O caps | [CLEANUP-2026-09-15.md](CLEANUP-2026-09-15.md) |
| Callout auth doctor | `bluechip callout-auth` / `named-creds --doctor`. 401 classes: `headers_absent`, `formulas_off`, `gen_auth_on_custom`, `principal_missing`, `principal_unknown`, `unknown`. Secrets never printed. |
| Apex-defined type explorer | `bluechip types` — IN_/OUT_/2XX / ExternalService via Tooling. Property paths for Flow Assignment. |
| Flow callout pack | `bluechip callout-pack` / `incident --callout`. Hand-off, not write authority. |
| DWU / integration preflight | `bluechip preflight --user` (+ optional `--fields`). FLS + NC principal. Does not Activate. |
| Boundary desk | `bluechip scratchpad --boundary` prints `logs --follow` + `wrangler tail`. No QML. |

**MCP tools (display-only):** `get_context`, `get_incident`, `get_limits`,
`list_flow_faults`, `get_hygiene`, `list_orgs` / `get_pin`, `get_named_creds`,
`diagnose_callout_auth`, `get_apex_types`, `get_callout_pack`, `run_preflight`,
`list_trace_flags`, `list_apex_logs`, `get_fls`, `list_offenders`, `get_desk`.

**Kept everywhere:** three-valued `sandboxState`, unknown ≠ 0/100, last-good on
hygiene (not Event Log bodies).

---

## Parked (not promised)

Work that is **not** on the tip.

| Item | Why it is parked |
| --- | --- |
| DX drift | Sandbox vs prod vs git when DX is in play. |
| Sharing forensics | “Why can’t X see Y?” |
| Hosted / Connected App | Only if we leave local `sf` (remote MCP, marketplace, scopes the CLI session cannot inherit). |
| QML / marketplace listing | CLI + Waybar + MCP is enough to operate on Omarchy. Quattro panel = Fabius. |
| Paid OutboundSync attach API | Out of scope for this MIT repo. Optional deep-link only. |
| FLS apply / Flow activate | Confirm matrix is documented. TraceFlag is the write that exists. |

---

## Connected App

The tip rides `sf org login` + Tooling-via-`sf`. No Connected App to register.

One becomes mandatory when any of these land: a host that cannot see the user’s
local `sf`; OAuth scopes the CLI session cannot inherit; writes Salesforce will
not allow via the CLI user’s session. Until then, do not scaffold one.

---

## What we do not do

- Marketplace listing
- QML on this tip
- OpenRouter / Clay sprawl
- Secrets in the repo
- Pretend the MIT hygiene probe is the paid OutboundSync SKU
- Claim Tooling / Metadata retrieve/deploy unless a call exists ([VISION.md](../VISION.md#api-honesty))
