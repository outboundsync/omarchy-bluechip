#!/usr/bin/env bash
# UX pass: compact bar, help --all, incident object, watch deltas, scratchpad, empty states.
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
PINFIX="$ROOT/tests/pin/fixtures"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-ux.XXXXXX")"
W1="$WORKDIR/w1"
W2="$WORKDIR/w2"
trap 'rm -rf "$WORKDIR"' EXIT

command -v jq >/dev/null || { echo "jq required"; exit 1; }
python3 "$ROOT/tests/wave1/gen_fixtures.py" "$W1"
python3 "$ROOT/tests/wave2/gen_fixtures.py" "$W2"
chmod +x "$STUB" "$BLUECHIP" "$ROOT/bin/bluechip-scratchpad"

ok() { PASS=$((PASS + 1)); printf '  ✓ %s\n' "$1"; }
bad() { FAIL=$((FAIL + 1)); printf '  ✗ %s\n' "$1"; }

assert_eq() {
  local got="$1" want="$2" msg="$3"
  if [[ "$got" == "$want" ]]; then ok "$msg"
  else bad "$msg (got '$got' want '$want')"; fi
}

assert_json() {
  local blob="$1" expr="$2" want="$3" msg="$4"
  local got
  got="$(printf '%s\n' "$blob" | jq -r "$expr" 2>/dev/null || echo "<jq-fail>")"
  assert_eq "$got" "$want" "$msg"
}

run() {
  local fixture="$1"
  shift
  local home="$WORKDIR/h-$$-$RANDOM"
  mkdir -p "$home/.config/bluechip/cache"
  chmod 700 "$home/.config/bluechip"
  HOME="$home" XDG_CONFIG_HOME="$home/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$fixture" \
    "$@"
}

printf '%s\n' "help — primary vs --all"
HELP="$("$BLUECHIP" --no-color help)"
echo "$HELP" | grep -q 'PRIMARY' && ok "default help has PRIMARY" || bad "default help missing PRIMARY"
echo "$HELP" | grep -q 'fls-propose' && bad "default help leaked fls-propose" || ok "default help hides fls-propose"
echo "$HELP" | grep -q 'types' && bad "default help leaked types stub" || ok "default help hides types"
echo "$HELP" | grep -q 'wl-copy' && ok "default help has pipe example" || bad "default help missing pipe"
HELPALL="$("$BLUECHIP" --no-color help --all)"
echo "$HELPALL" | grep -q 'fls-propose' && ok "help --all lists fls-propose" || bad "help --all missing fls-propose"
echo "$HELPALL" | grep -q 'scratchpad' && ok "help --all lists scratchpad" || bad "help --all missing scratchpad"

printf '%s\n' "bar — SBX identity + worst signal; unknown not 0%"
BAR="$(run "$W1/ok" "$BLUECHIP" --no-color --refresh bar 2>/dev/null)"
assert_json "$BAR" '.text' "SBX · 20%" "sandbox idle is SBX · 20%"
assert_json "$BAR" '.class | index("sandbox") != null' "true" "bar class sandbox"
assert_json "$BAR" '.class | index("ok") != null' "true" "healthy peak class ok"
assert_json "$BAR" '.tooltip | test("click: pin")' "true" "tooltip has click map"
assert_json "$BAR" '.tooltip | test("middle-click: doctor")' "true" "tooltip has doctor path"
assert_json "$BAR" '.sandboxState' "sandbox" "bar sandboxState"

BARF="$(run "$W2/ok" "$BLUECHIP" --no-color --refresh bar 2>/dev/null)"
assert_json "$BARF" '.text' "SBX · 1 fault" "faults beat a healthy 20% peak"
assert_json "$BARF" '.class | index("warn") != null' "true" "faults set warn class"

