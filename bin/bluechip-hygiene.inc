# shellcheck shell=bash
# Hygiene probe — sourced by bin/bluechip. Describe (or field probe) before
# multi-field COUNT. Per-field ok|unknown. Never score a miss as 100 via //0.
# Last-good cache is written only on ok|partial with measured data.

hygiene_config() {
  local f="$STATE_DIR/hygiene.json" blob rc=0
  if [[ -f "$f" ]]; then
    blob="$(read_capped_file "$f" 65536)" || rc=$?
    if [[ $rc -ne 0 || -z "$blob" ]]; then
      echo "[]"
      return 1
    fi
    printf '%s\n' "$blob"
    return 0
  fi
  cat <<'JSON'
[
  {"api":"Lead","label":"Leads","filter":"IsConverted = false","fields":["Email","Phone","Company","Title","Industry"],"activityDays":90,"dupeField":"Email"},
  {"api":"Contact","label":"Contacts","filter":"","fields":["Email","Phone","Title"],"activityDays":90,"dupeField":"Email"},
  {"api":"Account","label":"Accounts","filter":"","fields":["Phone","Website","Industry"],"activityDays":180,"dupeField":"Name"},
  {"api":"Opportunity","label":"Opportunities","filter":"IsClosed = false","fields":["Amount","NextStep"],"activityDays":30,"dupeField":null,"overdue":"CloseDate < TODAY"}
]
JSON
}

hy_color() {
  local s="${1:-}"
  [[ -z "$s" || "$s" == "null" ]] && { printf '%s' "$GREY"; return; }
  if (( s >= 80 )); then printf '%s' "$GREEN"
  elif (( s >= 60 )); then printf '%s' "$YELLOW"
  else printf '%s' "$RED"; fi
}

hygiene_error_json() {
  local code="$1" message="$2"
  jq -nc --arg code "$code" --arg msg "$message" '
    {
      ok: false,
      availability: "unknown",
      overall: {score: null, grade: null, availability: "unknown"},
      measuredAt: null,
      org: {id: null, sandboxState: "unknown"},
      objects: [],
      error: {code: $code, message: $msg}
    }'
}

# Validate hygiene.json: object/field API names + filter fragments. No unsanitized SOQL.
hygiene_validate_config() {
  local cfg="$1" n idx api filter dupe overdue f
  if ! jq -e 'type=="array" and length>=1' <<<"$cfg" >/dev/null 2>&1; then
    echo "hygiene.json must be a non-empty JSON array"
    return 1
  fi
  n="$(jq 'length' <<<"$cfg")"
  for ((idx=0; idx<n; idx++)); do
    api="$(jq -r ".[$idx].api // empty" <<<"$cfg")"
    soql_api_name_ok "$api" || { echo "invalid object API name: ${api:-<empty>}"; return 1; }
    filter="$(jq -r ".[$idx].filter // empty" <<<"$cfg")"
    soql_filter_ok "$filter" || { echo "rejected filter on $api (SOQL allowlist)"; return 1; }
    dupe="$(jq -r ".[$idx].dupeField // empty" <<<"$cfg")"
    if [[ -n "$dupe" ]]; then
      soql_field_ref_ok "$dupe" || { echo "invalid dupeField on $api: $dupe"; return 1; }
    fi
    overdue="$(jq -r ".[$idx].overdue // empty" <<<"$cfg")"
    soql_filter_ok "$overdue" || { echo "rejected overdue filter on $api"; return 1; }
    while IFS= read -r f; do
      [[ -z "$f" ]] && continue
      soql_field_ref_ok "$f" || { echo "invalid field API name on $api: $f"; return 1; }
    done < <(jq -r ".[$idx].fields[]?" <<<"$cfg")
  done
  return 0
}

_unknown_probe() {
  jq -nc --arg r "${1:-transient}" '{availability:"unknown", reason:$r, count:null}'
}

_ok_probe() {
  jq -nc --argjson c "${1:-0}" '{availability:"ok", reason:null, count:$c}'
}

