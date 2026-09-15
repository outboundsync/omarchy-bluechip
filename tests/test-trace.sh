#!/usr/bin/env bash
# TraceFlag confirm matrix + status unknown + log neutralize.
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-tr.XXXXXX")"
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

assert_json() {
  local blob="$1" expr="$2" want="$3" msg="$4"
  local got
  got="$(printf '%s\n' "$blob" | jq -r "$expr" 2>/dev/null || echo "<jq-fail>")"
  assert_eq "$got" "$want" "$msg"
}

run() {
  local case="$1"
  shift
  local home="$WORKDIR/h-$case-$$-$RANDOM"
  mkdir -p "$home/.config/bluechip/cache"
  chmod 700 "$home/.config/bluechip"
  HOME="$home" XDG_CONFIG_HOME="$home/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/$case" \
    "$@"
}

printf '%s\n' "trace status — tooling ok"
ST="$(run ok "$BLUECHIP" --no-color --refresh --json trace status 2>/dev/null)"
assert_json "$ST" '.availability' "ok" "status ok"
assert_json "$ST" '.org.sandboxState' "sandbox" "status sandboxState"
assert_json "$ST" '.flags[0].id' "7tf000000000001AAA" "flag id"

printf '%s\n' "trace status — tooling miss is unknown (not empty-as-ready)"
SM="$(run tooling-miss "$BLUECHIP" --no-color --refresh --json trace status 2>/dev/null)"
assert_json "$SM" '.availability' "unknown" "miss unknown"
assert_json "$SM" '.flags | length' "0" "no invented flags"
assert_json "$SM" '.error.code' "object_forbidden" "miss reason"

printf '%s\n' "trace start — sandbox --yes"
START="$(run ok "$BLUECHIP" --no-color --refresh --json --yes trace start --minutes 15 2>/dev/null)"
assert_json "$START" '.ok' "true" "sandbox --yes start"
assert_json "$START" '.created.logType' "USER_DEBUG" "USER_DEBUG"

printf '%s\n' "trace start — prod --yes banned"
if run prod "$BLUECHIP" --no-color --refresh --json --yes trace start >/dev/null 2>&1; then
  bad "prod --yes should be banned"
else
  ok "prod --yes banned"
fi

printf '%s\n' "trace start — prod requires PROD"
if run prod "$BLUECHIP" --no-color --refresh --json trace start </dev/null >/dev/null 2>&1; then
  bad "prod start without confirm should fail"
else
  ok "prod start without confirm refused"
fi
STARTP="$(run prod bash -c 'printf "PROD\n" | "$1" --no-color --refresh --json trace start --minutes 10' _ "$BLUECHIP" 2>/dev/null)"
assert_json "$STARTP" '.ok' "true" "prod start with PROD confirm"

printf '%s\n' "trace stop — sandbox --yes"
STOP="$(run ok "$BLUECHIP" --no-color --refresh --json --yes trace stop --id 7tf000000000001AAA 2>/dev/null)"
assert_json "$STOP" '.ok' "true" "stop ok"
assert_json "$STOP" '.deleted[0]' "7tf000000000001AAA" "deleted id"

printf '%s\n' "logs — neutralize + bound"
LG="$(run ok "$BLUECHIP" --no-color --refresh --json logs --body 2>/dev/null)"
assert_json "$LG" '.availability' "ok" "logs ok"
assert_json "$LG" '.org.sandboxState' "sandbox" "logs sandboxState"
BODY="$(jq -r '.logs[0].body // empty' <<<"$LG")"
if [[ "$BODY" == *Bearer* || "$BODY" == *password=* || "$BODY" == *Authorization:* ]]; then
  bad "log body leaked secret line"
else
  ok "log body redacted Authorization/password lines"
fi
[[ "$BODY" == *hello* ]] && ok "log body kept non-secret text" || bad "log body over-redacted"

printf '%s\n' "logs --follow bounded"
FOL="$(run ok env BLUECHIP_FOLLOW_ITERS=1 "$BLUECHIP" --no-color --refresh logs --follow 2>/dev/null)" || true
[[ "$FOL" == *07L000000000001AAA* || "$FOL" == *hello* || "$FOL" == *Apex* ]] \
  && ok "follow printed a log" || bad "follow produced nothing: ${FOL:0:120}"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
