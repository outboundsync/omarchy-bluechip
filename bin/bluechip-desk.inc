# shellcheck shell=bash
# Multi-org desk pulse + clipboard → context hand-off.
# Each org keeps its own PROD / SANDBOX / UNKNOWN chrome. Never mix badges.

DESK_MAX="${BLUECHIP_DESK_MAX:-8}"
CLIP_MAX="${BLUECHIP_CLIP_MAX:-4000}"
CLIP_PACK="${CLIP_PACK:-$CACHE_DIR/clipboard-pack.md}"

desk_safe_alias() {
  printf '%s' "$1" | tr -c 'A-Za-z0-9._-' '_'
}

desk_read_aliases() {
  [[ -f "$STATE_FILE" ]] || return 0
  jq -r '.deskOrgs[]? // empty' "$STATE_FILE" 2>/dev/null
}

desk_write_aliases() {
  local -a aliases=("$@")
  ensure_state_dir
  [[ -f "$STATE_FILE" ]] || echo '{}' | write_secure "$STATE_FILE"
  local arr
  arr="$(printf '%s\n' "${aliases[@]}" | jq -R -s 'split("\n") | map(select(length>0))')"
  jq --argjson d "$arr" '.deskOrgs = $d' "$STATE_FILE" | write_secure "$STATE_FILE"
}

