# Bluechip vision (parked) — admin cockpit

**Locked direction 2026-09-14:** primary user is a **Salesforce System Administrator** on Omarchy (bleeding-edge / agentic Linux desktop). Seller lookup chip is a possible later wedge, not the mountain.

Unofficial · works with Salesforce® · not affiliated with Salesforce, Inc.

## Why this person

Omarchy’s early users skew technical, agent-native, allergic to browser tax. That maps to Salesforce admins and platform owners more than to AE seat-only sellers. Build for the person who *runs* the org.

## The job

Make Omarchy the **best place on earth to operate and troubleshoot a Salesforce org** — especially sandboxes, broken automations, and limit pressure — with agents as force multipliers and a human confirm gate on anything that writes.

## Three surfaces (do not collapse)

These are three products. Mixing them was the v1 copy bug.

| Surface | License | What it is | What it is not |
| --- | --- | --- | --- |
| **Cockpit** | MIT | Org pin, PROD/SANDBOX/`UNKNOWN` chrome, limits, Flow faults, Setup changes, agent context pack | A grade, a router, a paid SKU |
| **Hygiene probe** | MIT, read-only | Per-object measured / unknown cards. Completeness only over measured fields. **No A–F until dimensions are measured** (H1–H10). All-unknown → no letter, no F | Org truth; OutboundSync product |
| **Paid attach** | OutboundSync SKU | Router / enrichment remediation. Bluechip may **optional deep-link** only | Something the MIT CLI “is” |

`sandboxState` is three-valued everywhere (`prod` \| `sandbox` \| `unknown`). Never print PROD when Organization/IsSandbox is unreadable.

Beachhead (first ship, recommend): **org pin + hard chrome + limits + Flow faults + context pack**. Hygiene attach is the invoice line, not the first QML surface.

## Confirm-before-write matrix

Writes only after explicit confirm. This table is the gate; slogans are not.

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

Wave 1 Salesforce writes: TraceFlag start/stop (and DebugLevel create if missing), confirm-gated. Local pin / Waybar style.css are not org writes. MCP has no write tools.

Wave 2: `fls-propose` is a **proposal diff only**. FLS apply / Flow activate stay later and would reuse this matrix (sandbox-first; type `PROD` on prod; `--yes` banned on prod / unknown; never an MCP write).

## Pillars

### 1. Org & sandbox awareness
- Fast switch / pin: prod vs partial vs full sandboxes vs scratch (where DX applies)
- Per-org pulse: which org am I “looking at” in the bar right now?
- Sandbox refresh age, status, who’s using which sandbox
- Never confuse sandbox debug for prod (hard visual + confirm)

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
- **Writes only after explicit confirm** (deploy, trace flag, perm change, user freeze)

## Bleeding-edge admin desktop (aspiration)

The admin at a sharp tech company opens Omarchy and sees: which org is hot, which Flow is bleeding, which integration is chewing API, which sandbox is stale — then asks an agent to investigate with full context already attached. Lightning Setup becomes the occasional deep link, not the home screen.

## Revenue wedge (parked, 2026-09-14)

**ICP assumption:** the Salesforce org is **already set up**. No “getting started with Salesforce” widget, no Trailhead-style onboarding chrome, no empty-org wizard. The user is a System Admin (or equivalent) who already runs the org.

**Paid attach:** OutboundSync data hygiene / data audit — built as OutboundSync product. Contract stub: [docs/HYGIENE-ATTACH.md](docs/HYGIENE-ATTACH.md) (finding schema, no PII Id dump by default, sandbox findings must not remediate prod). Bluechip may optional-deep-link; the local `COUNT(field)` probe is **not** that product.

**Not the SKU:** sandbox provisioning, Flow test infrastructure, and confirm-gated agents stay useful beachheads and dogfood; they are not the primary invoice line until proven.

**Brand:** Bluechip remains the open Omarchy plugin name for now. Shipping furniture under an OutboundSync-facing label later is fine; do not confuse the MIT cockpit with the paid hygiene/router story. No “What does yours get?” grade marketing in the MIT CLI.

## Explicit non-goals (for now)

- Rebuilding Lightning Setup or Full DX IDE in QML
- Competing with frontier agents as the AI itself
- Full org data sync to the Linux filesystem
- Seller-only CRM chrome as the headline (can layer later)
- **Salesforce getting-started / empty-org onboarding** — assume the account exists; no Setup-for-beginners widget
- Owning Salesforce admin training or Trailhead replacement

## Park rule

Harris authorized ship **2026-09-15**. `bluechip-v1` is the **active build tip**.
`main` may still hold this parked vision until a later promote — do not silently
rewrite `main`. No Connected App, no QML scaffold, no marketplace listing until
a later wave requires them. Wave 1 rides `sf` + Tooling-via-`sf`.

See [docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md).

## Secrets / cache

- State dir `~/.config/bluechip/` is `chmod 700` on first write; cache and credentials files are `0600`.
- Never store or display a Consumer Secret in a settings UI (Enricherino rule). v1 has no Connected App and strips `accessToken` / `clientSecret` / `clientId` from snapshots and the context pack.
- Hygiene config is code: object/field API names must match describe or a strict regex before interpolation into SOQL.

## Grounded in Harris × Brutus pain (2026-08 → 2026-09)

