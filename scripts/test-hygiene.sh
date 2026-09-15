#!/usr/bin/env bash
# Hygiene acceptance tests H1–H10 (adversarial review 2026-09-15).
# Stub `sf` via BLUECHIP_SF + BLUECHIP_FIXTURE. No live org required.
set -o pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLUECHIP="$ROOT/bin/bluechip"
STUB="$ROOT/tests/hygiene/sf"
PASS=0
FAIL=0
WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/bluechip-h.XXXXXX")"
CASES="$WORKDIR/cases"
trap 'rm -rf "$WORKDIR"' EXIT

command -v jq >/dev/null || { echo "jq required"; exit 1; }
command -v python3 >/dev/null || { echo "python3 required"; exit 1; }
chmod +x "$STUB" "$BLUECHIP" "$ROOT/bin/bluechip-hygiene.inc" 2>/dev/null || true
python3 "$ROOT/tests/hygiene/gen_fixtures.py" "$CASES"

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

run_hygiene() {
  local case="$1"
  shift
  local home="$WORKDIR/home-$case-$$-$RANDOM"
  mkdir -p "$home/.config/bluechip/cache"
  chmod 700 "$home/.config/bluechip"
  cp "$CASES/$case/config.json" "$home/.config/bluechip/hygiene.json"
  HOME="$home" XDG_CONFIG_HOME="$home/.config" \
    BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$CASES/$case" \
    BLUECHIP_DUPE_CAP="${BLUECHIP_DUPE_CAP:-200}" \
    "$BLUECHIP" --no-color --refresh hygiene --json "$@"
  local ec=$?
  echo "$home" >&2
  return "$ec"
}

# ---------------------------------------------------------------------------
printf '%s\n' "H1 — object FLS / not queryable"
H1="$(run_hygiene h1 2>"$WORKDIR/h1.err")"
H1_EC=$?
assert_eq "$H1_EC" "0" "H1 exit 0"
assert_json "$H1" '.ok' "true" "H1 ok"
assert_json "$H1" '.objects[] | select(.api=="Lead") | .availability' "unknown" "H1 Lead unknown"
assert_json "$H1" '.objects[] | select(.api=="Lead") | .reason' "object_forbidden" "H1 Lead object_forbidden"
assert_json "$H1" '.objects[] | select(.api=="Contact") | .availability' "ok" "H1 Contact measured"
assert_json "$H1" '.availability' "partial" "H1 overall partial (Lead omitted)"
assert_json "$H1" '.overall.score != null' "true" "H1 overall scored from Contact only"

# ---------------------------------------------------------------------------
printf '%s\n' "H2 — field FLS (Harris case)"
H2="$(run_hygiene h2 2>"$WORKDIR/h2.err")"
assert_eq "$?" "0" "H2 exit 0"
assert_json "$H2" '.objects[0].availability' "ok" "H2 object stays available"
assert_json "$H2" '.objects[0].dimensions.completeness.fields["Custom__c"].availability' "unknown" "H2 Custom__c unknown"
assert_json "$H2" '.objects[0].dimensions.completeness.fields["Custom__c"].reason' "field_forbidden" "H2 field_forbidden"
assert_json "$H2" '.objects[0].dimensions.completeness.fields.Email.availability' "ok" "H2 Email measured"
assert_json "$H2" '.objects[0].dimensions.completeness.availability' "partial" "H2 completeness only on measured fields"
H2_SCORE="$(jq -r '.objects[0].dimensions.completeness.score' <<<"$H2")"
# 900/1200 = 75 — must NOT drop Lead from overall
assert_eq "$H2_SCORE" "75" "H2 completeness 75 (Email only)"
assert_json "$H2" '.overall.score != null' "true" "H2 overall not inflated by dropping Lead"

# ---------------------------------------------------------------------------
printf '%s\n' "H3 — field does not exist"
H3="$(run_hygiene h3 2>/dev/null)"
assert_eq "$?" "0" "H3 exit 0"
assert_json "$H3" '.objects[0].availability' "ok" "H3 object not dead"
assert_json "$H3" '.objects[0].dimensions.completeness.fields["NoSuch__c"].reason' "invalid_field" "H3 invalid_field"

# ---------------------------------------------------------------------------
printf '%s\n' "H4 — dimension probe unknown (never score 100 via //0)"
H4="$(run_hygiene h4 2>/dev/null)"
assert_eq "$?" "0" "H4 exit 0"
assert_json "$H4" '.objects[0].dimensions.freshness.availability' "unknown" "H4 freshness unknown"
assert_json "$H4" '.objects[0].dimensions.freshness.score' "null" "H4 freshness score null (not 100)"
assert_json "$H4" '.availability' "partial" "H4 overall partial"
assert_json "$H4" '.overall.availability' "partial" "H4 overall.availability partial"

