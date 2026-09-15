# shellcheck shell=bash
# Incident pack + bar signal + watch deltas.
# Sourced by bin/bluechip after named-creds / trace / desk.

PULSE_SECS="${BLUECHIP_PULSE_SECS:-20}"

env_chip() { # <snapshot> -> PROD|SBX|?
  case "$(sandbox_state "$1")" in
    sandbox) printf 'SBX' ;;
    prod)    printf 'PROD' ;;
    *)       printf '?' ;;
  esac
}

snapshot_faults() { # <snapshot> -> "count availability"
  local snap="$1" n av
  n="$(jq -r '.faults.count // empty' <<<"$snap" 2>/dev/null || true)"
  av="$(jq -r '.faults.availability // empty' <<<"$snap" 2>/dev/null || true)"
  if [[ "$av" == "ok" && "$n" =~ ^[0-9]+$ ]]; then
    printf '%s ok' "$n"
    return
  fi
  printf '? unknown'
}

# Sets SIGNAL_KIND SIGNAL_LABEL SIGNAL_TEXT SIGNAL_SEV SIGNAL_PCT SIGNAL_FAULTS SIGNAL_FAULTS_AV
# kind: limit | fault | unknown
compute_bar_signal() {
  local snap="$1"
  local pct sev pk_name used max faults fav
  read -r pct sev pk_name used max < <(peak_util "$snap")
  read -r faults fav < <(snapshot_faults "$snap")
  SIGNAL_PCT="$pct"
  SIGNAL_FAULTS="$faults"
  SIGNAL_FAULTS_AV="$fav"
  SIGNAL_LABEL="$(neutralize_plaintext "$pk_name" 80)"
  SIGNAL_KIND="unknown"
  SIGNAL_SEV="unknown"
  SIGNAL_TEXT="?"

  local limit_sev="$sev"
  local fault_sev="ok"
  if [[ "$fav" == "ok" && "$faults" =~ ^[0-9]+$ ]] && (( faults > 0 )); then
    fault_sev="warn"
  fi

  # Worst: crit limit > warn limit > faults > ok limit > unknown.
  if [[ "$limit_sev" == "crit" ]]; then
    SIGNAL_KIND="limit"
    SIGNAL_SEV="crit"
    SIGNAL_TEXT="$(printf '%d%%' "${pct:-0}")"
  elif [[ "$limit_sev" == "warn" ]]; then
    SIGNAL_KIND="limit"
    SIGNAL_SEV="warn"
    SIGNAL_TEXT="$(printf '%d%%' "${pct:-0}")"
  elif [[ "$fault_sev" == "warn" ]]; then
    SIGNAL_KIND="fault"
    SIGNAL_SEV="warn"
    if (( faults == 1 )); then SIGNAL_TEXT="1 fault"
    else SIGNAL_TEXT="${faults} faults"; fi
  elif [[ "$limit_sev" == "ok" ]]; then
    SIGNAL_KIND="limit"
    SIGNAL_SEV="ok"
    SIGNAL_TEXT="$(printf '%d%%' "${pct:-0}")"
  else
    SIGNAL_KIND="unknown"
    SIGNAL_SEV="unknown"
    if [[ "$fault_sev" == "warn" ]]; then
      SIGNAL_KIND="fault"
      SIGNAL_SEV="warn"
      SIGNAL_TEXT="${faults} faults"
    else
      SIGNAL_TEXT="?"
    fi
  fi
}

bar_pulse_active() {
  local until
  [[ -f "$WATCH_STATE" ]] || return 1
  until="$(jq -r '.pulseUntil // 0' "$WATCH_STATE" 2>/dev/null || echo 0)"
  [[ "$until" =~ ^[0-9]+$ ]] || return 1
  (( until > $(date +%s) ))
}

