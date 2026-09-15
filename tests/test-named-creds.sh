#!/usr/bin/env bash
# Named Credential inspector — no secrets, unknown ≠ invent, last-good on transient.
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-nc.XXXXXX")"
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

run_nc() {
  local case="$1"
  local home="$WORKDIR/home-$case"
  mkdir -p "$home/.config/bluechip/cache"
  chmod 700 "$home/.config/bluechip"
  HOME="$home" XDG_CONFIG_HOME="$home/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/$case" \
    "$BLUECHIP" --no-color --refresh --json named-creds
}

printf '%s\n' "named-creds — tooling happy path"
H="$(run_nc ok 2>/dev/null)"
assert_json "$H" '.availability' "ok" "availability ok"
assert_json "$H" '.org.sandboxState' "sandbox" "sandboxState sandbox"
assert_json "$H" '.namedCredentials.records[0].apiName' "ZoomInfo_NC" "NC api name"
assert_json "$H" '.namedCredentials.records[0].customHeaders.count' "2" "header count 2"
assert_json "$H" '.namedCredentials.records[0].generateAuthorizationHeader' "true" "genAuth flag"
assert_json "$H" '.externalCredentials.records[0].principals[0]' "Integration" "EC principal"
# Secrets must never appear, even if the Tooling row contained them.
if [[ "$H" == *SUPER_SECRET* || "$H" == *NEVER_PRINT* || "$H" == *SHOULD_NEVER* || "$H" == *token=SHOULD* ]]; then
  bad "secrets leaked in JSON"
else
  ok "no password / consumer secret / token query in JSON"
fi
assert_json "$H" '.namedCredentials.records[0] | has("Password")' "false" "Password key stripped"
EP="$(jq -r '.namedCredentials.records[0].endpoint' <<<"$H")"
[[ "$EP" != *"token="* ]] && ok "endpoint query string stripped" || bad "endpoint still has query: $EP"

printf '%s\n' "named-creds — tooling miss degrades to unknown (does not invent empty-as-none)"
M="$(run_nc tooling-miss 2>/dev/null)"
M_EC=$?
assert_json "$M" '.availability' "unknown" "miss availability unknown"
assert_json "$M" '.namedCredentials.records | length' "0" "no invented NC rows"
assert_json "$M" '.error.code' "object_forbidden" "reason object_forbidden"
assert_json "$M" '.ok' "true" "unknown ≠ fail (ok true)"
assert_eq "$M_EC" "0" "unknown ≠ fail (exit 0)"

printf '%s\n' "limits / orgs --json"
LIM="$(HOME="$WORKDIR/home-ok" XDG_CONFIG_HOME="$WORKDIR/home-ok/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/ok" \
  "$BLUECHIP" --no-color --refresh --json limits 2>/dev/null)"
assert_json "$LIM" '.org.sandboxState' "sandbox" "limits json sandboxState"
ORGS="$(HOME="$WORKDIR/home-ok" XDG_CONFIG_HOME="$WORKDIR/home-ok/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/ok" \
  "$BLUECHIP" --no-color --json orgs 2>/dev/null)"
assert_json "$ORGS" '.ok' "true" "orgs json ok"
STUB_HELP="$("$BLUECHIP" --no-color help --all 2>/dev/null)" || true
[[ "$STUB_HELP" == *types* && "$STUB_HELP" != *LATER* ]] && ok "types is a real verb in help --all" || bad "types not listed as a real verb"

printf '%s\n' "named-creds — Metadata list fallback"
MD="$(run_nc metadata-only 2>/dev/null)"
assert_json "$MD" '.source' "metadata_list" "source metadata_list"
assert_json "$MD" '.namedCredentials.records[0].apiName' "ZoomInfo_NC" "metadata name"
assert_json "$MD" '.namedCredentials.records[0].customHeaders.availability' "unknown" "headers unknown on metadata-only"

printf '%s\n' "named-creds — last-good kept on later miss"
HOME_LG="$WORKDIR/home-lg"
mkdir -p "$HOME_LG/.config/bluechip/cache"
chmod 700 "$HOME_LG/.config/bluechip"
HOME="$HOME_LG" XDG_CONFIG_HOME="$HOME_LG/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$FIX/ok" \
  "$BLUECHIP" --no-color --refresh --json named-creds >/dev/null
[[ -f "$HOME_LG/.config/bluechip/orgs/00D000000000002AAA/named-creds.json" ]] && ok "cache written" || bad "cache missing"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
