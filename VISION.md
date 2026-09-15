# Bluechip vision — admin cockpit

Primary user: a **Salesforce System Administrator** on Omarchy. Seller lookup is
a later wedge, not the mountain.

## The job

Operate and troubleshoot a Salesforce org from the Linux desk — especially
sandboxes, broken automations, and limit pressure — with agents as force
multipliers and a human confirm gate on anything that writes. Lightning Setup
is the occasional deep link, not the home screen.

Build for the person who *runs* the org: technical, agent-native, allergic to
browser tax.

## Three surfaces (do not collapse)

These are three products. Mixing them was the copy bug.

| Surface | License | What it is | What it is not |
| --- | --- | --- | --- |
| **Cockpit** | MIT | Org pin, PROD/SANDBOX/`UNKNOWN` chrome, limits, Flow faults, Setup changes, agent context pack | A grade, a router, a paid SKU |
| **Hygiene probe** | MIT, read-only | Per-object measured / unknown cards. Completeness only over measured fields. **No A–F until dimensions are measured** (H1–H10). All-unknown → no letter, no F | Org truth; OutboundSync product |
| **Paid attach** | OutboundSync SKU | Router / enrichment remediation. Bluechip may **optional deep-link** only | Something the MIT CLI “is” |

`sandboxState` is three-valued everywhere (`prod` \| `sandbox` \| `unknown`). Never print PROD when Organization/IsSandbox is unreadable.

The cockpit epicenter is **org pin + hard chrome + limits + Flow faults + context pack**. Hygiene attach is the invoice line, not a QML surface.

## Confirm-before-write matrix

Writes only after explicit confirm. This table is the gate.

| Write | Allowed on | Confirm UX | Forbidden |
| --- | --- | --- | --- |
| Pin org | any | none if same `IsSandbox`; confirm if crossing sandbox↔prod | Agent pin without UI |
| TraceFlag / log | sandbox default; prod extra | type alias | `--yes`, MCP write tool |
| FLS / perm change | sandbox first | type alias + field list | “missing Edit → invent field” |
| Flow activate / metadata deploy | sandbox first | type `PROD` + alias on prod | Agent deploy from context pack |
| User freeze | prod allowed, extra | type username | batch freeze from agent |

Hard rules:

- No hidden `--force`. **Ban silent `--yes` for prod.**
- No confirm remembered across orgs.
- **Context pack is not write authority.** Agents are display-only unless this matrix is satisfied in the Bluechip UI.
- Hosted SObject MCP writes (if any) are out of scope until a Tooling/Metadata call exists.

**On this tip:** TraceFlag start/stop (and DebugLevel create if missing) is the
only org write, confirm-gated. `fls-propose` is a proposal diff only. Local pin /
Waybar `style.css` are not org writes. MCP has no write tools. FLS apply / Flow
activate would reuse this matrix (sandbox-first; type `PROD`; `--yes` banned on
prod / unknown; never MCP).

## First beachhead (when Harris unparks)

Do not boil the ocean across all six pillars at once. Ship in this order:

1. **Org pin + hard sandbox/prod chrome** — impossible to misread which org is active
2. **API & limit pulse** — amber before red, aggregates only in the bar
3. **Flow fault console** — failed interviews surfaced ambiently
4. **Agent context pack** — org id, sandbox state, Flow interview, limits, recent changes — one paste for Cursor/Claude

Hygiene scoring, paid attach, access forensics, and DX drift follow after the cockpit is real. Seller chip stays later.

## Surfaces (do not conflate)

Three distinct products share a desk but must not blur in copy, UX, or brand:

| Surface | License / owner | Job | Grade / CTA rule |
| --- | --- | --- | --- |
| **Cockpit** | MIT Bluechip plugin | Pin, chrome, limits, Flow faults, changes, context pack | No letter grade; no “remediate at scale” CTA |
| **Hygiene probe** | MIT Bluechip (read-only) | Per-object measured/unknown cards from describe-first SOQL | **No A–F** until the measured contract and acceptance tests H1–H10 pass ([review](./docs/ADVERSARIAL-REVIEW-2026-09-15.md)) |
| **Paid attach** | OutboundSync SKU | Router/enrichment remediation at scale | Real attach API + finding schema; Bluechip may deep-link only |

