# Hygiene attach contract (stub)

One page. This is **not** an implementation. Bluechip’s local probe is not the
OutboundSync product. A remediate CTA is allowed only as an **optional
deep-link** (`BLUECHIP_REMEDIATE_URL`) with that disclaimer.

## Identity

| Field | Meaning |
| --- | --- |
| `orgId` | 18-char Salesforce org id (from `sf org display` / Organization) |
| `sandboxState` | `prod` \| `sandbox` \| `unknown` — three-valued; never assume prod |
| `connectionId` | OutboundSync connection (paid side). **Not** invented by Bluechip. |
| `measuredAt` | Unix seconds of the last **persisted** ok/partial probe |

Per-org vs per-connection: Bluechip keys last-good cache to the local pin /
default org. OutboundSync attach keys findings to `orgId` + `connectionId`.
Do not merge sandbox findings into a prod connection.

## Finding schema (outline)

```json
{
  "schema": "bluechip.hygiene.finding/v0",
  "orgId": "00D…",
  "sandboxState": "sandbox",
  "object": "Lead",
  "dimension": "completeness | freshness | dupes | ownership | pipeline",
  "availability": "ok | partial | unknown",
  "reason": "object_forbidden | field_forbidden | invalid_field | transient | timeout | null",
  "metric": { "kind": "fill_rate | stale_count | dupe_excess | orphan_count", "value": 0 },
  "field": "Email",
  "recordIds": []
}
```

- Default payload is **aggregates** (counts, fill-rates, group counts).
- **Do not dump record Ids** (PII) unless the admin explicitly opts in on the
  paid attach. The MIT CLI never prints Id lists for hygiene.
- `availability: unknown` findings travel as unknown — they are not a score of 0
  or 100 and must not create remediation work.

## Who runs the audit

The running user is whoever `sf` is logged in as (often a sysadmin, sometimes an
integration user). FLS is **that** user’s FLS. Missing **Edit** is irrelevant for
a read-only probe; missing **Read** is `field_forbidden` / `object_forbidden`.

## Sandbox must not remediate prod

- If `sandboxState != "prod"`, attach consumers **must refuse** to apply
  remediation to a production org / production connection.
- If `sandboxState == "unknown"`, refuse remediation entirely.
- Deep-link query params may include `orgId` + `sandboxState` so the paid app
  can enforce the same gate.

## What Bluechip will not do

- Call an OutboundSync write API from this MIT repo.
- Pretend `COUNT(field)` exact-match hygiene **is** the paid audit/router SKU.
- Send Consumer Secrets, access tokens, or raw `sf org display` secrets.