printf '%s\n' "incident — one object, no secrets"
INC="$(run "$W1/ok" "$BLUECHIP" --no-color --refresh --json incident 2>/dev/null)"
assert_json "$INC" '.ok' "true" "incident ok"
assert_json "$INC" '.sandboxState' "sandbox" "incident sandboxState"
assert_json "$INC" '.signal.severity' "ok" "incident signal ok"
assert_json "$INC" '.namedCreds.namedCount' "1" "incident NC count"
assert_json "$INC" '.namedCreds.namedApiNames[0]' "ZoomInfo_NC" "incident NC api name"
assert_json "$INC" '.flowFaults.availability' "ok" "incident flows ok"
assert_json "$INC" '.logs.availability' "ok" "incident logs ok"
if [[ "$INC" == *SUPER_SECRET* || "$INC" == *SHOULD_NEVER_LEAK* || "$INC" == *NEVER_PRINT* ]]; then
  bad "incident leaked a secret"
else
  ok "incident has no secrets"
fi
DOCJ="$(run "$W1/ok" "$BLUECHIP" --no-color --refresh --json doctor 2>/dev/null)"
assert_json "$DOCJ" '.namedCreds.namedCount' "1" "doctor --json reuses incident builder"

printf '%s\n' "doctor / changes honest empty"
DOC="$(run "$W1/ok" "$BLUECHIP" --no-color --refresh doctor 2>/dev/null)"
echo "$DOC" | grep -q 'none' && ok "doctor empty faults/changes say none" || bad "doctor missing none: ${DOC: -200}"
echo "$DOC" | grep -q 'Copy pack' && ok "doctor shows Copy pack" || bad "doctor missing Copy pack"
CHG="$(run "$W1/ok" "$BLUECHIP" --no-color --refresh --json changes 2>/dev/null)"
assert_json "$CHG" '.availability' "ok" "changes json ok"
assert_json "$CHG" '.changes | length' "0" "changes none is empty array not blank fail"

printf '%s\n' "watch --once --jsonl is a baseline heartbeat (no fake transition)"
WL="$(run "$W1/ok" "$BLUECHIP" --no-color --refresh watch --once --jsonl 2>/dev/null)"
assert_json "$WL" '.event' "heartbeat" "first watch tick is heartbeat"
assert_json "$WL" '.signal.severity' "ok" "watch jsonl has signal"

printf '%s\n' "watch emits limit_cross on real warn→crit (and not on first tick)"
CRIT="$WORKDIR/crit"
mkdir -p "$CRIT"
cp -a "$W1/ok/." "$CRIT/"
python3 - "$CRIT" <<'PY'
import json, sys
from pathlib import Path
d = Path(sys.argv[1])
blob = {"status": 0, "result": [{"name": "DailyApiRequests", "max": 10000, "remaining": 400}]}
(d / "limits.json").write_text(json.dumps(blob) + "\n")
PY
HOME_W="$WORKDIR/watch-cross"
mkdir -p "$HOME_W/.config/bluechip/cache"
chmod 700 "$HOME_W/.config/bluechip"
printf '{"peakSev":"ok","faultCount":0}\n' > "$HOME_W/.config/bluechip/cache/watch.json"
chmod 600 "$HOME_W/.config/bluechip/cache/watch.json"
WX="$(HOME="$HOME_W" XDG_CONFIG_HOME="$HOME_W/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$CRIT" \
  "$BLUECHIP" --no-color --refresh watch --once --jsonl 2>/dev/null)"
assert_json "$WX" '.event' "limit_cross" "seeded ok→crit is limit_cross"
assert_json "$WX" '.transition' "1" "limit_cross marked as transition"

printf '%s\n' "scratchpad helper (no Hyprland required)"
BIND="$("$ROOT/bin/bluechip-scratchpad" --print-bind)"
echo "$BIND" | grep -q 'bluechip-scratchpad' && ok "print-bind mentions helper" || bad "print-bind: $BIND"
DRY="$("$ROOT/bin/bluechip-scratchpad" --dry-run)"
echo "$DRY" | grep -q 'logs --follow' && ok "dry-run spawns logs --follow" || bad "dry-run: $DRY"
SCRATCH="$("$BLUECHIP" --no-color scratchpad --print-bind 2>/dev/null)"
echo "$SCRATCH" | grep -q 'SUPER SHIFT' && ok "bluechip scratchpad --print-bind" || bad "cli scratchpad bind missing"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