desk_remember_alias() {
  local alias="$1" a
  org_alias_ok "$alias" || return 1
  local -a cur=()
  mapfile -t cur < <(desk_read_aliases)
  local -a out=("$alias")
  for a in "${cur[@]}"; do
    [[ "$a" == "$alias" ]] && continue
    org_alias_ok "$a" || continue
    out+=("$a")
    (( ${#out[@]} >= DESK_MAX )) && break
  done
  desk_write_aliases "${out[@]}"
}

desk_default_aliases() {
  local -a d=()
  mapfile -t d < <(desk_read_aliases)
  if [[ ${#d[@]} -gt 0 ]]; then
    printf '%s\n' "${d[@]}"
    return 0
  fi
  local pin
  pin="$(pinned_org)"
  [[ -n "$pin" ]] && printf '%s\n' "$pin"
}

pulse_one_alias() {
  local alias="$1"
  org_alias_ok "$alias" || {
    jq -nc --arg a "$alias" '{alias:$a,id:null,sandboxState:"unknown",peakPct:null,peakName:null,limitsAvailability:"unknown",faults:null,faultsAvailability:"unknown",pinned:false,error:"invalid_alias"}'
    return 0
  }
  local snap
  snap="$(
    ORG_OVERRIDE="$alias"
    SNAPSHOT_FILE="$CACHE_DIR/pulse.$(desk_safe_alias "$alias").json"
    set_oflag
    load_snapshot 2>/dev/null
  )" || snap=""
  local pin
  pin="$(pinned_org)"
  if [[ -z "$snap" ]]; then
    jq -nc --arg a "$alias" --argjson pinned "$([[ "$alias" == "$pin" ]] && echo true || echo false)" \
      '{alias:$a,id:null,sandboxState:"unknown",peakPct:null,peakName:null,limitsAvailability:"unknown",faults:null,faultsAvailability:"unknown",pinned:$pinned,error:"unreadable"}'
    return 0
  fi
  local pct sev pk_name used max lav faults fav
  read -r pct sev pk_name used max < <(peak_util "$snap")
  lav="$(jq -r '.limitsAvailability // "ok"' <<<"$snap")"
  faults="$(jq -r '.faults.count // empty' <<<"$snap")"
  fav="$(jq -r '.faults.availability // "unknown"' <<<"$snap")"
  if [[ ! "$faults" =~ ^[0-9]+$ ]]; then
    faults="null"
    fav="unknown"
  fi
  local pct_json="null"
  [[ "$sev" != "unknown" && "$pct" =~ ^[0-9]+$ ]] && pct_json="$pct"
  jq -n \
    --arg a "$alias" \
    --arg id "$(org_id "$snap")" \
    --arg st "$(sandbox_state "$snap")" \
    --argjson pct "$pct_json" \
    --arg pk "$(neutralize_plaintext "$pk_name" 80)" \
    --arg lav "$lav" \
    --argjson faults "$faults" \
    --arg fav "$fav" \
    --argjson pinned "$([[ "$alias" == "$pin" ]] && echo true || echo false)" \
    '{
       alias: $a,
       id: (if $id == "unknown" or $id == "" then null else $id end),
       sandboxState: $st,
       peakPct: $pct,
       peakName: (if $pk == "" or $pk == "-" then null else $pk end),
       limitsAvailability: (if $lav == "ok" then "ok" else "unknown" end),
       faults: $faults,
       faultsAvailability: $fav,
       pinned: $pinned,
       error: null
     }'
}

compute_desk() {
  local -a aliases=()
  mapfile -t aliases < <(desk_default_aliases)
  local rows="[]" a pulse
  for a in "${aliases[@]}"; do
    [[ -z "$a" ]] && continue
    pulse="$(pulse_one_alias "$a")"
    rows="$(jq -c --argjson r "$pulse" '. + [$r]' <<<"$rows")"
  done
  local pin
  pin="$(pinned_org)"
  jq -n --arg pin "$pin" --argjson orgs "$rows" \
    '{
       ok: true,
       pinnedOrg: (if $pin == "" then null else $pin end),
       orgs: $orgs,
       error: null
     }'
}

cmd_desk() {
  local sub="${1:-}"
  case "$sub" in
    add)
      shift
      local alias="${1:-}"
      org_alias_ok "$alias" || die "desk add needs a valid org alias"
      have_sf || die "sf CLI not found."
      pin_fetch_identity "$alias" || die "could not read org '$alias' (is it authed?)"
      pin_current_identity || true
      if [[ -z "$(pinned_org)" && "${PIN_CUR_STATE:-unknown}" == "unknown" ]]; then
        PIN_CUR_STATE="$PIN_FETCH_STATE"
      fi
      pin_confirm_switch "$alias" "$PIN_FETCH_STATE" "$PIN_FETCH_ID" || return 1
      desk_remember_alias "$alias"
      printf 'Desk: added %s%s%s (%s)\n' "$BOLD" "$alias" "$RESET" "$PIN_FETCH_STATE"
      ;;
    rm|remove)
      shift
      local alias="${1:-}" a
      org_alias_ok "$alias" || die "desk rm needs a valid org alias"
      local -a cur=() out=()
      mapfile -t cur < <(desk_read_aliases)
      for a in "${cur[@]}"; do
        [[ "$a" == "$alias" ]] && continue
        out+=("$a")
      done
      desk_write_aliases "${out[@]}"
      printf 'Desk: removed %s\n' "$alias"
      ;;
    help|-h|--help)
      cat <<EOF
${BOLD}bluechip desk${RESET} — multi-org pulse (each org keeps its own chrome)

  ${CYAN}desk${RESET} [--json]          Table: alias, sandboxState, peak limit %, faults
  ${CYAN}desk add${RESET} <alias>       Watch an org (same pin confirm rules when crossing)
  ${CYAN}desk rm${RESET} <alias>        Drop an org from the desk set
  ${CYAN}bar --all${RESET}              Waybar JSON for the pinned org; tooltip lists desk
                         rows with per-org PROD/SANDBOX/UNKNOWN — never mixed.

Pinning an org also remembers it on the desk (max ${DESK_MAX}).
The hot org (pin) is the only badge on the chip itself.
EOF
      ;;
    --json|"")
      local want_json=0
      [[ "$sub" == "--json" || "${WANT_JSON:-0}" -eq 1 ]] && want_json=1
      local h
      h="$(compute_desk)"
      if [[ $want_json -eq 1 ]]; then
        printf '%s\n' "$h"
        return 0
      fi
      local n
      n="$(jq -r '.orgs | length' <<<"$h")"
      printf '%sDesk%s  pin %s\n\n' "$BOLD" "$RESET" "$(jq -r '.pinnedOrg // "—"' <<<"$h")"
      if [[ "$n" == "0" ]]; then
        printf '  %sNo desk orgs. Pin one, or: bluechip desk add <alias>%s\n' "$DIM" "$RESET"
        return 0
      fi
      jq -r '.orgs[] | [
          (if .pinned then "*" else " " end),
          .alias,
          .sandboxState,
          ((.peakPct // "?")|tostring),
          (.peakName // "—"),
          (if .faultsAvailability == "ok" then (.faults|tostring) else "?" end),
          (.id // "unknown")
        ] | @tsv' <<<"$h" \
      | while IFS=$'\t' read -r mark alias state pct pk faults oid; do
          local envcolor="$GREY" env="UNKNOWN"
          case "$state" in
            sandbox) env="SANDBOX"; envcolor="$YELLOW" ;;
            prod) env="PROD"; envcolor="$RED" ;;
          esac
          local star=" "
          [[ "$mark" == "*" ]] && star="${YELLOW}★${RESET}"
          printf '  %s %-22s %s%-8s%s %s  %-22s faults %s  %s\n' \
            "$star" "$(neutralize_plaintext "$alias" 22)" \
            "$envcolor" "$env" "$RESET" \
            "$(if [[ "$pct" =~ ^[0-9]+$ ]]; then printf '%s%%' "$pct"; else printf 'unknown'; fi)" \
            "$(neutralize_plaintext "$pk" 22)" \
            "$(if [[ "$faults" == "?" ]]; then printf 'unknown'; else neutralize_plaintext "$faults" 8; fi)" \
            "$(neutralize_plaintext "$oid" 18)"
        done
      printf '\n  %sEach row has its own chrome — prod and sandbox are never one badge.%s\n' "$DIM" "$RESET"
      ;;
    *)
      die "unknown desk command: $sub (try: bluechip desk help)"
      ;;
  esac
}

