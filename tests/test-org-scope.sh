#!/usr/bin/env bash
# Org-scoped cache isolation: pin switch must not serve another org's snapshot.
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
FIX="$ROOT/tests/pin/fixtures"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-orgscope.XXXXXX")"
trap 'rm -rf "$WORKDIR"' EXIT

chmod +x "$STUB" "$BLUECHIP" "$ROOT/bin/bluechip-scratchpad"

ok() { PASS=$((PASS + 1)); printf '  ✓ %s\n' "$1"; }
bad() { FAIL=$((FAIL + 1)); printf '  ✗ %s\n' "$1"; }

PROD_ID="00D000000000001AAA"
SBX_ID="00D000000000002AAA"

printf '%s\n' "pin switch does not serve org A snapshot as org B"
HOME_X="$WORKDIR/home-x"
mkdir -p "$HOME_X/.config/bluechip/cache"
chmod 700 "$HOME_X/.config/bluechip"
printf '{"pinnedOrg":"prod-org"}\n' > "$HOME_X/.config/bluechip/state.json"
chmod 600 "$HOME_X/.config/bluechip/state.json"
HOME="$HOME_X" XDG_CONFIG_HOME="$HOME_X/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/cross" \
  "$BLUECHIP" --no-color --refresh bar >/dev/null 2>&1 || true

A_SNAP="$HOME_X/.config/bluechip/orgs/$PROD_ID/snapshot.json"
B_SNAP="$HOME_X/.config/bluechip/orgs/$SBX_ID/snapshot.json"
if [[ -f "$A_SNAP" ]]; then
  ok "prod snapshot written under orgs/${PROD_ID}"
else
  bad "prod snapshot missing (legacy=$(ls -la "$HOME_X/.config/bluechip/cache" 2>/dev/null))"
fi
AID="$(jq -r '.display.id // .org.Id // empty' "$A_SNAP" 2>/dev/null || true)"
[[ "$AID" == "$PROD_ID" ]] && ok "prod snapshot id is A" || bad "prod snapshot id '$AID'"

# Tamper A so a leak is obvious, then pin sandbox.
jq '.display.alias = "TAMPERED_ORG_A"' "$A_SNAP" > "$A_SNAP.tmp" && mv "$A_SNAP.tmp" "$A_SNAP"
chmod 600 "$A_SNAP"

HOME="$HOME_X" XDG_CONFIG_HOME="$HOME_X/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/cross" \
  bash -c 'printf "SANDBOX\n" | "$1" --no-color pin sbx' _ "$BLUECHIP" >/dev/null 2>&1 || true

if [[ -f "$B_SNAP" ]]; then
  ok "sandbox snapshot written under orgs/${SBX_ID}"
else
  bad "sandbox snapshot missing after pin switch"
fi
BID="$(jq -r '.display.id // .org.Id // empty' "$B_SNAP" 2>/dev/null || true)"
[[ "$BID" == "$SBX_ID" ]] && ok "sandbox snapshot id is B" || bad "sandbox snapshot id '$BID'"

BARB="$(HOME="$HOME_X" XDG_CONFIG_HOME="$HOME_X/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/cross" \
  "$BLUECHIP" --no-color bar 2>/dev/null)" || true
echo "$BARB" | jq -e --arg id "$SBX_ID" '.tooltip | test($id)' >/dev/null \
  && ok "bar after switch tooltips org B" \
  || bad "bar after switch missing B: $BARB"
echo "$BARB" | grep -q TAMPERED_ORG_A && bad "bar served tampered org A alias" || ok "bar did not serve org A alias"
echo "$BARB" | jq -e '.sandboxState == "sandbox"' >/dev/null \
  && ok "bar sandboxState is sandbox after switch" \
  || bad "bar sandboxState after switch: $BARB"

# Legacy flat snapshot for A must not be adopted as B.
LEGACY="$HOME_X/.config/bluechip/cache/snapshot.json"
if [[ -f "$LEGACY" ]]; then
  grep -q TAMPERED_ORG_A "$B_SNAP" && bad "org B snapshot contains A tamper" || ok "org B snapshot clean of A tamper"
