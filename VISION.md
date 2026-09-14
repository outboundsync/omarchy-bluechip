# Bluechip vision (parked) — admin cockpit

**Locked direction 2026-09-14:** primary user is a **Salesforce System Administrator** on Omarchy (bleeding-edge / agentic Linux desktop). Seller lookup chip is a possible later wedge, not the mountain.

Unofficial · works with Salesforce® · not affiliated with Salesforce, Inc.

## Why this person

Omarchy’s early users skew technical, agent-native, allergic to browser tax. That maps to Salesforce admins and platform owners more than to AE seat-only sellers. Build for the person who *runs* the org.

## The job

Make Omarchy the **best place on earth to operate and troubleshoot a Salesforce org** — especially sandboxes, broken automations, and limit pressure — with agents as force multipliers and a human confirm gate on anything that writes.

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

## Explicit non-goals (for now)

- Rebuilding Lightning Setup or Full DX IDE in QML
- Competing with frontier agents as the AI itself
- Full org data sync to the Linux filesystem
- Seller-only CRM chrome as the headline (can layer later)

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
