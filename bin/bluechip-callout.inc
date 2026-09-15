# shellcheck shell=bash
# Flow-build wave: callout-auth doctor, Apex types, callout pack, preflight.
# Read-only. Never SELECT ParameterValue / Password / Authorization values.

CALLOUT_TYPES_MAX="${BLUECHIP_TYPES_MAX:-30}"
CALLOUT_BODY_MAX="${BLUECHIP_TYPES_BODY_MAX:-65536}"
APEX_PARSE="$(dirname "${BASH_SOURCE[0]}")/bluechip-apex-parse.py"

_ca_usage_auth() {
  cat <<EOF
${BOLD}bluechip callout-auth${RESET} — Named / External Credential 401-class doctor (read-only)

  ${CYAN}callout-auth${RESET} [--user <username|id|AutomatedProcess>] [--json]
  ${CYAN}named-creds --doctor${RESET}   same builder

Diagnoses (never secrets; never fake healthy):
  headers_absent     genAuth off and custom header count 0
  formulas_off       headers present/needed but AllowMergeFieldsInHeader false
  gen_auth_on_custom GenerateAuthorizationHeader on and custom headers present
  principal_missing  user has no External Credential principal access
  principal_unknown  principal access not queryable
  unknown            Tooling miss

Never shown: Consumer Secret, password, Authorization values, ParameterValue.
EOF
}

_ca_usage_types() {
  cat <<EOF
${BOLD}bluechip types${RESET} — Apex-defined / HTTP Callout type explorer (read-only)

  ${CYAN}types${RESET} [ClassName ...] [--json]
      Discover IN_/OUT_/2XX / ExternalService classes via Tooling, or describe
      named classes. Property paths are for Flow Assignment click-coaching.

Size-capped, neutralized. No deploy / retrieve write.
EOF
}

_ca_usage_pack() {
  cat <<EOF
${BOLD}bluechip callout-pack${RESET} — Flow callout hand-off (display-only)

  ${CYAN}callout-pack${RESET} <FlowApiName> [--json]
  ${CYAN}incident --callout${RESET} <FlowApiName> [--json]

One pack: Flow identity + NC/auth doctor slice + type shapes + recent ApexLog ids.
Unknown slices keep a reason. Not write authority.
EOF
}

_ca_usage_preflight() {
  cat <<EOF
${BOLD}bluechip preflight${RESET} — DWU / Automated Process / integration-user gate (read-only)

  ${CYAN}preflight${RESET} --user <username|id|AutomatedProcess> [--fields Object.Field,...] [--json]

Reuses FLS + callout-auth principal checks. Unknown on miss. Does not Activate.
EOF
}

_ca_special_user() {
  local who="$1"
  case "${who,,}" in
    automatedprocess|automated_process|autoproc|"automated process") echo automated_process ;;
    defaultworkflowuser|default_workflow_user|dwu|"default workflow user") echo default_workflow_user ;;
    *) echo "" ;;
  esac
}

_ca_resolve_user_raw() {
  local who="$1" kind raw q
  kind="$(_ca_special_user "$who")"
  if [[ "$kind" == "automated_process" ]]; then
    q="SELECT Id, Username, Name, ProfileId, Profile.Name, IsActive, UserType FROM User WHERE Name = 'Automated Process' OR UserType = 'AutomatedProcess' LIMIT 1"
    raw="$(_fls_query "$q")" && { printf '%s\n' "$raw"; return 0; }
    q="SELECT Id, Username, Name, ProfileId, IsActive, UserType FROM User WHERE Name = 'Automated Process' LIMIT 1"
    raw="$(_fls_query "$q")" && { printf '%s\n' "$raw"; return 0; }
    printf '%s\n' "$raw"
    return 1
  fi
  if [[ "$kind" == "default_workflow_user" ]]; then
    q="SELECT Id, Username, Name, ProfileId, Profile.Name, IsActive FROM User WHERE Name = 'Default Workflow User' LIMIT 1"
    raw="$(_fls_query "$q")" && { printf '%s\n' "$raw"; return 0; }
    printf '%s\n' "$raw"
    return 1
  fi
  _fls_resolve_user "$who"
}

