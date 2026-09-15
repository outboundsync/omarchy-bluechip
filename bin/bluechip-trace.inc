# shellcheck shell=bash
# TraceFlag start/status/stop (confirm-gated writes) + ApexLog tail (read).
# MCP must not call start/stop. Bodies are neutralized and size-capped.

LOG_MAX_BYTES="${BLUECHIP_LOG_MAX_BYTES:-8192}"
LOG_MAX_COUNT="${BLUECHIP_LOG_MAX_COUNT:-20}"

_trace_iso_now() { date -u +%Y-%m-%dT%H:%M:%S.000+0000; }

_trace_iso_plus_minutes() {
  local m="$1"
  date -u -d "+${m} minutes" +%Y-%m-%dT%H:%M:%S.000+0000 2>/dev/null \
    || date -u -v+"${m}"M +%Y-%m-%dT%H:%M:%S.000+0000
}

_trace_error() {
  local code="$1" message="$2"
  jq -nc --arg code "$code" --arg msg "$message" \
    '{ok:false,availability:"unknown",org:{id:null,sandboxState:"unknown",alias:null},flags:[],error:{code:$code,message:$msg}}'
}

_current_user_id() {
  local snap="$1" user uname raw
  user="$(jq -r '.display.userId // empty' <<<"$snap")"
  if sf_record_id_ok "$user"; then
    printf '%s' "$user"
    return 0
  fi
  uname="$(jq -r '.display.username // empty' <<<"$snap")"
  [[ -z "$uname" || "$uname" == *$'\n'* ]] && return 1
  raw="$(sf_try data query "${OFLAG[@]}" -q \
    "SELECT Id FROM User WHERE Username = '$(soql_escape_literal "$uname")' LIMIT 1")" || return 1
  user="$(jq -r '.result.records[0].Id // empty' <<<"$raw")"
  sf_record_id_ok "$user" || return 1
  printf '%s' "$user"
}

_trace_query_flags() {
  _nc_tooling_try \
    "SELECT Id, LogType, StartDate, ExpirationDate, TracedEntityId, DebugLevelId, TracedEntity.Name FROM TraceFlag" \
    "SELECT Id, LogType, StartDate, ExpirationDate, TracedEntityId, DebugLevelId FROM TraceFlag"
}

# If named-creds include is not sourced (shouldn't happen), provide a tiny fallback.
# _nc_tooling_try is defined in bluechip-named-creds.inc (sourced first).