The MIT repo must not sell the wedge with a stub URL or a local heuristic grade. Context pack is display-only — not write authority.

## Org identity: `sandboxState` (three-valued, everywhere)

`sandboxState` is **`prod` | `sandbox` | `unknown`** — never a boolean, never default unknown to prod.

Required on every surface that names the active org:

- Status bar / Waybar chip
- `doctor` (or equivalent health card)
- **Agent context pack** (same helper as bar — no separate `is_sandbox // false` path)
- Watch / background poll payloads
- Hygiene attach hand-off

When org metadata is unreadable, show **unknown** (amber), not silent PROD. Pin and switch across sandbox↔prod boundaries require confirm (see matrix below).

## Pillars

### 1. Org & sandbox awareness
- Fast switch / pin: prod vs partial vs full sandboxes vs scratch (where DX applies)
- Per-org pulse: which org am I “looking at” in the bar right now? (`sandboxState` three-valued — see above)
- Sandbox refresh age, status, who’s using which sandbox
- Never confuse sandbox debug for prod (hard visual + confirm when crossing sandbox↔prod)
- Show 18-char org id in chrome; pin to prod requires explicit confirm

### 2. Flow troubleshooting (hero pain)
- Failed / faulted Flow interviews surfaced ambiently
- Open interview detail, element faults, debug replay path without Setup archaeology
- Trace flags / logs tied to the Flow you’re staring at
- Agent: “explain why this interview faulted” → cite element + limits + data; propose fix; **confirm before metadata deploy**

### 3. API & limit utilization
- Ambient bar: API daily/rolling, streaming, async apex, storage, email, platform events — amber before red
- Who/what is burning the limit (connected app, user, integration pattern)
- Agent: “what ate API since 9am?” → ranked offenders + cut suggestions

### 4. Jobs, events, integrations
- Apex flex queue / scheduled / batch failures
- Platform events & CDC backpressure signals
- Named Credentials / Connected Apps / callout health (auth expiry, 401 spikes)
- OutboundSync-shaped pipes welcome as first-class citizens later — still general admin tool

### 5. Access & change forensics
- “Why can’t X see Y?” — profile / perm set / group / sharing / OWD in one explanation
- Setup Audit Trail + “what changed since Friday?”
- Deploy / drift: sandbox vs prod vs git (when DX is in play)

### 6. Agent leverage (Omarchy-native)
- Salesforce API / Tooling / Metadata / (where available) MCP = muscle
- Omarchy Agents / Cursor / Claude on the desk = brain
- Bluechip = seat, pulse, context pack (“this org, this Flow interview, these limits”)
- **Writes only after explicit confirm** (deploy, trace flag, perm change, user freeze) — see confirm matrix
- Context pack is **not** authorization: agents may write via other tools; Bluechip must not imply the pack blessed a deploy

## Revenue wedge (parked)

**ICP:** the Salesforce org is **already set up**. No getting-started widget, no
empty-org wizard. The user already runs the org.

**Paid attach:** OutboundSync data hygiene / data audit — OutboundSync product.
Contract stub: [docs/HYGIENE-ATTACH.md](docs/HYGIENE-ATTACH.md). Bluechip may
optional-deep-link; the local `COUNT(field)` probe is **not** that product.

**Not the SKU:** sandbox provisioning, Flow test infrastructure, and confirm-gated
agents. No “What does yours get?” grade marketing in the MIT CLI.

**Brand:** Bluechip remains the open Omarchy plugin name. Do not confuse the MIT
cockpit with the paid hygiene/router story.

### Hygiene attach contract (stub — spec before any scoring UI)

Paid attach is built as OutboundSync product, surfaced through this plugin. Before any hygiene UX or “remediate” CTA ships, lock:

**Identity:** org id (18-char) + `sandboxState`; per-connection OutboundSync identity when applicable; which user/perm runs the audit (integration user FLS trap applies).

**Finding schema (outline):**