# ---------------------------------------------------------------------------
printf '%s\n' "H5 — transient 502 must not wipe last-good"
H5_HOME="$WORKDIR/home-h5"
mkdir -p "$H5_HOME/.config/bluechip/cache"
chmod 700 "$H5_HOME/.config/bluechip"
cp "$CASES/h5/config.json" "$H5_HOME/.config/bluechip/hygiene.json"
LAST_GOOD='{"ok":true,"availability":"ok","overall":{"score":81,"grade":"B","availability":"ok"},"measuredAt":1000,"org":{"id":"00D000000000001AAA","sandboxState":"prod"},"objects":[],"error":null}'
printf '%s\n' "$LAST_GOOD" > "$H5_HOME/.config/bluechip/cache/hygiene.json"
chmod 600 "$H5_HOME/.config/bluechip/cache/hygiene.json"
H5="$(HOME="$H5_HOME" XDG_CONFIG_HOME="$H5_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$CASES/h5" \
  "$BLUECHIP" --no-color --refresh hygiene --json 2>/dev/null)"
H5_EC=$?
assert_eq "$H5_EC" "1" "H5 exit non-zero"
assert_json "$H5" '.ok' "false" "H5 ok false"
assert_json "$H5" '.error.code' "transient" "H5 structured transient"
AFTER="$(cat "$H5_HOME/.config/bluechip/cache/hygiene.json")"
assert_json "$AFTER" '.overall.grade' "B" "H5 last-good grade kept"
assert_json "$AFTER" '.measuredAt' "1000" "H5 last-good not overwritten"
assert_eq "$(test -f "$H5_HOME/.config/bluechip/cache/hygiene.error.json" && echo yes || echo no)" "yes" "H5 doctor stale flag written"
DOC="$(HOME="$H5_HOME" XDG_CONFIG_HOME="$H5_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$CASES/h5" \
  "$BLUECHIP" --no-color doctor 2>/dev/null || true)"
[[ "$DOC" == *stale* ]] && ok "H5 doctor shows last-good + stale" || bad "H5 doctor missing stale"

# ---------------------------------------------------------------------------
printf '%s\n' "H6 — all objects unknown: no letter grade / no F"
H6="$(run_hygiene h6 2>/dev/null)"
H6_EC=$?
assert_eq "$H6_EC" "0" "H6 exit 0 (scan completed)"
assert_json "$H6" '.availability' "unknown" "H6 overall unknown"
assert_json "$H6" '.overall.grade' "null" "H6 no letter grade"
assert_json "$H6" '.overall.score' "null" "H6 no score"
assert_json "$H6" '.overall.grade == "F"' "false" "H6 is not F"

# ---------------------------------------------------------------------------
printf '%s\n' "H7 — empty object: not 100 and not F"
H7="$(run_hygiene h7 2>/dev/null)"
assert_eq "$?" "0" "H7 exit 0"
assert_json "$H7" '.objects[0].availability' "ok" "H7 availability ok"
assert_json "$H7" '.objects[0].total' "0" "H7 total 0"
assert_json "$H7" '.objects[0].score' "null" "H7 object score null (not 100)"
assert_json "$H7" '.overall.grade' "null" "H7 no F"

# ---------------------------------------------------------------------------
printf '%s\n' "H8 — integration user ≠ sysadmin (object Read, field Read missing)"
H8="$(run_hygiene h8 2>/dev/null)"
assert_eq "$?" "0" "H8 exit 0"
assert_json "$H8" '.objects[0].availability' "ok" "H8 object available"
assert_json "$H8" '.objects[0].dimensions.completeness.fields["SavvyCal_Id__c"].reason' "field_forbidden" "H8 field_forbidden (missing Edit irrelevant)"
assert_json "$H8" '.objects[0].dimensions.completeness.fields.Email.availability' "ok" "H8 Email measured"

# ---------------------------------------------------------------------------
printf '%s\n' "H9 — structured errors (sf missing / unauth / offline); bar still JSON"
H9A="$("$BLUECHIP" --no-color --refresh hygiene --json 2>/dev/null)" || true
# default HOME may have sf — force missing binary
H9_MISS="$(HOME="$WORKDIR/empty1" XDG_CONFIG_HOME="$WORKDIR/empty1/.config" \
  BLUECHIP_SF="/nonexistent/bluechip-sf" \
  "$BLUECHIP" --no-color hygiene --json 2>/dev/null)" || true
assert_json "$H9_MISS" '.ok' "false" "H9 sf_missing ok false"
assert_json "$H9_MISS" '.error.code' "sf_missing" "H9 sf_missing code"