desk_tooltip_block() {
  local h="${1:-}"
  [[ -n "$h" ]] || h="$(compute_desk)"
  jq -r '.orgs[] | [
      (if .pinned then "*" else " " end),
      .alias,
      (.sandboxState | ascii_upcase),
      (if .peakPct == null then "?" else (.peakPct|tostring) end),
      (if .faultsAvailability == "ok" then (.faults|tostring) else "?" end)
    ] | @tsv' <<<"$h" \
  | while IFS=$'\t' read -r mark alias env pct faults; do
      printf '%s %s  %s  %s%%  faults %s\n' "$mark" \
        "$(neutralize_plaintext "$alias" 22)" \
        "$(neutralize_plaintext "$env" 8)" \
        "$(neutralize_plaintext "$pct" 4)" \
        "$(neutralize_plaintext "$faults" 8)"
    done
}

# ---------------------------------------------------------------------------
# Clipboard → context
# ---------------------------------------------------------------------------
clipboard_read() {
  local max="${CLIP_MAX:-4000}" head_bin paste_bin
  head_bin="$(pinned_cmd head || true)"
  [[ -n "$head_bin" ]] || head_bin="head"
  if [[ -n "${1:-}" ]]; then
    printf '%s' "${1:0:max}"
    return 0
  fi
  if [[ ! -t 0 ]]; then
    "$head_bin" -c "$max"
    return 0
  fi
  paste_bin="$(pinned_cmd wl-paste || true)"
  if [[ -n "$paste_bin" ]]; then
    PATH="${BLUECHIP_PINNED_PATH}" "$paste_bin" -n 2>/dev/null | "$head_bin" -c "$max" && return 0
  fi
  if command -v wl-paste >/dev/null 2>&1; then
    wl-paste -n 2>/dev/null | "$head_bin" -c "$max" && return 0
  fi
  if command -v xclip >/dev/null 2>&1; then
    xclip -selection clipboard -o 2>/dev/null | "$head_bin" -c "$max" && return 0
  fi
  if command -v xsel >/dev/null 2>&1; then
    xsel --clipboard --output 2>/dev/null | "$head_bin" -c "$max" && return 0
  fi
  if command -v pbpaste >/dev/null 2>&1; then
    pbpaste 2>/dev/null | "$head_bin" -c "$max" && return 0
  fi
  return 1
}

clipboard_write() {
  local body="$1"
  if command -v wl-copy >/dev/null 2>&1; then
    printf '%s' "$body" | wl-copy
    return 0
  fi
  if command -v xclip >/dev/null 2>&1; then
    printf '%s' "$body" | xclip -selection clipboard
    return 0
  fi
  if command -v pbcopy >/dev/null 2>&1; then
    printf '%s' "$body" | pbcopy
    return 0
  fi
  return 1
}

clipboard_classify() {
  local raw="$1" s
  s="$(printf '%s' "$raw" | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//')"
  [[ -z "$s" ]] && { echo unrecognized; return; }
  if [[ "$s" == flow:* ]]; then echo flow_api; return; fi
  if [[ "$s" == interview:* ]]; then echo interview; return; fi
  if [[ "$s" == org:* ]]; then echo org; return; fi
  if sf_org_id_ok "$s" && [[ "${s:0:3}" == "00D" || "${s:0:3}" == "00d" ]]; then
    echo org; return
  fi
  if sf_record_id_ok "$s"; then
    case "${s:0:3}" in
      00D|00d) echo org ;;
      4I9|3I9|4i9|3i9) echo interview ;;
      07L|07l) echo apexlog ;;
      300|301|3D0|3d0) echo flow_def ;;
      *) echo record ;;
    esac
    return
  fi
  if soql_api_name_ok "$s" && [[ "$s" == *_* ]]; then
    echo flow_api
    return
  fi
  echo unrecognized
}

