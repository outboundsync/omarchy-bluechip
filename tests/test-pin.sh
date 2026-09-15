#!/usr/bin/env bash
# Pin boundary confirm tests (prod↔sandbox, unknown org id gate).
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
FIX="$ROOT/tests/pin/fixtures"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-pin.XXXXXX")"
trap 'rm -rf "$WORKDIR"' EXIT

chmod +x "$STUB" "$BLUECHIP" "$ROOT/bin/bluechip-hygiene.inc" 2>/dev/null || true

ok() { PASS=$((PASS + 1)); printf '  ✓ %s\n' "$1"; }
bad() { FAIL=$((FAIL + 1)); printf '  ✗ %s\n' "$1"; }

assert_pin_fails() {
  local msg="$1"
  shift
  if "$@" >/dev/null 2>&1; then bad "$msg (expected failure)"
  else ok "$msg"; fi
}

assert_pin_ok() {
  local msg="$1"
  shift
  if "$@" >/dev/null 2>&1; then ok "$msg"
  else bad "$msg (expected success)"; fi
}

setup_prod_pin() {
  local home="$WORKDIR/home-prod"
  mkdir -p "$home/.config/bluechip/cache"
  chmod 700 "$home/.config/bluechip"
  printf '{"pinnedOrg":"prod-org"}\n' > "$home/.config/bluechip/state.json"
  chmod 600 "$home/.config/bluechip/state.json"
  HOME="$home" XDG_CONFIG_HOME="$home/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/cross" \
    "$BLUECHIP" --no-color --refresh bar >/dev/null 2>&1 || true
  printf '%s' "$home"
}

printf '%s\n' "bar — org id in chrome"
BAR_HOME="$(setup_prod_pin)"
BAR="$(env HOME="$BAR_HOME" XDG_CONFIG_HOME="$BAR_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/cross" \
  "$BLUECHIP" --no-color bar 2>/dev/null)"
echo "$BAR" | jq -e '.text | test("00D0")' >/dev/null \
  && ok "bar text includes truncated org id" \
  || bad "bar missing org id: $BAR"
echo "$BAR" | jq -e '.tooltip | test("Org Id: 00D000000000001AAA")' >/dev/null \
  && ok "bar tooltip has full org id" \
  || bad "bar tooltip missing full org id"

printf '%s\n' "pin — cross prod→sandbox requires confirm"
PROD_HOME="$(setup_prod_pin)"
assert_pin_fails "refuse without confirm" \
  env HOME="$PROD_HOME" XDG_CONFIG_HOME="$PROD_HOME/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/cross" \
    "$BLUECHIP" --no-color pin sbx
assert_pin_ok "allow with SANDBOX confirm" \
  env HOME="$PROD_HOME" XDG_CONFIG_HOME="$PROD_HOME/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/cross" \
    bash -c 'printf "SANDBOX\n" | "$1" --no-color pin sbx' _ "$BLUECHIP"
PINNED="$(jq -r '.pinnedOrg' "$PROD_HOME/.config/bluechip/state.json")"
[[ "$PINNED" == "sbx" ]] && ok "pin persisted sbx" || bad "pin not persisted (got $PINNED)"

printf '%s\n' "pin — unknown sandboxState requires org id"
UNK_HOME="$WORKDIR/home-unk"
mkdir -p "$UNK_HOME/.config/bluechip/cache"
chmod 700 "$UNK_HOME/.config/bluechip"
printf '{"pinnedOrg":"prod-org"}\n' > "$UNK_HOME/.config/bluechip/state.json"
assert_pin_fails "refuse unknown without org id" \
  env HOME="$UNK_HOME" XDG_CONFIG_HOME="$UNK_HOME/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/unknown" \
    "$BLUECHIP" --no-color pin mystery
assert_pin_ok "allow unknown with matching org id" \
  env HOME="$UNK_HOME" XDG_CONFIG_HOME="$UNK_HOME/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/unknown" \
    bash -c 'printf "00D000000000003AAA\n" | "$1" --no-color pin mystery' _ "$BLUECHIP"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
