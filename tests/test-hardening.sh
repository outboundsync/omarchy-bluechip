#!/usr/bin/env bash
# HANCORE-style hardening: caps, exclusive cache writes, no secret persist, cheap bar.
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-hard.XXXXXX")"
FIX="$WORKDIR/fix"
trap 'rm -rf "$WORKDIR"' EXIT

command -v jq >/dev/null || { echo "jq required"; exit 1; }
python3 "$ROOT/tests/wave1/gen_fixtures.py" "$FIX"
chmod +x "$STUB" "$BLUECHIP"

ok() { PASS=$((PASS + 1)); printf '  ✓ %s\n' "$1"; }
bad() { FAIL=$((FAIL + 1)); printf '  ✗ %s\n' "$1"; }

assert_eq() {
  local got="$1" want="$2" msg="$3"
  if [[ "$got" == "$want" ]]; then ok "$msg"
  else bad "$msg (got '$got' want '$want')"; fi
}

run() {
  local fixture="$1"
  shift
  local home="${HOME_OVERRIDE:-$WORKDIR/h-$$-$RANDOM}"
  mkdir -p "$home/.config/bluechip/cache"
  chmod 700 "$home/.config/bluechip"
  HOME="$home" XDG_CONFIG_HOME="$home/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$fixture" \
    "$@"
}

printf '%s\n' "SELECT allowlist — never ParameterValue / ConsumerSecret"
if grep -R -n --include='*.inc' --include='bluechip' -E '"SELECT[^"]*ParameterValue' "$ROOT/bin" >/dev/null; then
  bad "ParameterValue appears in a SELECT"
else
  ok "no ParameterValue in SELECT"
fi
if grep -R -n --include='*.inc' --include='bluechip' -E '"SELECT[^"]*(ConsumerSecret|clientSecret|Password|accessToken)' "$ROOT/bin" >/dev/null; then
  bad "secret column appears in a SELECT"
else
  ok "no secret-shaped columns in SELECT"
fi

printf '%s\n' "snapshot cache never stores accessToken"
HOME1="$WORKDIR/snap-home"
SNAP_OUT="$(HOME_OVERRIDE="$HOME1" run "$FIX/ok" "$BLUECHIP" --no-color --refresh --json pin 2>/dev/null)" || true
CACHE="$HOME1/.config/bluechip/orgs/00D000000000002AAA/snapshot.json"
if [[ ! -f "$CACHE" ]]; then
  CACHE="$HOME1/.config/bluechip/cache/snapshot.json"
fi
if [[ -f "$CACHE" ]]; then
  if grep -q accessToken "$CACHE" || grep -q SHOULD_NEVER_LEAK "$CACHE"; then
    bad "snapshot cache leaked accessToken"
  else
    ok "snapshot cache has no accessToken"
  fi
  mode="$(stat -c %a "$CACHE" 2>/dev/null || stat -f %OLp "$CACHE")"
  assert_eq "$mode" "600" "snapshot cache mode 600"
  dmode="$(stat -c %a "$HOME1/.config/bluechip" 2>/dev/null || stat -f %OLp "$HOME1/.config/bluechip")"
  assert_eq "$dmode" "700" "state dir mode 700"
else
  bad "snapshot cache missing: $SNAP_OUT"
fi

printf '%s\n' "write_secure refuses symlink dest"
HOME2="$WORKDIR/sym-home"
OID="00D000000000002AAA"
mkdir -p "$HOME2/.config/bluechip/cache" "$HOME2/.config/bluechip/orgs/$OID" "$WORKDIR/evil"
chmod 700 "$HOME2/.config/bluechip" "$HOME2/.config/bluechip/orgs/$OID"
printf 'TARGET\n' > "$WORKDIR/evil/target"
ln -s "$WORKDIR/evil/target" "$HOME2/.config/bluechip/orgs/$OID/snapshot.json"
HOME="$HOME2" XDG_CONFIG_HOME="$HOME2/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/ok" \
  "$BLUECHIP" --no-color --refresh doctor >/dev/null 2>"$WORKDIR/sym.err" || true
if grep -q TARGET "$WORKDIR/evil/target" && ! grep -q SHOULD_NEVER_LEAK "$WORKDIR/evil/target"; then
  ok "symlink target not overwritten with snapshot"
else
  bad "wrote through symlink"
fi
grep -q 'symlink' "$WORKDIR/sym.err" && ok "stderr mentions symlink refuse" || bad "missing symlink refuse: $(cat "$WORKDIR/sym.err")"