# Diagnose already-safe NC/EC records. Never invent healthy on a miss.
_ca_diagnose_json() {
  jq -c '
    def flag_known($v): $v == true or $v == false;
    def diagnose_nc:
      . as $r
      | (if $r.customHeaders.availability == "ok" or $r.customHeaders.availability == "partial"
         then ($r.customHeaders.count // 0) else null end) as $hc
      | (if $r.customHeaders.availability == "ok" or $r.customHeaders.availability == "partial"
         then ($r.customHeaders.present == true) else null end) as $hp
      | []
      | if (flag_known($r.generateAuthorizationHeader) | not) and ($hc == null)
          then . + ["unknown"]
        else .
        end
      | if ($r.generateAuthorizationHeader == false) and ($hc == 0)
          then . + ["headers_absent"]
        else .
        end
      | if ($r.allowMergeFieldsInHeader == false) and (
            ($hp == true) or ($r.generateAuthorizationHeader == false)
          )
          then . + ["formulas_off"]
        else .
        end
      | if ($r.generateAuthorizationHeader == true) and ($hp == true)
          then . + ["gen_auth_on_custom"]
        else .
        end
      | unique;
    def diagnose_ec:
      . as $r
      | []
      | if (($r.principals // []) | length) == 0
          then . + ["principal_unknown"]
        else .
        end
      | if ($r.customHeaders.availability != "ok" and $r.customHeaders.availability != "partial")
          then . + ["unknown"]
        else .
        end
      | unique;
    def row($kind):
      . as $r
      | (if $kind == "named_credential" then diagnose_nc else diagnose_ec end) as $reasons
      | {
          apiName: $r.apiName,
          kind: $kind,
          reasons: $reasons,
          likely401: ([$reasons[] | select(. == "headers_absent" or . == "formulas_off" or . == "principal_missing")] | length > 0),
          generateAuthorizationHeader: $r.generateAuthorizationHeader,
          allowMergeFieldsInHeader: $r.allowMergeFieldsInHeader,
          headerCount: $r.customHeaders.count,
          headerAvailability: $r.customHeaders.availability,
          principals: ($r.principals // [])
        };
    {
      named: [(.namedCredentials.records // [])[] | row("named_credential")],
      external: [(.externalCredentials.records // [])[] | row("external_credential")]
    }
  '
}

_ca_principal_access() {
  local uid="$1"
  local raw why
  if ! sf_record_id_ok "$uid"; then
    echo '{"availability":"unknown","reason":"user_unresolved","grants":[]}'
    return 0
  fi
  raw="$(_fls_query \
    "SELECT Id, ParentId, ExternalCredentialPrincipalId FROM ExternalCredentialPrincipalAccess")" \
    || raw="$(_fls_query \
      "SELECT Id, ParentId, SetupEntityId FROM SetupEntityAccess WHERE SetupEntityType = 'ExternalCredentialPrincipal'")" \
    || {
      why="$(classify_sf_error "${raw:-}")"
      echo '{"availability":"unknown","reason":"'"$why"'","grants":[]}'
      return 0
    }
  local psa
  psa="$(_fls_query \
    "SELECT PermissionSetId FROM PermissionSetAssignment WHERE AssigneeId = '$(soql_escape_literal "$uid")'")" \
    || {
      why="$(classify_sf_error "${psa:-}")"
      echo '{"availability":"unknown","reason":"'"$why"'","grants":[]}'
      return 0
    }
  local acc_recs psa_ids
  acc_recs="$(jq -c '[.result.records[]? | {permissionSetId:(.ParentId // null), principalId:(.ExternalCredentialPrincipalId // .SetupEntityId // null)}]' <<<"$raw")" \
    || acc_recs='[]'
  psa_ids="$(jq -c '[.result.records[]? | .PermissionSetId]' <<<"$psa")" || psa_ids='[]'
  jq -nc --argjson grants "$acc_recs" --argjson ids "$psa_ids" \
    '{availability:"ok", reason:null, grants:$grants, assignedPermissionSetIds:$ids}'
}

_ca_merge_principals() {
  local diagnoses="$1" access="$2" principals="$3"
  jq -c --arg a "$access" --arg p "$principals" '
    ($a | fromjson) as $a
    | ($p | fromjson) as $p
    | . as $d
    | def granted($pid):
        ($a.assignedPermissionSetIds // []) as $ps
        | [($a.grants // [])[]? | select(.principalId == $pid and ((.permissionSetId as $x | $ps | index($x)) != null))] | length > 0;
    if $a.availability != "ok" then
      {named: ($d.named // []), external: (($d.external // []) | map(.reasons = ((.reasons + ["principal_unknown"]) | unique)))}
    else
      {
        named: ($d.named // []),
        external: (($d.external // []) | map(
          . as $row
          | [($p.records // [])[]? | select((.name as $n | ($row.principals // []) | index($n)) != null) | .id] as $ids
          | if (($row.principals // []) | length) == 0 then
              .reasons = ((.reasons + ["principal_unknown"]) | unique)
            elif ($ids | length) == 0 then
              .
            elif ([ $ids[]? | granted(.) ] | any) then
              .
            else
              .reasons = ((.reasons + ["principal_missing"]) | unique)
              | .likely401 = true
            end
        ))
      }
    end
  ' <<<"$diagnoses"
}

compute_callout_auth() {
  local who="${1:-}"
  have_sf || { probe_error_json sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local nc_h diagnoses userblob='null' access='{"availability":"skipped","reason":"no_user","grants":[]}'
  nc_h="$(compute_named_creds)" || nc_h="$(probe_error_json transient "named credential inspect failed")"

  nc_h="$(jq -c . <<<"$nc_h")" || nc_h="$(probe_error_json transient "named credential inspect failed")"
  if [[ "$(jq -r '.availability' <<<"$nc_h")" == "unknown" ]]; then
    jq -c '
      . + {
        diagnoses: [{
          apiName: null,
          kind: "unknown",
          reasons: ["unknown"],
          likely401: false,
          generateAuthorizationHeader: null,
          allowMergeFieldsInHeader: null,
          headerCount: null,
          headerAvailability: "unknown",
          principals: []
        }],
        principalAccess: {availability:"unknown", reason:(.error.code // "unknown"), user:null, grants:[]}
      }' <<<"$nc_h" || return 1
    return 0
  fi

  diagnoses="$(_ca_diagnose_json <<<"$nc_h")"

  if [[ -n "$who" ]]; then
    local uraw uid princ='{"records":[]}' prow
    uraw="$(_ca_resolve_user_raw "$who")" || uraw=""
    uid="$(jq -r '.result.records[0].Id // empty' <<<"${uraw:-}")"
    if sf_record_id_ok "$uid"; then
      userblob="$(jq -c '.result.records[0] | {
        id: .Id,
        username: .Username,
        name: .Name,
        userType: (.UserType // null),
        profileId: .ProfileId,
        isActive: .IsActive
      }' <<<"$uraw")"
      access="$(_ca_principal_access "$uid")"
      prow="$(_nc_tooling_try \
        "SELECT Id, ExternalCredentialId, PrincipalName, PrincipalType FROM ExternalCredentialPrincipal")" \
        || prow=""
      if [[ -n "$prow" ]] && jq -e '.result.records' <<<"$prow" >/dev/null 2>&1; then
        princ="$(jq -c '{records: [.result.records[]? | {id:.Id, name:.PrincipalName, externalCredentialId:.ExternalCredentialId}]}' <<<"$prow")"
      else
        princ="$(jq -c '{
          records: [
            (.externalCredentials.records // [])[]?
            | . as $ec
            | ($ec.principals // [])[]?
            | {id: $ec.id, name: ., externalCredentialId: $ec.id}
          ]
        }' <<<"$nc_h")"
      fi
      diagnoses="$(_ca_merge_principals "$diagnoses" "$access" "$princ")"
    else
      local why
      why="$(classify_sf_error "${uraw:-}")"
      access="$(jq -nc --arg r "$why" '{availability:"unknown",reason:$r,grants:[]}')"
      diagnoses="$(jq -c '.external |= map(.reasons = ((.reasons + ["principal_unknown"]) | unique))' <<<"$diagnoses")"
    fi
  fi

  diagnoses="$(jq -c . <<<"$diagnoses" 2>/dev/null)" || diagnoses='{"named":[],"external":[]}'
  access="$(jq -c . <<<"$access" 2>/dev/null)" || access='{"availability":"unknown","reason":"invalid","grants":[]}'
  userblob="$(jq -c . <<<"$userblob" 2>/dev/null)" || userblob='null'
  jq -c --arg d "$diagnoses" --arg user "$userblob" --arg acc "$access" '
    ($d | fromjson) as $d
    | ($user | fromjson) as $user
    | ($acc | fromjson) as $acc
    | (([$d.named[]?] + [$d.external[]?])) as $all
    | . + {
        diagnoses: $all,
        principalAccess: {
          availability: $acc.availability,
          reason: $acc.reason,
          user: $user,
          grants: ($acc.grants // [])
        }
      }
  ' <<<"$nc_h" | redact_secrets_json
}

cmd_callout_auth() {
  local want_json=0 who=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      --user) who="${2:-}"; shift 2 ;;
      --user=*) who="${1#*=}"; shift ;;
      --doctor) shift ;;
      help|-h|--help) _ca_usage_auth; return 0 ;;
      *) die "unknown callout-auth option: $1" ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1
  if [[ -n "$who" ]]; then
    if [[ ${#who} -gt 255 || "$who" == *$'\n'* || "$who" == *';'* || "$who" == *'|'* ]]; then
      die "invalid --user"
    fi
  fi

  if ! have_sf; then
    local err; err="$(probe_error_json sf_missing "sf CLI not found")"
    if [[ $want_json -eq 1 ]]; then printf '%s\n' "$err"; else printf '%s\n' "bluechip: sf CLI not found." >&2; fi
    return 1
  fi

  local h
  h="$(compute_callout_auth "$who")" || h="$(probe_error_json transient "callout-auth failed")"

  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi

  cmd_named_creds_print "$h"
  printf '\n%s401-class diagnoses%s\n' "$BOLD" "$RESET"
  if [[ "$(jq -r '.diagnoses | length' <<<"$h")" == "0" ]]; then
    printf '  %snone measured%s\n' "$DIM" "$RESET"
  else
    jq -r '.diagnoses[] | [
        (.apiName // "?"),
        .kind,
        ((.reasons // []) | join(",")),
        ((.likely401 // false) | tostring)
      ] | @tsv' <<<"$h" \
    | while IFS=$'\t' read -r api kind reasons likely; do
        api="$(neutralize_plaintext "$api" 80)"
        reasons="$(neutralize_plaintext "${reasons:-unknown}" 120)"
        local mark="·"
        [[ "$likely" == "true" ]] && mark="!"
        [[ "$reasons" == "unknown" || "$reasons" == "" ]] && mark="?"
        printf '  %s %-28s %-22s %s\n' "$mark" "$api" "$kind" "${reasons:-—}"
      done
  fi
  if [[ "$(jq -r '.principalAccess.availability' <<<"$h")" != "skipped" ]]; then
    printf '\n  principalAccess %s' "$(jq -r '.principalAccess.availability' <<<"$h")"
    local pr
    pr="$(jq -r '.principalAccess.reason // empty' <<<"$h")"
    [[ -n "$pr" ]] && printf ' (%s)' "$(neutralize_plaintext "$pr" 80)"
    printf '\n'
  fi
  printf '\n  %sPack is display-only — auth repair is Setup / Brutus, not an MCP write.%s\n' "$DIM" "$RESET"
  return 0
}

# Human table reused by named-creds --doctor (same chrome as named-creds).
cmd_named_creds_print() {
  local h="$1"
  local snap state env envcolor
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  state="$(sandbox_state "${snap:-"{}"}")"
  case "$state" in
    sandbox) env="SANDBOX"; envcolor="$YELLOW" ;;
    prod)    env="PROD";    envcolor="$RED" ;;
    *)       env="UNKNOWN"; envcolor="$GREY" ;;
  esac

  printf '%sCallout auth doctor%s  %s%s%s  %s%s%s\n' \
    "$BOLD" "$RESET" "$envcolor" "$env" "$RESET" "$GREY" "$(org_id "${snap:-"{}"}")" "$RESET"
  printf '%ssource: %s · secrets never printed%s\n\n' \
    "$DIM" "$(jq -r '.source' <<<"$h")" "$RESET"

  if [[ "$(jq -r '.availability' <<<"$h")" == "unknown" ]]; then
    printf '  %sunknown%s  %s\n' "$GREY" "$RESET" \
      "$(neutralize_plaintext "$(jq -r '.error.message // .namedCredentials.reason // "tooling unavailable"' <<<"$h")" 200)"
    return 0
  fi

  local ncount
  ncount="$(jq -r '.namedCredentials.records | length' <<<"$h")"
  printf '%sNamed Credentials%s  %s(%s)%s\n' "$BOLD" "$RESET" "$DIM" "$ncount" "$RESET"
  if [[ "$ncount" == "0" ]]; then
    printf '  %s(none)%s\n' "$DIM" "$RESET"
  else
    jq -r '.namedCredentials.records[] | [
        .apiName, (.type//"?"),
        (.customHeaders.availability//"unknown"),
        ((.customHeaders.present//false)|tostring),
        ((.customHeaders.count//"")|tostring),
        ((.generateAuthorizationHeader//"")|tostring),
        ((.allowMergeFieldsInHeader//"")|tostring),
        (.endpoint//"")
      ] | @tsv' <<<"$h" \
    | while IFS=$'\t' read -r api typ hav hpres hcnt gen form endpoint; do
        printf '  %-28s %-16s headers %s  genAuth %s  formulas %s\n' \
          "$(neutralize_plaintext "$api" 80)" \
          "$(neutralize_plaintext "$typ" 40)" \
          "$(_nc_hdr "$hav" "$hpres" "$hcnt")" "$(_nc_flag "$gen")" "$(_nc_flag "$form")"
        endpoint="$(strip_url_secrets "$endpoint")"
        [[ -n "$endpoint" ]] && printf '     %s%s%s\n' "$DIM" "$endpoint" "$RESET"
      done
  fi
}

# ---------------------------------------------------------------------------
# types
# ---------------------------------------------------------------------------

_types_discover_query() {
  _nc_tooling_try \
    "SELECT Id, Name, NamespacePrefix, Status, ApiVersion, LastModifiedDate FROM ApexClass WHERE Name LIKE 'IN_%' OR Name LIKE 'OUT_%' OR Name LIKE '%2XX%' OR Name LIKE 'ExternalService%' ORDER BY Name LIMIT ${CALLOUT_TYPES_MAX}" \
    "SELECT Id, Name, NamespacePrefix, Status, LastModifiedDate FROM ApexClass WHERE Name LIKE 'IN_%' OR Name LIKE 'OUT_%' OR Name LIKE '%2XX%' ORDER BY Name LIMIT ${CALLOUT_TYPES_MAX}" \
    "SELECT Id, Name FROM ApexClass WHERE Name LIKE '%2XX%' OR Name LIKE 'IN_%' ORDER BY Name LIMIT ${CALLOUT_TYPES_MAX}"
}

_types_named_in() {
  local first=1 n
  for n in "$@"; do
    soql_api_name_ok "$n" || continue
    if [[ $first -eq 1 ]]; then printf "'%s'" "$(soql_escape_literal "$n")"; first=0
    else printf ",'%s'" "$(soql_escape_literal "$n")"; fi
  done
}

_types_fetch_details() {
  local inlist="$1"
  [[ -z "$inlist" ]] && return 1
  _nc_tooling_try \
    "SELECT Id, Name, NamespacePrefix, Status, ApiVersion, SymbolTable FROM ApexClass WHERE Name IN ($inlist) LIMIT ${CALLOUT_TYPES_MAX}" \
    "SELECT Id, Name, NamespacePrefix, Status, Body FROM ApexClass WHERE Name IN ($inlist) LIMIT ${CALLOUT_TYPES_MAX}" \
    "SELECT Id, Name, Body FROM ApexClass WHERE Name IN ($inlist) LIMIT ${CALLOUT_TYPES_MAX}"
}

_types_parse() {
  local raw="$1"
  local payload
  payload="$(jq -c '{classes: [.result.records[]? | {
      Name, NamespacePrefix, Body, SymbolTable
    }]}' <<<"$raw")"
  if [[ ! -x "$APEX_PARSE" && ! -f "$APEX_PARSE" ]]; then
    echo '{"classes":[]}'
    return 0
  fi
  printf '%s\n' "$payload" | python3 "$APEX_PARSE" 2>/dev/null || echo '{"classes":[]}'
}

compute_types() {
  local names=("$@")
  have_sf || { probe_error_json sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  orgblob="$(org_json_blob "$snap")"

  local mode="discover" list_raw="" why="" avail="unknown"
  if [[ ${#names[@]} -gt 0 ]]; then
    mode="named"
    local inlist
    inlist="$(_types_named_in "${names[@]}")"
    if [[ -z "$inlist" ]]; then
      jq -n --argjson org "$orgblob" \
        '{ok:false,availability:"unknown",org:$org,source:"none",mode:"named",classes:[],error:{code:"config_invalid",message:"no valid Apex class names"}}'
      return 0
    fi
    list_raw="$(_types_fetch_details "$inlist")" && avail="ok" || why="$(classify_sf_error "${list_raw:-}")"
  else
    list_raw="$(_types_discover_query)" && avail="ok" || why="$(classify_sf_error "${list_raw:-}")"
    if [[ "$avail" == "ok" ]]; then
      local discovered=() inlist
      mapfile -t discovered < <(jq -r '.result.records[]?.Name // empty' <<<"$list_raw")
      inlist="$(_types_named_in "${discovered[@]}")"
      if [[ -n "$inlist" ]]; then
        local det
        det="$(_types_fetch_details "$inlist")" && list_raw="$det"
      fi
    fi
  fi

  if [[ "$avail" != "ok" ]]; then
    local hard=true
    case "$why" in object_forbidden|unsupported|invalid_field) hard=false ;; esac
    jq -n --argjson org "$orgblob" --arg c "$why" --argjson ok "$hard" --arg mode "$mode" \
      '{ok:($ok|not),availability:"unknown",org:$org,source:"tooling",mode:$mode,classes:[],error:{code:$c,message:("ApexClass Tooling query failed: "+$c)}}'
    return 0
  fi

  local parsed
  parsed="$(_types_parse "$list_raw")"
  # Neutralize names/paths; drop any leaked body/symbol table.
  parsed="$(jq -c '
    {
      classes: [(.classes // [])[]? | {
        name: .name,
        namespace: .namespace,
        kind: .kind,
        source: .source,
        properties: [(.properties // [])[]? | {path, name, type, visibility}],
        inner: .inner,
        assignmentPaths: [(.assignmentPaths // [])[]? | {path, type, class, kind}]
      }]
    }' <<<"$parsed")"

  jq -n --argjson org "$orgblob" --argjson p "$parsed" --arg mode "$mode" --argjson measuredAt "$(date +%s)" '
    {
      ok: true,
      availability: "ok",
      source: "tooling",
      mode: $mode,
      measuredAt: $measuredAt,
      org: $org,
      classes: $p.classes,
      error: null
    }
  ' | redact_secrets_json
}

cmd_types() {
  local want_json=0 names=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      help|-h|--help) _ca_usage_types; return 0 ;;
      --) shift ;;
      -*) die "unknown types option: $1" ;;
      *)
        soql_api_name_ok "$1" || die "invalid Apex class name: $1"
        names+=("$1")
        shift
        ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1

  if ! have_sf; then
    local err; err="$(probe_error_json sf_missing "sf CLI not found")"
    if [[ $want_json -eq 1 ]]; then printf '%s\n' "$err"; else printf '%s\n' "bluechip: sf CLI not found." >&2; fi
    return 1
  fi

  local h
  h="$(compute_types "${names[@]}")" || h="$(probe_error_json transient "types inspect failed")"

  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi

  local snap env envcolor
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  case "$(sandbox_state "${snap:-"{}"}")" in
    sandbox) env="SANDBOX"; envcolor="$YELLOW" ;;
    prod) env="PROD"; envcolor="$RED" ;;
    *) env="UNKNOWN"; envcolor="$GREY" ;;
  esac
  printf '%sApex / HTTP Callout types%s  %s%s%s\n' "$BOLD" "$RESET" "$envcolor" "$env" "$RESET"
  printf '%smode: %s · property paths are for Flow Assignment%s\n\n' \
    "$DIM" "$(jq -r '.mode' <<<"$h")" "$RESET"

  if [[ "$(jq -r '.availability' <<<"$h")" == "unknown" ]]; then
    printf '  %sunknown%s  %s\n' "$GREY" "$RESET" \
      "$(neutralize_plaintext "$(jq -r '.error.message // "ApexClass unavailable"' <<<"$h")" 200)"
    return 0
  fi

  local n
  n="$(jq -r '.classes | length' <<<"$h")"
  if [[ "$n" == "0" ]]; then
    printf '  %snone%s\n' "$DIM" "$RESET"
    return 0
  fi

  jq -r '.classes[] | . as $c | (
      ["CLASS", $c.name, ($c.kind//""), ($c.source//"")] ,
      (($c.assignmentPaths // $c.properties)[]? | ["PROP", .path, .type, ""])
    ) | @tsv' <<<"$h" \
  | while IFS=$'\t' read -r kind a b c; do
      if [[ "$kind" == "CLASS" ]]; then
        printf '\n  %s%s%s  %s%s%s\n' "$CYAN" "$(neutralize_plaintext "$a" 80)" "$RESET" \
          "$DIM" "$(neutralize_plaintext "$b" 40)" "$RESET"
      else
        printf '    %-40s %s\n' "$(neutralize_plaintext "$a" 40)" "$(neutralize_plaintext "$b" 40)"
      fi
    done
  printf '\n'
  return 0
}

# ---------------------------------------------------------------------------
# callout-pack
# ---------------------------------------------------------------------------

_pack_flowdef() {
  local name="$1"
  soql_api_name_ok "$name" || return 1
  _nc_tooling_try \
    "SELECT Id, DeveloperName, MasterLabel, ActiveVersionId FROM FlowDefinition WHERE DeveloperName = '$(soql_escape_literal "$name")' LIMIT 1" \
    "SELECT Id, DeveloperName, MasterLabel FROM FlowDefinition WHERE DeveloperName = '$(soql_escape_literal "$name")' LIMIT 1"
}

_pack_esr() {
  _nc_tooling_try \
    "SELECT Id, DeveloperName, NamedCredential, Description, Status FROM ExternalServiceRegistration" \
    "SELECT Id, DeveloperName, NamedCredential FROM ExternalServiceRegistration"
}

compute_callout_pack() {
  local flow="$1"
  have_sf || { probe_error_json sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  orgblob="$(org_json_blob "$snap")"

  if ! soql_api_name_ok "$flow"; then
    jq -n --argjson org "$orgblob" \
      '{ok:false,availability:"unknown",org:$org,flow:null,error:{code:"config_invalid",message:"Flow API name required"}}'
    return 0
  fi

  local flow_raw flow_av="unknown" flow_reason="" flow_obj='null'
  flow_raw="$(_pack_flowdef "$flow")" && flow_av="ok" || flow_reason="$(classify_sf_error "${flow_raw:-}")"
  if [[ "$flow_av" == "ok" ]]; then
    flow_obj="$(jq -c --arg n "$flow" '
      (.result.records // []) | map(select(.DeveloperName == $n)) | .[0] as $r
      | if $r == null then null
        else {id:$r.Id, apiName:$r.DeveloperName, label:($r.MasterLabel//null), activeVersionId:($r.ActiveVersionId//null)}
        end
    ' <<<"$flow_raw")"
    if [[ "$flow_obj" == "null" ]]; then
      flow_av="ok"
      flow_reason="none"
    fi
  fi

  local auth types logs interviews esr
  auth="$(compute_callout_auth "")" || auth="$(probe_error_json transient "auth doctor failed")"
  types="$(compute_types)" || types="$(probe_error_json transient "types failed")"
  logs="$(compute_logs 0 2>/dev/null)" || logs="$(probe_error_json transient "ApexLog list failed")"
  interviews="$(sf_try data query "${OFLAG[@]}" -q \
    "SELECT Id, InterviewLabel, InterviewStatus, CurrentElement, CreatedDate FROM FlowInterview WHERE InterviewStatus IN ('Error','Paused') ORDER BY CreatedDate DESC LIMIT 10")" \
    || interviews=""

  local iv_av="unknown" iv_reason="" iv_recs='[]'
  if [[ -n "$interviews" ]] && jq -e '.status == 0' <<<"$interviews" >/dev/null 2>&1; then
    iv_av="ok"
    iv_recs="$(jq -c --arg n "$flow" '
      [.result.records[]? | select((.InterviewLabel // "") == $n or ((.InterviewLabel // "") | contains($n)))
        | {id:.Id, label:.InterviewLabel, status:.InterviewStatus, currentElement:.CurrentElement}]
    ' <<<"$interviews")"
  else
    iv_reason="$(classify_sf_error "${interviews:-}")"
  fi

  local nc_link='[]' nc_reason="not_in_flow_metadata"
  esr="$(_pack_esr 2>/dev/null)" || esr=""
  if [[ -n "$esr" ]] && jq -e '.status == 0' <<<"$esr" >/dev/null 2>&1; then
    nc_link="$(jq -c --arg n "$flow" '
      [.result.records[]? | select((.DeveloperName // "") == $n or ((.DeveloperName // "") | contains($n)))
        | {apiName:(.NamedCredential // .DeveloperName), source:"external_service_registration"}]
    ' <<<"$esr")"
    nc_reason=null
  fi
  # Name-token overlap with NC api names (best-effort, never invent Setup menus).
  if [[ "$(jq -r 'length' <<<"$nc_link")" == "0" ]]; then
    nc_link="$(jq -c --arg n "$flow" '
      [.namedCredentials.records[]? | select(.apiName != null) | . as $r
        | select(
            ($n | test($r.apiName; "i"))
            or ($r.apiName | test($n; "i"))
          )
        | {apiName: $r.apiName, source:"name_overlap"}]
    ' <<<"$auth")"
    if [[ "$(jq -r 'length' <<<"$nc_link")" != "0" ]]; then
      nc_reason="name_overlap"
    fi
  fi

  local auth_slice
  auth_slice="$(jq -c --argjson link "$nc_link" '
    {
      availability: .availability,
      diagnoses: (
        if ($link | length) > 0 then
          [ .diagnoses[]? | select(.apiName as $a | [$link[]?.apiName] | index($a) != null) ]
        else .diagnoses
        end
      ),
      namedApiNames: [.namedCredentials.records[]?.apiName]
    }
  ' <<<"$auth")"

  local type_sum
  type_sum="$(jq -c '
    {
      availability: .availability,
      reason: (.error.code // null),
      classes: [.classes[]? | {name, kind, paths:[.assignmentPaths[]?.path]}]
    }
  ' <<<"$types")"

  local log_slice
  log_slice="$(jq -c --arg n "$flow" '
    {
      availability: (.availability // "unknown"),
      ids: [
        (.logs // .recent // [])[]?
        | select((.operation // .Operation // "") | test($n; "i") or true)
        | {id:(.id // .Id), operation:(.operation // .Operation // null), status:(.status // .Status // null)}
      ][:8]
    }
  ' <<<"$logs")"

  local overall="ok"
  [[ "$flow_av" != "ok" || "$(jq -r '.availability' <<<"$auth")" == "unknown" || "$(jq -r '.availability' <<<"$types")" == "unknown" ]] \
    && overall="partial"
  [[ "$flow_av" == "unknown" && "$(jq -r '.availability' <<<"$auth")" == "unknown" ]] && overall="unknown"

  local md
  md="$(jq -nr --argjson org "$orgblob" --arg flow "$flow" --argjson f "$flow_obj" \
    --arg fav "$flow_av" --arg fr "${flow_reason:-}" --argjson auth "$auth_slice" \
    --argjson types "$type_sum" --argjson logs "$log_slice" --argjson iv "$iv_recs" \
    --argjson link "$nc_link" --arg ncr "${nc_reason:-}" '
    [
      "# Callout pack",
      "",
      "- **Org:** \($org.id // "unknown") · \($org.sandboxState)",
      "- **Flow:** \($flow) · availability \($fav)" + (if $fr != "" then " (\($fr))" else "" end),
      (if $f != null then "- **Flow id:** \($f.id) · activeVersion \($f.activeVersionId // "?")" else "- **FlowDefinition:** \(if $fav=="ok" then "none" else "unknown" end)" end),
      "- **Named Cred link:** " + (if ($link|length)>0 then ($link|map(.apiName)|join(", ")) else "unknown (\($ncr))" end),
      "- **Auth diagnoses:** " + (
          if $auth.availability == "unknown" then "unknown"
          elif ($auth.diagnoses|length)==0 then "none"
          else ($auth.diagnoses|map((.apiName//"?") + ":" + ((.reasons//[])|join("+")))|join("; "))
          end),
      "- **Types:** " + (if $types.availability=="ok" then (($types.classes|map(.name)|join(", ")) | if .=="" then "none" else . end) else "unknown" end),
      "- **ApexLog ids:** " + (if $logs.availability=="ok" then (($logs.ids|map(.id)|join(", ")) | if .=="" then "none" else . end) else "unknown" end),
      "- **Interviews:** " + (if ($iv|length)>0 then ($iv|map(.id)|join(", ")) else "none" end),
      "",
      "_Callout pack is display-only — not write authority. Do not Activate from this pack._"
    ] | join("\n")
  ')"
  md="$(neutralize_plaintext "$md" 4000)"

  jq -n \
    --argjson org "$orgblob" \
    --arg flow "$flow" \
    --argjson fobj "$flow_obj" \
    --arg fav "$flow_av" \
    --arg fr "${flow_reason:-}" \
    --argjson link "$nc_link" \
    --arg ncr "${nc_reason:-}" \
    --argjson auth "$auth_slice" \
    --argjson types "$type_sum" \
    --argjson logs "$log_slice" \
    --argjson iv "$iv_recs" \
    --arg ivav "$iv_av" \
    --arg ivr "${iv_reason:-}" \
    --arg md "$md" \
    --arg overall "$overall" \
    '{
       ok: true,
       availability: $overall,
       org: $org,
       flow: {
         apiName: $flow,
         availability: $fav,
         reason: (if $fr=="" then null else $fr end),
         identity: $fobj
       },
       namedCredential: {
         availability: (if $ncr=="not_in_flow_metadata" and ($link|length)==0 then "unknown" else "ok" end),
         reason: (if $ncr=="" then null else $ncr end),
         linked: $link
       },
       auth: $auth,
       types: $types,
       logs: $logs,
       interviews: {availability:$ivav, reason:(if $ivr=="" then null else $ivr end), records:$iv},
       markdown: $md,
       error: null,
       note: "hand-off — not write authority"
     }' | redact_secrets_json
}

cmd_callout_pack() {
  local want_json=0 flow=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      help|-h|--help) _ca_usage_pack; return 0 ;;
      -*) die "unknown callout-pack option: $1" ;;
      *) flow="$1"; shift ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1
  [[ -z "$flow" ]] && { _ca_usage_pack; return 2; }

  if ! have_sf; then
    local err; err="$(probe_error_json sf_missing "sf CLI not found")"
    if [[ $want_json -eq 1 ]]; then printf '%s\n' "$err"; else printf '%s\n' "bluechip: sf CLI not found." >&2; fi
    return 1
  fi

  local h
  h="$(compute_callout_pack "$flow")" || h="$(probe_error_json transient "callout-pack failed")"

  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi
  printf '%s\n' "$(jq -r '.markdown' <<<"$h")"
}

# ---------------------------------------------------------------------------
# preflight
# ---------------------------------------------------------------------------

compute_preflight() {
  local who="$1"
  shift
  local fields=("$@")
  have_sf || { probe_error_json sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  orgblob="$(org_json_blob "$snap")"

  if [[ -z "$who" ]]; then
    jq -n --argjson org "$orgblob" \
      '{ok:false,availability:"unknown",org:$org,error:{code:"config_invalid",message:"--user is required"}}'
    return 0
  fi

  local auth fls='null' fls_av="skipped"
  auth="$(compute_callout_auth "$who")" || auth="$(probe_error_json transient "auth doctor failed")"

  if [[ ${#fields[@]} -gt 0 ]]; then
    local fls_who="$who"
    case "$(_ca_special_user "$who")" in
      automated_process) fls_who="Automated Process" ;;
      default_workflow_user) fls_who="Default Workflow User" ;;
    esac
    # compute_fls expects username or id; special names go through _ca_resolve first.
    local uraw uid
    uraw="$(_ca_resolve_user_raw "$who")" || uraw=""
    uid="$(jq -r '.result.records[0].Id // empty' <<<"${uraw:-}")"
    if sf_record_id_ok "$uid"; then
      fls="$(compute_fls "$uid" "${fields[@]}")" || fls="$(probe_error_json transient "fls failed")"
      fls_av="$(jq -r '.availability // "unknown"' <<<"$fls")"
    else
      fls="$(jq -n --argjson org "$orgblob" --arg c "$(classify_sf_error "${uraw:-}")" \
        '{ok:true,availability:"unknown",org:$org,fields:[],summary:{ok:0,missing_read:0,missing_edit:0,unknown:0},error:{code:$c,message:"user lookup failed"}}')"
      fls_av="unknown"
    fi
  fi

  local auth_av
  auth_av="$(jq -r '.availability // "unknown"' <<<"$auth")"
  local overall="ok"
  [[ "$auth_av" != "ok" && "$auth_av" != "partial" ]] && overall="partial"
  [[ "$fls_av" == "unknown" ]] && overall="partial"
  [[ "$auth_av" == "unknown" && "$fls_av" == "unknown" ]] && overall="unknown"
  [[ "$auth_av" == "unknown" && "$fls_av" == "skipped" ]] && overall="unknown"

  local blockers
  blockers="$(jq -c --argjson auth "$auth" --argjson fls "$fls" --arg fav "$fls_av" '
    [
      ($auth.diagnoses // [])[]? | select(.likely401 == true) | {kind:"auth", apiName, reasons},
      (if $fav == "skipped" then empty
       else ($fls.fields // [])[]? | select(.gap == "missing_read" or .gap == "missing_edit")
         | {kind:"fls", ref, gap}
       end)
    ]
  ' <<<'{}')"

  jq -n \
    --argjson org "$orgblob" \
    --arg who "$who" \
    --argjson auth "$auth" \
    --argjson fls "$fls" \
    --arg fav "$fls_av" \
    --arg overall "$overall" \
    --argjson blockers "$blockers" \
    '{
       ok: true,
       availability: $overall,
       org: $org,
       user: ($auth.principalAccess.user // {requested:$who}),
       auth: {
         availability: $auth.availability,
         diagnoses: ($auth.diagnoses // []),
         principalAccess: $auth.principalAccess
       },
       fls: (if $fav == "skipped" then {availability:"skipped", fields:[], summary:null} else {availability:$fav, fields:($fls.fields//[]), summary:($fls.summary//null), error:($fls.error//null)} end),
       blockers: $blockers,
       note: "preflight is display-only — does not Activate the Flow",
       error: null
     }' | redact_secrets_json
}

cmd_preflight() {
  local want_json=0 who="" fields_raw=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      --user) who="${2:-}"; shift 2 ;;
      --user=*) who="${1#*=}"; shift ;;
      --fields) fields_raw="${2:-}"; shift 2 ;;
      --fields=*) fields_raw="${1#*=}"; shift ;;
      help|-h|--help) _ca_usage_preflight; return 0 ;;
      *) die "unknown preflight option: $1" ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1
  [[ -z "$who" ]] && { _ca_usage_preflight; return 2; }

  local fields=()
  if [[ -n "$fields_raw" ]]; then
    _fls_parse_fields "$fields_raw" || die "invalid --fields (Object.Field,...)"
    fields=("${FLS_FIELDS[@]}")
  fi

  if ! have_sf; then
    local err; err="$(probe_error_json sf_missing "sf CLI not found")"
    if [[ $want_json -eq 1 ]]; then printf '%s\n' "$err"; else printf '%s\n' "bluechip: sf CLI not found." >&2; fi
    return 1
  fi

  local h
  h="$(compute_preflight "$who" "${fields[@]}")" || h="$(probe_error_json transient "preflight failed")"

  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi

  local snap env envcolor
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  case "$(sandbox_state "${snap:-"{}"}")" in
    sandbox) env="SANDBOX"; envcolor="$YELLOW" ;;
    prod) env="PROD"; envcolor="$RED" ;;
    *) env="UNKNOWN"; envcolor="$GREY" ;;
  esac
  printf '%sPreflight%s  %s%s%s  user %s\n' \
    "$BOLD" "$RESET" "$envcolor" "$env" "$RESET" \
    "$(neutralize_plaintext "$(jq -r '.user.username // .user.name // .user.requested // "?"' <<<"$h")" 80)"
  printf '%sdisplay-only · does not Activate%s\n\n' "$DIM" "$RESET"

  printf '%sAuth / NC%s  %s\n' "$BOLD" "$RESET" "$(jq -r '.auth.availability' <<<"$h")"
  jq -r '.auth.diagnoses[]? | [(.apiName//"?"), ((.reasons//[])|join(","))] | @tsv' <<<"$h" \
    | while IFS=$'\t' read -r api reasons; do
        printf '  %-28s %s\n' "$(neutralize_plaintext "$api" 80)" "$(neutralize_plaintext "$reasons" 80)"
      done

  if [[ "$(jq -r '.fls.availability' <<<"$h")" != "skipped" ]]; then
    printf '\n%sFLS%s  %s\n' "$BOLD" "$RESET" "$(jq -r '.fls.availability' <<<"$h")"
    jq -r '.fls.fields[]? | [.ref, .gap] | @tsv' <<<"$h" \
      | while IFS=$'\t' read -r ref gap; do
          printf '  %-40s %s\n' "$(neutralize_plaintext "$ref" 40)" "$gap"
        done
  fi

  local nb
  nb="$(jq -r '.blockers | length' <<<"$h")"
  printf '\n  blockers: %s\n' "$nb"
  printf '  %sHuman next: fix FLS / NC principal in Setup, then bluechip trace start before Activate.%s\n' "$DIM" "$RESET"
}