# Describe object. Echo describe.result JSON or set HY_OBJ_REASON and return 1.
hygiene_describe() {
  local api="$1" raw qable
  raw="$(sf_try sobject describe --sobject "$api" "${OFLAG[@]}")" || {
    HY_OBJ_REASON="$(classify_sf_error "$raw")"
    [[ "$HY_OBJ_REASON" == "invalid_field" ]] && HY_OBJ_REASON="object_forbidden"
    return 1
  }
  qable="$(jq -r '.result.queryable // true' <<<"$raw")"
  if [[ "$qable" != "true" ]]; then
    HY_OBJ_REASON="object_forbidden"
    return 1
  fi
  jq -c '.result' <<<"$raw"
}

# Field availability from describe: missing → invalid_field; accessible:false → field_forbidden.
hygiene_field_status() {
  local describe="$1" field="$2" meta acc
  meta="$(jq -c --arg n "$field" '[.fields[]? | select(.name == $n)][0] // null' <<<"$describe")"
  if [[ "$meta" == "null" || -z "$meta" ]]; then
    echo "invalid_field"
    return 1
  fi
  # jq `//` treats false as missing — do not use it for accessible.
  acc="$(jq -r 'if .accessible == false then "false" else "true" end' <<<"$meta")"
  if [[ "$acc" == "false" ]]; then
    echo "field_forbidden"
    return 1
  fi
  echo "ok"
  return 0
}

# Classify a failed COUNT that included a described field as field_forbidden.
_count_fail_reason() {
  local raw="$1" described="${2:-0}"
  local r
  r="$(classify_sf_error "$raw")"
  if [[ "$r" == "invalid_field" && "$described" == "1" ]]; then
    echo field_forbidden
  else
    echo "$r"
  fi
}

# Probe COUNT(Id) [+ COUNT(field)…] after describe. Field-by-field fallback if the batch dies.
hygiene_completeness() {
  local api="$1" where="$2" describe="$3"
  shift 3
  local candidates=("$@")
  local sel="COUNT(Id) total" i=0 f raw rec total fieldsjson idx cnt r
  for f in "${candidates[@]}"; do
    sel="$sel, COUNT($f) f$i"
    i=$((i+1))
  done
  raw="$(sf_try data query "${OFLAG[@]}" -q "SELECT $sel FROM $api$where")" || {
    # Batch died — do not mark the object dead. Probe COUNT(Id), then each field.
    raw="$(sf_try data query "${OFLAG[@]}" -q "SELECT COUNT(Id) total FROM $api$where")" || {
      echo "$(_count_fail_reason "$raw" 0)"
      return 1
    }
    rec="$(jq -c '.result.records[0] // {}' <<<"$raw")"
    total="$(jq -r '.total // 0' <<<"$rec")"
    fieldsjson="{}"
    idx=0
    for f in "${candidates[@]}"; do
      raw="$(sf_try data query "${OFLAG[@]}" -q "SELECT COUNT($f) c FROM $api$where")" || {
        r="$(_count_fail_reason "$raw" 1)"
        fieldsjson="$(jq -c --arg n "$f" --arg r "$r" \
          '. + {($n): {availability:"unknown", reason:$r, filled:null}}' <<<"$fieldsjson")"
        continue
      }
      cnt="$(jq -r '.result.records[0].c // 0' <<<"$raw")"
      fieldsjson="$(jq -c --arg n "$f" --argjson v "${cnt:-0}" \
        '. + {($n): {availability:"ok", reason:null, filled:$v}}' <<<"$fieldsjson")"
    done
    jq -nc --argjson t "${total:-0}" --argjson fields "$fieldsjson" '{total:$t, fields:$fields}'
    return 0
  }
  rec="$(jq -c '.result.records[0] // {}' <<<"$raw")"
  total="$(jq -r '.total // 0' <<<"$rec")"
  fieldsjson="{}"
  idx=0
  for f in "${candidates[@]}"; do
    cnt="$(jq -r --arg k "f$idx" '.[$k] // 0' <<<"$rec")"
    fieldsjson="$(jq -c --arg n "$f" --argjson v "${cnt:-0}" \
      '. + {($n): {availability:"ok", reason:null, filled:$v}}' <<<"$fieldsjson")"
    idx=$((idx+1))
  done
  jq -nc --argjson t "${total:-0}" --argjson fields "$fieldsjson" '{total:$t, fields:$fields}'
}

