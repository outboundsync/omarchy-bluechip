# Agent playbook — Bluechip desk

Recipes for **runtime / desk agents** (Cursor/Claude using Bluechip MCP or CLI).
The contract is [AGENTS.md](../AGENTS.md). This file is how, not why.

**Hard stop:** MCP is **display-only**. Context / incident / clipboard packs are
**hand-off, not write authority.** Never deploy, activate, FLS-apply, TraceFlag-create,
or pin from a paste. `--yes` is banned on prod / unknown.

Prefer MCP when wired. Otherwise the matching CLI `--json` verb.

---

## MCP tool → CLI verb

| MCP (display-only) | CLI | Notes |
| --- | --- | --- |
| `get_incident` | `bluechip incident --json` | Same builder as `doctor --json` |
| `get_context` | `bluechip context` (`--json` optional) | Markdown pack by default |
| `get_limits` | `bluechip limits --json` | Miss → unknown, not 0% |
| `list_offenders` | `bluechip offenders --json [--since 9am]` | Event Log; Flex Credits unknown |
| `list_flow_faults` | `bluechip flows --json` | Best-effort interviews |
| `get_hygiene` | `bluechip hygiene --json` | Measured / unknown; no letter until measured |
| `list_orgs` | `bluechip orgs --json` | sf-authed orgs + pin |
| `get_pin` | `bluechip pin --json` | **Read-only** — will not change the pin |
| `get_named_creds` | `bluechip named-creds --json` | Secrets never returned |
| `diagnose_callout_auth` | `bluechip callout-auth --json` | 401 classes; optional `--user` |
| `get_apex_types` | `bluechip types --json [Class…]` | IN_/OUT_2XX property paths |
| `get_callout_pack` | `bluechip callout-pack --json <Flow>` | Hand-off, not write authority |
| `run_preflight` | `bluechip preflight --json --user …` | FLS + NC principal; no Activate |
| `list_trace_flags` | `bluechip trace status --json` | No create/stop |
| `list_apex_logs` | `bluechip logs --json` | Metadata only; bodies via CLI tail |
| `get_fls` | `bluechip fls --json --user … --fields …` | Read vs Edit gaps |
| `get_desk` | `bluechip desk --json` | Per-org `sandboxState` |

**CLI-only (not MCP):** `bluechip pin <alias>` · `trace start\|stop` · `fls-propose` · `logs --follow` · `watch` · `clipboard` · `scratchpad` (`--boundary` prints the Worker + org tail pair).

Pipes: `bluechip incident \| wl-copy` · `bluechip incident --json \| jq .signal`.

Fixture-shaped samples (no live org, no secrets): [examples/incident.sample.json](examples/incident.sample.json), [examples/context.sample.md](examples/context.sample.md).

---

## Org on fire?

1. `get_incident` / `bluechip incident --json`.
2. Read `sandboxState` first (`prod` | `sandbox` | `unknown`). Never assume PROD when unknown.
3. Interpret `signal`:
   - `kind: limit` + `severity: warn|crit` → limit bleeding (`text` is peak %).
   - `kind: fault` → Flow interviews (`flowFaults.count`).
   - `kind: unknown` / `severity: unknown` → a probe missed; **not** a healthy org.
4. `availability` on the pack: `ok` | `partial` | `unknown`. Partial means some slices missed — use those slices' `error.reason`, do not fill zeros.
5. Markdown is for paste. JSON is for machines. Neither authorizes a write.

## Wrong org?

1. `list_orgs` / `get_pin` — which alias is hot, what `sandboxState` is on the pin.
2. **Do not pin via MCP.** There is no pin-write tool.
3. Human pins: `bluechip pin <alias>` (or Waybar left-click / `bluechip-switch`). Crossing prod↔sandbox is typed confirm; unknown requires the 18-char org Id.
4. After pin, re-run `get_incident`. Chrome must match the org you intend (`PROD` quiet, `SBX` loud, `?` muted).

## Limit bleeding?

1. `get_limits` — headline utilization. If `availability != ok`, say **unknown (limits API missed)** — never 0% green.
2. `list_offenders` (`--since 9am` or ISO). Event Log File when visible. Miss → unknown with reason, **never fake 0**. Flex Credits stay unknown (no stable CLI object).
3. Suggest cuts from *named* offenders only. Do not invent a Connected App as the culprit if the Event Log did not say so.

## Flow faulted?

