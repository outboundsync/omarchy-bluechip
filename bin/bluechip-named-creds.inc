# shellcheck shell=bash
# Named Credential / External Credential inspector — read-only.
# Tooling first; Metadata list (names only) as fallback. Never print secrets.

NC_CACHE="${NC_CACHE:-$CACHE_DIR/named-creds.json}"
NC_ERROR_FILE="${NC_ERROR_FILE:-$CACHE_DIR/named-creds.error.json}"

# Try Tooling SOQL queries in order. Echo CLI JSON on first success.
# Sets NC_LAST_REASON / NC_LAST_RAW on failure.
_nc_tooling_try() {
  local raw reason q
  NC_LAST_REASON=""
  NC_LAST_RAW=""
  for q in "$@"; do
    raw="$(sf_try data query --use-tooling-api "${OFLAG[@]}" -q "$q")" || {
      NC_LAST_RAW="$raw"
      reason="$(classify_sf_error "$raw")"
      NC_LAST_REASON="$reason"
      case "$reason" in
        object_forbidden|unauthenticated|unsupported)
          # Command substitution is a subshell — echo the error JSON so the
          # caller can classify (NC_LAST_REASON would otherwise be lost).
          printf '%s\n' "$raw"
          return 1
          ;;
      esac
      continue
    }
    printf '%s\n' "$raw"
    return 0
  done
  [[ -n "$NC_LAST_RAW" ]] && printf '%s\n' "$NC_LAST_RAW"
  return 1
}

_nc_metadata_list() {
  local typ="$1"
  sf_try org list metadata --metadata-type "$typ" "${OFLAG[@]}"
}