printf '%s\n' "sf stdout cap"
HUGE="$WORKDIR/huge"
mkdir -p "$HUGE"
cp -a "$FIX/ok/." "$HUGE/"
python3 - "$HUGE" <<'PY'
import json, sys
from pathlib import Path
d = Path(sys.argv[1])
pad = "A" * 1_200_000
blob = {"status": 0, "result": {"id": "00D000000000001AAA", "accessToken": "SHOULD_NEVER_LEAK", "pad": pad}}
(d / "org-display.json").write_text(json.dumps(blob))
PY
HOME_H="$WORKDIR/huge-home"
mkdir -p "$HOME_H/.config/bluechip/cache"
chmod 700 "$HOME_H/.config/bluechip"
HUGE_ERR="$WORKDIR/huge.err"
HOME="$HOME_H" XDG_CONFIG_HOME="$HOME_H/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$HUGE" BLUECHIP_SF_MAX_BYTES=65536 \
  "$BLUECHIP" --no-color --refresh bar >/dev/null 2>"$HUGE_ERR" || true
if grep -q 'AAAAAAAA' "$HOME_H/.config/bluechip/orgs/00D000000000001AAA/snapshot.json" 2>/dev/null \
   || grep -q 'AAAAAAAA' "$HOME_H/.config/bluechip/cache/snapshot.json" 2>/dev/null; then
  bad "oversize sf output was cached"
else
  ok "oversize sf output not cached"
fi

printf '%s\n' "clipboard refuses url / flag-shaped ingest"
CLIP_URL="$(run "$FIX/ok" "$BLUECHIP" --no-color --refresh --json clipboard --text 'https://evil.example/x' 2>/dev/null)" || true
echo "$CLIP_URL" | jq -e '.ok == false' >/dev/null && ok "clipboard url refused" || bad "clipboard url: $CLIP_URL"
CLIP_FLAG="$(run "$FIX/ok" "$BLUECHIP" --no-color --refresh --json clipboard --text '-o prod' 2>/dev/null)" || true
echo "$CLIP_FLAG" | jq -e '.ok == false' >/dev/null && ok "clipboard flag-shaped refused" || bad "clipboard flag: $CLIP_FLAG"
CLIP_NL="$(run "$FIX/ok" "$BLUECHIP" --no-color --refresh --json clipboard --text $'SavvyCal\nWebhook' 2>/dev/null)" || true
echo "$CLIP_NL" | jq -e '.ok == false' >/dev/null && ok "clipboard newline refused" || bad "clipboard newline: $CLIP_NL"

printf '%s\n' "bar uses cache TTL — no extra org display on second tick"
HOME_B="$WORKDIR/bar-home"
mkdir -p "$HOME_B/.config/bluechip/cache"
chmod 700 "$HOME_B/.config/bluechip"
TRACE1="$WORKDIR/trace1"
TRACE2="$WORKDIR/trace2"
HOME="$HOME_B" XDG_CONFIG_HOME="$HOME_B/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/ok" BLUECHIP_SF_TRACE="$TRACE1" \
  "$BLUECHIP" --no-color --refresh bar >/dev/null 2>&1 || true
HOME="$HOME_B" XDG_CONFIG_HOME="$HOME_B/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/ok" BLUECHIP_SF_TRACE="$TRACE2" \
  "$BLUECHIP" --no-color bar >/dev/null 2>&1 || true
c1="$(grep -c 'org display' "$TRACE1" 2>/dev/null || echo 0)"
c2="$(grep -c 'org display' "$TRACE2" 2>/dev/null || echo 0)"
[[ "$c1" -ge 1 ]] && ok "first bar --refresh called org display" || bad "first bar missed org display ($c1)"
assert_eq "$c2" "0" "cached bar tick skipped org display"

printf '%s\n' "cwd sf impostor does not win when BLUECHIP_SF is absolute"
CWD="$WORKDIR/cwd"
mkdir -p "$CWD"
printf '#!/bin/sh\necho IMPOSTOR\n' > "$CWD/sf"
chmod +x "$CWD/sf"
HOME_C="$WORKDIR/cwd-home"
mkdir -p "$HOME_C/.config/bluechip/cache"
chmod 700 "$HOME_C/.config/bluechip"
(
  cd "$CWD"
  HOME="$HOME_C" XDG_CONFIG_HOME="$HOME_C/.config" PATH="$CWD:/usr/bin:/bin" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/ok" \
    "$BLUECHIP" --no-color --refresh --json pin >/dev/null 2>&1
)
if grep -q IMPOSTOR "$HOME_C/.config/bluechip/orgs/00D000000000002AAA/snapshot.json" 2>/dev/null \
   || grep -q IMPOSTOR "$HOME_C/.config/bluechip/cache/snapshot.json" 2>/dev/null; then
  bad "cwd sf impostor was used"
else
  ok "absolute BLUECHIP_SF beat cwd impostor"
fi

printf '%s\n' "watch --once accepts interval 0 (clamped, no busy-loop)"
WATCH="$(run "$FIX/ok" "$BLUECHIP" --no-color --refresh watch --once --jsonl 0 2>/dev/null)" || true
echo "$WATCH" | jq -e '.event == "heartbeat"' >/dev/null && ok "watch --once 0 still one heartbeat" || bad "watch 0: $WATCH"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
