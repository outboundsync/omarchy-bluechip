# Bluechip

![status: wave 1 on bluechip-v1](https://img.shields.io/badge/status-wave%201%20(bluechip--v1)-blue) ![license: MIT](https://img.shields.io/badge/license-MIT-green) ![writes: confirm-gated TraceFlag](https://img.shields.io/badge/writes-confirm--gated%20TraceFlag-lightgrey)

Salesforce **admin cockpit** for [Omarchy](https://omarchy.org). Harris authorized
ship **2026-09-15**. **`bluechip-v1` is the active build tip.** `main` may still
hold parked vision until a later promote — this README is for *this* branch.

MIT cockpit ≠ paid OutboundSync hygiene SKU. Ambient pulse in the status bar:
three-valued **PROD / SANDBOX / UNKNOWN** chrome you can't misread, live API-limit
gauges (unknown when the Limits API misses — not 0% green), a **read-only data
probe**, Flow faults, Named Credential inspector, confirm-gated TraceFlag + log
tail, and a local **display-only MCP**. Rides your existing `sf` login —
**no Connected App required for Wave 1, no stored secrets.**

Ranked remainder: [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md).

**ID:** `outboundsync.bluechip` · **Author:** OutboundSync / Harris Kenny · **License:** MIT

![bluechip doctor](assets/doctor-card.svg)

## Three surfaces

| Surface | What you get |
| --- | --- |
| **Cockpit (MIT)** | Pin, chrome, limits, Flow faults, Setup changes, context pack, Named Cred inspector, TraceFlag + logs, local MCP |
| **Hygiene probe (MIT)** | Per-object measured / unknown cards. Completeness only over fields we actually measured. **No A–F until measured** (H1–H10). All-unknown → no letter, no F, last-good kept |
| **Paid attach** | OutboundSync SKU. Optional deep-link only — **local probe ≠ OutboundSync product**. See [docs/HYGIENE-ATTACH.md](docs/HYGIENE-ATTACH.md) |

Beachhead shipped. Wave 1 adds agent MCP + NC inspector + confirm-gated TraceFlag.

## Why

Omarchy's early users are technical, agent-native, allergic to browser tax —
Salesforce admins and platform owners more than seat-only sellers. Bluechip
makes Omarchy a place to *operate* an org: which org is hot, which limit is
bleeding, which Flow faulted, what changed since Friday — ambient, in the OS,
with your agent one paste away. Lightning Setup becomes the occasional deep
link, not the home screen.

## The trust story (why this is safe to run today)

- **Reads are the default.** Hygiene, limits, flows, named-creds, context, and the MCP
  never write. No deploys, no field edits, no Flow activations.
- **TraceFlag is the only Wave 1 write** — `bluechip trace start|stop`, confirm-gated
  (sandbox-first; type `PROD` on prod; `--yes` banned on prod / unknown). Not exposed
  as an MCP tool.
- **Your login, not a new one.** Everything shells out to the Salesforce CLI (`sf ... --json`)
  under *your* session. No Connected App to register or approve for Wave 1.
- **No secrets stored or printed.** The access token that `sf org display` returns is
  stripped before anything is cached; `context` bundles are scrubbed too. Named Cred
  inspector never shows Consumer Secret, password, Authorization values, or
  `ParameterValue`. State lives in `~/.config/bluechip/` (`chmod 700` on first write;
  cache/credentials `0600`).

## What this branch actually calls

| CLI | Used? |
| --- | --- |
| `sf org display` | yes |
| `sf org list` / `sf org list limits` | yes |
| `sf data query` | yes (Organization, User, FlowInterview, SetupAuditTrail, ApexLog, hygiene aggregates) |
| `sf sobject describe` | yes (hygiene, **before** multi-field COUNT) |
| `sf data query --use-tooling-api` | **yes** — NamedCredential, ExternalCredential (+ principal / parameter *counts*), TraceFlag, DebugLevel. Safe fields only; never `Password` / `ParameterValue` / `Metadata` |
| `sf data create/delete record --use-tooling-api` | **yes** — TraceFlag start/stop (and DebugLevel create if none exists). Confirm-gated CLI only |
| `sf apex get log` | **yes** — log tail bodies (neutralized, size-capped) |
| `sf org list metadata` | **fallback only** — NamedCredential / ExternalCredential *names*. Not retrieve, not deploy |
| Metadata retrieve / deploy | **not called** |

Context pack is **not write authority**. MCP tools are display-only. Agents must
not deploy from a paste. Silent `--yes` for prod is banned.

**Connected App:** not required while the operator has a working `sf` session.
Becomes mandatory for hosted/remote MCP, marketplace packaging, or OAuth scopes
the CLI session cannot inherit. See [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md).

## Install

Requires **Omarchy** (or any Hyprland + Waybar setup), plus [`sf`](https://developer.salesforce.com/tools/salesforcecli)
and `jq`.

```bash
git clone https://github.com/outboundsync/omarchy-bluechip
cd omarchy-bluechip
./install.sh              # links bluechip into ~/.local/bin, checks deps, offers Waybar wiring

npm i -g @salesforce/cli  # if you don't have sf yet
sf org login web          # authorize an org (do this once per org)
bluechip doctor           # screenshot your org
```

### Install the chip (Waybar)

`install.sh` appends the styles for you. To finish, add the module to a `modules-*` array in
`~/.config/waybar/config.jsonc` and paste the object from [`waybar/config.snippet.jsonc`](waybar/config.snippet.jsonc):

```jsonc
"modules-right": ["custom/bluechip", "clock", "battery"],
// ... plus the "custom/bluechip": { ... } object from the snippet
```

Then reload: `pkill -SIGUSR2 waybar`. The chip inherits your Omarchy theme's font; PROD stays
quiet, **SANDBOX is loud**, UNKNOWN is neither.

## Commands

| Command | What it does |
| --- | --- |
| `bluechip doctor` | Shareable org-vitals card — org, chrome, limits, last probe, Flow faults |
| `bluechip bar` | Waybar JSON: org + PROD/SANDBOX/UNKNOWN + peak limit % (or `?%` if limits missed) |
| `bluechip limits` | Full limit-utilization table (amber/red) |
| `bluechip hygiene [--json]` | Read-only data probe — completeness / freshness / duplicates / ownership |
| `bluechip flows` | Errored / paused Flow interviews *(best-effort)* |
| `bluechip changes` | Recent Setup Audit Trail — "what changed since Friday" |
| `bluechip context [--json]` | Paste-ready incident bundle (display-only; not write authority) |
| `bluechip named-creds [--json]` | Named / External Credential inspector (flags + principals; **never secrets**) |
| `bluechip trace start\|status\|stop` | Confirm-gated TraceFlag (CLI only; type `PROD` on prod) |
| `bluechip logs [--follow] [--body]` | Recent ApexLog tail — neutralized, size-capped |
| `bluechip orgs` / `pin <alias\|->` | List sf-authed orgs; pin one for Bluechip |
| `bluechip mcp-config` | Print Cursor / Claude stdio MCP snippet |
| `bluechip watch [secs]` | `mako` notification on amber→red limits / new flow faults |
| `bluechip refresh` | Force-refresh the cached snapshot |

Wave 2 stubs (help only): `fls`, `offenders`, `types`, `clipboard` — see
[docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md).

Global: `-o/--org <alias>` (target one org), `--refresh`, `--json`, `--yes`
(sandbox-only write confirm), `--no-color`.

Tune thresholds with env vars: `BLUECHIP_WARN_PCT` (75), `BLUECHIP_CRIT_PCT` (90),
`BLUECHIP_CACHE_TTL` (120s), `BLUECHIP_DUPE_CAP` (200).

Optional paid-attach deep-link (off by default): `BLUECHIP_REMEDIATE_URL`.
Printed only when set, with “local probe ≠ OutboundSync product.”

See [docs/DEMO.md](docs/DEMO.md) for a 60-second walkthrough.

## Local MCP (display-only)

Zero extra runtime deps (`python3` stdlib). The server shells out to `bin/bluechip`
— it does not fork scoring, SOQL, or confirm rules.

```bash
bluechip mcp-config
```

Cursor / Claude Desktop snippet (`~/.cursor/mcp.json` or equivalent):

```json
{
  "mcpServers": {
    "bluechip": {
      "command": "python3",
      "args": ["/ABS/PATH/omarchy-bluechip/mcp/server.py"],
      "env": {
        "BLUECHIP_BIN": "/ABS/PATH/omarchy-bluechip/bin/bluechip"
      }
    }
  }
}
```

Tools: `get_context` / `get_incident`, `get_limits`, `list_flow_faults`,
`get_hygiene`, `list_orgs` / `get_pin`, `get_named_creds`, `list_trace_flags`,
`list_apex_logs`. **No TraceFlag create, no pin write, no deploy.**

## Hygiene probe

![bluechip hygiene](assets/hygiene-card.svg)

`bluechip hygiene` is a **read-only probe**, not a product grade. It describe-first
(or field-probes) **before** any multi-field `COUNT`, so one FLS-hidden custom
field cannot mark the whole object unavailable. Acceptance tests H1–H10 live in
[`scripts/test-hygiene.sh`](scripts/test-hygiene.sh).

- **Completeness** — fill-rate on **measured** fields only (`ok` \| `unknown` per field).
- **Freshness / ownership / duplicates / pipeline** — each dimension is `ok` or
  `unknown`. A miss is `score: null`, never 100 via `//0`.
- **Empty object** — `total = 0`, not 100 and not F.
- **All objects unknown** — overall `availability: unknown`, **no letter / no F**,
  last-good `~/.config/bluechip/cache/hygiene.json` is kept.
- Duplicate `GROUP BY` is capped (`BLUECHIP_DUPE_CAP`, default 200); `truncated: true`.

Configure objects/fields per org in `~/.config/bluechip/hygiene.json`. Names must
be Salesforce API identifiers (describe or strict regex) — unsanitized fragments
are rejected, not interpolated into SOQL.

`--json` matches the review shape (`availability`, `overall`, `objects[].dimensions`,
`error.code` ∈ `sf_missing` \| `unauthenticated` \| `transient` \| `config_invalid`).

## The agent context pack

`bluechip context` assembles org id, edition, instance, live limits, open Flow
faults, and recent `ApexLog` ids into one Markdown paste. It is a **hand-off**,
not authorization to write. `sandboxState` is `PROD` / `SANDBOX` / `UNKNOWN` —
never PROD when the Organization row is unreadable.

## Scope

**On this branch (Wave 1):** org pin + three-valued chrome · API/limit pulse ·
hygiene probe · Flow-fault list · Setup change feed · agent context pack ·
Named Cred inspector · confirm-gated TraceFlag + log tail · local display-only
MCP · `doctor` card · optional `watch`.

**Deferred:** FLS matrix · limit offenders · multi-org bar · clipboard hotkey ·
FLS/Flow writes · Apex type explorer · DX drift · sharing forensics ·
Connected App / QML / marketplace · OutboundSync attach beyond an optional
deep-link. See [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md) and [VISION.md](VISION.md).

## Limitations (honest)

- **Flow faults are best-effort.** Hard faults roll back and may not persist as
  `FlowInterview` rows; Bluechip surfaces errored/paused interviews and points at
  `changes` for the rest.
- **You need what `sf` can see.** Some reads want *View Setup and Configuration* /
  *View Event Log Files*. Denied probes are `unknown` with a reason enum — they
  are not scored as 0 or 100.
- **Hygiene is exact-match and heuristic.** Duplicates are exact `GROUP BY`
  matches, not fuzzy; fill-rate ≠ correctness. Describe-first, then a small set
  of aggregate queries, all read-only.
- Targets **bash 5 / Linux** (Omarchy). Develops fine on macOS with the Salesforce CLI installed.

## Tests

```bash
./scripts/test-hygiene.sh   # H1–H10 + limits-unknown + context UNKNOWN + SOQL allowlist + 700/600
./tests/test-plaintext.sh && ./tests/test-pin.sh
./tests/test-named-creds.sh && ./tests/test-trace.sh && ./tests/test-mcp.sh
```

No live org required (stub `sf` JSON fixtures).

## Unofficial disclaimer

**Bluechip is unofficial.** It is **not** affiliated with, endorsed by, or sponsored by
Salesforce, Inc. or any related entity. This project does **not** ship Salesforce logos,
wordmarks, or brand assets. "Salesforce" and related marks belong to their owners. Referential use
only: works with Salesforce®.

Named **Bluechip** — bar chip, blue-chip orgs, a nod to Salesforce blue — without those marks in
the title.

## License

MIT — [LICENSE](./LICENSE)