1. `list_flow_faults` — errored/paused `FlowInterview` rows (best-effort).
2. `get_context` / incident `flowFaults.interviews` for labels + current element.
3. **Hard faults roll back** and may not persist as interviews. If `count` is 0/`none` but the human saw a fault, point at `changes` (Setup Audit Trail) and ask them to `bluechip trace start` (confirm-gated CLI) + `bluechip logs --follow`.
4. Propose a Metadata diff or next Setup click. **Human confirms.** Do not activate the Flow.

## Named Cred auth?

1. `get_named_creds` for inventory. `diagnose_callout_auth` / `bluechip callout-auth` for 401 classes.
2. Reasons you may name (only when Tooling measured them):
   - `headers_absent` — `GenerateAuthorizationHeader` off and custom header count 0 (Auth Parameter alone never sent `Authorization`).
   - `formulas_off` — headers present/needed but `AllowMergeFieldsInHeader` is false.
   - `gen_auth_on_custom` — generated Authorization **and** custom headers (conflicting pattern).
   - `principal_missing` — user (or Automated Process / DWU) has no External Credential principal access.
   - `principal_unknown` / `unknown` — probe missed. **Never fake healthy.**
3. **Never returned:** Consumer Secret, password, `ParameterValue`, Authorization values, `Metadata` blob.
4. If `availability: unknown`, say the Tooling/list probe missed — do not guess the header formula.
5. Auth repair is Brutus/human in Setup. Agents do not invent a Connected App.

## Flow callout before Activate?

1. `run_preflight` / `bluechip preflight --user <username|AutomatedProcess> [--fields Object.Field,…]`.
   Report FLS missing Read vs missing Edit separately, plus NC principal / 401-class diagnoses.
2. `get_apex_types` / `bluechip types [IN_… OUT_2XX …]` for Flow Assignment property paths. Do not invent fields.
3. `get_callout_pack` / `bluechip callout-pack <FlowApiName>` — one hand-off (Flow identity + auth slice + types + ApexLog ids). Unknown slices keep a reason.
4. **Do not Activate.** Boundary debug: tell the human `bluechip scratchpad --boundary` (prints `bluechip logs --follow` beside `wrangler tail`) and `bluechip trace start` (confirm-gated CLI). Chip stays ambient.

## FLS for an integration user?

1. `get_fls` with `user` (username or Id) and `fields` as `Object.Field,Object.Field`.
2. Report **missing Read** vs **missing Edit** separately. Unknown on SOQL miss — do not invent a Setup menu or a custom field.
3. `bluechip fls-propose --user … --fields …` is **CLI proposal only** (diff). Not an MCP tool. Apply is not shipped.
4. House rule: probe the org before inventing fields (Harris × Brutus pain #5).

## Hand off to human write

Pack must include:

- Org Id (18-char) + `sandboxState` (three-valued) + alias
- Worst signal + which probe is unknown
- Flow interview Id / NC API name / ApexLog Ids when relevant
- The exact CLI the human should run (not “the agent will do it”)

Confirm matrix ([VISION.md](../VISION.md#confirm-before-write-matrix)) — what this tip allows:

| Write | Who runs it | Gate |
| --- | --- | --- |
| Pin (cross prod↔sandbox) | Human CLI / Waybar | Type `PROD` / `SANDBOX` / alias |
| Pin (unknown) | Human CLI | Type 18-char org Id |
| TraceFlag start/stop | Human CLI only | Sandbox: `SANDBOX` or alias (`--yes` ok). Prod: type `PROD`. Unknown: org Id. `--yes` banned on prod/unknown |
| `fls-propose` | Human CLI | Diff only — no apply |
| Flow activate / FLS apply / Metadata deploy | **not shipped** | Would be sandbox-first + type `PROD`; never MCP |

Agent line to the human: “I did not write the org. Run `bluechip trace start` in a terminal and type the confirm string.”

---

## Interpreting `unknown`

| Token | Meaning | Not |
| --- | --- | --- |
| `none` | Probe ran; zero rows | A miss |
| `unknown (reason)` | Probe did not complete | 0, 100, F, or PROD |
| `availability: unknown` on hygiene overall | All objects unmeasured; last-good kept | A letter grade |
| `sandboxState: unknown` | `Organization.IsSandbox` unread | Prod |

Reason enums you will see include `sf_missing`, `unauthenticated`, `transient`, `config_invalid`, `limits_unavailable`, plus probe-specific `object_forbidden` / `field_forbidden`. Quote the reason; do not map it to a score.

---

## Out of scope (do not start from a pack)

Connected App · QML / Quattro panel · marketplace · paid OutboundSync write API · FLS apply · Flow activate · inventing SF org policy as shipped.
