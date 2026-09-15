#!/usr/bin/env bash
# Wave 2: FLS matrix, offenders, desk, clipboard, fls-propose.
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
PINFIX="$ROOT/tests/pin/fixtures"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-w2.XXXXXX")"
FIX="$WORKDIR/fix"
trap 'rm -rf "$WORKDIR"' EXIT

command -v jq >/dev/null || { echo "jq required"; exit 1; }
python3 "$ROOT/tests/wave2/gen_fixtures.py" "$FIX"
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

printf '%s\n' "fls — missing Edit vs missing Read"
FLS="$(run ok "$BLUECHIP" --no-color --refresh --json fls \
  --user integration@example.com \
  --fields Account.SavvyCal_Id__c,Contact.Email,Lead.SavvyCal_X__c 2>/dev/null)"
assert_json "$FLS" '.availability' "ok" "fls availability ok"
assert_json "$FLS" '.org.sandboxState' "sandbox" "fls sandboxState"
assert_json "$FLS" '.fields[] | select(.ref=="Account.SavvyCal_Id__c") | .gap' "missing_edit" "SavvyCal missing_edit"
assert_json "$FLS" '.fields[] | select(.ref=="Account.SavvyCal_Id__c") | .effectiveRead' "true" "SavvyCal still readable"
assert_json "$FLS" '.fields[] | select(.ref=="Contact.Email") | .gap' "missing_read" "Contact.Email missing_read"
assert_json "$FLS" '.fields[] | select(.ref=="Lead.SavvyCal_X__c") | .gap' "missing_read" "custom field absent row = missing_read"
assert_json "$FLS" '.summary.missing_edit' "1" "one missing_edit"
if [[ "$FLS" == *Setup*Menu* || "$FLS" == *"click-path"* ]]; then
  bad "fls invented a Setup menu"
else
  ok "fls did not invent Setup menus"
fi

printf '%s\n' "fls — Tooling/SOQL miss is unknown (not empty-as-ok)"
FM="$(run fls-miss "$BLUECHIP" --no-color --refresh --json fls \
  --user integration@example.com --fields Account.SavvyCal_Id__c 2>/dev/null)"
assert_json "$FM" '.availability' "unknown" "fls miss unknown"
assert_json "$FM" '.fields[] | select(.ref=="Account.SavvyCal_Id__c") | .gap' "unknown" "requested field stays unknown (not granted)"
assert_json "$FM" '.error.code' "object_forbidden" "fls miss reason"

printf '%s\n' "fls-propose — proposal only, no write"
PROP="$(run ok "$BLUECHIP" --no-color --refresh --json fls-propose \
  --user integration@example.com --fields Account.SavvyCal_Id__c 2>/dev/null)"
assert_json "$PROP" '.proposal.applied' "false" "proposal not applied"
assert_json "$PROP" '.proposal.operations[0].ref' "Account.SavvyCal_Id__c" "propose target field"
assert_json "$PROP" '.proposal.confirmMatrix.mcp' "never — proposal and apply stay CLI-only" "mcp never"
assert_json "$PROP" '.proposal.confirmMatrix.prod' "type PROD; --yes banned" "prod confirm documented"

printf '%s\n' "offenders — ranked when Event Log body exists"
OFF="$(run ok "$BLUECHIP" --no-color --refresh --json offenders --since 9am 2>/dev/null)"
assert_json "$OFF" '.availability' "ok" "offenders ok"
assert_json "$OFF" '.org.sandboxState' "sandbox" "offenders sandboxState"
assert_json "$OFF" '.offenders.byUser[0].userId' "005000000000009AAA" "top user"
assert_json "$OFF" '.offenders.byUser[0].count' "2" "top user count 2"
assert_json "$OFF" '.sources.flex.availability' "unknown" "flex stays unknown"
if [[ "$OFF" == *SHOULD_NEVER_LEAK* || "$OFF" == *Bearer* ]]; then
  bad "offenders leaked secret-shaped CSV line"
else
  ok "offenders CSV did not leak Authorization line as a user id"
fi

printf '%s\n' "offenders — Event Log miss is unknown, not fake 0"
OM="$(run offenders-miss "$BLUECHIP" --no-color --refresh --json offenders 2>/dev/null)"
assert_json "$OM" '.availability' "unknown" "event log miss unknown"
assert_json "$OM" '.offenders.byUser | length' "0" "no invented offenders"
assert_json "$OM" '.error.message | test("fake 0")' "true" "honest not-fake-0 message"

printf '%s\n' "offenders — LogFile body unavailable is unknown, not 0"
OA="$(run api-miss "$BLUECHIP" --no-color --refresh --json offenders 2>/dev/null)"
assert_json "$OA" '.availability' "unknown" "body miss unknown"
assert_json "$OA" '.files | length' "1" "file metadata still listed"
assert_json "$OA" '.offenders.byUser | length' "0" "no fake ranked users"