else
  ok "no leftover flat snapshot required after scoped write"
fi

printf '%s\n' "org dir 0700 / snapshot 0600 after pin switch"
dmode="$(stat -c %a "$HOME_X/.config/bluechip/orgs/$SBX_ID" 2>/dev/null || echo missing)"
fmode="$(stat -c %a "$B_SNAP" 2>/dev/null || echo missing)"
[[ "$dmode" == "700" ]] && ok "org cache dir 700" || bad "org cache dir mode $dmode"
[[ "$fmode" == "600" ]] && ok "org snapshot 600" || bad "org snapshot mode $fmode"
omode="$(stat -c %a "$HOME_X/.config/bluechip/orgs" 2>/dev/null || echo missing)"
[[ "$omode" == "700" ]] && ok "orgs/ dir 700" || bad "orgs/ mode $omode"

printf '%s\n' "legacy A snapshot is not copied onto B when aliases differ"
HOME_L="$WORKDIR/home-legacy"
mkdir -p "$HOME_L/.config/bluechip/cache"
chmod 700 "$HOME_L/.config/bluechip"
printf '{"pinnedOrg":"sbx"}\n' > "$HOME_L/.config/bluechip/state.json"
chmod 600 "$HOME_L/.config/bluechip/state.json"
# Plant a flat cache that belongs to prod-org (A), not sbx (B).
cat > "$HOME_L/.config/bluechip/cache/snapshot.json" <<EOF
{"ts":1,"display":{"id":"$PROD_ID","alias":"prod-org"},"org":{"Id":"$PROD_ID","IsSandbox":false},"limits":[],"limitsAvailability":"ok","faults":{"availability":"ok","count":0,"reason":null}}
EOF
chmod 600 "$HOME_L/.config/bluechip/cache/snapshot.json"
printf '%s\n' '{"ok":true,"org":{"id":"'"$PROD_ID"'"},"overall":{"grade":"A"}}' \
  > "$HOME_L/.config/bluechip/cache/hygiene.json"
chmod 600 "$HOME_L/.config/bluechip/cache/hygiene.json"

HOME="$HOME_L" XDG_CONFIG_HOME="$HOME_L/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/cross" \
  "$BLUECHIP" --no-color --refresh bar >/dev/null 2>&1 || true
B2="$HOME_L/.config/bluechip/orgs/$SBX_ID/snapshot.json"
H2="$HOME_L/.config/bluechip/orgs/$SBX_ID/hygiene.json"
if [[ -f "$B2" ]]; then
  jq -e --arg id "$SBX_ID" '.display.id == $id or .org.Id == $id' "$B2" >/dev/null \
    && ok "refresh on sbx wrote B, not planted A" \
    || bad "sbx snapshot reused planted A"
else
  bad "sbx org-scoped snapshot missing after planted legacy A"
fi
[[ -f "$H2" ]] && bad "org B adopted org A hygiene last-good" || ok "org B did not adopt org A hygiene"

printf '%s\n' "scratchpad --boundary --dry-run (no Hyprland)"
BD="$("$ROOT/bin/bluechip-scratchpad" --boundary --dry-run)"
echo "$BD" | grep -q 'logs --follow' && ok "boundary dry-run prints logs --follow" || bad "boundary dry-run missing logs"
echo "$BD" | grep -q 'wrangler tail' && ok "boundary dry-run prints wrangler tail" || bad "boundary dry-run missing wrangler"
echo "$BD" | grep -q 'hyprctl dispatch' && ok "boundary dry-run prints hyprctl recipe" || bad "boundary dry-run missing hyprctl"
echo "$BD" | grep -q 'No QML' && ok "boundary dry-run says no QML" || bad "boundary dry-run missing no QML"
echo "$BD" | grep -q 'human canvas' && ok "boundary dry-run mentions Setup canvas" || bad "boundary dry-run missing canvas"
BIND="$("$ROOT/bin/bluechip-scratchpad" --boundary --print-bind)"
echo "$BIND" | grep -q 'bluechip-scratchpad --boundary --apply' && ok "boundary print-bind has apply bind" || bad "print-bind: $BIND"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
