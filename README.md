# Bluechip

![status: tip · MIT cockpit](https://img.shields.io/badge/status-tip%20·%20MIT%20cockpit-blue) ![license: MIT](https://img.shields.io/badge/license-MIT-green) ![writes: confirm-gated TraceFlag](https://img.shields.io/badge/writes-confirm--gated%20TraceFlag-lightgrey)

Salesforce **admin cockpit** for [Omarchy](https://omarchy.org). Pin an org, see
whether it is **PROD / SANDBOX / UNKNOWN**, probe what is on fire, pack that for
an agent, and confirm any write yourself.

**`pin → pulse → probe → pack → agent → confirm write`**

Harness: [docs/UX-PASS.md](docs/UX-PASS.md) · 60-second walkthrough:
[docs/DEMO.md](docs/DEMO.md) · Remainder: [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md)

**ID:** `outboundsync.bluechip` · **Author:** OutboundSync / Harris Kenny · **License:** MIT

![bluechip doctor](assets/doctor-card.svg)

## For agents

**[AGENTS.md](AGENTS.md)** — coding this repo and using it at the desk. Recipes:
**[docs/AGENT-PLAYBOOK.md](docs/AGENT-PLAYBOOK.md)**. Proof suites live there (no live org).

## Trust

- **Reads are the default.** Probes, packs, and MCP never write. `fls-propose` is a diff only.
- **TraceFlag is the only org write** — `bluechip trace start|stop`, confirm-gated
  (sandbox-first; type `PROD` on prod; `--yes` banned on prod / unknown). Not an
  MCP tool.
- **Your `sf` login.** Everything shells out to `sf ... --json` under your session.
- **No secrets stored or printed.** Access tokens are stripped before cache;
  Named Cred inspector never shows secrets. State lives in `~/.config/bluechip/`
  (`chmod 700`; cache/credentials `0600`).

## Install

Requires **Omarchy** (or Hyprland + Waybar), [`sf`](https://developer.salesforce.com/tools/salesforcecli),
and `jq`.

**install → `sf org login web` → `bluechip doctor` → chip.**

```bash
git clone https://github.com/outboundsync/omarchy-bluechip
cd omarchy-bluechip
./install.sh              # links bluechip into ~/.local/bin, checks deps, offers Waybar wiring

npm i -g @salesforce/cli  # if you don't have sf yet
sf org login web
bluechip doctor
```

### Chip (Waybar)

`install.sh` appends the styles. Add the module to a `modules-*` array in
`~/.config/waybar/config.jsonc` from [`waybar/config.snippet.jsonc`](waybar/config.snippet.jsonc),
then `pkill -SIGUSR2 waybar`.

```jsonc
"modules-right": ["custom/bluechip", "clock", "battery"],
// ... plus the "custom/bluechip": { ... } object from the snippet
```

The chip is **identity + worst signal** (`SBX · 62%`, `PROD · 1 fault`, `? · ?`).
PROD stays quiet, SANDBOX is loud, UNKNOWN is muted — never fake-green.
Left-click pins, middle opens `doctor`, right refreshes.

Copy pack: `bluechip incident | wl-copy`. Optional Hyprland binds and scratchpad:
[docs/UX-PASS.md](docs/UX-PASS.md).

## Common jobs

Primary verbs: `bluechip help`. Advanced: `bluechip help --all`.
Product SoT: [docs/UX-PASS.md](docs/UX-PASS.md).

| Job | Command |
| --- | --- |
| Org vitals | `bluechip doctor [--json]` |
| Pin an org | `bluechip orgs` then `pin <alias>` |
| What's on fire | `bluechip incident` (or `limits` / `flows`) |
| Hand-off for an agent | `bluechip incident \| wl-copy` |
| Callout 401 / Flow types | `bluechip callout-auth` · `types` · `callout-pack <Flow>` |
| Confirm-gated debug | `bluechip trace start` then `logs --follow` |

Pipes: `bluechip context | wl-copy` · `bluechip incident --json | jq .signal`

Global: `-o/--org`, `--refresh`, `--json`, `--yes` (sandbox-only write confirm),
`--no-color`.

## Local MCP (display-only)

```bash
bluechip mcp-config
```

Wraps `bin/bluechip`. **No TraceFlag create, no FLS apply, no pin write, no deploy.**
Tools and recipes: [AGENTS.md](AGENTS.md) · [docs/AGENT-PLAYBOOK.md](docs/AGENT-PLAYBOOK.md).

## Hygiene probe

![bluechip hygiene](assets/hygiene-card.svg)

`bluechip hygiene` is a **read-only probe**, not a product grade. Completeness
only over fields actually measured. A miss is unknown, never 0 or 100.
All-unknown → no letter, last-good kept. Acceptance:
[`scripts/test-hygiene.sh`](scripts/test-hygiene.sh) (H1–H10). Config:
`~/.config/bluechip/hygiene.json`. Local probe ≠ OutboundSync product —
[docs/HYGIENE-ATTACH.md](docs/HYGIENE-ATTACH.md).

## Not this (yet)

Not a letter-grade product, not Lightning Setup, not a Connected App, not QML,
not FLS/Flow apply, not Metadata retrieve/deploy. Packs are not write authority.

Flow faults are best-effort (hard faults may not persist as `FlowInterview` rows).
You need what `sf` can see — denied probes are `unknown`, not scored as 0 or 100.
Hygiene is exact-match and heuristic. Targets bash 5 / Linux (Omarchy).

Salesforce I/O actually called: [VISION.md](VISION.md#api-honesty).
Parked remainder: [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md). Confirm matrix:
[VISION.md](VISION.md#confirm-before-write-matrix).

Implementers: ranked backlog on **[docs/SHIP-BACKLOG.md](./docs/SHIP-BACKLOG.md)**.

## Unofficial disclaimer

**Bluechip is unofficial.** Not affiliated with, endorsed by, or sponsored by
Salesforce, Inc. Referential use only: works with Salesforce®. Named Bluechip —
bar chip, blue-chip orgs — without those marks in the title.

## License

MIT — [LICENSE](./LICENSE)
