# Adversarial review — Bluechip (2026-09-15)

**Reviewer:** Janus (OS eng)  
**Subject:** parked vision + shape on `main`, plus the unmerged `origin/bluechip-v1` experiment  
**Repo:** `outboundsync/omarchy-bluechip`  
**Verdict:** **Not ready to unpark.** Vision is directionally locked and useful; it is not a shippable contract for hygiene scoring, FLS/unknown degradation, confirm-gated writes, or the paid attach. Do not treat `bluechip-v1` as the parked source of truth.

This review does **not** scaffold QML, create a Connected App, unpark, merge, or deploy.

---

## How to read this

- **`main` (parked tip, `00f0031`)** is the product of record: `README.md`, `PARKED.md`, `VISION.md`, MIT `LICENSE`. No implementation.
- **`origin/bluechip-v1` (`a7441b4`)** is a Dreamforce-week soft-unpark: bash CLI + Waybar chip + a local `bluechip hygiene` scorer. It is **not** on `main`. Cited only as evidence of how the vision collapses under first contact — not as shipping code.
- Claims of code that does not exist on `main` are marked **ABSENT**. Side-branch code is marked **v1-only**.

Lens source: Fabius / Quattro plugin must-not-repeat rules (unknown ≠ fail; last-good on 502; surface load errors; structured dry-run errors; 0600 secrets; neutralize PlainText at model entry; bound caches / ingest caps; confirm-gated writes; probe org before inventing menus; hosted SObject MCP is not enough). Checklist adapted from Janus `adversarial-review.md` to this admin-cockpit / hygiene-wedge product.

---

## Ranked findings

### P0 — act before any unpark or any “graceful degradation” / grade claim