clipboard_token() {
  local raw="$1"
  raw="$(printf '%s' "$raw" | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//')"
  raw="${raw#flow:}"
  raw="${raw#interview:}"
  raw="${raw#org:}"
  printf '%s' "$raw"
}

_clipboard_flow_meta() {
  local name="$1"
  soql_api_name_ok "$name" || return 1
  sf_try data query --use-tooling-api "${OFLAG[@]}" -q \
    "SELECT Id, DeveloperName, MasterLabel, ActiveVersionId FROM FlowDefinition WHERE DeveloperName = '$(soql_escape_literal "$name")' LIMIT 1"
}

_clipboard_interview() {
  local id="$1"
  sf_record_id_ok "$id" || return 1
  sf_try data query "${OFLAG[@]}" -q \
    "SELECT Id, InterviewLabel, InterviewStatus, CurrentElement, PauseLabel, CreatedDate FROM FlowInterview WHERE Id = '$(soql_escape_literal "$id")' LIMIT 1"
}

compute_clipboard() {
  local text="$1"
  have_sf || { probe_error_json sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob
  snap="$(load_snapshot 2>/dev/null)" || snap=""
  orgblob="$(org_json_blob "$snap")"

  if ((${#text} > CLIP_MAX)); then
    text="${text:0:CLIP_MAX}"
  fi
  text="$(neutralize_plaintext "$text" "$CLIP_MAX")"
  if ! clipboard_ingest_ok "$text"; then
    jq -n --argjson org "$orgblob" \
      '{ok:false,availability:"unknown",org:$org,kind:"unrecognized",token:null,packPath:null,markdown:null,error:{code:"unrecognized",message:"clipboard refused (url, flag-shaped, or empty)"}}'
    return 0
  fi
  local kind token
  kind="$(clipboard_classify "$text")"
  token="$(clipboard_token "$text")"

  if [[ "$kind" == "unrecognized" ]]; then
    jq -n --argjson org "$orgblob" --arg k "$kind" \
      '{ok:false,availability:"unknown",org:$org,kind:$k,token:null,packPath:null,markdown:null,error:{code:"unrecognized",message:"clipboard is not a Flow API name, interview Id, or org Id"}}'
    return 0
  fi

  local extra="" meta=""
  extra="$(printf '## Clipboard detection\n- **Kind:** %s\n- **Token:** %s\n' \
    "$(neutralize_plaintext "$kind" 32)" \
    "$(neutralize_plaintext "$token" 120)")"

  case "$kind" in
    org)
      extra+=$'\n'"- **Org Id (from clipboard):** $(neutralize_plaintext "$token" 22)"
      extra+=$'\n'"- Pin is unchanged (crossing still needs confirm)."
      ;;
    interview|record)
      meta="$(_clipboard_interview "$token" 2>/dev/null)" || meta=""
      if [[ -n "$meta" ]] && jq -e '.status == 0' <<<"$meta" >/dev/null 2>&1; then
      extra+=$'\n'"$(jq -r --arg id "$token" '
          (.result.records // []) | map(select(.Id == $id)) | .[0] as $r
          | if $r == null then "- Interview: (no FlowInterview row)"
            else "- **Interview:** \($r.Id) · \($r.InterviewStatus) · \($r.InterviewLabel // "?") · \($r.CurrentElement // $r.PauseLabel // "—")"
            end
        ' <<<"$meta")"
      else
        extra+=$'\n'"- Interview lookup: unknown (query miss) — not invented."
      fi
      ;;
    flow_api|flow_def)
      local fname="$token"
      if [[ "$kind" == "flow_def" ]]; then fname="$token"; fi
      meta="$(_clipboard_flow_meta "$fname" 2>/dev/null)" || meta=""
      if [[ -n "$meta" ]] && jq -e '.status == 0 and (.result.records | length > 0)' <<<"$meta" >/dev/null 2>&1; then
        extra+=$'\n'"$(jq -r --arg n "$fname" '
          (.result.records // []) | map(select(.DeveloperName == $n)) | .[0] as $r
          | if $r == null then "- FlowDefinition: (no row)"
            else "- **Flow:** \($r.DeveloperName) · \($r.MasterLabel // "") · activeVersion \($r.ActiveVersionId // "?")"
            end
        ' <<<"$meta")"
      else
        extra+=$'\n'"- FlowDefinition: unknown or no row for $(neutralize_plaintext "$fname" 80)"
      fi
      extra+=$'\n'"- Matched paused/errored interviews are listed below when labels match."
      ;;
    apexlog)
      extra+=$'\n'"- **ApexLog id:** $(neutralize_plaintext "$token" 22) — pull with \`bluechip logs --body\`"
      ;;
  esac

  extra+=$'\n'$'\n'"_Clipboard pack is display-only — not write authority._"$'\n'

  local flows changes logs env state
  state="$(sandbox_state "$snap")"
  case "$state" in
    sandbox) env="SANDBOX" ;;
    prod) env="PROD" ;;
    *) env="UNKNOWN" ;;
  esac
  flows="$(sf_json data query "${OFLAG[@]}" -q \
    "SELECT InterviewLabel, InterviewStatus, CurrentElement, CreatedDate FROM FlowInterview WHERE InterviewStatus IN ('Error','Paused') ORDER BY CreatedDate DESC LIMIT 10")" || flows='{"records":[]}'
  changes="$(sf_json data query "${OFLAG[@]}" -q \
    "SELECT Action, Section, CreatedDate, CreatedBy.Name FROM SetupAuditTrail ORDER BY CreatedDate DESC LIMIT 10")" || changes='{"records":[]}'
  logs="$(sf_json data query "${OFLAG[@]}" -q \
    "SELECT Id, Operation, Status, StartTime, LogLength FROM ApexLog ORDER BY StartTime DESC LIMIT 10")" || logs='{"records":[]}'

  if [[ "$kind" == "flow_api" ]]; then
    local matched
    matched="$(jq -r --arg n "$token" '
      [.records[]? | select((.InterviewLabel // "") == $n or ((.InterviewLabel // "") | contains($n)))]
      | if length == 0 then empty else
          (["## Interviews matching clipboard Flow name"] + map("- [\(.InterviewStatus)] \(.InterviewLabel) — \(.CurrentElement // "—")"))
          | join("\n")
        end
    ' <<<"$flows")"
    [[ -n "$matched" ]] && extra+=$'\n'"$matched"$'\n'
  fi

  local md
  md="$(_cmd_context_markdown "$snap" "$env" "$flows" "$changes" "$logs" "$extra")"
  ensure_state_dir
  printf '%s\n' "$md" | write_secure "$CLIP_PACK"

  jq -n --argjson org "$orgblob" --arg k "$kind" --arg t "$token" \
    --arg path "$CLIP_PACK" --arg md "$md" \
    '{ok:true,availability:"ok",org:$org,kind:$k,token:$t,packPath:$path,markdown:$md,error:null}' \
    | redact_secrets_json
}

cmd_clipboard() {
  local want_json=0 do_copy=0 text=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      --copy) do_copy=1; shift ;;
      --text) text="${2:-}"; shift 2 ;;
      --text=*) text="${1#*=}"; shift ;;
      help|-h|--help)
        cat <<EOF
