# Bluechip vision (parked) — admin cockpit

**Locked direction 2026-09-14:** primary user is a **Salesforce System Administrator** on Omarchy (bleeding-edge / agentic Linux desktop). Seller lookup chip is a possible later wedge, not the mountain.

Unofficial · works with Salesforce® · not affiliated with Salesforce, Inc.

## Why this person

Omarchy’s early users skew technical, agent-native, allergic to browser tax. That maps to Salesforce admins and platform owners more than to AE seat-only sellers. Build for the person who *runs* the org.

## The job

Make Omarchy the **best place on earth to operate and troubleshoot a Salesforce org** — especially sandboxes, broken automations, and limit pressure — with agents as force multipliers and a human confirm gate on anything that writes.

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

## Bleeding-edge admin desktop (aspiration)

The admin at a sharp tech company opens Omarchy and sees: which org is hot, which Flow is bleeding, which integration is chewing API, which sandbox is stale — then asks an agent to investigate with full context already attached. Lightning Setup becomes the occasional deep link, not the home screen.

## Revenue wedge (parked, 2026-09-14)

**ICP assumption:** the Salesforce org is **already set up**. No “getting started with Salesforce” widget, no Trailhead-style onboarding chrome, no empty-org wizard. The user is a System Admin (or equivalent) who already runs the org.

**Paid attach:** OutboundSync data hygiene / data audit capabilities — built as OutboundSync product, made available through this Omarchy plugin. Ambient “see the mess” on the admin desk; remediation that can route through OutboundSync (including the router / enrichment path) when they want to act at scale.

**Not the SKU:** sandbox provisioning, Flow test infrastructure, and confirm-gated agents stay useful beachheads and dogfood; they are not the primary invoice line until proven.

**Brand:** Bluechip remains the open Omarchy plugin name for now. Shipping furniture under an OutboundSync-facing label later is fine; do not confuse the MIT cockpit with the paid hygiene/router story.

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
- Seller-only CRM chrome as the headline (can layer later)
- **Salesforce getting-started / empty-org onboarding** — assume the account exists; no Setup-for-beginners widget
- Owning Salesforce admin training or Trailhead replacement
- **Letter grades / org hygiene scores** until H1–H10 and the attach contract above are implemented
- Claiming graceful degradation of scores without measured unknown handling

## Park rule

Shape and vision only until Harris unparks. No Connected App, no QML scaffold required to keep this document honest.

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
- **APIs:** REST + **Tooling** + **Metadata** (hosted SObject MCP is not enough — that was the Brutus ceiling)  
- **Local:** `sf` CLI optional for DX parity; Bluechip should not require VS Code  
- **Hard rule:** never store Consumer Secret in the plugin settings UI (Enricherino `~/.config` 0600 pattern)

### Agent leverage on this desk

Agent does not replace Brutus/Harris judgment. It gets a **pre-attached incident bundle** (org id, Flow API name, interview Id, fault message, NC API name, recent ApexLog Ids) and proposes the next Setup click or a Metadata diff — human confirms. That is exactly the gap when MCP could SOQL but couldn’t see why a callout type was wrong.