_dim_count() {
  local q="$1"
  local raw c
  raw="$(sf_try data query "${OFLAG[@]}" -q "$q")" || {
    _unknown_probe "$(_count_fail_reason "$raw" 1)"
    return 0
  }
  c="$(jq -r '.result.records[0].c // 0' <<<"$raw")"
  _ok_probe "${c:-0}"
}

_hygiene_dead_object() {
  local api="$1" label="$2" reason="$3" days="$4" dupe="$5" overdue="$6"
  jq -nc --arg api "$api" --arg label "$label" --arg r "$reason" --argjson days "$days" \
    --arg dupe "$dupe" --arg overdue "$overdue" '
    {
      api:$api, label:$label, availability:"unknown", reason:$r, total:null,
      activityDays:$days, dupeField:$dupe,
      fields:{},
      stale: {availability:"unknown", reason:$r, count:null},
      orphaned: {availability:"unknown", reason:$r, count:null},
      dupes: {availability:"unknown", reason:$r, groups:null, records:null, truncated:false},
      overdue: (if $overdue != "" then {availability:"unknown", reason:$r, count:null, openTotal:null} else null end)
    }'
}

# Worker: describe first, then COUNT only measured fields. Object stays available
# if any field (or COUNT(Id)) is measured.
hygiene_object() {
  local obj="$1" api label filter dupeField overdue days
  api="$(jq -r '.api' <<<"$obj")"
  label="$(jq -r '.label // .api' <<<"$obj")"
  filter="$(jq -r '.filter // ""' <<<"$obj")"
  dupeField="$(jq -r '.dupeField // empty' <<<"$obj")"
  overdue="$(jq -r '.overdue // empty' <<<"$obj")"
  days="$(jq -r '.activityDays // 90' <<<"$obj")"
  [[ "$days" =~ ^[0-9]+$ ]] && (( days >= 1 && days <= 3650 )) || days=90

  local describe
  HY_OBJ_REASON=""
  describe="$(hygiene_describe "$api")" || {
    _hygiene_dead_object "$api" "$label" "${HY_OBJ_REASON:-object_forbidden}" "$days" "$dupeField" "$overdue"
    return 0
  }

  local where="" andf=""
  [[ -n "$filter" ]] && { where=" WHERE $filter"; andf=" AND ($filter)"; }

  local fieldsjson="{}" candidates=() f st
  while IFS= read -r f; do
    [[ -z "$f" ]] && continue
    st="$(hygiene_field_status "$describe" "$f" || true)"
    if [[ "$st" != "ok" ]]; then
      [[ -z "$st" ]] && st="invalid_field"
      fieldsjson="$(jq -c --arg n "$f" --arg r "$st" \
        '. + {($n): {availability:"unknown", reason:$r, filled:null}}' <<<"$fieldsjson")"
    else
      candidates+=("$f")
    fi
  done < <(jq -r '.fields[]?' <<<"$obj")

  local comp raw_reason
  if ! comp="$(hygiene_completeness "$api" "$where" "$describe" "${candidates[@]}")"; then
    raw_reason="$comp"
    _hygiene_dead_object "$api" "$label" "${raw_reason:-transient}" "$days" "$dupeField" "$overdue"
    return 0
  fi

  local total
  total="$(jq -r '.total // 0' <<<"$comp")"
  fieldsjson="$(jq -nc --argjson a "$fieldsjson" --argjson c "$comp" '$a + $c.fields')"

  local stale orphaned
  stale="$(_dim_count "SELECT COUNT(Id) c FROM $api WHERE (LastActivityDate < LAST_N_DAYS:$days OR LastActivityDate = null)$andf")"
  orphaned="$(_dim_count "SELECT COUNT(Id) c FROM $api WHERE Owner.IsActive = false$andf")"

  local dupes
  if [[ -n "$dupeField" ]]; then
    local d raw glen grec
    raw="$(sf_try data query "${OFLAG[@]}" -q \
      "SELECT COUNT(Id) ct FROM $api WHERE $dupeField != null$andf GROUP BY $dupeField HAVING COUNT(Id) > 1 LIMIT $DUPE_GROUP_CAP")" || {
      d="$(_count_fail_reason "$raw" 1)"
      dupes="$(jq -nc --arg r "$d" '{availability:"unknown", reason:$r, groups:null, records:null, truncated:false}')"
    }
    if [[ -z "${dupes:-}" ]]; then
      d="$(jq -c '.result' <<<"$raw")"
      glen="$(jq -r '.records | length' <<<"$d")"
      grec="$(jq -r '[.records[].ct] | add // 0' <<<"$d")"
      local trunc=false
      (( glen >= DUPE_GROUP_CAP )) && trunc=true
      dupes="$(jq -nc --argjson g "$glen" --argjson r "$grec" --argjson t "$trunc" \
        '{availability:"ok", reason:null, groups:$g, records:$r, truncated:$t}')"
    fi
  else
    dupes="$(jq -nc '{availability:"ok", reason:null, groups:0, records:0, truncated:false}')"
  fi

  local overduej="null"
  if [[ -n "$overdue" ]]; then
    local oc
    oc="$(_dim_count "SELECT COUNT(Id) c FROM $api WHERE $overdue$andf")"
    overduej="$(jq -c --argjson t "${total:-0}" '. + {openTotal:$t}' <<<"$oc")"
  fi

  jq -nc \
    --arg api "$api" --arg label "$label" --argjson days "$days" --arg dupe "${dupeField:-}" \
    --argjson total "${total:-0}" --argjson fields "$fieldsjson" \
    --argjson stale "$stale" --argjson orphaned "$orphaned" --argjson dupes "$dupes" \
    --argjson overdue "$overduej" '
    {
      api:$api, label:$label, availability:"ok", reason:null, total:$total,
      activityDays:$days, dupeField:$dupe,
      fields:$fields, stale:$stale, orphaned:$orphaned, dupes:$dupes, overdue:$overdue
    }'
}

