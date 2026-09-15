# Active build tip — Harris authorized ship 2026-09-15

This `bluechip-v1` branch is the **active build tip** for the MIT Salesforce
admin cockpit on Omarchy. Harris authorized **ship everything** (Wave 1 first).

**`main` may still hold the parked vision** until a later promote. Do not
silently rewrite `main` from this branch — open a separate docs PR into `main`
when it is time to promote the story.

Wave 2 on this tip: FLS / perm-set matrix (read + proposal-only), Event Log
offenders, multi-org desk, clipboard → context. UX pass (same tip): compact
chip, incident object, watch deltas, scratchpad — [docs/UX-PASS.md](docs/UX-PASS.md).
TraceFlag remains the only confirm-gated org write. Ranked remainder:
[docs/SHIP-BACKLOG.md](docs/SHIP-BACKLOG.md). Agent contract (coding + runtime):
[AGENTS.md](AGENTS.md). Recipes: [docs/AGENT-PLAYBOOK.md](docs/AGENT-PLAYBOOK.md).

Connected App / QML / marketplace listing remain parked until a later wave
needs them. Wave 1 rides `sf` CLI + Tooling-via-`sf` — no Connected App.

The local `bluechip hygiene` probe is a **read-only MIT experiment**. It is not the
OutboundSync product. Letter grades appear only when H1–H10 measured-data rules
pass; an all-unknown scan has **no grade** and does **not** overwrite last-good
cache.

---

# Parked 2026-09-14 (still the rule on `main` until promote)

**Name:** Bluechip (Harris + Agrippa).
**Repo:** `outboundsync/omarchy-bluechip` (not kenhara).
**ICP pivot:** Salesforce **System Admin** on Omarchy (not seller-first).

Vision: [VISION.md](./VISION.md)

Do not scaffold QML, Connected App, or marketplace listing until a wave requires
them. Fabius = Omarchy shell craft. Brutus = org/Connected App policy if needed.