compute_trace_status() {
  have_sf || { _trace_error sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob raw
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  orgblob="$(org_json_blob "$snap")"

  raw="$(_trace_query_flags)" || {
    local why hard=false
    why="$(classify_sf_error "${raw:-}")"
    case "$why" in unauthenticated|transient|timeout|sf_missing) hard=true ;; esac
    jq -n --argjson org "$orgblob" --arg c "$why" --argjson hard "$hard" \
      '{ok:($hard|not),availability:"unknown",org:$org,flags:[],error:{code:$c,message:("TraceFlag Tooling query failed: "+$c)}}'
    return 0
  }

  jq -n --argjson org "$orgblob" --argjson raw "$raw" '
    {
      ok: true,
      availability: "ok",
      org: $org,
      flags: [
        ($raw.result.records // [])[]?
        | {
            id: .Id,
            logType: .LogType,
            tracedEntityId: .TracedEntityId,
            tracedEntityName: (.TracedEntity.Name // null),
            debugLevelId: .DebugLevelId,
            startDate: .StartDate,
            expirationDate: .ExpirationDate
          }
      ],
      error: null
    }' | redact_secrets_json
}

_trace_pick_debug_level() {
  local raw id
  raw="$(_nc_tooling_try \
    "SELECT Id, DeveloperName, MasterLabel FROM DebugLevel ORDER BY LastModifiedDate DESC LIMIT 20" \
    "SELECT Id, DeveloperName FROM DebugLevel LIMIT 20")" || return 1
  id="$(jq -r '
    (.result.records // [])
    | (map(select(.DeveloperName == "SFDC_DevConsole" or .DeveloperName == "Bluechip_DEBUG")) | .[0].Id)
      // .[0].Id // empty
  ' <<<"$raw")"
  sf_record_id_ok "$id" || return 1
  printf '%s' "$id"
}

_trace_create_debug_level() {
  local raw
  raw="$(sf_try data create record --use-tooling-api --sobject DebugLevel "${OFLAG[@]}" \
    --values "DeveloperName=Bluechip_DEBUG MasterLabel=Bluechip_DEBUG ApexCode=DEBUG ApexProfiling=INFO Callout=INFO Database=INFO System=INFO Validation=INFO Visualforce=INFO Workflow=INFO")" \
    || return 1
  jq -r '.result.id // .result.Id // empty' <<<"$raw"
}

cmd_trace_status() {
  local want_json=0
  [[ "${1:-}" == "--json" || "${WANT_JSON:-0}" -eq 1 ]] && want_json=1
  local h
  h="$(compute_trace_status)" || h="$(_trace_error transient "TraceFlag status failed")"
  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi
  local snap state env
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  case "$(sandbox_state "${snap:-"{}"}")" in
    sandbox) env="SANDBOX" ;;
    prod) env="PROD" ;;
    *) env="UNKNOWN" ;;
  esac
  printf '%sTraceFlags%s  %s%s%s  %s%s%s\n\n' \
    "$BOLD" "$RESET" "$YELLOW" "$env" "$RESET" "$GREY" "$(org_id "${snap:-"{}"}")" "$RESET"
  if [[ "$(jq -r '.availability' <<<"$h")" != "ok" ]]; then
    printf '  %sunknown%s  %s\n' "$GREY" "$RESET" \
      "$(neutralize_plaintext "$(jq -r '.error.message // "Tooling TraceFlag unavailable"' <<<"$h")" 200)"
    return 0
  fi
  local n; n="$(jq -r '.flags | length' <<<"$h")"
  if [[ "$n" == "0" ]]; then
    printf '  %sNo TraceFlags.%s  Start one: bluechip trace start\n' "$DIM" "$RESET"
    return 0
  fi
  jq -r '.flags[] | [.id, (.logType//"?"), (.tracedEntityName//.tracedEntityId//"?"), (.expirationDate//"—")] | @tsv' <<<"$h" \
  | while IFS=$'\t' read -r id typ who exp; do
      printf '  %s  %-14s  %s  exp %s\n' \
        "$(neutralize_plaintext "$id" 22)" \
        "$(neutralize_plaintext "$typ" 14)" \
        "$(neutralize_plaintext "$who" 40)" \
        "$(neutralize_plaintext "$exp" 32)"
    done
}

_trace_parse_minutes() {
  local m="${1:-30}"
  [[ "$m" =~ ^[0-9]+$ ]] && (( m >= 1 && m <= 1440 )) || die "minutes must be 1–1440"
  printf '%s' "$m"
}

