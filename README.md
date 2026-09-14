# Bluechip

![status: parked](https://img.shields.io/badge/status-parked-lightgrey)

Salesforce context in the Omarchy bar — lookup, meeting prep, My Tasks. Unofficial.

**ID (planned):** `outboundsync.bluechip`  
**Author:** OutboundSync / Harris Kenny  
**License:** MIT  
**Status:** Parked 2026-09-14 — shape only, not shipping yet.

## What it is

A native Omarchy Quattro `bar-widget` (not Electron) so a seller living on Omarchy can get CRM truth without replacing Gmail or Google Calendar:

1. **Lookup** — paste email / name → Salesforce Contact, Lead, or Account card  
2. **Meeting context** — next Google Calendar event → matching Account / Opp (join on attendee email)  
3. **My Tasks** — Salesforce Tasks due today  

Optional later: thin wiring to Salesforce API / MCP and seller skills. **Not** an agent competitor to Cursor or Grok Bot — shell furniture + packaging.

## What it is not

- A second inbox or second calendar  
- Full Salesforce → Linux contact sync  
- Affiliated with, endorsed by, or sponsored by Salesforce, Inc.

## Unofficial disclaimer

**Bluechip is unofficial.** It is **not** affiliated with, endorsed by, or sponsored by Salesforce, Inc. or any related entity. This project does **not** ship Salesforce logos, wordmarks, or brand assets. “Salesforce” and related marks belong to their owners. Use referential only: works with Salesforce®.

Named **Bluechip** — bar chip, blue-chip accounts, a nod to Salesforce blue — without using their marks in the product title.

## Why OutboundSync

OutboundSync already lives next to Salesforce every day (SEP → CRM). Bluechip is the desktop face for founders and sellers on Omarchy who want the same CRM in the shell. HubSpot-shaped siblings can come later under a different name.

## Parked next steps (when we pick it back up)

- [ ] Omarchy plugin scaffold (`manifest.json`, `BarWidget.qml`, `Panel.qml`)  
- [ ] Connected App / OAuth secrets under `~/.config/bluechip/` (Enricherino pattern)  
- [ ] Lookup v1 against Salesforce REST  
- [ ] Meeting-context join from local calendar identity  
- [ ] Marketplace listing + preview  

## Install (not ready)

```bash
# later: omarchy plugin add <git-url>
```

---

OutboundSync · MIT · parked on purpose
