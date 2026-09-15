# Bluechip

![status: experimental side branch](https://img.shields.io/badge/status-experimental%20(not%20main)-lightgrey) ![license: MIT](https://img.shields.io/badge/license-MIT-green) ![read-only](https://img.shields.io/badge/writes-none%20(read--only)-lightgrey)

Salesforce **admin cockpit** experiment for [Omarchy](https://omarchy.org) on the
`bluechip-v1` side branch. **`main` remains parked** until Harris unparks — this
is not the product of record.

Ambient pulse in the status bar: three-valued **PROD / SANDBOX / UNKNOWN** chrome
you can't misread, live API-limit gauges (unknown when the Limits API misses — not
0% green), a **read-only data probe**, Flow faults, Setup-change feed, and a
one-command **agent context pack**. Read-only. Rides your existing `sf` login —
**no Connected App, no stored secrets.**

**ID:** `outboundsync.bluechip` · **Author:** OutboundSync / Harris Kenny · **License:** MIT

![bluechip doctor](assets/doctor-card.svg)

## Three surfaces

| Surface | What you get |
| --- | --- |
| **Cockpit (MIT)** | Pin, chrome, limits, Flow faults, Setup changes, context pack |
| **Hygiene probe (MIT)** | Per-object measured / unknown cards. Completeness only over fields we actually measured. **No A–F until measured** (H1–H10). All-unknown → no letter, no F, last-good kept |
| **Paid attach** | OutboundSync SKU. Optional deep-link only — **local probe ≠ OutboundSync product**. See [docs/HYGIENE-ATTACH.md](docs/HYGIENE-ATTACH.md) |

Beachhead: org pin + chrome + limits + Flow faults + context pack first.

## Why

Omarchy's early users are technical, agent-native, allergic to browser tax —
Salesforce admins and platform owners more than seat-only sellers. Bluechip
makes Omarchy a place to *operate* an org: which org is hot, which limit is
bleeding, which Flow faulted, what changed since Friday — ambient, in the OS,
with your agent one paste away. Lightning Setup becomes the occasional deep
link, not the home screen.

## The trust story (why this is safe to run today)

- **Read-only.** This branch never writes to your org. No deploys, no field edits, no activations.
- **Your login, not a new one.** Everything shells out to the Salesforce CLI (`sf ... --json`)
  under *your* session. No Connected App to register or approve.
- **No secrets stored.** The access token that `sf org display` returns is stripped before
  anything is cached; `context` bundles are scrubbed too. Consumer Secret is never shown.
  State lives in `~/.config/bluechip/` (`chmod 700` on first write; cache/credentials `0600`).

## What this branch actually calls

| CLI | Used? |
| --- | --- |
| `sf org display` | yes |
| `sf org list` / `sf org list limits` | yes |
| `sf data query` | yes (Organization, FlowInterview, SetupAuditTrail, ApexLog, hygiene aggregates) |
| `sf sobject describe` | yes (hygiene, **before** multi-field COUNT) |
| Tooling API / Metadata API | **not called** — do not claim them until a call exists |

Context pack is **not write authority**. Agents are display-only unless the
[confirm matrix](VISION.md#confirm-before-write-matrix) is satisfied. Silent
`--yes` for prod is banned.

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
| `bluechip context` | Paste-ready incident bundle (display-only; not write authority) |
| `bluechip orgs` / `pin <alias\|->` | List sf-authed orgs; pin one for Bluechip |
| `bluechip watch [secs]` | `mako` notification on amber→red limits / new flow faults |
| `bluechip refresh` | Force-refresh the cached snapshot |

Global: `-o/--org <alias>` (target one org), `--refresh`, `--no-color`.

Tune thresholds with env vars: `BLUECHIP_WARN_PCT` (75), `BLUECHIP_CRIT_PCT` (90),
`BLUECHIP_CACHE_TTL` (120s), `BLUECHIP_DUPE_CAP` (200).

Optional paid-attach deep-link (off by default): `BLUECHIP_REMEDIATE_URL`.
Printed only when set, with “local probe ≠ OutboundSync product.”

See [docs/DEMO.md](docs/DEMO.md) for a 60-second walkthrough.

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

**On this branch (read-only):** org pin + three-valued chrome · API/limit pulse ·
hygiene probe · Flow-fault list · Setup change feed · agent context pack ·
`doctor` card · optional `watch`.

**Deferred (still parked on `main`):** Connected App / External Client App · a
local Bluechip MCP over Tooling/Metadata · Metadata deploys · **confirm-gated
writes** · element-level Flow replay · OutboundSync attach beyond an optional
deep-link. See [VISION.md](VISION.md).

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
