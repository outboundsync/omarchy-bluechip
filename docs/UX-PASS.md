# Bluechip UX pass — harness product SoT

Product source of truth for the Omarchy cockpit **as a UNIX harness**, not a QML
app. Agent pipeline: [AGENTS.md](../AGENTS.md). Recipes:
[AGENT-PLAYBOOK.md](AGENT-PLAYBOOK.md). Parked remainder:
[SHIP-BACKLOG.md](SHIP-BACKLOG.md).

TraceFlag is the only org write. MCP stays display-only.

---

## Lens

### Omarchy plugin patterns

Omarchy plugins that feel native are **small, theme-inheriting, and
composable**:

| Pattern | How Bluechip uses it |
| --- | --- |
| Waybar JSON module | `bluechip bar` → `{text, tooltip, class, percentage}` |
| Loud vs quiet chrome | SANDBOX is amber and bold; PROD is quiet; UNKNOWN is muted, never fake-green |
| Hyprland binds | Optional. Chip `on-click` already pins; scratchpad / clipboard binds are documented, not required |
| Special workspace | `bluechip scratchpad` summons logs / an `sf`+`jq` shell without a QML panel |
| mako | Notify on **real transitions only** (`watch`), toast on **clipboard success only** |
| Walker / fuzzel / wofi | `bluechip-switch` for pin; no custom GTK picker |
| Inherit the theme | `waybar/bluechip.css` sets signal color, not a private font stack |

The chip is a **plugin to the bar**, not a dashboard that replaced the bar.

### UNIX harness (not a TUI)

Bluechip is a bag of **verbs** with stdout, `--json`, and pipes. Agents and
humans share the same commands.

```
pin → pulse → probe → pack → agent → confirm write
```

| Stage | Verb | Surface |
| --- | --- | --- |
| **pin** | `orgs` / `pin` | which org is hot; confirm if crossing prod↔sandbox |
| **pulse** | `bar` / `limits` / `watch` | worst signal on the chip; tables in the terminal |
| **probe** | `hygiene` / `fls` / `named-creds` / `offenders` | measured / unknown — never invent |
| **pack** | `incident` / `context` / `clipboard` | one object for a paste or MCP |
| **agent** | MCP display-only + `\| wl-copy` | brain; not write authority |
| **confirm write** | `trace start\|stop` only | sandbox-first; type `PROD` on prod; `--yes` banned on prod / unknown |

Progressive disclosure:

1. **Bar** — identity + worst signal (`SBX · 62%`).
2. **Doctor / desk** — tables a human can screenshot.
3. **`--json` / MCP** — full graph for an agent.

Default path: **install → `sf org login web` → `bluechip doctor` → chip.**
[DEMO.md](DEMO.md) is the 60-second script. Unfinished stubs (`types`) stay off
default help.

Thresholds: `BLUECHIP_WARN_PCT` (75), `BLUECHIP_CRIT_PCT` (90),
`BLUECHIP_CACHE_TTL` (120s), `BLUECHIP_DUPE_CAP` (200).

### What already wins vs a paste into Claude

Frontier agents reason over a paste. They do not sit in the OS:

- **Ambient chrome they cannot spoof.** SANDBOX background you cannot miss;
  UNKNOWN that refuses to look healthy.
- **Pin you confirm.** Crossing prod↔sandbox is a typed gate.
- **Honesty under FLS.** Unknown is a first-class value (H1–H10).
- **Context pack already assembled.** Org id, limits, Flow faults, ApexLog ids.
- **Writes are not implied.** Packs are display-only. TraceFlag is CLI + confirm.
- **Pipes.** `bluechip context | wl-copy` lands in any agent.

---

## Chip

Idle chip text is **identity + worst signal**:

```text
SBX · 62%
PROD · 1 fault
? · ?
```

- **Identity:** `PROD` / `SBX` / `?` from three-valued `sandboxState`.
- **Worst signal:** critical limit % beats warn % beats Flow fault count beats
  a healthy peak %. Unknown limits/faults never render as `0%` green.
- **Classes:** `prod|sandbox|unknown` plus `ok|warn|crit|unknown`. Optional
  `pulse` only after a real `watch` transition. UNKNOWN stays muted.
- **Tooltip hierarchy:** worst signal first; org id / instance / user next;
  click map last (`click: pin · middle: doctor · right: refresh`).
