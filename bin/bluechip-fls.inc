# shellcheck shell=bash
# FLS / perm-set matrix probe (read-only) + proposal-only diff.
# Missing FieldPermissions row = no grant (Salesforce stores grants, not denials).
# Never invent Setup menus. Unknown on query miss.

FLS_MAX_FIELDS="${BLUECHIP_FLS_MAX_FIELDS:-40}"
FLS_MAX_PS="${BLUECHIP_FLS_MAX_PS:-20}"
FLS_EMPTY_RESULT='{"status":0,"result":{"records":[]}}'
FLS_CACHE="${FLS_CACHE:-$CACHE_DIR/fls.json}"

fls_field_ref_ok() {
  [[ "$1" =~ ^[A-Za-z][A-Za-z0-9_]{0,79}\.[A-Za-z][A-Za-z0-9_]{0,79}$ ]]
}

_fls_error() {
  local code="$1" message="$2"
  local snap orgblob
  snap="$(load_snapshot 2>/dev/null)" || snap=""
  orgblob="$(org_json_blob "$snap")"
  jq -n --argjson org "$orgblob" --arg c "$code" --arg m "$message" \
    '{ok:false,availability:"unknown",org:$org,user:null,assignments:[],fields:[],summary:{ok:0,missing_read:0,missing_edit:0,unknown:0},proposal:null,error:{code:$c,message:$m}}'
}

_fls_usage() {
  cat <<EOF
${BOLD}bluechip fls${RESET} — Field-Level Security / perm-set matrix (read-only)

  ${CYAN}fls${RESET} --user <username|id> --fields Object.Field[,Object.Field...] [--json]
      Compare required fields to ObjectPermissions + FieldPermissions
      granted by the user's profile and assigned permission sets.
      Missing Read vs missing Edit are distinct. A Tooling/SOQL miss is
      unknown — Bluechip will not invent Setup click-paths.

  ${CYAN}fls-propose${RESET}  (same flags)
      Print a proposal diff only. Does ${BOLD}not${RESET} apply. Future apply
      would use the TraceFlag confirm matrix (sandbox-first; type PROD
      on prod; --yes banned on prod / unknown; never an MCP write).

Max ${FLS_MAX_FIELDS} fields. Names are Salesforce API identifiers only.
EOF
}

