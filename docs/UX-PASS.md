# Bluechip UX pass — harness product SoT

Harris **2026-09-15**. This file is the product source of truth for the
Omarchy cockpit **as a UNIX harness**, not a QML app. Code on `bluechip-v1`
implements this pass in the same PR as the doc. Do not split docs-only vs
code.

MIT cockpit ≠ paid OutboundSync hygiene SKU. TraceFlag remains the only org
write. MCP stays display-only. Quattro / QML marketplace plugin is **later
Fabius polish** — not this PR.

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

Default path (simplify): **install → `sf org login web` → `bluechip doctor` → chip.**

### What already wins vs Claude / Codex

Frontier agents are strong at *reasoning over a paste*. They are weak at
*sitting in the OS*:

- **Ambient chrome they cannot spoof.** A Waybar chip whose SANDBOX background
  you cannot miss, and UNKNOWN that refuses to look healthy.
- **Pin you confirm.** Crossing prod↔sandbox is a typed gate, not a vibe.
- **Honesty under FLS.** Unknown is a first-class value (H1–H10). Hosted
  Salesforce MCP still cannot see Metadata / custom fields; Bluechip says so.
- **Context pack already assembled.** Org id, limits, Flow faults, ApexLog ids
  — without the operator hunting Setup.
- **Writes are not implied.** The pack is display-only. TraceFlag is CLI +
  confirm. MCP has no create/apply tools.
- **Pipes.** `bluechip context | wl-copy` lands in any agent. No Electron
  sidecar.

Keep those. This pass polishes the harness around them; it does not add a
second UI framework.

---

## Ten polish items (this PR)

### 1. One chip, three reads

Idle chip text is **identity + worst signal**, compact:

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

Click → doctor/panel is Waybar `on-click` / `on-click-middle` already. This
pass tightens copy, not the binding.

### 2. Verbs language

Default `bluechip` / `bluechip help` lists **primary verbs only**. Rarely used
commands and unfinished stubs live behind `bluechip help --all`.

Primary: `doctor`, `bar`, `pin`, `orgs`, `limits`, `flows`, `incident`,
`context`, `watch`, `help`.

Every primary command that returns data supports `--json` (bar is JSON
already).

Pipes (print these in help):

```bash
bluechip context | wl-copy
bluechip incident --json | jq '.signal'
bluechip hygiene --json | jq '.overall'
```

### 3. Incident mode

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
`bluechip incident --json` (no longer a silent alias of markdown `context`).

### 4. Watch as entr

`bluechip watch [--jsonl] [--once] [secs]` is a delta loop, not a heartbeat
spam:

| Transition | Notify (mako) | Waybar `pulse` |
| --- | --- | --- |
| limit `ok|warn` → `crit`, or `ok` → `warn` | yes | yes |
| new Flow fault (count increased) | yes | yes |
| NC availability `ok` → `unknown` **if** last-good cache exists (cheap; no extra Tooling storm) | yes | yes |
| first observation / heartbeat | no | no |
| unknown ↔ known without a fault/limit cross | no | no |

`--jsonl` writes one JSON object per tick for agents (`event` + current
signal). `--once` is for tests and one-shot probes.

### 5. Scratchpad helper

`bluechip scratchpad` / `bin/bluechip-scratchpad`: Hyprland **special
workspace** summon. Spawn-if-missing a terminal running `bluechip logs --follow`
(or `--sf` for an `sf`+`jq` shell). No QML. Bind is documented, not installed.

```
bind = SUPER SHIFT, B, exec, bluechip-scratchpad
```

### 6. Clipboard

Wave 2 shipped clipboard → context. Polish:

- `notify-send` **only on success** (`bluechip-clipboard`). Unrecognized
  selection stays quiet.
- Neutralize remote strings on ingest (already required; keep it).
- README / doctor tooltip: **Copy pack** =
  `bluechip incident | wl-copy` or `bluechip clipboard --copy`.

### 7. Honest empty states

Never a silent blank where a probe ran. Use **`unknown (reason)`** vs
**`none`**:

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

### 8. Composer pipeline

Documented here and in README, short:

**`pin → pulse → probe → pack → agent → confirm write`**

Agents do not skip to write. Context / incident packs are not authorization.

### 9. Simplify the default path

1. `./install.sh`
2. `sf org login web`
3. `bluechip doctor`
4. Add the Waybar chip

`docs/DEMO.md` stays a **60-second walkthrough**. README stops duplicating the
script; it links here + DEMO. Unfinished stubs (`types`) stay off default help.

### 10. Extend later (not this PR)

- Apex type explorer (`bluechip types`)
- Connected App / hosted MCP
- Confirm-gated FLS apply / Flow activate
- **Quattro QML panel** (Fabius) — doctor-as-widget, copy-pack button in-panel
- Promote `bluechip-v1` → `main`
- Paid OutboundSync attach API

---

## Constraints (do not regress)

- H1–H10 + Wave 1/2 suites stay green.
- MCP display-only: no new write tools.
- Confirm matrix unchanged. Silent `--yes` banned on prod / unknown.
- No secrets in output; neutralize remote strings.
- `sandboxState` is `prod | sandbox | unknown` everywhere.
- Harris merges.

## Commands added or reshaped

| Command | Change |
| --- | --- |
| `bluechip bar` | Compact identity + worst signal; tooltip hierarchy; `pulse` class |
| `bluechip help` / `help --all` | Primary vs advanced verbs |
| `bluechip incident [--json]` | One incident object |
| `bluechip doctor --json` | Same builder as incident |
| `bluechip watch [--jsonl] [--once]` | Deltas + optional JSONL |
| `bluechip scratchpad` | Special-workspace helper |
| `bluechip changes [--json]` | Honest none / unknown |
| `bluechip-clipboard` | Toast on success only |
