#!/usr/bin/env bash
# Unit tests for neutralize_plaintext (bluechip-lib.inc).
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=../bin/bluechip-lib.inc
source "$ROOT/bin/bluechip-lib.inc"
PASS=0
FAIL=0

ok() { PASS=$((PASS + 1)); printf '  ✓ %s\n' "$1"; }
bad() { FAIL=$((FAIL + 1)); printf '  ✗ %s\n' "$1"; }

assert_eq() {
  local got="$1" want="$2" msg="$3"
  if [[ "$got" == "$want" ]]; then ok "$msg"
  else bad "$msg (got '$got' want '$want')"; fi
}

printf '%s\n' "neutralize_plaintext"

assert_eq "$(neutralize_plaintext 'hello')" "hello" "plain passthrough"
assert_eq "$(neutralize_plaintext $'\x1b[31mred\x1b[0m')" "red" "strip ANSI"
assert_eq "$(neutralize_plaintext 'a<b>bold</b>c')" "aboldc" "strip HTML tag delimiters"
assert_eq "$(neutralize_plaintext $'line\x07break')" "line break" "control chars to space"
assert_eq "$(neutralize_plaintext "$(printf 'x%.0s' {1..600})" 20)" "$(printf 'x%.0s' {1..20})…" "length cap"

printf '%s\n' "org id helpers"
assert_eq "$(sf_org_id_ok 00D000000000001AAA && echo yes || echo no)" "yes" "18-char org id ok"
assert_eq "$(sf_org_id_ok bogus && echo yes || echo no)" "no" "reject bogus id"
SNAP_OK='{"display":{"id":"00D000000000001AAA"},"org":{"Id":"00D000000000001AAA"}}'
SNAP_EMPTY='{}'
SNAP_ID='{"display":{"id":"00D000000000001AAA"}}'
assert_eq "$(org_id "$SNAP_OK")" "00D000000000001AAA" "org_id from snapshot"
assert_eq "$(org_id "$SNAP_EMPTY")" "unknown" "org_id missing → unknown"
assert_eq "$(org_id_trunc "$SNAP_ID")" "00D0…1AAA" "org_id_trunc"

printf '%s\n' "sandbox_state is a single token"
SNAP_SBX='{"org":{"IsSandbox":true},"display":{"id":"00D000000000001AAA"}}'
SNAP_PRD='{"org":{"IsSandbox":false},"display":{"id":"00D000000000001AAA"}}'
SNAP_UNK='{"org":{},"display":{"id":"00D000000000001AAA"}}'
assert_eq "$(sandbox_state "$SNAP_SBX")" "sandbox" "sandbox_state sandbox"
assert_eq "$(sandbox_state "$SNAP_PRD")" "prod" "sandbox_state prod"
assert_eq "$(sandbox_state "$SNAP_UNK")" "unknown" "sandbox_state unknown"
assert_eq "$(sandbox_state "$SNAP_SBX" | tr '\n' '|')" "sandbox" "sandbox_state no extra newline"

printf '%s\n' "url / secret helpers"
assert_eq "$(strip_url_secrets 'https://api.example.com/v1?token=abc#x')" "https://api.example.com/v1" "strip query+fragment"
REDACTED="$(jq -nc '{Password:"x",apiName:"Zoom",consumerSecret:"y"}' | redact_secrets_json)"
assert_eq "$(jq -r 'has("Password")' <<<"$REDACTED")" "false" "redact Password key"
assert_eq "$(jq -r '.apiName' <<<"$REDACTED")" "Zoom" "redact keeps apiName"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