Real friction already felt — not hypothetical:

1. **Flow HTTP Callout / Apex-defined types** — IN_/OUT_2XX class soup, async-path-only callouts, Text pickers hiding Apex objects, loops just to read `mobilePhone`, Debug-before-Activate ritual. Hardest click-path Harris hit; wanted one step at a time, not dual-path docs.
2. **Named Credential + External Credential auth** — Custom header formulas (`Authorization={! $Credential…}`), Generate Authorization Header vs Allow Formulas traps; ZoomInfo client-credentials and later OutboundSync Router creds.
3. **Org split brain** — patterns built in `outboundsyncinc-dev-ed` then re-done on live team EE; easy to debug the wrong org.
4. **Salesforce MCP ceiling** — Cursor hosted MCP = SObject CRUD/SOQL only; **no Metadata / custom fields**. Fields and much Setup stayed click-coaching. Flex Credits / Event Log metering became its own project.
5. **Integration user FLS / Perm Set traps** — SavvyCal fields needed Read+Edit on Integration PS via API after Setup miss; “probe MCP/org before inventing menus” became house rule.
6. **Boundary debugging** — Cloudflare Worker + Salesforce (wrangler tail, Illegal invocation, webhook mapping) while SF side was Named Cred / Connected App / PS.

### What Omarchy would need to make *that* work easier

| Capability | Why (from pain) | Access / tooling required |
| --- | --- | --- |
| **Org pin + hard sandbox/prod chrome** | Stop debugging Dev Ed thinking it’s EE | Multiple org auth (JWT or browser OAuth); bar badge impossible to miss |
| **Flow fault console** | Interview faults + element errors without Setup archaeology | Tooling API: `FlowInterview`, Flow definition; View Flows / Manage Flow |
| **Apex-defined type explorer** | See IN_/OUT_2XX shapes the Callout generated | Tooling/Apex Class describe; or retrieve Flow + related Apex |
| **One-click Trace Flag + log tail** | Debug-before-Activate without Log page hell | Tooling `TraceFlag`, `ApexLog`; local log stream |
| **Named Cred / Ext Cred inspector** | “What header formula is live?” without leaking secrets | Metadata/Tooling read of NC/EC; secret values never shown — formula + status only |
| **Perm Set / FLS matrix for integration users** | Catch missing Edit before smoke test | SOQL + Tooling FieldPermissions / ObjectPermissions; compare PS to required field list |
| **API + Flex Credit pulse** | Hosted MCP burn; callout volume | Event Monitoring (`SALESFORCE_HOSTED_MCP`), Limits API, optional Digital Wallet read; local ledger |
| **Agent context pack** | Cursor/Grok get “this org, this Flow, last fault, this NC” without paste | Local Bluechip MCP over Tooling/Metadata/SOQL — **beyond** sObject-only hosted MCP |
| **Confirm-gated writes** | Deploy TraceFlag, FLS fix, Flow activate | Metadata/Tooling write scopes; always confirm; sandbox-first |

### Access stack (required to be real)

- **Connected App / External Client App** for Bluechip (separate from Cursor MCP): scopes for API, refresh; Admin-approved; Sysadmin-only to start
- **User perms:** API Enabled, View Setup, Manage Flow / View All Data as needed, View Event Log Files (for MCP/Flex forensics), Modify Metadata if deploy features ship
- **APIs (vision):** REST + **Tooling** + **Metadata** (hosted SObject MCP is not enough — that was the Brutus ceiling)
- **Local:** `sf` CLI optional for DX parity; Bluechip should not require VS Code
- **Hard rule:** never store Consumer Secret in the plugin settings UI (Enricherino `~/.config` 0600 pattern)

### API honesty — vision vs this side branch

Do not say “Tooling” until a Tooling call exists.

| Call | Vision | `bluechip-v1` Wave 1 |
| --- | --- | --- |
| `sf org display` | identity | **yes** |
| `sf org list` / `sf org list limits` | limits pulse | **yes** (limits miss → unknown, not 0% green) |
| `sf data query` (SOQL / REST) | Organization, FlowInterview, SetupAuditTrail, ApexLog, hygiene aggregates | **yes** (+ User for TraceFlag target) |
| `sf sobject describe` | probe-before-invent / FLS | **yes** (hygiene, before COUNT) |
| Tooling `NamedCredential` / `ExternalCredential` | NC inspector | **yes** (safe fields + parameter *counts*; never secrets) |
| Tooling `TraceFlag` / `DebugLevel` | debug-before-activate | **yes** (status read; start/stop confirm-gated CLI) |
| Tooling Flow definition / FieldPermissions | Flow replay, FLS matrix | **not called** (Wave 2+) |
| `sf apex get log` | log tail | **yes** (neutralized, capped) |
| Metadata `list` (NC/EC names) | fallback when Tooling misses | **yes** — names only |
| Metadata retrieve/deploy | confirm-gated writes | **not called** |

### Agent leverage on this desk

Agent does not replace Brutus/Harris judgment. It gets a **pre-attached incident bundle** (org id, Flow API name, interview Id, fault message, NC API name, recent ApexLog Ids) and proposes the next Setup click or a Metadata diff — human confirms. That is exactly the gap when MCP could SOQL but couldn’t see why a callout type was wrong.