# Optional header/parameter counts — never SELECT ParameterValue.
_nc_header_counts() {
  local raw
  raw="$(_nc_tooling_try \
    "SELECT NamedCredentialId, COUNT(Id) headerCount FROM NamedCredentialParameter WHERE ParameterType = 'HttpHeader' GROUP BY NamedCredentialId" \
    "SELECT NamedCredentialId, COUNT(Id) headerCount FROM NamedCredentialParameter WHERE ParameterType = 'Header' GROUP BY NamedCredentialId")" \
    || {
      raw="$(_nc_tooling_try \
        "SELECT NamedCredentialId, COUNT(Id) headerCount FROM NamedCredentialParameter GROUP BY NamedCredentialId")" \
        || { echo '{"availability":"unknown","reason":"'"${NC_LAST_REASON:-object_forbidden}"'","byId":{}}'; return 0; }
      jq -c --arg r "${NC_LAST_REASON:-transient}" '
        {
          availability: "partial",
          reason: "parameter_type_unknown",
          byId: ( [.result.records[]? | {key:(.NamedCredentialId // ""), value:(.headerCount // 0)}]
                  | map(select(.key != "")) | from_entries )
        }' <<<"$raw"
      return 0
    }
  jq -c '
    {
      availability: "ok",
      reason: null,
      byId: ( [.result.records[]? | {key:(.NamedCredentialId // ""), value:(.headerCount // 0)}]
              | map(select(.key != "")) | from_entries )
    }' <<<"$raw"
}

_ec_principal_rows() {
  local raw
  raw="$(_nc_tooling_try \
    "SELECT Id, ExternalCredentialId, PrincipalName, PrincipalType, SequenceNumber FROM ExternalCredentialPrincipal")" \
    || { echo '{"availability":"unknown","reason":"'"$(classify_sf_error "${raw:-}")"'","records":[]}'; return 0; }
  jq -c '
    {
      availability: "ok",
      reason: null,
      records: [.result.records[]? | {
        id: .Id,
        externalCredentialId: .ExternalCredentialId,
        name: .PrincipalName,
        type: .PrincipalType
      }]
    }' <<<"$raw"
}

_ec_auth_header_counts() {
  local raw
  raw="$(_nc_tooling_try \
    "SELECT ExternalCredentialId, COUNT(Id) c FROM ExternalCredentialParameter WHERE ParameterType = 'AuthHeader' GROUP BY ExternalCredentialId")" \
    || { echo '{"availability":"unknown","reason":"'"$(classify_sf_error "${raw:-}")"'","byId":{}}'; return 0; }
  jq -c '
    {
      availability: "ok",
      reason: null,
      byId: ( [.result.records[]? | {key:(.ExternalCredentialId // ""), value:(.c // 0)}]
              | map(select(.key != "")) | from_entries )
    }' <<<"$raw"
}

_nc_map_named() {
  local raw="$1" headers="$2"
  jq -c --argjson headers "$headers" '
    .result.records // [] | map(
      {
        id: .Id,
        apiName: .DeveloperName,
        label: .MasterLabel,
        kind: "named_credential",
        type: (.NamedCredentialType // .Protocol // null),
        protocol: (.Protocol // null),
        principalType: (.PrincipalType // null),
        principals: (if .PrincipalType != null then [.PrincipalType] else [] end),
        endpoint: .Endpoint,
        generateAuthorizationHeader: (if has("GenerateAuthorizationHeader") then .GenerateAuthorizationHeader else null end),
        allowMergeFieldsInHeader: (if has("AllowMergeFieldsInHeader") then .AllowMergeFieldsInHeader else null end),
        allowMergeFieldsInBody: (if has("AllowMergeFieldsInBody") then .AllowMergeFieldsInBody else null end),
        lastModified: (.LastModifiedDate // null),
        customHeaders: (
          if $headers.availability == "ok" or $headers.availability == "partial" then
            (($headers.byId[.Id] // 0) as $n
             | {availability:$headers.availability, reason:$headers.reason, present:($n>0), count:$n})
          else
            {availability:"unknown", reason:($headers.reason // "unknown"), present:null, count:null}
          end
        ),
        health: {availability:"unknown", reason:"not_queryable", status:null}
      }
    )' <<<"$raw"
}

_nc_map_external() {
  local raw="$1" principals="$2" authh="$3"
  jq -c --argjson principals "$principals" --argjson authh "$authh" '
    .result.records // [] | map(
      . as $ec
      | ([ $principals.records[]? | select(.externalCredentialId == $ec.Id) | .name ]) as $pnames
      | {
          id: $ec.Id,
          apiName: $ec.DeveloperName,
          label: $ec.MasterLabel,
          kind: "external_credential",
          type: ($ec.AuthenticationProtocol // null),
          protocol: ($ec.AuthenticationProtocol // null),
          principalType: null,
          principals: $pnames,
          endpoint: null,
          generateAuthorizationHeader: null,
          allowMergeFieldsInHeader: null,
          allowMergeFieldsInBody: null,
          lastModified: ($ec.LastModifiedDate // null),
          customHeaders: (
            if $authh.availability == "ok" then
              (($authh.byId[$ec.Id] // 0) as $n
               | {availability:"ok", reason:null, present:($n>0), count:$n})
            else
              {availability:"unknown", reason:($authh.reason // "unknown"), present:null, count:null}
            end
          ),
          health: {availability:"unknown", reason:"not_queryable", status:null}
        }
    )' <<<"$raw"
}

_nc_from_metadata() {
  local raw="$1" kind="$2"
  jq -c --arg kind "$kind" '
    [
      (.result // [])[]?
      | {
          id: (.id // null),
          apiName: .fullName,
          label: .fullName,
          kind: $kind,
          type: null,
          protocol: null,
          principalType: null,
          principals: [],
          endpoint: null,
          generateAuthorizationHeader: null,
          allowMergeFieldsInHeader: null,
          allowMergeFieldsInBody: null,
          lastModified: (.lastModifiedDate // null),
          customHeaders: {availability:"unknown", reason:"metadata_list_only", present:null, count:null},
          health: {availability:"unknown", reason:"metadata_list_only", status:null},
          source: "metadata_list"
        }
    ]' <<<"$raw"
}

_nc_sanitize_records() {
  # Strip URL query strings + secret keys + neutralize strings.
  jq -c '
    map(
      .endpoint = (if .endpoint != null then (.endpoint | split("?")[0] | split("#")[0]) else null end)
    )
  ' | redact_secrets_json
}

compute_named_creds() {
  have_sf || { probe_error_json sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob
  snap="$(load_snapshot 2>/dev/null)" || snap=""
  orgblob="$(org_json_blob "$snap")"

  local nc_raw ec_raw nc_avail="unknown" ec_avail="unknown" nc_reason="" ec_reason=""
  local source="unknown" warnings="[]"

  nc_raw="$(_nc_tooling_try \
    "SELECT Id, DeveloperName, MasterLabel, Endpoint, PrincipalType, Protocol, NamedCredentialType, GenerateAuthorizationHeader, AllowMergeFieldsInHeader, AllowMergeFieldsInBody, CreatedDate, LastModifiedDate FROM NamedCredential" \
    "SELECT Id, DeveloperName, MasterLabel, Endpoint, PrincipalType, Protocol, GenerateAuthorizationHeader, AllowMergeFieldsInHeader, LastModifiedDate FROM NamedCredential" \
    "SELECT Id, DeveloperName, MasterLabel, Endpoint, PrincipalType, Protocol, LastModifiedDate FROM NamedCredential" \
    "SELECT Id, DeveloperName, MasterLabel, Endpoint FROM NamedCredential" \
    "SELECT Id, DeveloperName, MasterLabel FROM NamedCredential")" \
    && nc_avail="ok" \
    || { nc_reason="$(classify_sf_error "${nc_raw:-}")"; }

  local headers principals authh
  headers='{"availability":"unknown","reason":"skipped","byId":{}}'
  principals='{"availability":"unknown","reason":"skipped","records":[]}'
  authh='{"availability":"unknown","reason":"skipped","byId":{}}'
  if [[ "$nc_avail" == "ok" ]]; then
    headers="$(_nc_header_counts)"
    source="tooling"
  fi

  ec_raw="$(_nc_tooling_try \
    "SELECT Id, DeveloperName, MasterLabel, AuthenticationProtocol, Description, CreatedDate, LastModifiedDate FROM ExternalCredential" \
    "SELECT Id, DeveloperName, MasterLabel, AuthenticationProtocol, LastModifiedDate FROM ExternalCredential" \
    "SELECT Id, DeveloperName, MasterLabel FROM ExternalCredential")" \
    && ec_avail="ok" \
    || { ec_reason="$(classify_sf_error "${ec_raw:-}")"; }

  if [[ "$ec_avail" == "ok" ]]; then
    principals="$(_ec_principal_rows)"
    authh="$(_ec_auth_header_counts)"
    source="tooling"
  fi

  # Metadata list fallback when Tooling could not see the objects.
  if [[ "$nc_avail" != "ok" ]]; then
    local md
    md="$(_nc_metadata_list NamedCredential)" && {
      nc_raw="$md"
      nc_avail="partial"
      nc_reason="tooling_unavailable"
      source="metadata_list"
      warnings="$(jq -nc '["NamedCredential details via Metadata list only — flags/headers unknown"]')"
    }
  fi
  if [[ "$ec_avail" != "ok" ]]; then
    local mde
    mde="$(_nc_metadata_list ExternalCredential)" && {
      ec_raw="$mde"
      ec_avail="partial"
      ec_reason="tooling_unavailable"
      [[ "$source" == "unknown" ]] && source="metadata_list"
      warnings="$(jq -c '. + ["ExternalCredential details via Metadata list only"]' <<<"$warnings")"
    }
  fi

  local nc_recs="[]" ec_recs="[]"
  if [[ "$nc_avail" == "ok" ]]; then
    nc_recs="$(_nc_map_named "$nc_raw" "$headers" | _nc_sanitize_records)"
  elif [[ "$nc_avail" == "partial" ]]; then
    nc_recs="$(_nc_from_metadata "$nc_raw" "named_credential" | _nc_sanitize_records)"
  fi
  if [[ "$ec_avail" == "ok" ]]; then
    ec_recs="$(_nc_map_external "$ec_raw" "$principals" "$authh" | _nc_sanitize_records)"
  elif [[ "$ec_avail" == "partial" ]]; then
    ec_recs="$(_nc_from_metadata "$ec_raw" "external_credential" | _nc_sanitize_records)"
  fi

  local overall="unknown" err="null" ok="true"
  if [[ "$nc_avail" == "ok" && ( "$ec_avail" == "ok" || "$ec_avail" == "partial" ) ]]; then
    overall="ok"
    [[ "$ec_avail" == "partial" ]] && overall="partial"
  elif [[ "$nc_avail" == "ok" && "$ec_avail" == "unknown" ]]; then
    overall="partial"
    warnings="$(jq -c --arg r "$ec_reason" '. + ["ExternalCredential: "+$r]' <<<"$warnings")"
  elif [[ "$nc_avail" == "partial" || "$ec_avail" == "partial" ]]; then
    overall="partial"
  elif [[ "$nc_avail" == "unknown" && "$ec_avail" == "unknown" ]]; then
    overall="unknown"
    local why="${nc_reason:-${ec_reason:-unknown}}"
    err="$(jq -nc --arg c "$why" --arg m "Named/External Credential Tooling not available ($why)" '{code:$c,message:$m}')"
    # object_forbidden / unsupported = degraded unknown (not a hard fail). Auth/transient fail.
    case "$why" in
      unauthenticated|transient|timeout|sf_missing) ok="false" ;;
    esac
  fi

  jq -n \
    --argjson org "$orgblob" \
    --arg av "$overall" \
    --arg src "$source" \
    --argjson nc "$nc_recs" \
    --argjson ec "$ec_recs" \
    --arg nca "$nc_avail" --arg ncr "${nc_reason:-}" \
    --arg eca "$ec_avail" --arg ecr "${ec_reason:-}" \
    --argjson warn "$warnings" \
    --argjson err "$err" \
    --argjson measuredAt "$(date +%s)" \
    --argjson okok "$ok" \
    '{
       ok: $okok,
       availability: $av,
       source: $src,
       measuredAt: $measuredAt,
       org: $org,
       namedCredentials: {availability:$nca, reason:(if $ncr=="" then null else $ncr end), records:$nc},
       externalCredentials: {availability:$eca, reason:(if $ecr=="" then null else $ecr end), records:$ec},
       warnings: $warn,
       error: $err
     }'
}

named_creds_should_persist() {
  local h="$1" avail code
  avail="$(jq -r '.availability // "unknown"' <<<"$h")"
  code="$(jq -r '.error.code // empty' <<<"$h")"
  case "$code" in
    transient|timeout|sf_missing|unauthenticated|unsupported) return 1 ;;
  esac
  [[ "$avail" == "ok" || "$avail" == "partial" ]]
}

_nc_flag() {
  local v="$1"
  case "$v" in
    true|True) echo "yes" ;;
    false|False) echo "no" ;;
    *) echo "?" ;;
  esac
}

_nc_hdr() {
  local av="$1" present="$2" count="$3"
  if [[ "$av" != "ok" && "$av" != "partial" ]]; then
    echo "?"
    return
  fi
  if [[ "$present" == "true" ]]; then
    echo "yes (${count:-?})"
  else
    echo "no"
  fi
}

cmd_named_creds() {
  local want_json=0
  [[ "${1:-}" == "--json" || "${WANT_JSON:-0}" -eq 1 ]] && want_json=1

  if ! have_sf; then
    local err; err="$(probe_error_json sf_missing "sf CLI not found")"
    if [[ $want_json -eq 1 ]]; then printf '%s\n' "$err"; else printf '%s\n' "bluechip: sf CLI not found." >&2; fi
    return 1
  fi

  local h
  h="$(compute_named_creds)" || h="$(probe_error_json transient "named credential inspect failed")"

  if named_creds_should_persist "$h"; then
    printf '%s\n' "$h" | write_secure "$NC_CACHE"
    rm -f "$NC_ERROR_FILE"
  else
    if jq -e '.error != null or .availability=="unknown"' <<<"$h" >/dev/null 2>&1; then
      ensure_state_dir
      jq -n --argjson e "$(jq -c '.error // {code:"unknown",message:"scan not persisted"}' <<<"$h")" \
        --argjson ts "$(date +%s)" '{error:$e,ts:$ts}' | write_secure "$NC_ERROR_FILE"
      # Transient: keep last-good; prefer last-good payload when we have one.
      if [[ -f "$NC_CACHE" ]] && [[ "$(jq -r '.error.code // empty' <<<"$h")" == "transient" \
        || "$(jq -r '.error.code // empty' <<<"$h")" == "timeout" ]]; then
        h="$(jq -c '. + {stale:true}' "$NC_CACHE")"
      fi
    fi
  fi

  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    jq -e '.ok != false' <<<"$h" >/dev/null
    return $?
  fi

  local snap state env envcolor
  snap="$(load_snapshot 2>/dev/null)" || snap=""
  state="$(sandbox_state "${snap:-"{}"}")"
  case "$state" in
    sandbox) env="SANDBOX"; envcolor="$YELLOW" ;;
    prod)    env="PROD";    envcolor="$RED" ;;
    *)       env="UNKNOWN"; envcolor="$GREY" ;;
  esac

  printf '%sNamed / External Credentials%s  %s%s%s  %s%s%s\n' \
    "$BOLD" "$RESET" "$envcolor" "$env" "$RESET" "$GREY" "$(org_id "${snap:-"{}"}")" "$RESET"
  printf '%ssource: %s · secrets never printed%s\n\n' \
    "$DIM" "$(jq -r '.source' <<<"$h")" "$RESET"

  if [[ "$(jq -r '.availability' <<<"$h")" == "unknown" ]]; then
    printf '  %sunknown%s  %s\n' "$GREY" "$RESET" \
      "$(neutralize_plaintext "$(jq -r '.error.message // .namedCredentials.reason // "tooling unavailable"' <<<"$h")" 200)"
    [[ -f "$NC_CACHE" ]] && printf '  %s(last-good cache kept)%s\n' "$DIM" "$RESET"
    return 0
  fi

  local ncount
  ncount="$(jq -r '.namedCredentials.records | length' <<<"$h")"
  printf '%sNamed Credentials%s  %s(%s)%s\n' "$BOLD" "$RESET" "$DIM" "$ncount" "$RESET"
  if [[ "$ncount" == "0" ]]; then
    if [[ "$(jq -r '.namedCredentials.availability' <<<"$h")" != "ok" ]]; then
      printf '  %sunknown (%s)%s\n' "$GREY" "$(jq -r '.namedCredentials.reason // "unavailable"' <<<"$h")" "$RESET"
    else
      printf '  %s(none)%s\n' "$DIM" "$RESET"
    fi
  else
    jq -r '.namedCredentials.records[] | [
        .apiName, (.label//""), (.type//"?"), (.principalType//""),
        (.customHeaders.availability//"unknown"),
        ((.customHeaders.present//false)|tostring),
        ((.customHeaders.count//"")|tostring),
        ((.generateAuthorizationHeader//"")|tostring),
        ((.allowMergeFieldsInHeader//"")|tostring),
        (.endpoint//""),
        ([.principals[]?] | join(","))
      ] | @tsv' <<<"$h" \
    | while IFS=$'\t' read -r api label typ ptype hav hpres hcnt gen form endpoint principals; do
        api="$(neutralize_plaintext "$api" 80)"
        typ="$(neutralize_plaintext "$typ" 40)"
        endpoint="$(strip_url_secrets "$endpoint")"
        printf '  %-28s %-16s principals %-16s headers %s  genAuth %s  formulas %s\n' \
          "$api" "$typ" "$(neutralize_plaintext "${ptype:-${principals:-—}}" 20)" \
          "$(_nc_hdr "$hav" "$hpres" "$hcnt")" "$(_nc_flag "$gen")" "$(_nc_flag "$form")"
        [[ -n "$endpoint" ]] && printf '     %s%s%s\n' "$DIM" "$endpoint" "$RESET"
      done
  fi

  local ecount
  ecount="$(jq -r '.externalCredentials.records | length' <<<"$h")"
  printf '\n%sExternal Credentials%s  %s(%s)%s\n' "$BOLD" "$RESET" "$DIM" "$ecount" "$RESET"
  if [[ "$ecount" == "0" ]]; then
    if [[ "$(jq -r '.externalCredentials.availability' <<<"$h")" != "ok" && \
          "$(jq -r '.externalCredentials.availability' <<<"$h")" != "partial" ]]; then
      printf '  %sunknown (%s)%s\n' "$GREY" "$(jq -r '.externalCredentials.reason // "unavailable"' <<<"$h")" "$RESET"
    else
      printf '  %s(none)%s\n' "$DIM" "$RESET"
    fi
  else
    jq -r '.externalCredentials.records[] | [
        .apiName, (.type//"?"), ([.principals[]?] | join(",")),
        (.customHeaders.availability//"unknown"),
        ((.customHeaders.present//false)|tostring),
        ((.customHeaders.count//"")|tostring)
      ] | @tsv' <<<"$h" \
    | while IFS=$'\t' read -r api typ principals hav hpres hcnt; do
        printf '  %-28s %-16s principals %-20s authHeader %s\n' \
          "$(neutralize_plaintext "$api" 80)" \
          "$(neutralize_plaintext "$typ" 40)" \
          "$(neutralize_plaintext "${principals:-—}" 20)" \
          "$(_nc_hdr "$hav" "$hpres" "$hcnt")"
      done
  fi

  if jq -e '.warnings | length > 0' <<<"$h" >/dev/null; then
    printf '\n'
    jq -r '.warnings[]' <<<"$h" | while IFS= read -r w; do
      printf '  %s· %s%s\n' "$DIM" "$(neutralize_plaintext "$w" 200)" "$RESET"
    done
  fi
  printf '\n  %sNever shown: Consumer Secret, password, Authorization values, ParameterValue.%s\n' "$DIM" "$RESET"
  return 0
}
