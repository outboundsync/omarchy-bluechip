# Bluechip

![status: wave 2 on bluechip-v1](https://img.shields.io/badge/status-wave%202%20(bluechip--v1)-blue) ![license: MIT](https://img.shields.io/badge/license-MIT-green) ![writes: confirm-gated TraceFlag · FLS proposal-only](https://img.shields.io/badge/writes-confirm--gated%20TraceFlag%20·%20FLS%20proposal--only-lightgrey)

Salesforce **admin cockpit** for [Omarchy](https://omarchy.org). Harris authorized
ship **2026-09-15**. **`bluechip-v1` is the active build tip.** `main` may still
hold parked vision until a later promote — this README is for *this* branch.

MIT cockpit ≠ paid OutboundSync hygiene SKU. Ambient pulse in the status bar:
three-valued **PROD / SANDBOX / UNKNOWN** chrome you can't misread, live API-limit
gauges (unknown when the Limits API misses — not 0% green), a **read-only data
probe**, Flow faults, Named Credential inspector, confirm-gated TraceFlag + log
tail, FLS / perm-set matrix, Event Log offenders, multi-org desk, clipboard
context hand-off, and a local **display-only MCP**. Rides your existing `sf` login —
**no Connected App required, no stored secrets.**

Ranked remainder: [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md).
Harness UX (chip, verbs, incident, watch): [docs/UX-PASS.md](docs/UX-PASS.md).

**ID:** `outboundsync.bluechip` · **Author:** OutboundSync / Harris Kenny · **License:** MIT

![bluechip doctor](assets/doctor-card.svg)

## Three surfaces

| Surface | What you get |
| --- | --- |
| **Cockpit (MIT)** | Pin, chrome, limits, Flow faults, Setup changes, context pack, Named Cred inspector, TraceFlag + logs, FLS matrix, offenders, desk, clipboard, local MCP |
| **Hygiene probe (MIT)** | Per-object measured / unknown cards. Completeness only over fields we actually measured. **No A–F until measured** (H1–H10). All-unknown → no letter, no F, last-good kept |
| **Paid attach** | OutboundSync SKU. Optional deep-link only — **local probe ≠ OutboundSync product**. See [docs/HYGIENE-ATTACH.md](docs/HYGIENE-ATTACH.md) |

Beachhead shipped. Wave 2 adds FLS probe, limit offenders, multi-org desk, and clipboard → context.

## Why

Omarchy's early users are technical, agent-native, allergic to browser tax —
Salesforce admins and platform owners more than seat-only sellers. Bluechip
makes Omarchy a place to *operate* an org: which org is hot, which limit is
bleeding, which Flow faulted, what changed since Friday — ambient, in the OS,
with your agent one paste away. Lightning Setup becomes the occasional deep
link, not the home screen.

## The trust story (why this is safe to run today)

- **Reads are the default.** Hygiene, limits, flows, named-creds, context, FLS,
  offenders, desk, clipboard, and the MCP never write. No deploys, no field edits,
  no Flow activations. `fls-propose` is a diff only.
- **TraceFlag is the only org write** — `bluechip trace start|stop`, confirm-gated
  (sandbox-first; type `PROD` on prod; `--yes` banned on prod / unknown). Not exposed
  as an MCP tool. FLS apply is not in this wave.
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
| `sf data query` | yes (Organization, User, FlowInterview, SetupAuditTrail, ApexLog, PermissionSetAssignment, PermissionSet, ObjectPermissions, FieldPermissions, EventLogFile, hygiene aggregates) |
| `sf sobject describe` | yes (hygiene, **before** multi-field COUNT) |
| `sf data query --use-tooling-api` | **yes** — NamedCredential, ExternalCredential (+ principal / parameter *counts*), TraceFlag, DebugLevel, FlowDefinition. Safe fields only; never `Password` / `ParameterValue` / `Metadata` |
| `sf data create/delete record --use-tooling-api` | **yes** — TraceFlag start/stop (and DebugLevel create if none exists). Confirm-gated CLI only |
| `sf apex get log` | **yes** — log tail bodies (neutralized, size-capped) |
| `sf api request rest` | **best-effort** — EventLogFile `LogFile` body for `offenders`. Unavailable → unknown, not fake 0 |
| `sf org list metadata` | **fallback only** — NamedCredential / ExternalCredential *names*. Not retrieve, not deploy |
| Metadata retrieve / deploy | **not called** |

Context pack is **not write authority**. MCP tools are display-only. Agents must
not deploy from a paste. Silent `--yes` for prod is banned. `fls-propose` does
not apply.

**Connected App:** not required while the operator has a working `sf` session.
Becomes mandatory for hosted/remote MCP, marketplace packaging, or OAuth scopes
the CLI session cannot inherit. See [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md).

## Install

Requires **Omarchy** (or any Hyprland + Waybar setup), plus [`sf`](https://developer.salesforce.com/tools/salesforcecli)
and `jq`. Default path (see [docs/UX-PASS.md](docs/UX-PASS.md)):

**install → `sf org login web` → `bluechip doctor` → chip.**

Composer pipeline: **`pin → pulse → probe → pack → agent → confirm write`**.

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

Then reload: `pkill -SIGUSR2 waybar`. The chip is **identity + worst signal**
(`SBX · 62%`, `PROD · 1 fault`, `? · ?`). PROD stays quiet, **SANDBOX is loud**,
UNKNOWN is muted — never fake-green. Tooltip: worst signal first, then org id,
then the click map. Progressive disclosure: bar = worst signal; `doctor` / `desk`
= tables; `--json` / MCP = full graph.

**Click map** (already in the snippet — copy is the contract):

| Click | Action |
| --- | --- |
| left | pin org (`bluechip-switch`) |
| middle | `bluechip doctor` in `$TERMINAL` |
| right | refresh snapshot + Waybar |

`bluechip bar --all` keeps that single-org chip chrome and puts the rest of the
desk in the tooltip (each row has its own badge).

Copy pack from the shell: `bluechip incident | wl-copy`.

### Clipboard / copy pack (optional)

No desktop binding is required. Copy pack:

```bash
bluechip incident | wl-copy
bluechip clipboard --copy     # if the selection looks like a Flow / interview / org Id
```

Hyprland, if you want a hotkey:

```
bind = SUPER SHIFT, V, exec, bluechip clipboard --copy
bind = SUPER SHIFT, B, exec, bluechip-scratchpad
```

`bluechip-clipboard` toasts **only on success**. Unrecognized selection stays
quiet. The CLI reads `wl-paste` / `xclip` / stdin and **neutralizes** remote
strings on ingest. Pack file: `~/.config/bluechip/cache/clipboard-pack.md` (0600).

### Scratchpad (optional)

`bluechip scratchpad` toggles a Hyprland special workspace and spawns
`bluechip logs --follow` if missing (`--sf` for an `sf`+`jq` shell). No QML.
`--print-bind` / `--dry-run` work without Hyprland.

## Commands

Primary verbs (`bluechip help`). Advanced + stubs: `bluechip help --all`.
Product SoT: [docs/UX-PASS.md](docs/UX-PASS.md). 60-second walkthrough:
[docs/DEMO.md](docs/DEMO.md).

| Command | What it does |
| --- | --- |
| `bluechip doctor [--json]` | Org-vitals card. `--json` is the incident object |
| `bluechip bar` | Waybar JSON: `PROD\|SBX\|?` + worst signal (`62%` / `1 fault` / `?`) |
| `bluechip bar --all` | Same chip chrome for the pin; tooltip lists desk orgs with **per-org** badges |
| `bluechip incident [--json]` | One pack: signal + limits + Flow faults + NC summary + logs + markdown |
| `bluechip limits` | Full limit-utilization table (amber/red; unknown if Limits missed) |
| `bluechip flows` | Errored / paused Flow interviews *(best-effort; none vs unknown)* |
| `bluechip context [--json]` | Markdown incident bundle (display-only; not write authority) |
| `bluechip orgs` / `pin <alias\|->` | List sf-authed orgs; pin one for Bluechip |
| `bluechip watch [--jsonl] [--once]` | mako + Waybar `pulse` on real transitions only |

Pipes: `bluechip context | wl-copy` · `bluechip incident --json | jq .signal` ·
`bluechip hygiene --json | jq .overall`

Global: `-o/--org <alias>` (target one org), `--refresh`, `--json`, `--yes`
(sandbox-only write confirm), `--no-color`.

Tune thresholds with env vars: `BLUECHIP_WARN_PCT` (75), `BLUECHIP_CRIT_PCT` (90),
`BLUECHIP_CACHE_TTL` (120s), `BLUECHIP_DUPE_CAP` (200).

Optional paid-attach deep-link (off by default): `BLUECHIP_REMEDIATE_URL`.
Printed only when set, with “local probe ≠ OutboundSync product.”

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

Tools: `get_context`, `get_incident` (`bluechip incident --json`), `get_limits`,
`list_flow_faults`, `get_hygiene`, `list_orgs` / `get_pin`, `get_named_creds`,
`list_trace_flags`, `list_apex_logs`, `get_fls`, `list_offenders`, `get_desk`.
**No TraceFlag create, no FLS apply, no pin write, no deploy.**

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

**On this branch (Wave 2 + UX pass):** org pin + three-valued chrome · compact
bar (`SBX · 62%`) · hygiene probe · Flow-fault list · Setup change feed ·
**incident pack** · Named Cred inspector · confirm-gated TraceFlag + log tail ·
FLS / perm-set matrix (read) + proposal-only diff · Event Log offenders ·
multi-org desk · clipboard → context (toast on success) · scratchpad ·
`watch` deltas · local display-only MCP · `doctor` card.

**Deferred:** Apex type explorer · FLS/Flow apply · DX drift · sharing forensics ·
Connected App / QML marketplace (Quattro panel = later Fabius) · OutboundSync
attach beyond an optional deep-link. See [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md)
and [VISION.md](VISION.md).

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
./tests/test-named-creds.sh && ./tests/test-trace.sh && python3 ./tests/test-mcp.sh
./tests/test-wave2.sh && ./tests/test-ux-pass.sh
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