cmd_trace_start() {
  local minutes=30 user_arg="" want_json=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      --minutes) minutes="$2"; shift 2 ;;
      --minutes=*) minutes="${1#*=}"; shift ;;
      --user) user_arg="$2"; shift 2 ;;
      --user=*) user_arg="${1#*=}"; shift ;;
      --yes) WANT_YES=1; shift ;;
      *) die "unknown trace start option: $1" ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1
  minutes="$(_trace_parse_minutes "$minutes")"

  have_sf || { [[ $want_json -eq 1 ]] && _trace_error sf_missing "sf CLI not found"; die "sf CLI not found."; }
  set_oflag
  local snap; take_snapshot || die "could not read org."
  snap="$SNAPSHOT_BLOB"
  local state oid alias
  state="$(sandbox_state "$snap")"
  oid="$(org_id "$snap")"
  alias="$(jq -r '.display.alias // empty' <<<"$snap")"
  [[ -z "$alias" ]] && alias="$(pinned_org)"
  [[ -z "$alias" ]] && alias="org"

  write_confirm "TraceFlag start" "$state" "$oid" "$alias" || return 1

  local uid
  if [[ -n "$user_arg" ]]; then
    [[ "$user_arg" == *$'\n'* || ${#user_arg} -gt 255 ]] && die "invalid --user"
    local rawu
    rawu="$(sf_try data query "${OFLAG[@]}" -q \
      "SELECT Id FROM User WHERE Username = '$(soql_escape_literal "$user_arg")' LIMIT 1")" \
      || die "could not resolve user"
    uid="$(jq -r '.result.records[0].Id // empty' <<<"$rawu")"
  else
    uid="$(_current_user_id "$snap")" || die "could not resolve current user Id"
  fi
  sf_record_id_ok "$uid" || die "invalid traced user id"

  local dl
  dl="$(_trace_pick_debug_level)" || dl="$(_trace_create_debug_level)" \
    || die "could not find or create a DebugLevel (Tooling write failed)"
  sf_record_id_ok "$dl" || die "invalid DebugLevel id"

  local start exp raw
  start="$(_trace_iso_now)"
  exp="$(_trace_iso_plus_minutes "$minutes")"
  raw="$(sf_try data create record --use-tooling-api --sobject TraceFlag "${OFLAG[@]}" \
    --values "TracedEntityId=$uid DebugLevelId=$dl LogType=USER_DEBUG StartDate=$start ExpirationDate=$exp")" \
    || {
      local why
      why="$(classify_sf_error "$raw")"
      if [[ $want_json -eq 1 ]]; then
        _trace_error "$why" "TraceFlag create failed ($why)"
      else
        printf 'bluechip: TraceFlag create failed (%s).\n' "$why" >&2
      fi
      return 1
    }

  local fid
  fid="$(jq -r '.result.id // .result.Id // empty' <<<"$raw")"
  local out
  out="$(jq -n --argjson org "$(org_json_blob "$snap")" --arg id "$fid" \
    --arg uid "$uid" --arg dl "$dl" --arg start "$start" --arg exp "$exp" \
    '{ok:true,availability:"ok",org:$org,created:{id:$id,tracedEntityId:$uid,debugLevelId:$dl,startDate:$start,expirationDate:$exp,logType:"USER_DEBUG"},error:null}')"
  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$out"
    return 0
  fi
  printf '%sTraceFlag started%s  %s (%sm)\n' "$GREEN$BOLD" "$RESET" "$(neutralize_plaintext "$fid" 22)" "$minutes"
  printf '  user %s · level %s · exp %s\n' \
    "$(neutralize_plaintext "$uid" 22)" "$(neutralize_plaintext "$dl" 22)" "$exp"
  printf '  tail: %sbluechip logs --follow%s\n' "$BOLD" "$RESET"
}

cmd_trace_stop() {
  local target="" want_json=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      --id) target="$2"; shift 2 ;;
      --id=*) target="${1#*=}"; shift ;;
      --yes) WANT_YES=1; shift ;;
      *)
        if sf_record_id_ok "$1"; then target="$1"; shift
        else die "unknown trace stop option: $1"; fi
        ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1
  have_sf || die "sf CLI not found."
  set_oflag
  local snap; take_snapshot || die "could not read org."
  snap="$SNAPSHOT_BLOB"
  local state oid alias
  state="$(sandbox_state "$snap")"
  oid="$(org_id "$snap")"
  alias="$(jq -r '.display.alias // empty' <<<"$snap")"
  [[ -z "$alias" ]] && alias="$(pinned_org)"
  [[ -z "$alias" ]] && alias="org"

  write_confirm "TraceFlag stop" "$state" "$oid" "$alias" || return 1

  local ids=()
  if [[ -n "$target" ]]; then
    sf_record_id_ok "$target" || die "invalid TraceFlag id"
    ids=("$target")
  else
    local st
    st="$(compute_trace_status)"
    mapfile -t ids < <(jq -r '.flags[]?.id // empty' <<<"$st")
  fi
  if [[ ${#ids[@]} -eq 0 ]]; then
    if [[ $want_json -eq 1 ]]; then
      jq -n --argjson org "$(org_json_blob "$snap")" \
        '{ok:true,availability:"ok",org:$org,deleted:[],error:null}'
    else
      echo "No TraceFlags to stop."
    fi
    return 0
  fi

  local deleted=() failed=()
  local id raw why
  for id in "${ids[@]}"; do
    [[ -z "$id" ]] && continue
    raw="$(sf_try data delete record --use-tooling-api --sobject TraceFlag --record-id "$id" "${OFLAG[@]}")" || {
      why="$(classify_sf_error "$raw")"
      failed+=("$id:$why")
      continue
    }
    deleted+=("$id")
  done

  if [[ $want_json -eq 1 ]]; then
    local delj failj
    delj="$(printf '%s\n' "${deleted[@]}" | jq -R -s 'split("\n") | map(select(length>0))')"
    failj="$(printf '%s\n' "${failed[@]}" | jq -R -s 'split("\n") | map(select(length>0))')"
    jq -n --argjson org "$(org_json_blob "$snap")" --argjson d "$delj" --argjson f "$failj" \
      '{ok:($f|length==0),availability:"ok",org:$org,deleted:$d,failed:$f,error:(if ($f|length)>0 then {code:"partial",message:"some TraceFlags could not be deleted"} else null end)}'
    [[ ${#failed[@]} -eq 0 ]]
    return $?
  fi
  if [[ ${#deleted[@]} -gt 0 ]]; then
    printf '%sStopped %s TraceFlag(s)%s\n' "$GREEN" "${#deleted[@]}" "$RESET"
  fi
  if [[ ${#failed[@]} -gt 0 ]]; then
    printf '%sFailed:%s %s\n' "$RED" "$RESET" "${failed[*]}"
    return 1
  fi
}

cmd_trace() {
  local sub="${1:-status}"
  shift || true
  case "$sub" in
    status|list) cmd_trace_status "$@" ;;
    start)       cmd_trace_start "$@" ;;
    stop)        cmd_trace_stop "$@" ;;
    help|-h|--help)
      cat <<EOF
${BOLD}bluechip trace${RESET} — confirm-gated TraceFlag writes (not exposed on MCP)

  ${CYAN}start${RESET} [--minutes N] [--user username] [--yes]
                      Create USER_DEBUG on the current (or named) user.
                      Confirm: SANDBOX/alias on sandbox; type PROD on prod.
                      --yes is sandbox-only (banned on prod / unknown).
  ${CYAN}status${RESET} [--json]     List TraceFlags via Tooling (unknown if unsupported).
  ${CYAN}stop${RESET} [--id <id>]    Delete TraceFlag(s). Same confirm matrix as start.

DebugLevel: reuse SFDC_DevConsole / Bluechip_DEBUG, or create Bluechip_DEBUG.
EOF
      ;;
    *) die "unknown trace command: $sub (try: bluechip trace help)" ;;
  esac
}

_redact_log_body() {
  local body="$1"
  body="$(neutralize_plaintext "$body" "$LOG_MAX_BYTES")"
  # Drop leftover secret-shaped lines (Authorization headers, tokens).
  body="$(printf '%s\n' "$body" | sed -E \
    '/[Aa]uthorization:[[:space:]]*/d;
     /[Cc]onsumer[[:space:]]*[Ss]ecret/d;
     /[Aa]ccess[-_ ][Tt]oken/d;
     /[Pp]assword[[:space:]]*[:=]/d')"
  printf '%s' "$body"
}

compute_logs() {
  local with_body="${1:-0}"
  have_sf || { probe_error_json sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob raw
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  orgblob="$(org_json_blob "$snap")"

  raw="$(sf_try data query "${OFLAG[@]}" -q \
    "SELECT Id, Application, DurationMilliseconds, Location, LogLength, LogUserId, Operation, Request, StartTime, Status FROM ApexLog ORDER BY StartTime DESC LIMIT $LOG_MAX_COUNT")" \
    || {
      local why; why="$(classify_sf_error "$raw")"
      jq -n --argjson org "$orgblob" --arg c "$why" \
        '{ok:false,availability:"unknown",org:$org,logs:[],error:{code:$c,message:("ApexLog query failed: "+$c)}}'
      return 0
    }

  local logs
  logs="$(jq -c '[.result.records[]? | {
      id: .Id,
      operation: .Operation,
      status: .Status,
      startTime: .StartTime,
      logLength: .LogLength,
      userId: .LogUserId,
      application: .Application,
      body: null
    }]' <<<"$raw")"

  if [[ "$with_body" == "1" ]]; then
    local id body rawb newlogs="[]"
    while IFS= read -r id; do
      [[ -z "$id" ]] && continue
      sf_record_id_ok "$id" || continue
      rawb="$(sf_try apex get log --log-id "$id" "${OFLAG[@]}")" || rawb=""
      body=""
      if [[ -n "$rawb" ]]; then
        body="$(jq -r '.result.log // .result // empty' <<<"$rawb" 2>/dev/null || true)"
        # apex get log --json may return a string result
        if [[ "$body" == "{"* ]]; then
          body="$(jq -r '.log // empty' <<<"$body" 2>/dev/null || true)"
        fi
      fi
      body="$(_redact_log_body "$body")"
      newlogs="$(jq -c --arg id "$id" --arg b "$body" \
        'map(if .id==$id then .body=$b else . end)' <<<"$logs")"
      logs="$newlogs"
    done < <(jq -r '.[].id' <<<"$logs")
  fi

  jq -n --argjson org "$orgblob" --argjson logs "$logs" \
    '{ok:true,availability:"ok",org:$org,logs:$logs,error:null}' | redact_secrets_json
}

cmd_logs() {
  local follow=0 with_body=0 want_json=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --follow|-f) follow=1; with_body=1; shift ;;
      --body) with_body=1; shift ;;
      --json) want_json=1; shift ;;
      *) die "unknown logs option: $1" ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1

  if [[ $follow -eq 1 ]]; then
    cmd_logs_follow "$want_json"
    return $?
  fi

  local h
  h="$(compute_logs "$with_body")" || h="$(probe_error_json transient "ApexLog list failed")"
  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi

  local snap env
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  case "$(sandbox_state "${snap:-"{}"}")" in
    sandbox) env="SANDBOX" ;;
    prod) env="PROD" ;;
    *) env="UNKNOWN" ;;
  esac
  printf '%sApex logs%s  %s%s%s  %s%s%s  %s(capped %s bytes · %s rows)%s\n\n' \
    "$BOLD" "$RESET" "$YELLOW" "$env" "$RESET" "$GREY" "$(org_id "${snap:-"{}"}")" "$RESET" \
    "$DIM" "$LOG_MAX_BYTES" "$LOG_MAX_COUNT" "$RESET"

  if [[ "$(jq -r '.availability' <<<"$h")" != "ok" ]]; then
    printf '  %sunknown%s  %s\n' "$GREY" "$RESET" \
      "$(neutralize_plaintext "$(jq -r '.error.message // "ApexLog unavailable"' <<<"$h")" 200)"
    return 0
  fi
  local n; n="$(jq -r '.logs | length' <<<"$h")"
  if [[ "$n" == "0" ]]; then
    printf '  %sNo ApexLog rows.%s\n' "$DIM" "$RESET"
    return 0
  fi
  jq -r '.logs[] | [.id, (.operation//"?"), (.status//"?"), (.startTime//""), ((.logLength//0)|tostring), (.body//"")] | @tsv' <<<"$h" \
  | while IFS=$'\t' read -r id op st tm ln body; do
      printf '  %s  %-16s %-12s  %s  %s bytes\n' \
        "$(neutralize_plaintext "$id" 22)" \
        "$(neutralize_plaintext "$op" 16)" \
        "$(neutralize_plaintext "$st" 12)" \
        "$(neutralize_plaintext "${tm%%.*}" 20)" \
        "$ln"
      if [[ -n "$body" ]]; then
        printf '%s%s%s\n\n' "$DIM" "$body" "$RESET"
      fi
    done
}