```json
{
  "findingId": "uuid",
  "orgId": "00D…",
  "sandboxState": "prod | sandbox | unknown",
  "objectApi": "Lead",
  "dimension": "completeness | freshness | ownership | dupes | overdue",
  "availability": "ok | partial | unknown",
  "reason": "object_forbidden | field_forbidden | invalid_field | transient | timeout",
  "aggregate": { "total": 1200, "affected": 42, "fieldApi": "Email" },
  "measuredAt": "ISO-8601"
}
```

**Transport rules:**

- Prefer **aggregates** (counts, field API names, dimension summaries) over record-Id dumps — minimize PII in the plugin and attach payload.
- `reason` is a closed enum; raw Salesforce errors may be attached length-capped and neutralized at model entry.
- **Sandbox findings must not remediate into prod** — attach and router enforce org + `sandboxState` match before any write path.
- Failed probe keeps last-good on disk; do not overwrite with a speculative F or A from missing data.

Full acceptance tests: H1–H10 in [docs/ADVERSARIAL-REVIEW-2026-09-15.md](./docs/ADVERSARIAL-REVIEW-2026-09-15.md).

### Scoring honesty

Until H1–H10 pass in a real harness, copy in README, DEMO, and this doc **must not** claim:

- “Graceful degradation” of hygiene scores
- “Says so rather than guessing” when probes fail
- Org-level **A–F letter grades** as truth

Unknown probes → `unknown` availability, not 0% green limits, not 100 on null dimensions, not F when every object is denied.

## Confirm-before-write matrix

Writes are gated by explicit human confirm — not slogans, not `--yes`, not agent tools that bypass UI.

| Write | Allowed on | Confirm UX | Forbidden |
| --- | --- | --- | --- |
| Pin org | any | none if same `IsSandbox`; confirm if crossing sandbox↔prod | Agent pin without UI |
| TraceFlag / log | sandbox default; prod extra | type org alias | `--yes`, silent prod flag, MCP write tool |
| FLS / perm change | sandbox first | type alias + field list | “missing Edit → invent field” |
| Flow activate / metadata deploy | sandbox first | type `PROD` + alias on prod | Agent deploy from context pack |
| User freeze | prod allowed, extra | type username | batch freeze from agent |

**Hard rules:**

- No hidden `--force` or remembered confirm across orgs.
- No silent `--yes` on prod writes.
- Context pack ≠ write authority.
- Hosted SObject MCP writes (if any) are out of scope until Tooling/Metadata paths exist with the same gate.

## Explicit non-goals (for now)

- Rebuilding Lightning Setup or Full DX IDE in QML
- Competing with frontier agents as the AI itself
- Full org data sync to the Linux filesystem
- Seller-only CRM chrome as the headline
- Salesforce getting-started / empty-org onboarding
- Owning Salesforce admin training or Trailhead replacement
- **Letter grades / org hygiene scores** until H1–H10 and the attach contract above are implemented
- Claiming graceful degradation of scores without measured unknown handling

## Park rule

**`main` is the active build tip** (promoted from `bluechip-v1`). No Connected App,
no QML scaffold, no marketplace listing until those surfaces unpark.

See [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md). Agent contract:
[AGENTS.md](AGENTS.md).

## Secrets / cache

- State dir `~/.config/bluechip/` is `chmod 700` on first write; org-bound
  artifacts live under `orgs/<18charOrgId>/` (`0700`); cache and credentials
  files are `0600`. Pin map / hygiene *config* stay global so a pin switch
  cannot reuse another org’s snapshot or last-good.
- Never store or display a Consumer Secret in a settings UI. This tip has no Connected App and strips `accessToken` / `clientSecret` / `clientId` from snapshots and the context pack.
- Hygiene config is code: object/field API names must match describe or a strict regex before interpolation into SOQL.

## Grounded in Harris × Brutus pain (2026-08 → 2026-09)

Real friction — why the hard rules exist:

1. **Flow HTTP Callout / Apex-defined types** — IN_/OUT_2XX class soup, Debug-before-Activate ritual.
2. **Named Credential + External Credential auth** — header formulas vs Generate Authorization Header traps; secrets must never print.
3. **Org split brain** — easy to debug Dev Ed thinking it is live EE.
4. **Salesforce MCP ceiling** — hosted MCP = SObject CRUD/SOQL; no Metadata / custom fields.
5. **Integration user FLS / Perm Set traps** — probe the org before inventing Setup menus.
6. **Boundary debugging** — Worker + Salesforce while the SF side was Named Cred / Connected App / PS.

