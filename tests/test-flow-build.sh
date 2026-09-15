#!/usr/bin/env bash
# Flow-build wave: callout-auth doctor, types, callout-pack, preflight.
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-fb.XXXXXX")"
W1="$WORKDIR/w1"
W3="$WORKDIR/w3"
trap 'rm -rf "$WORKDIR"' EXIT

command -v jq >/dev/null || { echo "jq required"; exit 1; }
python3 "$ROOT/tests/wave1/gen_fixtures.py" "$W1"
python3 "$ROOT/tests/wave3/gen_fixtures.py" "$W3"
chmod +x "$STUB" "$BLUECHIP" "$ROOT/bin/bluechip-apex-parse.py" "$ROOT/bin/bluechip-scratchpad"

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

printf '%s\n' "apex-parse — SymbolTable + Body, no secrets"
PARSE="$(printf '%s\n' '{"classes":[{"Name":"IN_LeadSearch","Body":"public class IN_LeadSearch { public String company; public String password; }","SymbolTable":{"name":"IN_LeadSearch","properties":[{"name":"company","type":"String"}],"innerClasses":[]}}]}' \
  | python3 "$ROOT/bin/bluechip-apex-parse.py")"
assert_json "$PARSE" '.classes[0].assignmentPaths[0].path' "IN_LeadSearch.company" "parser path company"
assert_json "$PARSE" '[.classes[0].properties[] | select(.name=="password")] | length' "0" "parser drops password property"

printf '%s\n' "callout-auth — 401 classes"
DOC="$(run "$W3/doctor" "$BLUECHIP" --no-color --refresh --json callout-auth 2>/dev/null)"
assert_json "$DOC" '.availability' "ok" "doctor availability ok"
assert_json "$DOC" '.org.sandboxState' "sandbox" "doctor sandboxState"
assert_json "$DOC" '[.diagnoses[] | select(.apiName=="HeadersAbsent_NC") | .reasons[]] | index("headers_absent") != null' "true" "headers_absent"
assert_json "$DOC" '[.diagnoses[] | select(.apiName=="FormulasOff_NC") | .reasons[]] | index("formulas_off") != null' "true" "formulas_off"
assert_json "$DOC" '[.diagnoses[] | select(.apiName=="ZoomInfo_NC") | .reasons[]] | index("gen_auth_on_custom") != null' "true" "gen_auth_on_custom"
assert_json "$DOC" '[.diagnoses[] | select(.apiName=="CustomHeader_OK") | .reasons[]] | length' "0" "healthy custom-header has no 401 class"
assert_json "$DOC" '[.diagnoses[] | select(.apiName=="HeadersAbsent_NC") | .likely401] | .[0]' "true" "headers_absent is likely401"
if [[ "$DOC" == *SUPER_SECRET* || "$DOC" == *ParameterValue* || "$DOC" == *SHOULD_STRIP* ]]; then
  bad "doctor leaked a secret"
else
  ok "doctor has no secrets"
fi

ALIAS="$(run "$W3/doctor" "$BLUECHIP" --no-color --refresh --json named-creds --doctor 2>/dev/null)"
assert_json "$ALIAS" '[.diagnoses[] | select(.apiName=="HeadersAbsent_NC") | .reasons[]] | index("headers_absent") != null' "true" "named-creds --doctor aliases"

printf '%s\n' "callout-auth — Tooling miss is unknown (not fake healthy)"
MISS="$(run "$W3/tooling-miss" "$BLUECHIP" --no-color --refresh --json callout-auth 2>/dev/null)"
assert_json "$MISS" '.availability' "unknown" "miss availability unknown"
assert_json "$MISS" '.diagnoses[0].reasons[0]' "unknown" "miss reason unknown"
assert_json "$MISS" '.ok' "true" "unknown ≠ fail"

printf '%s\n' "callout-auth — AutomatedProcess principal_missing"
PR="$(run "$W3/doctor" "$BLUECHIP" --no-color --refresh --json callout-auth --user AutomatedProcess 2>/dev/null)"
assert_json "$PR" '.principalAccess.user.name' "Automated Process" "resolved Automated Process"
assert_json "$PR" '[.diagnoses[] | select(.kind=="external_credential") | .reasons[]] | index("principal_missing") != null' "true" "principal_missing when no ECPA grant"