cmd_logs_follow() {
  local want_json="${1:-0}"
  have_sf || die "sf CLI not found."
  set_oflag
  local seen="{}" snap env
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  case "$(sandbox_state "${snap:-"{}"}")" in
    sandbox) env="SANDBOX" ;;
    prod) env="PROD" ;;
    *) env="UNKNOWN" ;;
  esac
  printf 'bluechip logs --follow  %s  Ctrl-C to stop\n' "$env" >&2

  local iters="${BLUECHIP_FOLLOW_ITERS:-0}" n=0 backoff=2
  while true; do
    local h id
    h="$(compute_logs 1)" || {
      sleep "$backoff"
      backoff=$(( backoff * 2 ))
      (( backoff > 16 )) && backoff=16
      n=$((n+1)); [[ "$iters" != "0" && $n -ge $iters ]] && break; continue
    }
    if [[ "$(jq -r '.availability' <<<"$h")" != "ok" ]]; then
      [[ $want_json -eq 1 ]] && printf '%s\n' "$h"
      sleep "$backoff"
      backoff=$(( backoff * 2 ))
      (( backoff > 16 )) && backoff=16
      n=$((n+1))
      [[ "$iters" != "0" && $n -ge $iters ]] && break
      continue
    fi
    backoff=2
    while IFS= read -r id; do
      [[ -z "$id" ]] && continue
      jq -e --arg id "$id" 'has($id)' <<<"$seen" >/dev/null && continue
      seen="$(jq -c --arg id "$id" '. + {($id):true}' <<<"$seen")"
      if [[ $want_json -eq 1 ]]; then
        jq -c --arg id "$id" '.logs[] | select(.id==$id)' <<<"$h"
      else
        jq -r --arg id "$id" '.logs[] | select(.id==$id) | [.id, (.operation//"?"), (.status//"?"), (.body//"")] | @tsv' <<<"$h" \
        | while IFS=$'\t' read -r lid op st body; do
            printf '  %s  %s  %s\n' "$lid" "$(neutralize_plaintext "$op" 16)" "$(neutralize_plaintext "$st" 12)"
            [[ -n "$body" ]] && printf '%s%s%s\n' "$DIM" "$body" "$RESET"
          done
      fi
    done < <(jq -r '.logs[]?.id // empty' <<<"$h")
    n=$((n+1))
    [[ "$iters" != "0" && $n -ge $iters ]] && break
    sleep "$backoff"
  done
}