_incident_hygiene_blob() {
  local f="$HYGIENE_CACHE"
  if [[ ! -f "$f" ]]; then
    jq -nc '{availability:"unknown",reason:"not_run",grade:null,score:null}'
    return
  fi
  jq -c '{
      availability: (.overall.availability // .availability // "unknown"),
      reason: (if (.overall.availability // .availability) == "ok" then null else "cached" end),
      grade: (.overall.grade // null),
      score: (.overall.score // null)
    }' "$f" 2>/dev/null \
    || jq -nc '{availability:"unknown",reason:"cache_unreadable",grade:null,score:null}'
}

_incident_nc_summary() {
  local h="$1"
  jq -c '
    {
      availability: (.availability // "unknown"),
      namedCount: ((.namedCredentials.records // []) | length),
      externalCount: ((.externalCredentials.records // []) | length),
      namedApiNames: [(.namedCredentials.records // [])[]? | .apiName],
      externalApiNames: [(.externalCredentials.records // [])[]? | .apiName],
      warnings: (.warnings // []),
      reason: (.error.message // .namedCredentials.reason // null)
    }' <<<"$h"
}

compute_incident() {
  have_sf || { probe_error_json sf_missing "sf CLI not found"; return 1; }
  local snap
  snap="$(load_snapshot)" || { probe_error_json "${SNAPSHOT_ERROR:-unauthenticated}" "could not read org"; return 1; }
  set_oflag

  compute_bar_signal "$snap"
  local orgblob env
  orgblob="$(org_json_blob "$snap")"
  case "$(sandbox_state "$snap")" in
    sandbox) env="SANDBOX" ;;
    prod)    env="PROD" ;;
    *)       env="UNKNOWN" ;;
  esac

  local flows_raw flows_why="null" flows_av="ok"
  flows_raw="$(sf_try data query "${OFLAG[@]}" -q \
    "SELECT Id, InterviewLabel, InterviewStatus, CurrentElement, PauseLabel, CreatedDate FROM FlowInterview WHERE InterviewStatus IN ('Error','Paused') ORDER BY CreatedDate DESC LIMIT 10")" \
    || {
      flows_av="unknown"
      flows_why="$(jq -nc --arg c "$(classify_sf_error "$flows_raw")" '{code:$c,message:("FlowInterview query failed: "+$c)}')"
      flows_raw='{"status":1,"result":{"records":[]}}'
    }
  local flows
  flows="$(jq -c '.result // {records:[]}' <<<"$flows_raw")"

  local changes_raw changes_why="null" changes_av="ok"
  changes_raw="$(sf_try data query "${OFLAG[@]}" -q \
    "SELECT Action, Section, CreatedDate, CreatedBy.Name, Display FROM SetupAuditTrail ORDER BY CreatedDate DESC LIMIT 10")" \
    || {
      changes_av="unknown"
      changes_why="$(jq -nc --arg c "$(classify_sf_error "$changes_raw")" '{code:$c,message:("SetupAuditTrail query failed: "+$c)}')"
      changes_raw='{"status":1,"result":{"records":[]}}'
    }
  local changes
  changes="$(jq -c '.result // {records:[]}' <<<"$changes_raw")"

  local logs_h logs_av="ok" logs_md
  logs_h="$(compute_logs 0 2>/dev/null)" || logs_h="$(probe_error_json transient "ApexLog list failed")"
  logs_av="$(jq -r '.availability // "unknown"' <<<"$logs_h")"
  logs_md="$(jq -c '{records: [(.logs // [])[]? | {
      Id: .id, Operation: .operation, Status: .status,
      StartTime: .startTime, LogLength: .logLength
    }]}' <<<"$logs_h")"

  local nc_h nc_sum
  nc_h="$(compute_named_creds 2>/dev/null)" || nc_h="$(probe_error_json transient "named credential inspect failed")"
  nc_sum="$(_incident_nc_summary "$nc_h")"

  local hy
  hy="$(_incident_hygiene_blob)"

  local extra
  extra="$(jq -nr --argjson nc "$nc_sum" --argjson hy "$hy" --argjson logs "$logs_h" '
    [
      "## Named / External Credentials (summary — secrets never included)",
      (if $nc.availability == "unknown" then
         "- unknown (\($nc.reason // "tooling unavailable"))"
       else
         "- Named: \($nc.namedCount)  External: \($nc.externalCount)",
         (if ($nc.namedApiNames|length)>0 then "- NC: \($nc.namedApiNames|join(", "))" else "- NC: none" end),
         (if ($nc.externalApiNames|length)>0 then "- EC: \($nc.externalApiNames|join(", "))" else "- EC: none" end)
       end),
      "",
      "## Data probe (cached)",
      (if $hy.availability == "unknown" or $hy.grade == null then
         "- unknown (not graded)"
       else
         "- grade \($hy.grade) (\($hy.score)/100)"
       end),
      "",
      "_Incident pack is display-only — not write authority._"
    ] | flatten | join("\n")
  ')"

  local md
  md="$(_cmd_context_markdown "$snap" "$env" "$flows" "$changes" "$logs_md" "$extra")"

  local lav
  lav="$(jq -r '.limitsAvailability // "ok"' <<<"$snap")"
  local overall="ok"
  [[ "$lav" != "ok" || "$flows_av" != "ok" || "$logs_av" != "ok" || "$(jq -r '.availability' <<<"$nc_h")" == "unknown" ]] \
    && overall="partial"
  [[ "$lav" != "ok" && "$flows_av" != "ok" ]] && overall="unknown"

  local headlines
  headlines="$(headline_rows "$snap" | jq -R -s -c '
    split("\n") | map(select(length>0) | split("\t")) | map(select(length>=4) | {
      used: (.[0]|tonumber),
      max: (.[1]|tonumber),
      pct: (.[2]|tonumber),
      name: .[3]
    })
  ')"

  jq -n \
    --argjson org "$orgblob" \
    --arg env "$env" \
    --arg kind "$SIGNAL_KIND" \
    --arg label "$SIGNAL_LABEL" \
    --arg text "$SIGNAL_TEXT" \
    --arg sev "$SIGNAL_SEV" \
    --arg pct "${SIGNAL_PCT}" \
    --arg faults "${SIGNAL_FAULTS}" \
    --arg fav "$SIGNAL_FAULTS_AV" \
    --arg lav "$lav" \
    --argjson headlines "$headlines" \
    --argjson snap "$snap" \
    --argjson flows "$flows" \
    --arg favflow "$flows_av" \
    --argjson flowerr "$flows_why" \
    --argjson changes "$changes" \
    --arg chav "$changes_av" \
    --argjson cherr "$changes_why" \
    --argjson logs "$logs_h" \
    --argjson nc "$nc_sum" \
    --argjson hy "$hy" \
    --arg md "$md" \
    --arg overall "$overall" \
    --argjson pctn "$( [[ "$SIGNAL_PCT" =~ ^[0-9]+$ ]] && echo "$SIGNAL_PCT" || echo null )" \
    --argjson faultn "$( [[ "$SIGNAL_FAULTS" =~ ^[0-9]+$ ]] && echo "$SIGNAL_FAULTS" || echo null )" \
    '{
       ok: true,
       availability: $overall,
       sandboxState: $org.sandboxState,
       org: $org,
       env: $env,
       signal: {
         kind: $kind,
         label: (if $label == "" or $label == "-" then null else $label end),
         text: $text,
         severity: $sev,
         peakPct: $pctn,
         faults: $faultn,
         faultsAvailability: $fav
       },
       limits: {
         availability: (if $lav == "ok" then "ok" else "unknown" end),
         headlines: (if $lav == "ok" then $headlines else [] end),
         error: (if $lav == "ok" then null else {code:"limits_unavailable",message:"limits API missed — not 0%"} end)
       },
       flowFaults: {
         availability: $favflow,
         count: (if $favflow == "ok" then (($flows.records // []) | length) else null end),
         interviews: (if $favflow == "ok" then ($flows.records // []) else [] end),
         note: "best-effort — hard faults may not persist",
         error: $flowerr
       },
       changes: {
         availability: $chav,
         records: (if $chav == "ok" then ($changes.records // []) else [] end),
         error: $cherr
       },
       namedCreds: $nc,
       logs: {
         availability: ($logs.availability // "unknown"),
         recent: ($logs.logs // []),
         error: ($logs.error // null)
       },
       hygiene: $hy,
       markdown: $md,
       error: null
     }' | redact_secrets_json
}

cmd_incident() {
  local want_json=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      help|-h|--help)
        cat <<EOF
${BOLD}bluechip incident${RESET} — one object: signal + limits + Flow faults + NC summary + logs

  ${CYAN}incident${RESET} [--json]
      Same builder as ${CYAN}doctor --json${RESET} and MCP ${CYAN}get_incident${RESET}.
      Display-only — not write authority.

  ${DIM}bluechip incident | wl-copy${RESET}
  ${DIM}bluechip incident --json | jq .signal${RESET}
EOF
        return 0
        ;;
      *) die "unknown incident option: $1" ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1

  local h
  h="$(compute_incident)" || h="$(probe_error_json transient "incident pack failed")"

  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi
  jq -r '.markdown' <<<"$h"
}

# ---------------------------------------------------------------------------
# Watch — deltas only (entr, not heartbeat spam)
# ---------------------------------------------------------------------------
_watch_nc_av() {
  [[ -f "$NC_CACHE" ]] || { printf ''; return; }
  jq -r '.availability // empty' "$NC_CACHE" 2>/dev/null || true
}

_watch_emit_jsonl() {
  local event="$1" extra="${2:-}"
  [[ "${WATCH_JSONL:-0}" -eq 1 ]] || return 0
  [[ -n "$extra" ]] || extra='{}'
  jq -nc --arg e "$event" --argjson x "$extra" --argjson ts "$(date +%s)" \
    --arg kind "$SIGNAL_KIND" --arg text "$SIGNAL_TEXT" --arg sev "$SIGNAL_SEV" \
    '{ts:$ts,event:$e,signal:{kind:$kind,text:$text,severity:$sev}} + $x'
}

_watch_notify_transition() {
  local event="$1" msg="$2" level="${3:-warning}"
  notify "$level" "$msg"
  local until
  until=$(( $(date +%s) + PULSE_SECS ))
  WATCH_PULSE_UNTIL="$until"
  WATCH_LAST_EVENT="$event"
}

cmd_watch() {
  local interval="300" once=0
  WATCH_JSONL=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --jsonl) WATCH_JSONL=1; shift ;;
      --once)  once=1; shift ;;
      --json)  WATCH_JSONL=1; shift ;;
      help|-h|--help)
        cat <<EOF
${BOLD}bluechip watch${RESET} — notify on real transitions (mako + Waybar pulse)

  ${CYAN}watch${RESET} [--jsonl] [--once] [seconds]
      Deltas: limit warn→crit, new Flow fault, NC cache ok→unknown.
      First observation is a baseline (no toast).

  ${DIM}bluechip watch --jsonl 60    # agent stream${RESET}
EOF
        return 0
        ;;
      *)
        if [[ "$1" =~ ^[0-9]+$ ]]; then interval="$1"; shift
        else die "unknown watch option: $1"; fi
        ;;
    esac
  done

  have_sf || die "sf CLI not found."
  if [[ "$interval" =~ ^[0-9]+$ ]]; then
    (( interval < WATCH_MIN_INTERVAL )) && interval="$WATCH_MIN_INTERVAL"
  else
    interval="$WATCH_MIN_INTERVAL"
  fi
  if [[ $WATCH_JSONL -eq 0 ]]; then
    pinned_cmd notify-send >/dev/null 2>&1 \
      || printf '%s(note: notify-send not found — will print alerts to stdout)%s\n' "$DIM" "$RESET" >&2
  fi
  ensure_state_dir
  [[ -f "$WATCH_STATE" ]] || echo '{}' | write_secure "$WATCH_STATE"
  if [[ $once -eq 0 && $WATCH_JSONL -eq 0 ]]; then
    printf 'bluechip watch: every %ss — deltas only (Ctrl-C to stop)\n' "$interval"
  fi

  while true; do
    FORCE_REFRESH=1
    local snap prevsev prevfaults prevnc
    WATCH_PULSE_UNTIL="$(jq -r '.pulseUntil // 0' "$WATCH_STATE" 2>/dev/null || echo 0)"
    WATCH_LAST_EVENT="$(jq -r '.lastEvent // "heartbeat"' "$WATCH_STATE" 2>/dev/null || echo heartbeat)"
    snap="$(load_snapshot 2>/dev/null)" || {
      _watch_emit_jsonl "unreadable" '{"ok":false}'
      [[ $once -eq 1 ]] && return 0
      sleep "$interval"
      continue
    }
    compute_bar_signal "$snap"
    prevsev="$(jq -r '.peakSev // empty' "$WATCH_STATE" 2>/dev/null || true)"
    prevfaults="$(jq -r '.faultCount // empty' "$WATCH_STATE" 2>/dev/null || true)"
    prevnc="$(jq -r '.ncAvailability // empty' "$WATCH_STATE" 2>/dev/null || true)"

    local event="heartbeat" fired=0
    if [[ -n "$prevsev" ]]; then
      if [[ "$SIGNAL_SEV" == "crit" && "$prevsev" != "crit" ]]; then
        _watch_notify_transition "limit_cross" \
          "$(org_name "$snap") — ${SIGNAL_LABEL:-limit} at ${SIGNAL_TEXT}" "critical"
        event="limit_cross"
        fired=1
      elif [[ "$SIGNAL_SEV" == "warn" && "$prevsev" == "ok" && "$SIGNAL_KIND" == "limit" ]]; then
        _watch_notify_transition "limit_warn" \
          "$(org_name "$snap") — ${SIGNAL_LABEL:-limit} at ${SIGNAL_TEXT}" "warning"
        event="limit_warn"
        fired=1
      fi
    fi
    if [[ "$SIGNAL_FAULTS_AV" == "ok" && "$SIGNAL_FAULTS" =~ ^[0-9]+$ && "$prevfaults" =~ ^[0-9]+$ ]]; then
      if (( SIGNAL_FAULTS > prevfaults )); then
        _watch_notify_transition "new_flow_fault" \
          "$(org_name "$snap") — Flow faults ${prevfaults}→${SIGNAL_FAULTS}" "critical"
        event="new_flow_fault"
        fired=1
      fi
    fi
    local ncav
    ncav="$(_watch_nc_av)"
    if [[ "$prevnc" == "ok" && "$ncav" == "unknown" ]]; then
      _watch_notify_transition "nc_degraded" \
        "$(org_name "$snap") — Named Credential inspect degraded to unknown" "warning"
      event="nc_degraded"
      fired=1
    fi

    if [[ $fired -eq 0 ]]; then
      event="heartbeat"
    fi
    _watch_emit_jsonl "$event" "$(jq -nc --argjson fired "$fired" --arg prev "$prevsev" '{transition:$fired,previousSev:$prev}')"

    local fault_store="null"
    [[ "$SIGNAL_FAULTS" =~ ^[0-9]+$ ]] && fault_store="$SIGNAL_FAULTS"
    jq -n \
      --arg s "$SIGNAL_SEV" \
      --argjson f "$fault_store" \
      --arg nc "${ncav}" \
      --argjson pulse "${WATCH_PULSE_UNTIL:-0}" \
      --arg ev "$event" \
      '{
         peakSev:$s,
         faultCount:$f,
         ncAvailability:(if $nc=="" then null else $nc end),
         pulseUntil:$pulse,
         lastEvent:$ev
       }' | write_secure "$WATCH_STATE"

    [[ $once -eq 1 ]] && return 0
    sleep "$interval"
  done
}

cmd_scratchpad() {
  local helper
  helper="$(dirname "$_bluechip_self")/bluechip-scratchpad"
  if [[ -x "$helper" ]]; then
    exec "$helper" "$@"
  fi
  die "scratchpad helper missing: $helper"
}

cmd_changes() {
  have_sf || die "sf CLI not found."
  set_oflag
  local want_json=0
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1
  local raw why
  raw="$(sf_try data query "${OFLAG[@]}" -q \
    "SELECT Action, Section, CreatedDate, CreatedBy.Name, Display FROM SetupAuditTrail ORDER BY CreatedDate DESC LIMIT 30")" \
    || {
      why="$(classify_sf_error "$raw")"
      if [[ $want_json -eq 1 ]]; then
        local snap; snap="$(load_snapshot 2>/dev/null)" || snap=""
        jq -n --argjson org "$(org_json_blob "$snap")" --arg c "$why" \
          '{ok:false,availability:"unknown",org:$org,changes:[],error:{code:$c,message:("SetupAuditTrail query failed: "+$c)}}'
        return 0
      fi
      printf '%sunknown (%s)%s — needs View Setup and Configuration.\n' "$GREY" "$why" "$RESET"
      return 0
    }
  local res
  res="$(jq -c '.result' <<<"$raw")"
  if [[ $want_json -eq 1 ]]; then
    local snap; snap="$(load_snapshot 2>/dev/null)" || snap=""
    jq -n --argjson org "$(org_json_blob "$snap")" --argjson res "$res" '
      {
        ok: true,
        availability: "ok",
        org: $org,
        changes: ($res.records // []),
        error: null
      }'
    return 0
  fi
  local n
  n="$(jq -r '.records | length' <<<"$res")"
  printf '%sRecent setup changes%s %s(newest first)%s\n\n' "$BOLD" "$RESET" "$DIM" "$RESET"
  if [[ "$n" == "0" ]]; then
    printf '  %snone%s\n' "$DIM" "$RESET"
    return 0
  fi
  jq -r '.records[] | [ (.CreatedDate), (.CreatedBy.Name//"?"), (.Section//"—"), (.Display//.Action) ] | @tsv' <<<"$res" \
  | while IFS=$'\t' read -r created who section what; do
      local when="${created%%.*}"; when="${when/T/ }"
      who="$(neutralize_plaintext "$who" 80)"
      section="$(neutralize_plaintext "$section" 80)"
      what="$(neutralize_plaintext "$what" 256)"
      printf '  %s%s%s  %s%-16s%s %s\n     %s%s%s\n' \
        "$GREY" "$when" "$RESET" "$CYAN" "$who" "$RESET" "$BLUE$section$RESET" "$DIM" "$what" "$RESET"
    done
}