1. **Hygiene scoring is unspecified on `main` and unsafe on `bluechip-v1`.** Harris’s robustness question (unavailable objects / FLS-hidden fields) is **N/A on `main`** (no scorer) and **FAIL on `bluechip-v1`**. See [Harris test](#harris-test-unavailable-object--fls-scoring-robustness). Do not ship, screenshot, or demo a letter grade until the acceptance tests in that section pass.

2. **`bluechip-v1` already soft-unparked the park rule.** `origin/bluechip-v1:PARKED.md` opens with “Unparked 2026-09-14 → v1 shipped” (`240e208`). `main:PARKED.md` still says parked, Harris-gated. That split is a process bug: agents and humans will read the wrong tip. **Fix (docs, now):** keep `main` parked; add a one-line “side branch exists, not the product of record” note to `PARKED.md` if you want the v1 experiment discoverable; do not merge v1 until Harris unparks.

3. **Unknown probes already become silent green or silent red (v1-only).** Fabius: unknown ≠ fail; never silent red / false PASS.
   - Limits API miss → `limits='[]'` in `build_snapshot` → `peak_util` emits `0 ok` → bar shows **0% green**. Failed lookup looks healthy.
   - Hygiene dimension miss (`LastActivityDate` / `Owner.IsActive` / dupe `GROUP BY` denied) → value `null` → `_hygiene_score_jq` uses `//0` → dimension scores **100**. Failed probe looks clean.
   - All objects `available:false` → `$scores` empty → `overall: 0` / grade **F**. Failed probe looks like a ruined org; also **overwrites** `~/.config/bluechip/cache/hygiene.json`, wiping last-good.
   **Fix (build, when unparked):** three-state per probe (`ok | warn | unknown`); overall grade is `unknown` unless every scored dimension is measured; never persist a speculative F/A over last-good.

4. **`context` still asserts PROD when the org row is unreadable (v1-only).** `a7441b4` fixed `bar`/`doctor` to `sandbox_state` ∈ {prod, sandbox, unknown}. `cmd_context` still uses `is_sandbox` (`.org.IsSandbox // false`) and prints `**PROD**` on miss (`origin/bluechip-v1:bin/bluechip`, context block). That is the org-split-brain failure `VISION.md` named as real Harris × Brutus pain. **Fix:** one helper, used everywhere; unknown is a first-class token in the agent pack.

### P1 — lock in docs before build; block merge if still open

5. **One-job drift: operate/troubleshoot vs screenshot grade vs paid router.** `VISION.md` “The job” is admin operate/troubleshoot (sandboxes, Flow faults, limits). Commit `00f0031` locks paid attach as OutboundSync hygiene/audit via the plugin. `bluechip-v1` then made the letter grade the hero (`README`, `docs/DEMO.md` “My org got a B”). Those are three products. Waterfall/router is **not** Bluechip’s job; it is a remediation hand-off. **Fix (docs):** split surfaces in `VISION.md`:
   - Cockpit (MIT): pin, chrome, limits, Flow, changes, context pack.
   - Hygiene *probe* (MIT, read-only): per-object measured/unknown cards — **no A–F** until the attach exists.
   - Paid attach (OutboundSync SKU): router/enrichment remediation. Bluechip may deep-link; it must not pretend the local `COUNT(field)` grade *is* the product.

6. **Revenue-wedge / MIT brand collision is already live on v1.** `VISION.md` (revenue wedge, `00f0031`): “do not confuse the MIT cockpit with the paid hygiene/router story.” v1 help, doctor, and DEMO print “Remediate at scale → OutboundSync” after a local heuristic score, while README still lists “OutboundSync data-hygiene attach” as **deferred to v2**. MIT repo is selling the wedge with a stub URL (`BLUECHIP_REMEDIATE_URL`). **Fix (docs):** wedge line is allowed only when a real attach contract exists (org id, measured findings, auth). Until then: no grade-as-marketing, no “remediate” CTA in the MIT CLI.

7. **Confirm-before-write is a slogan, not a gate.** `VISION.md` pillar 6 / table: “Writes only after explicit confirm.” No definition of confirm (keypress vs type org alias vs type `PROD`), no `--yes` ban, no rule that MCP/agent tools are display-only, no sandbox-first enforcement, no “context pack is not write authority.” v1 is read-only today (good) but `install.sh` already writes `~/.config/waybar/style.css` and `bluechip-switch` pins any alias with one click — including prod — with no confirm. **Fix (docs, before any write feature):** confirm matrix in `VISION.md` (see [Confirm gate](#confirm-gate-loopholes)). Agent must not gain a write tool that bypasses it.

8. **Hosted-MCP honesty is good; v1 overclaims Tooling.** `VISION.md` access stack is correct: REST + **Tooling** + **Metadata**; hosted SObject MCP is the Brutus ceiling. v1 README says “REST + Tooling + Limits” while `bin/bluechip` uses `sf org display`, `sf org list limits`, and `sf data query` only. No Tooling retrieve, no Metadata, no NC/EC inspector, no Apex-defined type explorer, no TraceFlag. **Fix (docs):** list v1-capable APIs vs vision-required APIs as a table; do not say Tooling until a Tooling call exists.

9. **Secret / cache story is incomplete even for the no-Connected-App path.** Vision hard rule (Enricherino): never store Consumer Secret in settings UI; `0600` under `~/.config`. v1 avoids a Connected App (good) and strips `accessToken` / `clientSecret` / `clientId` from the snapshot (good). Gaps: `STATE_DIR` is `chmod 700` only on `pin`; `cache/snapshot.json`, `cache/hygiene.json`, `cache/watch.json` are created with umask defaults; hygiene config is unsanitized SOQL (`filter` / `fields` interpolated). **Fix (docs + future build):** chmod 700/600 on first write; treat `hygiene.json` as code (allowlist object/field API names via describe); never a settings paste box; never print Consumer Secret. Connected App (when Harris asks Brutus) still needs the 0600 file story written before unpark.

10. **Hygiene attach has no data model.** `00f0031` says “built as OutboundSync product, made available through this plugin” and “router / enrichment path.” Nothing specifies: finding schema, per-org vs per-connection identity, which user/perm runs the audit, how FLS-unknown findings travel, whether Bluechip sends record Ids (PII) or aggregates, how sandbox findings are blocked from prod remediation. **Fix (docs):** a one-page attach contract in `VISION.md` (or a parked `docs/HYGIENE-ATTACH.md`) before any scoring UI. Without it the wedge is a link.

### P2 — sharpen before Fabius starts the shell

11. **Scope still sprawls across six pillars + seller later.** Non-goals correctly kill Setup IDE, Trailhead, empty-org wizards, seller chrome as headline (`8718666`, `00f0031`). The remaining mountain is still “best place on earth to operate an org” across sandboxes, Flow, limits, jobs, sharing, DX drift, *and* hygiene. **Fix:** pick a first beachhead (recommend: org pin + hard chrome + limits + Flow faults + context pack). Hygiene attach is the invoice line, not the first QML surface.

12. **Sandbox/prod mitigations are stronger in prose than in a switcher.** Vision: “hard visual + confirm.” v1 CSS does make SANDBOX loud and PROD quiet (good). Pin/switch has no confirm, no org-id checksum in the bar, no “you are about to pin PROD.” Scratch vs partial vs full is unnamed in the chip. **Fix:** pin confirm when target is prod or when switching across `IsSandbox` boundaries; show 18-char org id in chrome.

13. **Integration-user FLS house rule is not applied to hygiene config.** `VISION.md` pain #5: probe org before inventing menus; missing field Edit ≠ invent field. v1 `hygiene_config()` invents Lead/Contact/Account/Opportunity field lists with no describe. One FLS-hidden or absent field fails the whole `COUNT(Id), COUNT(f0), …` query (`INVALID_FIELD`) and marks the **object** unavailable. **Fix:** describe → intersect configured fields → per-field `unknown`; never invent custom fields.

14. **Unbounded ingest (v1-only).** Dupe query is `GROUP BY <field> HAVING COUNT(Id) > 1` with **no LIMIT**. Salesforce will return up to the query row cap; jq slurps it. Account-by-Name as default `dupeField` is a known explosion. Bar path is better (aggregates only). **Fix:** cap groups (e.g. 200), report `truncated:true`, never dump group keys into Waybar.

15. **PlainText / model-entry neutralization is unnamed.** Vision does not say that Flow fault messages, Setup Audit Trail `Display`, ApexLog, and hygiene labels are untrusted text. v1 interpolates them into the context pack and Waybar tooltip. **Fix:** neutralize at model entry (strip ANSI/control chars; cap length); document it.

16. **No tests, no error shape, no dry-run.** v1 has zero tests. Failed `sf` calls discard stderr (`2>/dev/null`). Offline / missing `sf` is a string die or a bar `unknown` — not a structured error a harness can assert. **Fix:** see [API / error shape](#api--error-shape-requirements-when-scoring-lands).

---

## Harris test: unavailable object / FLS scoring robustness

> Robustness check — that the scoring handles objects that are unavailable (e.g., a custom field the running user can't query) without choking, since we claim graceful degradation.

### Current evidence

| Layer | What exists | Claim of graceful degradation? |
| --- | --- | --- |
| `main` @ `00f0031` | **No scoring code.** Repo is `README.md`, `PARKED.md`, `VISION.md`, `LICENSE`, `.gitignore`. | **No.** `VISION.md` never says “graceful degradation,” never defines a score, never defines FLS behavior for an audit. Closest text: paid “data hygiene / data audit” (`00f0031`); FLS pain as *integration-user probe-before-invent* (pain #5, `09b6a0e`); “If a query is denied…” does **not** appear on `main`. |
| `origin/bluechip-v1` @ `a7441b4` | `hygiene_object` / `compute_hygiene` / `cmd_hygiene` in `bin/bluechip` (~lines 418–610). | **Implied, not specified.** Comment: “if the object/fields aren't queryable it returns `{available:false}`.” README Limitations: “If a query is denied, Bluechip says so rather than guessing.” Object-level UI: `n/a (no query access)`. |

**Pass / fail / NA**

- **`main` (product of record): NA.** There is no scorer to choke or degrade. The *claim* of graceful degradation **must not be made** until the tests below exist and pass.
- **`bluechip-v1` (if anyone treats it as the scorer): FAIL.** It does not meet the robustness check as stated.

### Why v1 fails the check (falsifiable)

The Harris example is “a custom field the running user can't query.” v1 puts configured fields in one SOQL:

`SELECT COUNT(Id) total, COUNT(Email) f0, COUNT(Custom__c) f1 FROM Lead …`

Salesforce rejects the **entire** query on `INVALID_FIELD` / FLS. `hygiene_object` then:

```text
comp="$(sf_json data query … SELECT $sel FROM $api$where")" \
  || { jq -nc … '{api,label,available:false}'; return 0; }
```

Consequences:

1. **Does not choke the process** (worker returns JSON; `compute_hygiene` continues). Narrow “no crash” bar: pass.
2. **Does not degrade by field.** One hidden custom field takes the whole object out of the average. Remaining objects’ scores become the org grade → **false PASS** at org level.
3. **Sister queries fail closed-as-clean.** Freshness / ownership / dupes / overdue use `2>/dev/null` and empty → `null` → scored as 0 issues → **100**. That is guessing, contradicting v1 README.
4. **All-denied org → F, and last-good is overwritten.** Silent red.
5. **No error shape.** `available:false` has no `reason` (`object_forbidden` \| `field_forbidden` \| `invalid_field` \| `transient` \| `timeout`). UI always says “no query access.”
6. **No tests.** The robustness check cannot be re-run in CI.

Vision is **underspecified** for these modes. `VISION.md` tells the integration-user FLS story and asks for a Perm Set / FLS matrix *feature*; it does not say what an audit score does when Describe or SOQL returns denied/unknown. That gap is why a first-week scorer invented `{available:false}` + `//0` and called it a grade.

### Required acceptance tests (must pass before any “graceful degradation” claim)

Treat these as the contract. Implementation language is unconstrained (bash now, QML later). Harness may stub `sf` JSON.

| ID | Setup | Expect |
| --- | --- | --- |
| **H1** Object FLS / not queryable | Completeness query returns `INVALID_TYPE` or object-level insufficient access for `Lead`. Other objects succeed. | Process exit 0. `objects[Lead].availability = "unknown"`, `reason = "object_forbidden"`. Lead **omitted from `overall`**. Other objects scored. CLI shows Lead as unknown/stub, panel still listed. `--json` includes the reason. |
| **H2** Field FLS (Harris case) | `Lead.Custom__c` in config; user cannot query it; `Email` is queryable. Completeness query would `INVALID_FIELD` if Custom__c is included. | Describe (or equivalent) **before** COUNT. `fields.Custom__c.availability = "unknown"`, `reason = "field_forbidden"`. Completeness computed **only** on measured fields. Object stays `available`. Overall not inflated by dropping Lead. |
| **H3** Field does not exist | Config names `NoSuch__c`. | Same as H2 with `reason = "invalid_field"`. Do not invent the field; do not mark the object dead. |
| **H4** Dimension probe unknown | `LastActivityDate` query fails; completeness succeeds. | `freshness = null`, `freshnessAvailability = "unknown"`. **Do not** score freshness as 100. Overall is `unknown` *or* explicitly `partial` with `measuredDimensions` listed — never a clean A from missing data. |
| **H5** Transient 502 / `sf` fail mid-scan | Completeness 502 after a previous successful `hygiene.json`. | Do not overwrite last-good. CLI structured error `transient`. Doctor keeps last-good grade + `stale`/`error` flag. Bar/chip does not go blank. |
| **H6** All objects unknown | Every object forbidden. | Overall `availability = "unknown"`, **no letter grade**, no F. Chip/doctor amber-unknown, not red. |
| **H7** Empty object | Object queryable, `COUNT(Id) = 0`. | `availability = "ok"`, `total = 0`, excluded from average; UI “no records” — not 100 and not F. |
| **H8** Integration user ≠ sysadmin | Run H1–H4 as a user with object Read but missing field Read (the SavvyCal trap). | Same results. Missing **Edit** is irrelevant for a read-only scan (do not invent write menus). |
| **H9** No crash / structured error | `sf` missing; no auth; offline. | Exit non-zero *or* exit 0 with `--json` `{ok:false, error:{code,message}}`. No stack/unbound `set -e` hole. Bar still emits valid JSON (`unknown` class). |
| **H10** Caps | Dupe groups > cap. | `dupes.truncated = true`, groups ≤ cap; Waybar payload stays bounded. |

Until H1–H10 exist, copy in README/DEMO/VISION **must not** say: graceful degradation, “says so rather than guessing,” or publish an A–F as org truth.

### API / error shape requirements (when scoring lands)

Minimum `--json` (names can change; semantics cannot):

```json
{
  "ok": true,
  "availability": "ok | partial | unknown",
  "overall": { "score": 81, "grade": "B", "availability": "partial" },
  "measuredAt": 0,
  "org": { "id": "00D…", "sandboxState": "prod | sandbox | unknown" },
  "objects": [
    {
      "api": "Lead",
      "availability": "ok | unknown",
      "reason": null,
      "total": 1200,
      "dimensions": {
        "completeness": { "availability": "partial", "score": 74, "fields": {
          "Email": { "availability": "ok", "filled": 900 },
          "Custom__c": { "availability": "unknown", "reason": "field_forbidden" }
        }},
        "freshness": { "availability": "unknown", "reason": "field_forbidden", "score": null }
      }
    }
  ],
  "error": null
}
```

Rules:

- `score` is number **only** when `availability` is `ok` or `partial` with ≥1 measured dimension; otherwise `null`.
- `reason` is a closed enum, not a raw Salesforce paragraph (raw message may be attached, length-capped, neutralized).
- Failed lookup keeps last-good on disk; new file is written only on `ok` or `partial` with `measuredAt`.
- Offline / missing CLI: `ok: false`, `error.code` ∈ {`sf_missing`,`unauthenticated`,`transient`,`config_invalid`}.

---

## Janus checklist (adapted)

### Is the one-job clear, or is scope sprawling?

**Partly clear, already sprawling.** Locked ICP (`8718666`): System Admin on Omarchy, not seller-first. Job sentence is operate/troubleshoot. Six pillars + agent desk + later seller chip + (`00f0031`) paid hygiene attach is a portfolio. v1 then added “fastfetch for your org” / letter-grade virality. Getting-started / empty-org is correctly banned. **Action:** freeze beachhead (finding 11); keep seller chip in non-goals until the cockpit is real.

### Is waterfall/router confused with Bluechip’s job?

**On paper, almost. On v1, yes.** `00f0031` says remediation *can route through* OutboundSync (router/enrichment) when they want to act at scale. That is attach, not cockpit. v1 implements none of the router and still prints the CTA. Do not invent Clay/OpenRouter sprawl; do not put SEP waterfall language in the MIT plugin. **Action:** finding 5–6, 10.

### Revenue wedge vs MIT cockpit — brand/confusion risks?

**High.** MIT name “Bluechip,” unofficial Salesforce disclaimer, and OutboundSync paid hygiene in the same README is the collision `VISION.md` warned about. v1 DEMO copy (“What does yours get?”) makes the MIT CLI the SKU. Risk: customers think the grade *is* OutboundSync; or that OutboundSync is a free bash script. **Action:** finding 6. Shipping furniture under an OutboundSync-facing label later is fine; do not lead with it from the MIT binary.

### Sandbox/prod mix-up mitigations strong enough on paper?

**Prose: yes. Contract: no.** Vision + Brutus pain #3 are the right fear. Required chrome is named (bar badge impossible to miss; confirm). Missing: confirm policy, unknown-org state as a *vision* requirement (only appeared in v1 bar/doctor), context-pack rule, pin-across-boundary rule. v1 `context` PROD default fails the paper test. **Action:** findings 4, 12. Add to `VISION.md` now: `sandboxState` is three-valued everywhere.

### Secret handling / Connected App story adequate before unpark?

**Adequate to stay parked; not adequate to unpark a Connected App.** Vision names Connected App / ECA, Admin-approved, sysadmin-only, 0600, no secret in settings UI. Brutus still owns org policy. v1’s “no Connected App, ride `sf`” is a valid *temporary* dodge and must not be sold as the security end-state (user’s `sf` session is often broader than a least-privilege ECA). Cache file modes and SOQL-from-config are open. **Action:** finding 9. Do not create the app in this review’s scope.

### Agent “confirm before write” — loopholes?

**Yes, on paper.** See finding 7 and the matrix below. Biggest loophole: `bluechip context` is designed to be pasted into Cursor/Claude, which *can* write via other MCP/sf plugins with no Bluechip confirm. Bluechip would have blessed the incident without gating the deploy.

### Park rule still honored, or soft-unpark language creeping in?

**Honored on `main`. Broken on `bluechip-v1`.** `PARKED.md` / README / VISION park rule on `main` is clean: no QML, no Connected App, unpark only when Harris says. `240e208` rewrote parked → v1 and called it shipped. This review must not merge that. **Action:** finding 2.

### What’s missing for the hygiene/scoring attach to be real?

Data model, identity, perms, degradation, and tests — findings 1, 10, Harris test. Also: which OutboundSync API the plugin calls; whether the running user is the admin or the integration user (FLS trap); sandbox findings must not remediate prod.

---

## Confirm gate (loopholes to close in VISION before any write)

Write this table into `VISION.md` (docs-only is enough while parked):

| Write | Allowed on | Confirm UX | Forbidden |
| --- | --- | --- | --- |
| Pin org | any | none if same `IsSandbox`; confirm if crossing sandbox↔prod | Agent pin without UI |
| TraceFlag / log | sandbox default; prod extra | type alias | `--yes`, MCP write tool |
| FLS / perm change | sandbox first | type alias + field list | “missing Edit → invent field” |
| Flow activate / metadata deploy | sandbox first | type `PROD` + alias on prod | Agent deploy from context pack |
| User freeze | prod allowed, extra | type username | batch freeze from agent |

Hard rules: no hidden `--force`; no confirm remembered across orgs; context pack is **not** authorization; hosted SObject MCP writes (if any) are out of scope until Tooling/Metadata exists.

---

## What is actually locked vs aspirational

| Claim | Source | Status |
| --- | --- | --- |
| Admin-first, not seller-first | `8718666`, `README.md`, `PARKED.md` | **Locked** |
| Unofficial / no SF marks | `a2391fd` → present | **Locked** |
| Org already exists; no getting-started chrome | `00f0031` | **Locked** |
| Paid attach = OutboundSync hygiene/audit, not sandbox SKU | `00f0031` | **Locked as intent**; attach unspec’d |
| MIT cockpit ≠ paid router story | `00f0031` | **Locked**; v1 already violates in copy |
| Hosted SObject MCP is not enough | `09b6a0e`, access stack | **Locked and honest** |
| Confirm-gated writes | `VISION.md` pillars | **Aspirational** |
| Six pillars + agent desk | `8718666` | **Aspirational mountain** |
| v1 CLI / Waybar / hygiene grade | `240e208`…`a7441b4` | **Side branch only; not parked truth** |
| Graceful degradation of scores | — | **Not a valid claim** |

---

## Fabius / Quattro lenses vs evidence

| Lens | `main` vision | `bluechip-v1` |
| --- | --- | --- |
| Unknown ≠ fail; amber/unknown; never silent red / false PASS | Unstated for scores; limits “amber before red” assumes a reading | **Fail** — 0% limits, 100 hygiene dims, F on all-unknown |
| Keep last-good on failed lookup; don’t wipe on 502 | Unstated | Snapshot: keep last-good if rebuild fails (**pass**). Hygiene cache: overwrite with F (**fail**). Bar: always JSON (**pass**). |
| Surface load errors; stub + keep expand | Unstated (no panels) | Flows: best-effort message (**partial**). Hygiene: “n/a (no query access)” with no reason (**partial**). stderr discarded. |
| Offline / dry-run structured errors | Unstated | String `die` / bar tooltip; no schema |
| Secrets 0600; never settings paste; never show Consumer Secret | Named | No paste box (**pass**). Cache files not 0600 (**fail**). Token stripped (**pass**). |
| PlainText neutralize at model entry | Unstated | Raw Trail/Flow/Apex into context + tooltip |
| Bound caches; ingest caps; no unbounded SOQL in the bar | Unstated | Bar uses aggregates (**pass**). Hygiene GROUP BY uncapped (**fail**). |
| Confirm-gated writes only | Named, unspecified | v1 Salesforce writes: none (**pass**). Pin/dotfiles: ungated. |
| Probe org before inventing menus; missing Edit ≠ invent field | Named (pain #5) | Hygiene invents field lists (**fail**) |
| Hosted SObject MCP ≠ enough | Named | Avoided MCP; also never reached Tooling/Metadata |

---

## Doc / future-build actions (Harris can tick)

**Docs, still parked (this repo, no QML):**

1. Keep `main` parked. Optionally one sentence in `PARKED.md`: `bluechip-v1` is an experiment, not unpark.
2. Add to `VISION.md`: three-state `sandboxState`; confirm matrix; hygiene attach contract stub; **no letter grade** until H1–H10.
3. Do **not** add “graceful degradation” language until tests exist.
4. Do not merge `bluechip-v1` as a drive-by unpark.

**When Harris unparks (Fabius / Brutus):**

1. Implement H1–H10 before any `hygiene` UX or DEMO copy.
2. One `sandboxState` helper for bar, doctor, context, watch, attach.
3. Describe-first field lists; per-field unknown; bound GROUP BY.
4. 700/600 on first write; no secret widgets.
5. Attach talks to OutboundSync with a real finding schema — or the CTA stays out of the MIT CLI.
6. Writes (if any) go through the confirm matrix; agents get no bypass.

---

## Out of scope (honored)

QML scaffold, marketplace listing, Connected App creation, Fabius personal plugins, Salesforce org changes, Clay canvas, OpenRouter, house-model shopping.

---

## Evidence index

| Ref | What |
| --- | --- |
| `a2391fd` | Initial park: seller bar lookup / meeting / tasks |
| `8718666` | Admin-first pivot; `VISION.md` born; seller demoted |
| `09b6a0e` | Brutus pain → Omarchy capability table; MCP ceiling; FLS house rule |
| `00f0031` | Revenue wedge: existing org; OutboundSync hygiene attach; not sandbox SKU |
| `main` files | `README.md`, `PARKED.md`, `VISION.md` — vision only |
| `240e208` | v1 unpark on `bluechip-v1`: `bin/bluechip`, Waybar, `install.sh` |
| `a7c3b31` | FlowInterview statuses Error/Paused; bar pct bind |
| `a7441b4` | Hygiene scorer + bar/doctor UNKNOWN; **context pack not updated** |
| v1 hygiene | `origin/bluechip-v1:bin/bluechip` `hygiene_config`, `hygiene_object`, `_hygiene_score_jq`, `compute_hygiene`, `cmd_hygiene` |
| v1 claims | `origin/bluechip-v1:README.md` Limitations; `docs/DEMO.md` grade copy; `PARKED.md` “v1 shipped” |