${BOLD}bluechip clipboard${RESET} — clipboard → context pack

  ${CYAN}clipboard${RESET} [--text <str>] [--copy] [--json]
      If the clipboard looks like a Flow API name, Flow interview Id, or
      org Id, enrich a context pack and print the path. --copy also writes
      the pack back to the clipboard.

Hyprland bind example (optional — CLI works without a DE bind). Copy pack:

  bind = SUPER SHIFT, V, exec, bluechip clipboard --copy
  bluechip incident | wl-copy

Reads wl-paste / xclip / stdin. Tests: --text.
EOF
        return 0
        ;;
      *) die "unknown clipboard option: $1" ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1

  if [[ -z "$text" ]]; then
    text="$(clipboard_read "")" || die "no clipboard (pipe text, pass --text, or install wl-paste)"
  fi

  local h
  h="$(compute_clipboard "$text")" || h="$(probe_error_json transient "clipboard hand-off failed")"

  if [[ "$(jq -r '.ok' <<<"$h")" != "true" ]]; then
    if [[ $want_json -eq 1 ]]; then
      printf '%s\n' "$h"
      return 1
    fi
    printf 'bluechip: clipboard not a Flow API name, interview Id, or org Id.\n' >&2
    return 1
  fi

  local md path
  md="$(jq -r '.markdown' <<<"$h")"
  path="$(jq -r '.packPath' <<<"$h")"
  if [[ $do_copy -eq 1 ]]; then
    clipboard_write "$md" || true
  fi
  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    return 0
  fi
  printf '%sClipboard → context%s  kind %s  %s\n' \
    "$BOLD" "$RESET" "$(neutralize_plaintext "$(jq -r '.kind' <<<"$h")" 32)" \
    "$(neutralize_plaintext "$(jq -r '.org.sandboxState' <<<"$h")" 16)"
  printf '  pack: %s\n' "$path"
  [[ $do_copy -eq 1 ]] && printf '  %scopied to clipboard%s\n' "$DIM" "$RESET"
}