| Capability | Why (from pain) | Access / tooling required |
| --- | --- | --- |
| **Org pin + hard sandbox/prod chrome** | Stop debugging the wrong org | Multiple org auth; bar badge impossible to miss |
| **Flow fault console** | Interview faults without Setup archaeology | Tooling: `FlowInterview`, Flow definition |
| **Apex-defined type explorer** | See IN_/OUT_2XX shapes | Tooling/Apex Class; or retrieve Flow + Apex |
| **One-click Trace Flag + log tail** | Debug-before-Activate without Log page hell | Tooling `TraceFlag`, `ApexLog` |
| **Named Cred / Ext Cred inspector** | Live header formula without leaking secrets | Metadata/Tooling read; secrets never shown |
| **Perm Set / FLS matrix for integration users** | Catch missing Edit before smoke test | SOQL FieldPermissions / ObjectPermissions |
| **API + Flex Credit pulse** | Who burned API since 9am | Event Monitoring, Limits API |
| **Agent context pack** | This org, this Flow, last fault, this NC | Local Bluechip MCP over Tooling/Metadata/SOQL |
| **Confirm-gated writes** | Deploy TraceFlag, FLS fix, Flow activate | Always confirm; sandbox-first |

### Access stack (required to be real)

- **Connected App / External Client App** when we leave local `sf`: API + refresh; Admin-approved; Sysadmin-only to start
- **User perms:** API Enabled, View Setup, Manage Flow / View All Data as needed, View Event Log Files, Modify Metadata if deploy features ship
- **APIs (vision):** REST + **Tooling** + **Metadata** (hosted SObject MCP is not enough)
- **Local:** `sf` CLI optional for DX parity; Bluechip should not require VS Code
- **Hard rule:** never store Consumer Secret in the plugin settings UI

## API honesty

Do not say “Tooling” until a Tooling call exists.

| Call | Vision | This tip |
| --- | --- | --- |
| `sf org display` | identity | **yes** |
| `sf org list` / `sf org list limits` | limits pulse | **yes** (limits miss → unknown, not 0% green) |
| `sf data query` (SOQL / REST) | Organization, FlowInterview, SetupAuditTrail, ApexLog, hygiene aggregates | **yes** (+ User, PermissionSet*, ObjectPermissions, FieldPermissions, EventLogFile) |
| `sf sobject describe` | probe-before-invent / FLS | **yes** (hygiene, before COUNT) |
| Tooling `NamedCredential` / `ExternalCredential` | NC inspector + callout-auth doctor | **yes** (safe fields + parameter *counts* + 401-class diagnoses; never secrets) |
| Tooling `ApexClass` (Name / SymbolTable / Body) | HTTP Callout / Apex-defined types | **yes** (size-capped, neutralized; no deploy) |
| SOQL `ExternalCredentialPrincipalAccess` / `SetupEntityAccess` | principal access for DWU / Automated Process | **best-effort** — miss → `principal_unknown` |
| Tooling `TraceFlag` / `DebugLevel` | debug-before-activate | **yes** (status read; start/stop confirm-gated CLI) |
| Tooling `FlowDefinition` | Flow replay | **yes** (clipboard/context; not full replay) |
| SOQL FieldPermissions / ObjectPermissions | FLS matrix | **yes** (read + `fls-propose` diff) |
| `sf apex get log` | log tail | **yes** (neutralized, capped) |
| `sf api request rest` (EventLogFile `LogFile`) | limit offenders | **best-effort** — miss → unknown, not fake 0 |
| Metadata `list` (NC/EC names) | fallback when Tooling misses | **yes** — names only |
| Metadata retrieve/deploy | confirm-gated writes | **not called** |

### Agent leverage on this desk

Agent does not replace Brutus/Harris judgment. It gets a **pre-attached incident
bundle** (org id, Flow API name, interview Id, fault message, NC API name, recent
ApexLog Ids) and proposes the next Setup click or a Metadata diff — human
confirms.