_hygiene_score_jq() {
  cat <<'JQ'
def fill_rate($filled; $n; $t):
  if $t == null or $t <= 0 or $n <= 0 then null
  else (100 * $filled / ($n * $t)) | floor end;

def leftover($bad; $t):
  if $t == null or $t <= 0 or $bad == null then null
  else (100 - 100 * $bad / $t) | floor end;

def letter($s):
  if $s == null then null
  elif $s >= 90 then "A"
  elif $s >= 80 then "B"
  elif $s >= 70 then "C"
  elif $s >= 60 then "D"
  else "F" end;

def score_object:
  . as $o
  | if $o.availability != "ok" then
      $o + {
        score: null,
        dimensions: {
          completeness: {availability:"unknown", reason:$o.reason, score:null, fields:{}},
          freshness:    {availability:"unknown", reason:$o.reason, score:null},
          ownership:    {availability:"unknown", reason:$o.reason, score:null},
          dupes:        {availability:"unknown", reason:$o.reason, score:null, truncated:false},
          pipeline:     (if $o.overdue != null then {availability:"unknown", reason:$o.reason, score:null} else null end)
        }
      }
    else
      ([($o.fields // {}) | to_entries[] | select(.value.availability == "ok")]) as $measured
      | ([($o.fields // {}) | to_entries[] | select(.value.availability != "ok")]) as $unkf
      | ($measured | length) as $n
      | ([ $measured[].value.filled ] | add // 0) as $filled
      | (
          if $n == 0 then
            {availability: (if ($unkf|length) > 0 then "unknown" else "ok" end),
             reason: (if ($unkf|length) > 0 then $unkf[0].value.reason else null end),
             score: null, fields: $o.fields}
          elif ($o.total // 0) == 0 then
            {availability: (if ($unkf|length) > 0 then "partial" else "ok" end),
             reason: null, score: null, fields: $o.fields}
          else
            {availability: (if ($unkf|length) > 0 then "partial" else "ok" end),
             reason: null, score: fill_rate($filled; $n; $o.total), fields: $o.fields}
          end
        ) as $comp
      | (if ($o.stale.availability // "unknown") == "ok" then
           {availability:"ok", reason:null, score: leftover($o.stale.count; $o.total)}
         else
           {availability:"unknown", reason:($o.stale.reason // "transient"), score:null}
         end) as $fresh
      | (if ($o.orphaned.availability // "unknown") == "ok" then
           {availability:"ok", reason:null, score: leftover($o.orphaned.count; $o.total)}
         else
           {availability:"unknown", reason:($o.orphaned.reason // "transient"), score:null}
         end) as $own
      | (if ($o.dupes.availability // "unknown") == "ok" then
           ((($o.dupes.records // 0) - ($o.dupes.groups // 0)) as $ex
            | {availability:"ok", reason:null, score: leftover($ex; $o.total),
               truncated: ($o.dupes.truncated // false),
               groups: $o.dupes.groups, records: $o.dupes.records})
         else
           {availability:"unknown", reason:($o.dupes.reason // "transient"), score:null, truncated:false}
         end) as $dup
      | (if $o.overdue == null then null
         elif ($o.overdue.availability // "unknown") == "ok" then
           {availability:"ok", reason:null, score: leftover($o.overdue.count; $o.overdue.openTotal)}
         else
           {availability:"unknown", reason:($o.overdue.reason // "transient"), score:null}
         end) as $pipe
      | ([$comp, $fresh, $own, $dup] + (if $pipe != null then [$pipe] else [] end)) as $dims
      | ([ $dims[] | select(.availability == "ok" and .score != null) | .score ]) as $scores
      | (if ($o.total // 0) == 0 then null
         elif ($scores|length) == 0 then null
         else (($scores|add) / ($scores|length) | floor) end) as $oscore
      | $o + {score:$oscore, dimensions:{
          completeness:$comp, freshness:$fresh, ownership:$own, dupes:$dup, pipeline:$pipe
        }}
    end;

(.objects | map(score_object)) as $objects
| [ $objects[] | select(.availability == "ok" and (.total // 0) > 0 and .score != null) ] as $scored
| [ $objects[] | select(.availability == "unknown") ] as $unk
| [ $objects[] | select(.availability == "ok") ] as $okobjs
| (
    if ($okobjs|length) == 0 then "unknown"
    elif ($unk|length) > 0 then "partial"
    elif any($objects[];
          .availability == "ok" and (
            .dimensions.completeness.availability == "unknown"
            or .dimensions.freshness.availability == "unknown"
            or .dimensions.ownership.availability == "unknown"
            or .dimensions.dupes.availability == "unknown"
            or (.dimensions.pipeline != null and .dimensions.pipeline.availability == "unknown")
          )) then "partial"
    else "ok"
    end
  ) as $avail
| (if ($scored|length) == 0 then null
   else ([ $scored[].score ] | add / length | floor) end) as $overall_score
| (if $avail == "unknown" then null else $overall_score end) as $out_score
| {
    ok: (.scanError == null),
    availability: $avail,
    overall: {
      score: $out_score,
      grade: (if $avail == "unknown" then null else letter($out_score) end),
      availability: $avail
    },
    measuredAt: .measuredAt,
    org: .org,
    objects: $objects,
    error: .scanError,
    wins: (
      [ $objects[] | select(.availability == "ok" and (.total // 0) > 0) | . as $o
        | (
            (( $o.fields // {} ) | to_entries[] | select(.value.availability == "ok" and .value.filled != null and .value.filled < $o.total)
              | ($o.total - .value.filled) as $m
              | {label:"\($m) \($o.label) missing \(.key)", count:$m, kind:"completeness"}),
            (if ($o.stale.availability == "ok") and (($o.stale.count // 0) > 0) then
               {label:"\($o.stale.count) \($o.label) stale (no activity \($o.activityDays)d+)", count:$o.stale.count, kind:"freshness"}
             else empty end),
            (if ($o.orphaned.availability == "ok") and (($o.orphaned.count // 0) > 0) then
               {label:"\($o.orphaned.count) \($o.label) owned by inactive users", count:$o.orphaned.count, kind:"ownership"}
             else empty end),
            (if ($o.dupes.availability == "ok")
                and ((($o.dupes.records // 0) - ($o.dupes.groups // 0)) > 0) then
               ((($o.dupes.records // 0) - ($o.dupes.groups // 0)) as $ex
                | {label:"\($ex) duplicate \($o.label) by \($o.dupeField)", count:$ex, kind:"dupes"})
             else empty end),
            (if ($o.overdue != null) and ($o.overdue.availability == "ok") and (($o.overdue.count // 0) > 0) then
               {label:"\($o.overdue.count) \($o.label) overdue (past close date)", count:$o.overdue.count, kind:"pipeline"}
             else empty end)
          ) ]
      | sort_by(.count) | reverse | .[0:6]
    )
  }
JQ
}

hygiene_org_blob() {
  local snap="${1:-}"
  if [[ -z "$snap" ]]; then
    jq -nc '{id:null, sandboxState:"unknown"}'
    return
  fi
  jq -n --arg id "$(org_id "$snap")" --arg st "$(sandbox_state "$snap")" \
    '{id: (if $id == "unknown" then null else $id end), sandboxState:$st}'
}

# Persist only ok|partial with measuredAt and no transient/timeout/auth error.
hygiene_should_persist() {
  local h="$1" avail code
  avail="$(jq -r '.availability // "unknown"' <<<"$h")"
  code="$(jq -r '.error.code // empty' <<<"$h")"
  case "$code" in
    transient|timeout|sf_missing|unauthenticated|config_invalid) return 1 ;;
  esac
  [[ "$avail" == "ok" || "$avail" == "partial" ]] || return 1
  jq -e '.measuredAt != null and (.objects | map(select(.availability=="ok")) | length) > 0' <<<"$h" >/dev/null
}

compute_hygiene() {
  have_sf || { hygiene_error_json sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local cfg err
  cfg="$(hygiene_config)" || { hygiene_error_json config_invalid "could not read hygiene config"; return 1; }
  if ! err="$(hygiene_validate_config "$cfg")"; then
    hygiene_error_json config_invalid "$err"
    return 1
  fi

  local snap orgblob
  snap="$(load_snapshot 2>/dev/null)" || snap=""
  orgblob="$(hygiene_org_blob "$snap")"

  HY_SCAN_TRANSIENT=0
  local tmp n idx jobs running=0
  jobs="${BLUECHIP_HYGIENE_JOBS:-4}"
  (( jobs < 1 )) && jobs=1
  (( jobs > 8 )) && jobs=8
  tmp="$(mktemp -d "${TMPDIR:-/tmp}/bluechip.XXXXXX")" || {
    hygiene_error_json transient "could not create temp dir"
    return 1
  }
  n="$(jq 'length' <<<"$cfg")"
  _hygiene_kill_jobs() {
    local p
    for p in $(jobs -p 2>/dev/null); do
      kill "$p" 2>/dev/null || true
    done
    wait 2>/dev/null || true
  }
  trap '_hygiene_kill_jobs; rm -rf "$tmp"' RETURN
  for ((idx=0; idx<n; idx++)); do
    hygiene_object "$(jq -c ".[$idx]" <<<"$cfg")" > "$tmp/$(printf '%02d' "$idx").json" 2>/dev/null &
    running=$((running + 1))
    if (( running >= jobs )); then
      if ! wait -n 2>/dev/null; then
        wait 2>/dev/null || true
        running=0
      else
        running=$((running - 1))
      fi
    fi
  done
  wait 2>/dev/null || true
  trap - RETURN
  local combined
  combined="$(cat "$tmp"/*.json 2>/dev/null | jq -s '.')"
  rm -rf "$tmp"
  [[ -n "$combined" && "$combined" != "null" ]] || {
    hygiene_error_json transient "hygiene scan produced no object results"
    return 1
  }

  local scan_error="null"
  if [[ "${HY_SCAN_TRANSIENT:-0}" == "1" ]] \
     || jq -e '[.[] | select(.reason=="transient" or .reason=="timeout")] | length > 0' <<<"$combined" >/dev/null; then
    local why
    why="$(jq -r '[.[] | select(.reason=="transient" or .reason=="timeout") | .reason][0] // "transient"' <<<"$combined")"
    scan_error="$(jq -nc --arg c "$why" --arg m "hygiene scan hit $why mid-probe" '{code:$c, message:$m}')"
  fi

  jq -n \
    --argjson objects "$combined" \
    --argjson org "$orgblob" \
    --argjson scanError "$scan_error" \
    --argjson measuredAt "$(date +%s)" \
    '{objects:$objects, org:$org, scanError:$scanError, measuredAt:$measuredAt}' \
    | jq "$(_hygiene_score_jq)"
}

hygiene_doctor_line() {
  local f="$HYGIENE_CACHE" e="$HYGIENE_ERROR_FILE"
  [[ -f "$f" ]] || { printf '%sunknown (not run)%s' "$GREY" "$RESET"; return; }
  local avail grade score
  avail="$(jq -r '.overall.availability // .availability // empty' "$f" 2>/dev/null)"
  grade="$(jq -r '.overall.grade // empty' "$f" 2>/dev/null)"
  score="$(jq -r '.overall.score // empty' "$f" 2>/dev/null)"
  if [[ "$avail" == "unknown" || -z "$grade" || "$grade" == "null" ]]; then
    printf '%sunknown (not graded)%s' "$GREY" "$RESET"
  else
    printf '%s%s%s %s(%s/100)%s' "$(hy_color "$score")$BOLD" "$grade" "$RESET" "$DIM" "$score" "$RESET"
    [[ "$avail" == "partial" ]] && printf ' %s· partial%s' "$DIM" "$RESET"
  fi
  if [[ -f "$e" ]]; then
    printf ' %s· stale%s' "$YELLOW" "$RESET"
  fi
}

_hygiene_dim_txt() {
  local score="$1" avail="$2"
  if [[ "$avail" != "ok" && "$avail" != "partial" ]]; then
    printf '?'
  elif [[ -z "$score" || "$score" == "null" ]]; then
    printf '?'
  else
    printf '%s' "$score"
  fi
}

cmd_hygiene() {
  local want_json=0
  [[ "${1:-}" == "--json" || "${WANT_JSON:-0}" -eq 1 ]] && want_json=1

  if ! have_sf; then
    local err
    err="$(hygiene_error_json sf_missing "sf CLI not found. Install @salesforce/cli and run: sf org login web")"
    if [[ $want_json -eq 1 ]]; then printf '%s\n' "$err"; else printf '%s\n' "bluechip: sf CLI not found." >&2; fi
    return 1
  fi

  local cfg verr
  cfg="$(hygiene_config)" || {
    local err; err="$(hygiene_error_json config_invalid "could not read hygiene config")"
    [[ $want_json -eq 1 ]] && printf '%s\n' "$err"
    return 1
  }
  if ! verr="$(hygiene_validate_config "$cfg")"; then
    local err; err="$(hygiene_error_json config_invalid "$verr")"
    if [[ $want_json -eq 1 ]]; then printf '%s\n' "$err"; else printf 'bluechip: %s\n' "$verr" >&2; fi
    return 1
  fi

  # Snapshot miss (no auth / offline) is a structured error — do not invent an org or a grade.
  if ! load_snapshot >/dev/null 2>&1; then
    local code="${SNAPSHOT_ERROR:-unauthenticated}"
    local err
    err="$(hygiene_error_json "$code" "could not read org ($code)")"
    if [[ $want_json -eq 1 ]]; then printf '%s\n' "$err"; else printf 'bluechip: could not read org (%s).\n' "$code" >&2; fi
    return 1
  fi

  [[ $want_json -eq 0 ]] && printf '%s  probing org data — describe first, then read-only aggregates…%s\n' "$DIM" "$RESET" >&2

  local h
  h="$(compute_hygiene)" || h="$(hygiene_error_json transient "hygiene scan failed")"

  if hygiene_should_persist "$h"; then
    printf '%s\n' "$h" | write_secure "$HYGIENE_CACHE"
    rm -f "$HYGIENE_ERROR_FILE"
  else
    # Failed / all-unknown / transient: keep last-good. Record the miss for doctor stale.
    if jq -e '.error != null' <<<"$h" >/dev/null 2>&1 \
       || [[ "$(jq -r '.availability' <<<"$h")" == "unknown" ]]; then
      ensure_state_dir
      jq -n --argjson e "$(jq -c '.error // {code:"unknown",message:"scan not persisted"}' <<<"$h")" \
        --argjson ts "$(date +%s)" '{error:$e, ts:$ts}' | write_secure "$HYGIENE_ERROR_FILE"
    fi
  fi

  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi

  local snap avail grade score gcolor
  snap="$(load_snapshot 2>/dev/null)" || snap=""
  avail="$(jq -r '.availability' <<<"$h")"
  grade="$(jq -r '.overall.grade // empty' <<<"$h")"
  score="$(jq -r '.overall.score // empty' <<<"$h")"

  if [[ "$avail" == "unknown" || -z "$grade" || "$grade" == "null" ]]; then
    printf '\n  %sOrg data probe%s   %sunknown (not graded)%s   %s%s%s\n\n' \
      "$BOLD" "$RESET" "$GREY" "$RESET" "$GREY" "$(org_name "${snap:-"{}"}")" "$RESET"
  else
    gcolor="$(hy_color "$score")"
    printf '\n  %sOrg data probe%s   %s%s%s %s(%s/100 · %s)%s   %s%s%s\n\n' \
      "$BOLD" "$RESET" "$gcolor$BOLD" "$grade" "$RESET" "$DIM" "$score" "$avail" "$RESET" \
      "$GREY" "$(org_name "$snap")" "$RESET"
  fi

  jq -r '.objects[] | [
      .api, .label, (.availability//"unknown"), (.reason//""), ((.total//"")|tostring),
      ((.score//"")|tostring),
      (.dimensions.completeness.availability//""), ((.dimensions.completeness.score//"")|tostring),
      (.dimensions.freshness.availability//""), ((.dimensions.freshness.score//"")|tostring),
      (.dimensions.ownership.availability//""), ((.dimensions.ownership.score//"")|tostring),
      (.dimensions.dupes.availability//""), ((.dimensions.dupes.score//"")|tostring),
      ((.dimensions.pipeline.availability//"")|tostring), ((.dimensions.pipeline.score//"")|tostring)
    ] | @tsv' <<<"$h" \
  | while IFS=$'\t' read -r api label oavail reason total oscore cav csc fav fsc oav osc dav dsc pav psc; do
      label="$(neutralize_plaintext "$label" 80)"
      if [[ "$oavail" != "ok" ]]; then
        printf '   %-20s %sunknown%s %s(%s)%s\n' "$label" "$GREY" "$RESET" "$DIM" "$(neutralize_plaintext "${reason:-unknown}" 64)" "$RESET"
        continue
      fi
      if [[ "$total" == "0" ]]; then
        printf '   %-20s %sno records%s\n' "$label" "$DIM" "$RESET"
        continue
      fi
      if [[ -n "$oscore" && "$oscore" != "null" ]]; then
        printf '   %-20.20s %s %s%3d%s   %s(%s recs)%s\n' \
          "$label" "$(bar_gauge "$oscore" 12)" "$(hy_color "$oscore")$BOLD" "$oscore" "$RESET" "$DIM" "$total" "$RESET"
      else
        printf '   %-20.20s %snot scored (no measured dimension)%s   %s(%s recs)%s\n' \
          "$label" "$GREY" "$RESET" "$DIM" "$total" "$RESET"
      fi
      printf '      %scompleteness %s · freshness %s · owners %s · dupes %s%s\n' \
        "$DIM" "$(_hygiene_dim_txt "$csc" "$cav")" "$(_hygiene_dim_txt "$fsc" "$fav")" \
        "$(_hygiene_dim_txt "$osc" "$oav")" "$(_hygiene_dim_txt "$dsc" "$dav")" "$RESET"
    done

  if jq -e '.wins | length > 0' <<<"$h" >/dev/null; then
    printf '\n  %sBiggest measured gaps%s\n' "$BOLD" "$RESET"
    jq -r '.wins[] | .label' <<<"$h" | while IFS= read -r w; do
      printf '   • %s\n' "$(neutralize_plaintext "$w" 200)"
    done
  fi

  if [[ -n "${BLUECHIP_REMEDIATE_URL:-}" ]]; then
    printf '\n  %sOptional attach%s (local probe ≠ OutboundSync product): %s\n' \
      "$DIM" "$RESET" "$BLUECHIP_REMEDIATE_URL"
  fi

  if jq -e '.ok == false' <<<"$h" >/dev/null; then
    return 1
  fi
  return 0
}
