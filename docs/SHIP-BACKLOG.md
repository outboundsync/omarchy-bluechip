# Ship backlog

**Build tip:** [`bluechip-v1`](https://github.com/outboundsync/omarchy-bluechip/tree/bluechip-v1)  
**Contract SoT:** `main` ([VISION.md](../VISION.md)) until Harris promotes.

Implement on `bluechip-v1`. Do not land CLI on `main` ahead of promote. Preserve adversarial-review contracts:

- `sandboxState` is three-valued (`prod` | `sandbox` | `unknown`) — never print PROD when `Organization.IsSandbox` is unreadable.
- **Confirm-before-write matrix** — context pack is not write authority; ban silent `--yes` on prod.
- **Hygiene H1–H10** — no letter grade until measured dimensions pass; all-unknown → no grade, no F, keep last-good.
- **No false graceful degradation** — limits/fields/probes that miss are `unknown`, not 0% green or scored 100.

## Already on `bluechip-v1` (read-only beachhead)

Org pin + three-valued chrome · API/limit pulse · hygiene probe · Flow-fault list (best-effort) · Setup change feed · agent context pack · `doctor` / `watch`. Tooling API, Metadata API, MCP, and confirm-gated writes are **not** called yet — do not claim them until a call exists.

---

## Wave 1 — agent muscle + integration forensics

| # | Item | Why | Contract notes |
| --- | --- | --- | --- |
| 1 | **Local Bluechip MCP** (Tooling + Metadata + SOQL) | Hosted SObject MCP is not enough; agents need Flow, NC, TraceFlag context | Read scopes first; writes out of scope until Wave 2 gate |
| 2 | **Named Credential / External Credential inspector** | “What header formula is live?” without leaking secrets | Formula + status only; never surface Consumer Secret |
| 3 | **TraceFlag + log tail** | Debug-before-Activate without Setup log archaeology | Sandbox default; prod requires confirm matrix row |

## Wave 2 — depth, multi-org, writes

| # | Item | Why | Contract notes |
| --- | --- | --- | --- |
| 4 | **FLS / perm-set matrix** (integration users) | Catch missing Edit before smoke tests | Compare required field list; sandbox-first writes |
| 5 | **API limit offenders** | “What ate API since 9am?” ranked by connected app / user / pattern | Advisory until Event Monitoring / Limits data is verified |
| 6 | **Multi-org polish** | Fast switch prod vs sandboxes; refresh age; stale-sandbox signals | Pin crossing sandbox↔prod triggers confirm |
| 7 | **Clipboard / agent handoff** | Richer incident bundle than raw `context` paste | Display-only unless confirm matrix satisfied |
| 8 | **Confirm-gated writes** | TraceFlag, FLS fix, Flow activate, metadata deploy | Implement matrix rows in UI; no hidden `--force` |

## Later — forensics + DX parity

| # | Item | Why |
| --- | --- | --- |
| 9 | **Apex-defined type explorer** | IN_/OUT_2XX shapes for Flow HTTP callouts |
| 10 | **DX / deploy drift** | Sandbox vs prod vs git when `sf` project is in play |
| 11 | **Sharing / “why can’t X see Y?”** | Profile, perm set, group, OWD in one explanation |

## Still parked (not in waves above)

- QML Omarchy shell module (Fabius)
- Connected App / External Client App registration (Brutus)
- Marketplace listing
- OutboundSync paid attach beyond optional deep-link ([hygiene attach story](../VISION.md))

Harris merges promote from `bluechip-v1` → `main`.
