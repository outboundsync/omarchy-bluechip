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
