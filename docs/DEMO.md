# Bluechip — demo & launch kit

A 60-second demo and copy for announcing at Dreamforce week.

## The hero shot

![bluechip doctor](../assets/doctor-card.svg)

`bluechip doctor` is **"fastfetch for your Salesforce org"** — one command, a card people
screenshot: org, a PROD/SANDBOX badge you can't misread, live limit gauges, open Flow faults,
and the last Setup change. Post it. That's the hook.

## 60-second demo script

```bash
# 0. You already did this once — bluechip rides it, no Connected App:
sf org login web

# 1. The card (screenshot this)
bluechip doctor

# 2. It lives in your bar. Amber before red, PROD vs SANDBOX unmistakable.
#    (Waybar chip — see README "Install the chip")

# 3. Never debug the wrong org again — pin from the bar, or:
bluechip orgs
bluechip pin acme-uat

# 4. The pain everyone knows — what's on fire and what changed:
bluechip limits
bluechip flows
bluechip changes

# 5. The part frontier agents can't do for themselves — hand Claude/Cursor
#    the whole incident, pre-assembled, zero copy-paste archaeology:
bluechip context | wl-copy      # now paste into your agent
```

## Why an admin cares (say this)

- **Org split-brain, solved.** A prod chip you cannot mistake for a sandbox. (The #1 way admins
  break things: debugging Dev Ed thinking it's EE.)
- **Limits before they page you.** API, storage, async Apex, platform events — amber → red in the
  bar, ambient, no Setup spelunking.
- **Agent context pack.** `bluechip context` assembles org id, limits, Flow faults, and ApexLog
  ids into one paste — the missing hand-off between your org and your agent.
- **Read-only and honest.** Uses *your* `sf` login. No Connected App, no stored secrets, nothing
  written to your org. Writes/confirm-gate come later, sandbox-first.

## Announcement copy

**X / short:**
> Your Salesforce org now lives in your Linux status bar. 🔵
> Bluechip for @omarchy: PROD/SANDBOX you can't misread, live API-limit gauges, Flow faults, and a
> one-command agent context pack for Claude/Cursor. Open source, MIT, uses your own `sf` login —
> no Connected App. `bluechip doctor` ↓ #Dreamforce

**LinkedIn / longer:**
> Salesforce admins on Linux: meet Bluechip.
> It puts your org's pulse in the Omarchy (Hyprland) status bar — a PROD vs SANDBOX badge you can't
> mistake, live limit utilization (API, storage, async Apex, platform events) that goes amber
> before red, and your errored/paused Flow interviews surfaced without Setup archaeology.
> The part I'm most excited about: `bluechip context` assembles a full incident bundle — org id,
> limits, Flow faults, recent ApexLog ids — into one paste for Claude or Cursor. The missing
> hand-off between your org and your agent.
> It's read-only and rides your existing `sf` CLI login: no Connected App to register, no secrets
> stored. MIT-licensed. Unofficial, not affiliated with Salesforce.
> Try it: `bluechip doctor` → screenshot your org. Repo in comments.

**Omarchy community (GitHub discussion #3457 / Discord):**
> Built a Waybar module for the Salesforce admins in here: Bluechip. Org pulse + PROD/SANDBOX
> chrome + limit gauges + Flow faults, all read-only over your own `sf` login. `bluechip doctor`
> is a neofetch-style org card. Feedback welcome — themes inherit from your Omarchy theme.

## Notes / honesty

- Flow faults are **best-effort**: hard faults roll back and may not persist as `FlowInterview`
  rows. Bluechip surfaces errored/paused interviews and points you at `bluechip changes`
  for the rest.
- Everything is derived from `sf ... --json` (REST + Tooling + Limits). If `sf` can see it, so
  can Bluechip; if it can't, Bluechip says so instead of guessing.