printf '%s\n' "types — discover + named"
TY="$(run "$W3/types" "$BLUECHIP" --no-color --refresh --json types 2>/dev/null)"
assert_json "$TY" '.availability' "ok" "types ok"
assert_json "$TY" '[.classes[].name] | index("IN_LeadSearch") != null' "true" "discovered IN_LeadSearch"
assert_json "$TY" '[.classes[] | select(.name=="IN_LeadSearch") | .assignmentPaths[]?.path] | index("IN_LeadSearch.company") != null' "true" "assignment path company"
assert_json "$TY" '[.classes[] | select(.name=="OUT_2XX") | .kind] | .[0]' "http_callout_2xx" "OUT_2XX kind"
if [[ "$TY" == *SHOULD_NEVER* || "$TY" == *"Bearer"* ]]; then
  bad "types leaked a secret from Body"
else
  ok "types has no body secrets"
fi

TN="$(run "$W3/types" "$BLUECHIP" --no-color --refresh --json types IN_LeadSearch 2>/dev/null)"
assert_json "$TN" '.mode' "named" "named mode"
assert_json "$TN" '.classes[0].name' "IN_LeadSearch" "named class"

TM="$(run "$W3/tooling-miss" "$BLUECHIP" --no-color --refresh --json types 2>/dev/null)"
assert_json "$TM" '.availability' "unknown" "types miss unknown"
assert_json "$TM" '.classes | length' "0" "types miss invents no classes"

printf '%s\n' "callout-pack — Flow + slices"
PK="$(run "$W3/doctor" "$BLUECHIP" --no-color --refresh --json callout-pack ZoomInfo_Callout 2>/dev/null)"
assert_json "$PK" '.flow.apiName' "ZoomInfo_Callout" "pack flow name"
assert_json "$PK" '.flow.identity.apiName' "ZoomInfo_Callout" "pack FlowDefinition"
assert_json "$PK" '.auth.diagnoses | length > 0' "true" "pack has auth diagnoses"
assert_json "$PK" '.types.availability' "ok" "pack types ok"
assert_json "$PK" '.note' "hand-off — not write authority" "pack not write authority"
echo "$(jq -r '.markdown' <<<"$PK")" | grep -q 'not write authority' && ok "pack markdown caveat" || bad "pack markdown missing caveat"

INC="$(run "$W3/doctor" "$BLUECHIP" --no-color --refresh --json incident --callout ZoomInfo_Callout 2>/dev/null)"
assert_json "$INC" '.flow.apiName' "ZoomInfo_Callout" "incident --callout aliases pack"

printf '%s\n' "preflight — FLS + headers_absent"
PF="$(run "$W3/preflight" "$BLUECHIP" --no-color --refresh --json preflight \
  --user integration@example.com --fields Account.SavvyCal_Id__c 2>/dev/null)"
assert_json "$PF" '.fls.fields[] | select(.ref=="Account.SavvyCal_Id__c") | .gap' "missing_edit" "preflight missing_edit"
assert_json "$PF" '[.auth.diagnoses[] | select(.apiName=="HeadersAbsent_NC") | .reasons[]] | index("headers_absent") != null' "true" "preflight headers_absent"
assert_json "$PF" '.note | test("does not Activate")' "true" "preflight does not Activate"
if [[ "$PF" == *Setup*Menu* ]]; then
  bad "preflight invented a Setup menu"
else
  ok "preflight did not invent Setup menus"
fi

printf '%s\n' "scratchpad --boundary"
BD="$("$ROOT/bin/bluechip-scratchpad" --boundary)"
echo "$BD" | grep -q 'logs --follow' && ok "boundary prints logs --follow" || bad "boundary missing logs"
echo "$BD" | grep -q 'wrangler tail' && ok "boundary prints wrangler tail" || bad "boundary missing wrangler"
echo "$BD" | grep -q 'No QML' && ok "boundary says no QML" || bad "boundary missing no QML"

printf '%s\n' "help --all lists new verbs; default help still hides them"
HELP="$("$BLUECHIP" --no-color help)"
echo "$HELP" | grep -q 'callout-auth' && bad "default help leaked callout-auth" || ok "default help hides callout-auth"
HELPALL="$("$BLUECHIP" --no-color help --all)"
echo "$HELPALL" | grep -q 'callout-auth' && ok "help --all lists callout-auth" || bad "help --all missing callout-auth"
echo "$HELPALL" | grep -q 'types' && ok "help --all lists types" || bad "help --all missing types"
echo "$HELPALL" | grep -q 'QML panel' && bad "help --all still stubs types as later" || ok "types no longer a LATER stub"

printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