- **`--all`:** chip chrome stays the pin; desk rows (each with their own
  badge) live in the tooltip.

## Verbs

Default `bluechip` / `bluechip help` lists **primary verbs only**. Rarely used
commands and unfinished stubs live behind `bluechip help --all`.

Primary: `doctor`, `bar`, `pin`, `orgs`, `limits`, `flows`, `incident`,
`context`, `watch`, `help`.

Every primary command that returns data supports `--json` (bar is JSON
already).

```bash
bluechip context | wl-copy
bluechip incident --json | jq '.signal'
bluechip hygiene --json | jq '.overall'
```

## Incident

`bluechip incident [--json]` packs **one object**:

- org identity + `sandboxState`
- worst signal (same ranking as the chip)
- headline limits (unknown if Limits missed)
- Flow faults (best-effort; unknown with a reason, or none)
- Named / External Credential **summary** (counts + api names; **never secrets**)
- recent ApexLog metadata (ids/status; no bodies)
- last hygiene probe (from cache; unknown if never run)
- the same markdown pack `context` already emits, plus those summaries

`doctor --json` reuses this builder. MCP `get_incident` calls
`bluechip incident --json`.

## Watch

`bluechip watch [--jsonl] [--once] [secs]` is a delta loop, not a heartbeat:

| Transition | Notify (mako) | Waybar `pulse` |
| --- | --- | --- |
| limit `ok|warn` → `crit`, or `ok` → `warn` | yes | yes |
| new Flow fault (count increased) | yes | yes |
| NC availability `ok` → `unknown` **if** last-good cache exists (cheap; no extra Tooling storm) | yes | yes |
| first observation / heartbeat | no | no |
| unknown ↔ known without a fault/limit cross | no | no |

`--jsonl` writes one JSON object per tick (`event` + current signal). `--once`
is for tests and one-shot probes.

## Scratchpad

`bluechip scratchpad` / `bin/bluechip-scratchpad`: Hyprland **special workspace**
summon. Spawn-if-missing a terminal running `bluechip logs --follow` (or `--sf`
for an `sf`+`jq` shell). No QML. Bind is documented, not installed.

```
bind = SUPER SHIFT, B, exec, bluechip-scratchpad
```

## Clipboard

- `notify-send` **only on success** (`bluechip-clipboard`). Unrecognized
  selection stays quiet.
- Neutralize remote strings on ingest.
- Copy pack: `bluechip incident | wl-copy` or `bluechip clipboard --copy`.

Optional bind:

```
bind = SUPER SHIFT, V, exec, bluechip clipboard --copy
```

## Honest empty states

Never a silent blank where a probe ran. Use **`unknown (reason)`** vs **`none`**:

| Surface | Miss | Empty-but-ok |
| --- | --- | --- |
| bar / limits | `?` + class `unknown` | `0%` only when Limits really returned max>0 used=0 |
| doctor gauges | `unknown (limits API missed — not 0%)` | — |
| doctor Flow faults | `unknown (reason)` | `none` |
| doctor last change | `unknown (reason)` | `none` |
| flows | `unknown (reason)` | `none` (plus the hard-fault caveat) |
| named-creds | `unknown (reason)` | `(none)` |
| desk | `unknown` per cell | `No desk orgs. Pin one…` |
| changes | `unknown (reason)` | `none` |

## Constraints (do not regress)

- H1–H10 and the test suites stay green.
- MCP display-only: no write tools.
- Confirm matrix unchanged. Silent `--yes` banned on prod / unknown.
- No secrets in output; neutralize remote strings.
- `sandboxState` is `prod | sandbox | unknown` everywhere.
- Harris merges.

## Commands

| Command | Role |
| --- | --- |
| `bluechip bar` | Compact identity + worst signal; tooltip hierarchy; `pulse` class |
| `bluechip help` / `help --all` | Primary vs advanced verbs |
| `bluechip incident [--json]` | One incident object |
| `bluechip doctor --json` | Same builder as incident |
| `bluechip watch [--jsonl] [--once]` | Deltas + optional JSONL |
| `bluechip scratchpad` | Special-workspace helper |
| `bluechip changes [--json]` | Honest none / unknown |
| `bluechip-clipboard` | Toast on success only |
