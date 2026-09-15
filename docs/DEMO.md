# Bluechip — 60-second walkthrough

Pin → pulse → probe → pack → agent → confirm write.

Install and trust: [README.md](../README.md). Harness: [UX-PASS.md](UX-PASS.md).

## The shots

![bluechip doctor](../assets/doctor-card.svg)

`bluechip doctor` — org, a PROD / SANDBOX / **UNKNOWN** badge, live limit gauges
(or unknown if Limits missed), last measured data probe, last Setup change.

![bluechip hygiene](../assets/hygiene-card.svg)

`bluechip hygiene` — per-object measured / unknown cards. A miss is `?`, not 100.
An all-unknown scan has no letter and does not overwrite last-good cache.

## Script

```bash
# 0. Once — Bluechip rides this session, no Connected App:
sf org login web

# 1. Vitals
bluechip doctor

# 2. Pin (never debug the wrong org)
bluechip orgs
bluechip pin acme-uat

# 3. Pack what's on fire for an agent (hand-off, not write authority)
bluechip incident | wl-copy

# 4. Confirm-gated debug (type SANDBOX / alias; type PROD on prod)
bluechip trace start
bluechip logs --follow
```

Waybar chip: README install. Optional MCP: `bluechip mcp-config` (display-only).

More verbs (`hygiene`, `named-creds`, `fls`, `offenders`, `desk`):
`bluechip help --all` and [UX-PASS.md](UX-PASS.md).

## Say this

- **Wrong-org chrome.** PROD you cannot mistake for a sandbox; UNKNOWN when
  `Organization.IsSandbox` was unreadable.
- **Limits before they page you.** Amber → red in the bar. A miss is unknown, not
  a healthy 0%.
- **One paste.** `bluechip incident` is hand-off, not authorization to deploy.
- **Reads default; TraceFlag is the only write**, confirm-gated, not MCP.

Do **not** use “What does yours get?” / letter-grade-as-org-truth copy.