printf '%s\n' "clipboard — Flow API name / interview / org Id"
CLIPF="$(run ok "$BLUECHIP" --no-color --refresh --json clipboard --text SavvyCal_Webhook 2>/dev/null)"
assert_json "$CLIPF" '.ok' "true" "flow api clipboard ok"
assert_json "$CLIPF" '.kind' "flow_api" "kind flow_api"
assert_json "$CLIPF" '.org.sandboxState' "sandbox" "clipboard sandboxState"
[[ "$(jq -r '.packPath' <<<"$CLIPF")" == *clipboard-pack.md ]] && ok "pack path written" || bad "pack path missing"
jq -r '.markdown' <<<"$CLIPF" | grep -q 'SavvyCal_Webhook' && ok "pack mentions flow" || bad "pack missing flow name"
jq -r '.markdown' <<<"$CLIPF" | grep -q 'SHOULD_NEVER_LEAK' && bad "clipboard pack leaked token" || ok "clipboard pack has no access token"

CLIPI="$(run ok "$BLUECHIP" --no-color --refresh --json clipboard --text 4I9000000000001AAA 2>/dev/null)"
assert_json "$CLIPI" '.kind' "interview" "kind interview"

CLIPO="$(run ok "$BLUECHIP" --no-color --refresh --json clipboard --text 00D000000000002AAA 2>/dev/null)"
assert_json "$CLIPO" '.kind' "org" "kind org"

if run ok "$BLUECHIP" --no-color --refresh --json clipboard --text "random junk from a webpage" >/dev/null 2>&1; then
  bad "unrecognized clipboard should fail"
else
  ok "unrecognized clipboard refused"
fi

printf '%s\n' "types is a real verb (no longer a stub)"
TYPES_HELP="$("$BLUECHIP" --no-color types --help 2>/dev/null)" || true
[[ "$TYPES_HELP" == *HTTP\ Callout* || "$TYPES_HELP" == *Apex-defined* ]] && ok "types help is the explorer" || bad "types help: $TYPES_HELP"

printf '%s\n' "desk + bar --all — per-org chrome, pin confirm on cross"
DESK_HOME="$WORKDIR/home-desk"
mkdir -p "$DESK_HOME/.config/bluechip/cache"
chmod 700 "$DESK_HOME/.config/bluechip"
printf '{"pinnedOrg":"prod-org","deskOrgs":["prod-org"]}\n' > "$DESK_HOME/.config/bluechip/state.json"
chmod 600 "$DESK_HOME/.config/bluechip/state.json"
env HOME="$DESK_HOME" XDG_CONFIG_HOME="$DESK_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$PINFIX/cross" \
  "$BLUECHIP" --no-color --refresh bar >/dev/null 2>&1 || true

assert_pin_fail() {
  if env HOME="$DESK_HOME" XDG_CONFIG_HOME="$DESK_HOME/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$PINFIX/cross" \
    "$BLUECHIP" --no-color desk add sbx >/dev/null 2>&1; then
    bad "desk add cross without confirm should fail"
  else
    ok "desk add cross refused without confirm"
  fi
}
assert_pin_fail

env HOME="$DESK_HOME" XDG_CONFIG_HOME="$DESK_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$PINFIX/cross" \
  bash -c 'printf "SANDBOX\n" | "$1" --no-color desk add sbx' _ "$BLUECHIP" >/dev/null 2>&1 \
  && ok "desk add with SANDBOX confirm" \
  || bad "desk add with SANDBOX confirm failed"

DESK="$(env HOME="$DESK_HOME" XDG_CONFIG_HOME="$DESK_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$PINFIX/cross" \
  "$BLUECHIP" --no-color --json desk 2>/dev/null)"
assert_json "$DESK" '[.orgs[].sandboxState] | unique | length' "2" "desk has both prod and sandbox states"
assert_json "$DESK" '.orgs | map(.sandboxState) | any(.=="prod")' "true" "desk includes prod row"
assert_json "$DESK" '.orgs | map(.sandboxState) | any(.=="sandbox")' "true" "desk includes sandbox row"
# Never a single mixed badge at the desk root.
assert_json "$DESK" 'has("sandboxState")' "false" "desk root has no mixed sandboxState"

BARALL="$(env HOME="$DESK_HOME" XDG_CONFIG_HOME="$DESK_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$PINFIX/cross" \
  "$BLUECHIP" --no-color bar --all 2>/dev/null)"
echo "$BARALL" | jq -e '.text | test("PROD")' >/dev/null \
  && ok "bar --all chip stays pinned PROD chrome" \
  || bad "bar --all text lost PROD: $BARALL"
echo "$BARALL" | jq -e '.tooltip | test("SANDBOX")' >/dev/null \
  && ok "bar --all tooltip lists SANDBOX desk row" \
  || bad "bar --all tooltip missing SANDBOX"
echo "$BARALL" | jq -e '.class | index("prod")' >/dev/null \
  && ok "bar --all class is pinned prod (not mixed)" \
  || bad "bar --all class mixed"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
