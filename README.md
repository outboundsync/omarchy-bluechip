# Bluechip

![status: v1](https://img.shields.io/badge/status-v1-blue) ![license: MIT](https://img.shields.io/badge/license-MIT-green) ![read-only](https://img.shields.io/badge/writes-none%20(read--only)-lightgrey)

Salesforce **admin cockpit** for [Omarchy](https://omarchy.org) — your org's pulse in the status
bar: PROD/SANDBOX chrome you can't misread, live API-limit gauges, Flow faults, Setup-change feed,
and a one-command **agent context pack**. Read-only. Rides your existing `sf` login — **no
Connected App, no stored secrets.**

**ID:** `outboundsync.bluechip` · **Author:** OutboundSync / Harris Kenny · **License:** MIT

![bluechip doctor](assets/doctor-card.svg)

## Why

Omarchy's early users are technical, agent-native, allergic to browser tax — Salesforce admins and
platform owners more than seat-only sellers. Bluechip makes Omarchy a great place to *operate* an
org: which org is hot, which limit is bleeding, which Flow faulted, what changed since Friday —
ambient, in the OS, with your agent one paste away. Lightning Setup becomes the occasional deep
link, not the home screen.

## The trust story (why this is safe to run today)

- **Read-only.** v1 never writes to your org. No deploys, no field edits, no activations.
- **Your login, not a new one.** Everything shells out to the Salesforce CLI (`sf ... --json`) —
  REST + Tooling + Limits APIs, under *your* session. No Connected App to register or approve.
- **No secrets stored.** The access token that `sf org display` returns is stripped before
  anything is cached; `context` bundles are scrubbed too. State lives in `~/.config/bluechip/`
  (`0600`).

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
quiet, **SANDBOX is loud**, and the peak limit drives amber → red.

## Commands

| Command | What it does |
| --- | --- |
| `bluechip doctor` | Shareable org-vitals card — *"fastfetch for your org"* |
| `bluechip bar` | Waybar JSON: org + PROD/SANDBOX + peak limit % |
| `bluechip limits` | Full limit-utilization table (amber/red) |
| `bluechip flows` | Errored / paused / waiting Flow interviews *(best-effort)* |
| `bluechip changes` | Recent Setup Audit Trail — "what changed since Friday" |
| `bluechip context` | Paste-ready incident bundle for Claude / Cursor / any agent |
| `bluechip orgs` / `pin <alias\|->` | List sf-authed orgs; pin one for Bluechip |
| `bluechip watch [secs]` | `mako` notification on amber→red limits / new flow faults |
| `bluechip refresh` | Force-refresh the cached snapshot |

Global: `-o/--org <alias>` (target one org), `--refresh`, `--no-color`.

Tune thresholds with env vars: `BLUECHIP_WARN_PCT` (75), `BLUECHIP_CRIT_PCT` (90),
`BLUECHIP_CACHE_TTL` (120s).

See [docs/DEMO.md](docs/DEMO.md) for the 60-second demo and launch copy.

## The agent context pack

`bluechip context` is the piece frontier agents can't do for themselves: it assembles org id,
edition, instance, live limits, open Flow faults, and recent `ApexLog` ids into one Markdown paste
— the missing hand-off between your org and your agent. This is the "context pack" pillar from the
[vision](VISION.md), shipped without a Connected App or a local MCP server.

## Scope

**In v1 (read-only):** org pin + PROD/SANDBOX chrome · API/limit pulse · Flow-fault list · Setup
change feed · agent context pack · `doctor` card · optional `watch` notifications.

**Deferred to v2:** Connected App / External Client App · a local Bluechip MCP over
Tooling/Metadata · Metadata deploys · **confirm-gated writes** (trace flag, FLS fix, flow
activate) · element-level Flow replay console · OutboundSync data-hygiene attach. See
[VISION.md](VISION.md).

## Limitations (honest)

- **Flow faults are best-effort.** Hard faults roll back and may not persist as `FlowInterview`
  rows; Bluechip surfaces errored/paused/waiting interviews and points at `changes` for the rest.
- **You need what `sf` can see.** Some reads want *View Setup and Configuration* / *View Event Log
  Files*. If a query is denied, Bluechip says so rather than guessing.
- Targets **bash 5 / Linux** (Omarchy). Develops fine on macOS with the Salesforce CLI installed.

## Unofficial disclaimer

**Bluechip is unofficial.** It is **not** affiliated with, endorsed by, or sponsored by
Salesforce, Inc. or any related entity. This project does **not** ship Salesforce logos,
wordmarks, or brand assets. "Salesforce" and related marks belong to their owners. Referential use
only: works with Salesforce®.

Named **Bluechip** — bar chip, blue-chip orgs, a nod to Salesforce blue — without those marks in
the title.

## License

MIT — [LICENSE](./LICENSE)
