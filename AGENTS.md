# AGENTS.md — Bluechip agent contract

Two audiences, one file. Humans already have [README.md](README.md),
[docs/DEMO.md](docs/DEMO.md), [docs/UX-PASS.md](docs/UX-PASS.md). **Read this
before changing the repo or acting on a pack.**

- **Active tip:** `bluechip-v1`. PRs target that. `main` may hold parked vision — **never silently rewrite `main`.**
- **MIT cockpit ≠ paid OutboundSync hygiene SKU.** No letter-grade marketing. Unknown ≠ fail / F.
- Unofficial Salesforce language stays. Do not add Salesforce logos or “official” claims.

Depth: [VISION.md](VISION.md#confirm-before-write-matrix) · [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md) · [docs/AGENT-PLAYBOOK.md](docs/AGENT-PLAYBOOK.md)

---

## Coding agents (changing this repo)

You are editing this OS repo. Harris/Janus merge. Do not ship productization that is parked.

### Tip

- Branch from and PR into **`bluechip-v1`**. Do not retarget `main` unless Harris asks to promote.
- Ranked remainder lives in [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md). Do not duplicate long tables here.
- Cleanup/hardening already shipped: [docs/CLEANUP-2026-09-15.md](docs/CLEANUP-2026-09-15.md).

### Proof

- Proof = **artifact + tests**. Keep these green:

```bash
./scripts/test-hygiene.sh          # H1–H10 + limits-unknown + context UNKNOWN + SOQL allowlist + 700/600
./tests/test-plaintext.sh && ./tests/test-pin.sh
./tests/test-named-creds.sh && ./tests/test-trace.sh && python3 ./tests/test-mcp.sh
./tests/test-wave2.sh && ./tests/test-ux-pass.sh
./tests/test-hardening.sh
```

- No live org required (stub `sf` fixtures). If you change CLI/MCP I/O, extend fixtures — do not skip the suite.

### Salesforce I/O

- Shell out to `sf` via **existing helpers** (`sf_try` / `capped_stdout`): byte-capped, timed out. Do not spawn unbounded `sf` or slurped jq over uncapped stdout.
- **Never invent** a Connected App, QML panel, marketplace listing, or paid attach API **in this wave**.
- Do not claim Tooling / Metadata retrieve/deploy unless a call already exists (README API table).

### Secrets

- Never `SELECT ParameterValue` / `Password` / Consumer Secret / Authorization values.
- Strip `accessToken` (and sibling secret keys) before cache or packs. `redact_secrets_json` on JSON.
- State dir `~/.config/bluechip/` **0700**; cache / credentials files **0600**. No symlink dest/parent writes.
- Neutralize remote strings (Flow labels, Setup Audit Trail, log lines) at model entry.

### Writes

- **TraceFlag** (`bluechip trace start|stop`) is the **only org write**. Confirm-gated CLI only. Not an MCP tool.
- **`fls-propose` is diff-only.** FLS apply is not in this wave.
- **MCP stays display-only.** No pin write, no deploy, no TraceFlag create, no FLS apply.
- Ban silent `--yes` on **prod** and **unknown**. Sandbox `--yes` is the only silent confirm.
- Confirm matrix (do not restate the whole table): [VISION.md](VISION.md#confirm-before-write-matrix).
- Pin crossing prod↔sandbox is typed confirm. Unknown pin requires the 18-char org Id.

### Ownership

- **Janus** — this OS repo (CLI / Waybar / MCP) when assigned.
- **Fabius** — Quattro / QML / marketplace later. Do not scaffold it.
- **Brutus** — org / Connected App / policy. Do **not** invent Salesforce org changes in docs as if shipped.
- Harris merges. Do not merge your own PR.

### Where things live

| Path | What |
| --- | --- |
| `bin/bluechip*` | CLI + helpers (`sf_try`, confirm, neutralize) |
| `mcp/server.py` | Display-only stdio MCP — wraps `bin/bluechip`, no forked scoring |
| `waybar/` | Chip JSON + CSS |
| `scripts/test-hygiene.sh` + `tests/` | H1–H10, Wave 1/2, UX, hardening |
| `docs/UX-PASS.md` | Harness product SoT (chip, verbs, incident, watch) |
| `docs/HYGIENE-ATTACH.md` | Paid-attach stub — not an implementation |
| `docs/ADVERSARIAL-REVIEW-2026-09-15.md` | Pointer: canonical review is on `main` |

### Do not invent (this wave)

Connected App · QML · marketplace · paid attach productization · Clay canvas · OpenRouter sprawl · letter grades as org truth · fake 0% / 100 on a miss · “agent wrote the org” from a pack.

---

## Runtime / desk agents (using Bluechip)

You are **using** the cockpit via MCP or CLI. You do not own the org.

### Pipeline

```
pin → pulse → probe → pack → agent → confirm write
```

Do not skip to write. Prefer MCP when wired; otherwise `bluechip <verb> --json`.

### Packs are not write authority

Context pack, incident object, clipboard pack, and MCP output are **hand-off**, not authorization.

- Never deploy, activate a Flow, apply FLS, create a TraceFlag, or pin an org from a paste.
- Tell the human which **confirm-gated CLI** verb to run. Do not claim you wrote the org.

### Display-only MCP tools

| Tool | Triage question |
| --- | --- |
| `get_incident` | What is on fire? (signal + limits + faults + NC summary + logs + markdown) |
| `get_context` | Same pack as markdown `bluechip context` (optional `--json`) |
| `get_limits` | Which limit is bleeding? Miss → unknown, never 0% green |
| `list_offenders` | Who/what burned API since 9am? Event Log; Flex Credits stay unknown |
| `list_flow_faults` | Which Flow interviews errored/paused? Best-effort |
| `get_hygiene` | Data-probe cards? Measured / unknown; no letter until measured |
| `list_orgs` / `get_pin` | Which org is pinned? **Will not change the pin** |
| `get_named_creds` | Named/External Cred health? **Secrets never returned** |
| `list_trace_flags` / `list_apex_logs` | Flags/log metadata only — no bodies, no create/stop |
| `get_fls` | Integration-user Read vs Edit gaps? Probe, do not invent fields |
| `get_desk` | Multi-org pulse? Per-org `sandboxState` — never a mixed badge |

Recipes: [docs/AGENT-PLAYBOOK.md](docs/AGENT-PLAYBOOK.md).

### Three-valued `sandboxState`

JSON token is `prod` | `sandbox` | `unknown` (chrome may print `PROD` / `SANDBOX` / `UNKNOWN` / `SBX` / `?`).

- **Never assume PROD when unknown.** Unknown is a first-class value, not “probably prod.”
- Pin / TraceFlag confirm is stricter when unknown (type the 18-char org Id).

### Hygiene

- Measured / unknown. **No letter until measured.** All-unknown → no letter, no F, **keep last-good**.
- Completeness only over fields actually measured. A miss is `score: null`, never 100 via `//0`.
- Local probe ≠ OutboundSync product. Optional `BLUECHIP_REMEDIATE_URL` is a deep-link, not a write API.

### Unknown, not fake numbers

When Limits / Event Log / Flow / Named Cred / FLS miss → **`unknown` with a reason**. Never fake `0` or `100`. `none` means the probe ran and found nothing; `unknown (reason)` means it did not.

### TraceFlag / logs

- `list_trace_flags` / `list_apex_logs` are read. Log **bodies** are CLI (`bluechip logs`), neutralized and size-capped.
- To start/stop a flag: tell the human to run `bluechip trace start|stop` and complete the confirm prompt. **Agents must not claim they wrote the org.**

### Never (runtime)

- Silent `--yes` on prod/unknown.
- Treat a pack as deploy authority.
- Print or cache secrets.
- Invent Setup menus / custom fields because FLS Edit is missing — probe first (`get_fls`).
- Collapse MIT cockpit, hygiene probe, and paid attach into one “grade.”