H9U="$(run_hygiene h9-unauth 2>/dev/null)" || true
assert_json "$H9U" '.ok' "false" "H9 unauth ok false"
assert_json "$H9U" '.error.code' "unauthenticated" "H9 unauthenticated"

H9O="$(run_hygiene h9-offline 2>/dev/null)" || true
assert_json "$H9O" '.ok' "false" "H9 offline ok false"
assert_json "$H9O" '.error.code' "transient" "H9 offline transient"

BAR="$(HOME="$WORKDIR/empty2" XDG_CONFIG_HOME="$WORKDIR/empty2/.config" \
  BLUECHIP_SF="/nonexistent/bluechip-sf" \
  "$BLUECHIP" --no-color bar 2>/dev/null)"
echo "$BAR" | jq -e '.class | index("unknown")' >/dev/null \
  && ok "H9 bar emits valid JSON with unknown class" \
  || bad "H9 bar JSON invalid: $BAR"

# ---------------------------------------------------------------------------
printf '%s\n' "H10 — GROUP BY cap / truncated"
H10="$(BLUECHIP_DUPE_CAP=5 run_hygiene h10 2>/dev/null)"
assert_eq "$?" "0" "H10 exit 0"
assert_json "$H10" '.objects[0].dimensions.dupes.truncated' "true" "H10 truncated true"
assert_json "$H10" '.objects[0].dimensions.dupes.groups' "5" "H10 groups ≤ cap 5"

# ---------------------------------------------------------------------------
printf '%s\n' "Extra — limits miss is unknown (not 0% green)"
LIM_HOME="$WORKDIR/home-lim"
mkdir -p "$LIM_HOME/.config/bluechip"
cp "$CASES/limits-miss/config.json" "$LIM_HOME/.config/bluechip/hygiene.json"
BARL="$(HOME="$LIM_HOME" XDG_CONFIG_HOME="$LIM_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$CASES/limits-miss" \
  "$BLUECHIP" --no-color --refresh bar 2>/dev/null)"
echo "$BARL" | jq -e '.class | index("unknown")' >/dev/null \
  && ok "limits-miss bar class unknown" \
  || bad "limits-miss bar not unknown: $BARL"
echo "$BARL" | jq -e '.text | test("0%") | not' >/dev/null \
  && ok "limits-miss bar text is not 0%" \
  || bad "limits-miss still shows 0%: $BARL"

# ---------------------------------------------------------------------------
printf '%s\n' "Extra — cmd_context never prints PROD when Organization is unreadable"
CTX_HOME="$WORKDIR/home-ctx"
mkdir -p "$CTX_HOME/.config/bluechip"
cp "$CASES/org-unknown/config.json" "$CTX_HOME/.config/bluechip/hygiene.json"
CTX="$(HOME="$CTX_HOME" XDG_CONFIG_HOME="$CTX_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$CASES/org-unknown" \
  "$BLUECHIP" --no-color --refresh context 2>/dev/null)"
if [[ "$CTX" == *"**UNKNOWN"** ]]; then ok "context pack uses UNKNOWN"
elif [[ "$CTX" == *"**PROD"** ]]; then bad "context pack still asserts PROD"
else bad "context pack missing sandbox token: ${CTX:0:200}"; fi

# ---------------------------------------------------------------------------
printf '%s\n' "Extra — SOQL-from-config allowlist"
INJ="$(run_hygiene soql-inject 2>/dev/null)" || true
assert_json "$INJ" '.error.code' "config_invalid" "injection filter rejected"

# ---------------------------------------------------------------------------
printf '%s\n' "Extra — cache/file modes 700/600 on first write"
MODE_HOME="$WORKDIR/home-mode"
mkdir -p "$MODE_HOME"
HOME="$MODE_HOME" XDG_CONFIG_HOME="$MODE_HOME/.config" \
  BLUECHIP_SF="$STUB" BLUECHIP_FIXTURE="$CASES/h2" \
  "$BLUECHIP" --no-color --refresh hygiene --json >/dev/null 2>&1 || true
STAT_DIR="$(stat -c '%a' "$MODE_HOME/.config/bluechip" 2>/dev/null || echo missing)"
STAT_CACHE="$(stat -c '%a' "$MODE_HOME/.config/bluechip/cache/hygiene.json" 2>/dev/null || echo missing)"
STAT_SNAP="$(stat -c '%a' "$MODE_HOME/.config/bluechip/cache/snapshot.json" 2>/dev/null || echo missing)"
assert_eq "$STAT_DIR" "700" "state dir 700"
assert_eq "$STAT_CACHE" "600" "hygiene.json 600"
assert_eq "$STAT_SNAP" "600" "snapshot.json 600"

# ---------------------------------------------------------------------------
printf '\n%s\n' "$PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