_fls_parse_fields() {
  local raw="$1" f trimmed
  FLS_FIELDS=()
  [[ -z "$raw" ]] && return 1
  IFS=',' read -ra _fls_parts <<<"$raw"
  for f in "${_fls_parts[@]}"; do
    trimmed="$(printf '%s' "$f" | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//')"
    [[ -z "$trimmed" ]] && continue
    fls_field_ref_ok "$trimmed" || return 2
    FLS_FIELDS+=("$trimmed")
    (( ${#FLS_FIELDS[@]} >= FLS_MAX_FIELDS )) && break
  done
  [[ ${#FLS_FIELDS[@]} -gt 0 ]]
}

_fls_soql_in_ids() {
  local first=1 id
  for id in "$@"; do
    sf_record_id_ok "$id" || continue
    if [[ $first -eq 1 ]]; then printf "'%s'" "$(soql_escape_literal "$id")"; first=0
    else printf ",'%s'" "$(soql_escape_literal "$id")"; fi
  done
}

_fls_soql_in_names() {
  local first=1 n
  for n in "$@"; do
    soql_api_name_ok "$n" || soql_field_ref_ok "$n" || continue
    if [[ $first -eq 1 ]]; then printf "'%s'" "$(soql_escape_literal "$n")"; first=0
    else printf ",'%s'" "$(soql_escape_literal "$n")"; fi
  done
}

_fls_query() {
  local q="$1" raw
  raw="$(sf_try data query "${OFLAG[@]}" -q "$q")" && { printf '%s\n' "$raw"; return 0; }
  raw="$(sf_try data query --use-tooling-api "${OFLAG[@]}" -q "$q")" && { printf '%s\n' "$raw"; return 0; }
  printf '%s\n' "$raw"
  return 1
}

_fls_resolve_user() {
  local who="$1" q raw
  [[ -z "$who" || "$who" == *$'\n'* || ${#who} -gt 255 ]] && return 1
  if sf_record_id_ok "$who"; then
    q="SELECT Id, Username, Name, ProfileId, Profile.Name, IsActive FROM User WHERE Id = '$(soql_escape_literal "$who")' LIMIT 1"
  else
    q="SELECT Id, Username, Name, ProfileId, Profile.Name, IsActive FROM User WHERE Username = '$(soql_escape_literal "$who")' LIMIT 1"
  fi
  raw="$(_fls_query "$q")" && { printf '%s\n' "$raw"; return 0; }
  if sf_record_id_ok "$who"; then
    q="SELECT Id, Username, Name, ProfileId, IsActive FROM User WHERE Id = '$(soql_escape_literal "$who")' LIMIT 1"
  else
    q="SELECT Id, Username, Name, ProfileId, IsActive FROM User WHERE Username = '$(soql_escape_literal "$who")' LIMIT 1"
  fi
  raw="$(_fls_query "$q")" || { printf '%s\n' "$raw"; return 1; }
  printf '%s\n' "$raw"
}

compute_fls() {
  local who="$1"
  shift
  local fields=("$@")
  have_sf || { _fls_error sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob
  snap="$(load_snapshot 2>/dev/null)" || snap=""
  orgblob="$(org_json_blob "$snap")"

  local uraw
  uraw="$(_fls_resolve_user "$who")" || {
    local why; why="$(classify_sf_error "$uraw")"
    _fls_error "$why" "User lookup failed ($why)"
    return 0
  }
  local uid profile_id
  uid="$(jq -r '.result.records[0].Id // empty' <<<"$uraw")"
  profile_id="$(jq -r '.result.records[0].ProfileId // empty' <<<"$uraw")"
  if ! sf_record_id_ok "$uid"; then
    jq -n --argjson org "$orgblob" \
      '{ok:true,availability:"unknown",org:$org,user:null,assignments:[],fields:[],summary:{ok:0,missing_read:0,missing_edit:0,unknown:0},proposal:null,error:{code:"not_found",message:"no User row for --user"}}'
    return 0
  fi

  local psa_raw psa_ok=0
  psa_raw="$(_fls_query \
    "SELECT PermissionSetId, PermissionSet.Name, PermissionSet.Label, PermissionSet.IsOwnedByProfile FROM PermissionSetAssignment WHERE AssigneeId = '$(soql_escape_literal "$uid")'")" \
    && psa_ok=1 \
    || psa_raw="$(_fls_query \
      "SELECT PermissionSetId FROM PermissionSetAssignment WHERE AssigneeId = '$(soql_escape_literal "$uid")'")" \
    && psa_ok=1

  local ps_profile_raw=""
  if sf_record_id_ok "$profile_id"; then
    ps_profile_raw="$(_fls_query \
      "SELECT Id, Name, Label, IsOwnedByProfile, ProfileId FROM PermissionSet WHERE ProfileId = '$(soql_escape_literal "$profile_id")' LIMIT 5")" || ps_profile_raw=""
  fi

  if [[ $psa_ok -ne 1 && -z "$ps_profile_raw" ]]; then
    local why; why="$(classify_sf_error "${psa_raw:-}")"
    jq -n --argjson org "$orgblob" --argjson u "$uraw" --arg c "$why" \
      '{ok:true,availability:"unknown",org:$org,user:{id:($u.result.records[0].Id),username:($u.result.records[0].Username),profileId:($u.result.records[0].ProfileId),profileName:($u.result.records[0].Profile.Name // null),isActive:($u.result.records[0].IsActive // null)},assignments:[],fields:[],summary:{ok:0,missing_read:0,missing_edit:0,unknown:0},proposal:null,error:{code:$c,message:("PermissionSetAssignment query failed: "+$c)}}'
    return 0
  fi

  local ids=()
  mapfile -t ids < <(jq -r '.result.records[]?.PermissionSetId // empty' <<<"${psa_raw:-$FLS_EMPTY_RESULT}" 2>/dev/null)
  if [[ -n "$ps_profile_raw" ]]; then
    local pid
    pid="$(jq -r '.result.records[0].Id // empty' <<<"$ps_profile_raw")"
    sf_record_id_ok "$pid" && ids+=("$pid")
  fi
  # Unique, validated, capped.
  local uniq=() id
  for id in "${ids[@]}"; do
    sf_record_id_ok "$id" || continue
    local seen=0 u
    for u in "${uniq[@]}"; do [[ "$u" == "$id" ]] && seen=1 && break; done
    [[ $seen -eq 1 ]] && continue
    uniq+=("$id")
    (( ${#uniq[@]} >= FLS_MAX_PS )) && break
  done

  local assignments
  assignments="$(jq -n --argjson psa "${psa_raw:-$FLS_EMPTY_RESULT}" --argjson prof "${ps_profile_raw:-$FLS_EMPTY_RESULT}" '
    [
      ($psa.result.records // [])[]? | {
        id: .PermissionSetId,
        name: (.PermissionSet.Name // .PermissionSet.Label // .PermissionSetId),
        isOwnedByProfile: (.PermissionSet.IsOwnedByProfile // false)
      }
    ] + [
      ($prof.result.records // [])[]? | {
        id: .Id,
        name: (.Name // .Label // .Id),
        isOwnedByProfile: (.IsOwnedByProfile // true)
      }
    ] | unique_by(.id)
  ')"

  local objects=() f obj
  for f in "${fields[@]}"; do
    obj="${f%%.*}"
    soql_api_name_ok "$obj" || continue
    local seen=0 o
    for o in "${objects[@]}"; do [[ "$o" == "$obj" ]] && seen=1 && break; done
    [[ $seen -eq 0 ]] && objects+=("$obj")
  done

  local obj_in field_in parent_in
  obj_in="$(_fls_soql_in_names "${objects[@]}")"
  field_in="$(_fls_soql_in_names "${fields[@]}")"
  parent_in="$(_fls_soql_in_ids "${uniq[@]}")"

  local objperm_av="unknown" fieldperm_av="unknown" objperm_raw="" fieldperm_raw="" obj_why="" field_why=""
  if [[ -n "$parent_in" && -n "$obj_in" ]]; then
    objperm_raw="$(_fls_query \
      "SELECT ParentId, SobjectType, PermissionsRead, PermissionsEdit, PermissionsCreate, PermissionsDelete FROM ObjectPermissions WHERE ParentId IN ($parent_in) AND SobjectType IN ($obj_in)")" \
      && objperm_av="ok" \
      || { obj_why="$(classify_sf_error "$objperm_raw")"; }
  elif [[ -z "$parent_in" ]]; then
    objperm_av="ok"
    objperm_raw='{"status":0,"result":{"records":[]}}'
  fi
  if [[ -n "$parent_in" && -n "$field_in" ]]; then
    fieldperm_raw="$(_fls_query \
      "SELECT ParentId, SobjectType, Field, PermissionsRead, PermissionsEdit FROM FieldPermissions WHERE ParentId IN ($parent_in) AND Field IN ($field_in)")" \
      && fieldperm_av="ok" \
      || { field_why="$(classify_sf_error "$fieldperm_raw")"; }
  elif [[ -z "$parent_in" ]]; then
    fieldperm_av="ok"
    fieldperm_raw='{"status":0,"result":{"records":[]}}'
  fi

  local overall="ok" err="null" okok="true"
  if [[ "$objperm_av" != "ok" && "$fieldperm_av" != "ok" ]]; then
    overall="unknown"
    local why="${obj_why:-${field_why:-unknown}}"
    err="$(jq -nc --arg c "$why" --arg m "ObjectPermissions/FieldPermissions query failed ($why)" '{code:$c,message:$m}')"
    case "$why" in unauthenticated|transient|timeout|sf_missing) okok="false" ;; esac
  elif [[ "$objperm_av" != "ok" || "$fieldperm_av" != "ok" ]]; then
    overall="partial"
  fi

  local fields_json
  fields_json="$(printf '%s\n' "${fields[@]}" | jq -R -s 'split("\n") | map(select(length>0))')"

  local matrix
  matrix="$(jq -n \
    --argjson org "$orgblob" \
    --argjson u "$uraw" \
    --argjson asg "$assignments" \
    --argjson fields "$fields_json" \
    --argjson objraw "${objperm_raw:-$FLS_EMPTY_RESULT}" \
    --argjson fieldraw "${fieldperm_raw:-$FLS_EMPTY_RESULT}" \
    --arg objav "$objperm_av" --arg fieldav "$fieldperm_av" \
    --arg av "$overall" --argjson err "$err" --argjson okok "$okok" \
    --arg objwhy "${obj_why:-}" --arg fieldwhy "${field_why:-}" \
    '
    def as_bool($v):
      if $v == true or $v == "true" then true
      elif $v == false or $v == "false" then false
      else false end;

    ($objraw.result.records // []) as $op
    | ($fieldraw.result.records // []) as $fp
    | ($asg) as $asg
    | ($fields | map(. as $r | ($r | split(".")) | {ref:$r, object:.[0], field:.[1]})) as $want
    | [
        $want[]
        | . as $w
        | ([$op[] | select(.SobjectType == $w.object) | select(as_bool(.PermissionsRead))] | length > 0) as $or
        | ([$op[] | select(.SobjectType == $w.object) | select(as_bool(.PermissionsEdit))] | length > 0) as $oe
        | ([$fp[] | select(.Field == $w.ref or .Field == ($w.object + "." + $w.field))]) as $frows
        | (($frows | length) > 0) as $hasf
        | (($frows | map(select(as_bool(.PermissionsRead))) | length) > 0) as $fr
        | (($frows | map(select(as_bool(.PermissionsEdit))) | length) > 0) as $fe
        | (
            [$frows[] | .ParentId]
            + [$op[] | select(.SobjectType == $w.object) | .ParentId]
            | unique
          ) as $via_ids
        | {
            object: $w.object,
            field: $w.field,
            ref: $w.ref,
            objectRead: (if $objav == "ok" then $or else null end),
            objectEdit: (if $objav == "ok" then $oe else null end),
            fieldRead: (
              if $fieldav != "ok" then null
              elif $hasf then $fr
              elif ($w.field | tostring | endswith("__c")) then false
              else null end),
            fieldEdit: (
              if $fieldav != "ok" then null
              elif $hasf then $fe
              elif ($w.field | tostring | endswith("__c")) then false
              else null end),
            via: [ $asg[] | select(.id as $id | $via_ids | index($id)) | .name ],
            objectAvailability: $objav,
            fieldAvailability: $fieldav
          }
        | .effectiveRead = (
            if .objectRead == null or .fieldRead == null then null
            else (.objectRead and .fieldRead) end)
        | .effectiveEdit = (
            if .objectEdit == null or .fieldEdit == null then null
            else (.objectRead == true and .fieldRead == true and .objectEdit and .fieldEdit) end)
        | .gap = (
            if .effectiveRead == null or (.effectiveEdit == null and $objav == "ok" and $fieldav == "ok") then "unknown"
            elif .effectiveRead != true then "missing_read"
            elif .effectiveEdit != true then "missing_edit"
            else "ok" end)
      ] as $rows
    | ($rows | map(.gap) | group_by(.) | map({key:.[0], value:length}) | from_entries) as $g
    | {
        ok: $okok,
        availability: $av,
        org: $org,
        user: {
          id: $u.result.records[0].Id,
          username: $u.result.records[0].Username,
          name: ($u.result.records[0].Name // null),
          profileId: $u.result.records[0].ProfileId,
          profileName: ($u.result.records[0].Profile.Name // null),
          isActive: ($u.result.records[0].IsActive // null)
        },
        assignments: $asg,
        fields: $rows,
        summary: {
          ok: ($g.ok // 0),
          missing_read: ($g.missing_read // 0),
          missing_edit: ($g.missing_edit // 0),
          unknown: ($g.unknown // 0)
        },
        sources: {
          objectPermissions: {availability:$objav, reason:(if $objwhy=="" then null else $objwhy end)},
          fieldPermissions: {availability:$fieldav, reason:(if $fieldwhy=="" then null else $fieldwhy end)}
        },
        note: "Missing FieldPermissions row = no FLS grant. Unknown on query miss — no Setup menus invented.",
        error: $err
      }
    ')"

  printf '%s\n' "$matrix" | redact_secrets_json
}

_fls_build_proposal() {
  local h="$1"
  jq -c '
    . as $h
    | (
        [.assignments[]? | select(.isOwnedByProfile != true)][0]
        // [.assignments[]?][0]
        // null
      ) as $target
    | [
        .fields[]?
        | select(.gap == "missing_read" or .gap == "missing_edit")
        | {
            ref: .ref,
            object: .object,
            field: .field,
            gap: .gap,
            grantObjectRead: (.objectRead != true),
            grantObjectEdit: (.objectEdit != true),
            grantFieldRead: (.fieldRead != true),
            grantFieldEdit: (.fieldEdit != true),
            parent: $target
          }
      ] as $ops
    | {
        applied: false,
        targetPermissionSet: $target,
        operations: $ops,
        confirmMatrix: {
          sandbox: "type SANDBOX or alias; --yes allowed",
          prod: "type PROD; --yes banned",
          unknown: "type 18-char org Id",
          mcp: "never — proposal and apply stay CLI-only"
        },
        note: "Proposal only. Apply is not in this wave (sandbox-first + type PROD when it lands)."
      }
  ' <<<"$h"
}

_fls_persist() {
  local h="$1"
  local avail code
  avail="$(jq -r '.availability // "unknown"' <<<"$h")"
  code="$(jq -r '.error.code // empty' <<<"$h")"
  case "$code" in
    transient|timeout|sf_missing|unauthenticated) return 1 ;;
  esac
  [[ "$avail" == "ok" || "$avail" == "partial" ]]
}

_fls_parse_args() {
  FLS_USER=""
  FLS_FIELDS_RAW=""
  FLS_WANT_JSON=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) FLS_WANT_JSON=1; shift ;;
      --user) FLS_USER="${2:-}"; shift 2 ;;
      --user=*) FLS_USER="${1#*=}"; shift ;;
      --fields) FLS_FIELDS_RAW="${2:-}"; shift 2 ;;
      --fields=*) FLS_FIELDS_RAW="${1#*=}"; shift ;;
      help|-h|--help) _fls_usage; return 2 ;;
      *) die "unknown fls option: $1 (try: bluechip fls --help)" ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && FLS_WANT_JSON=1
  return 0
}

_fls_mark() {
  case "$1" in
    true) printf '%s✓%s' "$GREEN" "$RESET" ;;
    false) printf '%s✗%s' "$RED" "$RESET" ;;
    *) printf '%s?%s' "$GREY" "$RESET" ;;
  esac
}

_cmd_fls_print() {
  local h="$1" propose="${2:-0}"
  local snap env envcolor
  snap="$(load_snapshot 2>/dev/null)" || snap=""
  case "$(sandbox_state "${snap:-{\}}")" in
    sandbox) env="SANDBOX"; envcolor="$YELLOW" ;;
    prod) env="PROD"; envcolor="$RED" ;;
    *) env="UNKNOWN"; envcolor="$GREY" ;;
  esac
  printf '%sFLS matrix%s  %s%s%s  %s%s%s\n' \
    "$BOLD" "$RESET" "$envcolor" "$env" "$RESET" "$GREY" "$(org_id "${snap:-{\}}")" "$RESET"

  if [[ "$(jq -r '.availability' <<<"$h")" == "unknown" && "$(jq -r '.fields | length' <<<"$h")" == "0" ]]; then
    printf '  %sunknown%s  %s\n' "$GREY" "$RESET" \
      "$(neutralize_plaintext "$(jq -r '.error.message // "permissions query unavailable"' <<<"$h")" 200)"
    return 0
  fi

  local uname pname
  uname="$(neutralize_plaintext "$(jq -r '.user.username // "?"' <<<"$h")" 80)"
  pname="$(neutralize_plaintext "$(jq -r '.user.profileName // "?"' <<<"$h")" 80)"
  printf '  user %s  profile %s\n' "$CYAN$uname$RESET" "$pname"
  printf '  %s%s%s\n\n' "$DIM" "$(jq -r '.note' <<<"$h")" "$RESET"

  jq -r '.fields[] | [.ref, ((.effectiveRead//"")|tostring), ((.effectiveEdit//"")|tostring), .gap, ([.via[]?] | join(","))] | @tsv' <<<"$h" \
  | while IFS=$'\t' read -r ref er ee gap via; do
      local gcolor="$GREEN"
      [[ "$gap" == "missing_read" ]] && gcolor="$RED"
      [[ "$gap" == "missing_edit" ]] && gcolor="$YELLOW"
      [[ "$gap" == "unknown" ]] && gcolor="$GREY"
      printf '  %-40s Read %s  Edit %s  %s%s%s  %s\n' \
        "$(neutralize_plaintext "$ref" 40)" \
        "$(_fls_mark "$er")" "$(_fls_mark "$ee")" \
        "$gcolor" "$gap" "$RESET" \
        "$(neutralize_plaintext "${via:-—}" 40)"
    done

  printf '\n  summary  ok %s  missing_read %s  missing_edit %s  unknown %s\n' \
    "$(jq -r '.summary.ok' <<<"$h")" \
    "$(jq -r '.summary.missing_read' <<<"$h")" \
    "$(jq -r '.summary.missing_edit' <<<"$h")" \
    "$(jq -r '.summary.unknown' <<<"$h")"

  if [[ "$propose" == "1" ]]; then
    printf '\n%sProposal (not applied)%s\n' "$BOLD" "$RESET"
    local nops target
    nops="$(jq -r '.proposal.operations | length' <<<"$h")"
    target="$(jq -r '.proposal.targetPermissionSet.name // "none — will not invent a PermissionSet"' <<<"$h")"
    target="$(neutralize_plaintext "$target" 80)"
    printf '  target PS: %s\n' "$target"
    if [[ "$nops" == "0" ]]; then
      printf '  %sno gaps to grant%s\n' "$DIM" "$RESET"
    else
      jq -r '.proposal.operations[] | [.ref, .gap, (.grantObjectRead|tostring), (.grantObjectEdit|tostring), (.grantFieldRead|tostring), (.grantFieldEdit|tostring)] | @tsv' <<<"$h" \
      | while IFS=$'\t' read -r ref gap gor goe gfr gfe; do
          printf '  %s  %s' "$(neutralize_plaintext "$ref" 40)" "$gap"
          [[ "$gfr" == "true" ]] && printf '  +Field.Read'
          [[ "$gfe" == "true" ]] && printf '  +Field.Edit'
          [[ "$gor" == "true" ]] && printf '  +Object.Read'
          [[ "$goe" == "true" ]] && printf '  +Object.Edit'
          printf '\n'
        done
    fi
    printf '\n  %sApply is not in this wave. Confirm matrix for a future apply:%s\n' "$DIM" "$RESET"
    printf '    sandbox: type SANDBOX or alias; --yes allowed\n'
    printf '    prod: type PROD; --yes banned\n'
    printf '    unknown: type 18-char org Id\n'
    printf '    MCP: never\n'
  fi
}

cmd_fls() {
  _fls_parse_args "$@" || return $?
  if [[ -z "$FLS_USER" || -z "$FLS_FIELDS_RAW" ]]; then
    _fls_usage
    return 2
  fi
  _fls_parse_fields "$FLS_FIELDS_RAW" || die "invalid --fields (need Object.Field API names, max $FLS_MAX_FIELDS)"
  local h
  h="$(compute_fls "$FLS_USER" "${FLS_FIELDS[@]}")" || h="$(_fls_error transient "FLS probe failed")"
  if _fls_persist "$h"; then
    printf '%s\n' "$h" | write_secure "$FLS_CACHE"
  fi
  if [[ $FLS_WANT_JSON -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi
  _cmd_fls_print "$h" 0
}

cmd_fls_propose() {
  _fls_parse_args "$@" || return $?
  if [[ -z "$FLS_USER" || -z "$FLS_FIELDS_RAW" ]]; then
    _fls_usage
    return 2
  fi
  _fls_parse_fields "$FLS_FIELDS_RAW" || die "invalid --fields (need Object.Field API names, max $FLS_MAX_FIELDS)"
  local h prop
  h="$(compute_fls "$FLS_USER" "${FLS_FIELDS[@]}")" || h="$(_fls_error transient "FLS probe failed")"
  prop="$(_fls_build_proposal "$h")"
  h="$(jq -c --argjson p "$prop" '. + {proposal:$p}' <<<"$h")"
  if [[ $FLS_WANT_JSON -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi
  _cmd_fls_print "$h" 1
}
