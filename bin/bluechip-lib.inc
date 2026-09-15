# shellcheck shell=bash
# Shared helpers for bluechip — neutralize remote strings, org id formatting.
# Sourced by bin/bluechip (and tests).

# Max length for Salesforce/API-sourced strings rendered to terminal, Waybar, or context.
PLAIN_TEXT_MAX="${BLUECHIP_PLAINTEXT_MAX:-500}"
SF_MAX_BYTES="${BLUECHIP_SF_MAX_BYTES:-1048576}"
SF_TIMEOUT_SECS="${BLUECHIP_SF_TIMEOUT:-60}"
FILE_MAX_BYTES="${BLUECHIP_FILE_MAX_BYTES:-1048576}"
WATCH_MIN_INTERVAL="${BLUECHIP_WATCH_MIN:-5}"
BLUECHIP_PINNED_PATH="${BLUECHIP_PINNED_PATH:-/usr/bin:/bin:/usr/local/bin}"

# Resolve `sf` against the caller PATH (nvm / ~/.local) to an absolute path.
# Do not pin PATH globally — node-based sf still needs nvm. notify-send /
# xdg-open use pinned_cmd (/usr/bin or /bin only).
bluechip_resolve_sf_bin() {
  local bin="${1:-${SF_BIN:-sf}}" resolved=""
  if [[ "$bin" == /* ]]; then
    printf '%s' "$bin"
    return 0
  fi
  resolved="$(command -v "$bin" 2>/dev/null || true)"
  if [[ -n "$resolved" ]]; then
    printf '%s' "$resolved"
    return 0
  fi
  printf '%s' "$bin"
}

# Absolute /usr/bin or /bin only — never PATH lookup (cwd / poisoned PATH).
pinned_cmd() {
  local name="$1" p
  [[ -n "$name" ]] || return 1
  case "$name" in
    */*|*..*|*" "*|*$'\n'*) return 1 ;;
  esac
  [[ "$name" =~ ^[A-Za-z0-9._+-]+$ ]] || return 1
  for p in "/usr/bin/$name" "/bin/$name"; do
    if [[ -f "$p" && -x "$p" ]]; then
      printf '%s' "$p"
      return 0
    fi
  done
  return 1
}

# Refuse open/copy targets that look like URLs or flag-injection.
path_is_unsafe() {
  local p="${1:-}"
  [[ -z "$p" ]] && return 0
  [[ "$p" == *://* ]] && return 0
  [[ "$p" == -* ]] && return 0
  return 1
}

# Capture stdout of argv, cap bytes, discard stderr. Restores pipefail.
# Does not rewrite PATH — `sf` may be a node script that needs nvm.
# Returns: 0 command ok, 1 command failed, 2 oversize, 124 timeout.
capped_stdout() {
  local max="${1:-$SF_MAX_BYTES}" out="" sf_ec=0
  shift
  local -a cmd=("$@") runner=()
  local timeout_bin="" head_bin=""
  timeout_bin="$(pinned_cmd timeout || true)"
  head_bin="$(pinned_cmd head || true)"
  [[ -n "$head_bin" ]] || head_bin="head"
  if [[ -n "$timeout_bin" ]]; then
    runner=("$timeout_bin" --signal=TERM --kill-after=2 "${SF_TIMEOUT_SECS:-60}")
  fi
  set +o pipefail
  out="$( "${runner[@]}" "${cmd[@]}" 2>/dev/null | "$head_bin" -c "$((max + 1))" )"
  sf_ec=${PIPESTATUS[0]}
  set -o pipefail
  if [[ $sf_ec -eq 124 ]]; then
    return 124
  fi
  if (( ${#out} > max )); then
    return 2
  fi
  printf '%s' "$out"
  if [[ $sf_ec -eq 141 || $sf_ec -eq 0 ]]; then
    return 0
  fi
  return 1
}

read_capped_file() {
  local f="$1" max="${2:-$FILE_MAX_BYTES}" out="" head_bin
  [[ -n "$f" && -f "$f" ]] || return 1
  [[ -L "$f" ]] && return 2
  head_bin="$(pinned_cmd head || true)"
  [[ -n "$head_bin" ]] || head_bin="head"
  out="$("$head_bin" -c "$((max + 1))" "$f")"
  if (( ${#out} > max )); then
    return 3
  fi
  printf '%s' "$out"
  return 0
}

notify_send_safe() {
  local title="${1:-Bluechip}" body="${2:-}" urg="${3:-normal}" ns
  title="$(neutralize_plaintext "$title" 80)"
  body="$(neutralize_plaintext "$body" 240)"
  case "$urg" in
    low|normal|critical) ;;
    *) urg="normal" ;;
  esac
  ns="$(pinned_cmd notify-send || true)"
  [[ -n "$ns" ]] || return 1
  PATH="${BLUECHIP_PINNED_PATH}" "$ns" -u "$urg" -a "Bluechip" -- "$title" "$body" >/dev/null 2>&1 || true
}

# xdg-open / copy helpers: argv array + -- ; refuse :// and leading -.
safe_xdg_open() {
  local target="$1" bin
  path_is_unsafe "$target" && return 1
  bin="$(pinned_cmd xdg-open || true)"
  [[ -n "$bin" ]] || return 1
  PATH="${BLUECHIP_PINNED_PATH}" "$bin" -- "$target" >/dev/null 2>&1
}

clipboard_ingest_ok() {
  local s="${1:-}"
  [[ -z "$s" ]] && return 1
  path_is_unsafe "$s" && return 1
  [[ "$s" == *$'\n'* || "$s" == *$'\r'* ]] && return 1
  return 0
}

# True when value looks like a Salesforce 15- or 18-char org id.
sf_org_id_ok() {
  [[ "$1" =~ ^[a-zA-Z0-9]{15}([a-zA-Z0-9]{3})?$ ]]
}

# Strip ANSI, control chars, rudimentary markup; collapse whitespace; length-cap.
# Use at model entry for any API-sourced string before terminal / JSON / context output.
neutralize_plaintext() {
  local s="${1:-}" max="${2:-$PLAIN_TEXT_MAX}"
  [[ -z "$s" ]] && { printf ''; return; }
  # ANSI escapes
  s="$(printf '%s' "$s" | sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g')"
  # C0 controls except tab/newline → space; DEL
  s="$(printf '%s' "$s" | tr '\000-\010\013\014\016-\037\177' ' ')"
  # Rudimentary HTML/XML tags (remote Display fields may contain markup)
  s="$(printf '%s' "$s" | sed -E 's/<[^>]*>//g')"
  # Collapse runs of whitespace
  s="$(printf '%s' "$s" | sed -E 's/[[:space:]]+/ /g; s/^ //; s/ $//')"
  if ((${#s} > max)); then
    s="${s:0:max}…"
  fi
  printf '%s' "$s"
}

# Raw org id from snapshot JSON (may be empty or invalid).
org_id_raw() {
  local snap="${1-}"
  [[ -n "$snap" ]] || snap='{}'
  jq -r '.display.id // .org.Id // empty' <<<"$snap" 2>/dev/null
}

# Validated org id or the literal "unknown" — never invent an id.
org_id() {
  local raw
  raw="$(org_id_raw "$1")"
  if sf_org_id_ok "$raw"; then
    printf '%s' "$raw"
  else
    printf 'unknown'
  fi
}

# Truncated org id for compact chrome (bar). Full id when short enough.
org_id_trunc() {
  local id
  id="$(org_id "$1")"
  [[ "$id" == "unknown" ]] && { printf 'unknown'; return; }
  if ((${#id} > 11)); then
    printf '%s…%s' "${id:0:4}" "${id: -4}"
  else
    printf '%s' "$id"
  fi
}

# sandboxState for a snapshot: prod | sandbox | unknown
# Do not use ${1:-{}} — bash parses that as ${1:-{} + a leftover `}`, so jq
# reads one object then fails on the extra brace and we echo a second line.
sandbox_state() {
  local snap="${1-}" out
  [[ -n "$snap" ]] || snap='{}'
  out="$(jq -r 'if (.org | has("IsSandbox")) then (if .org.IsSandbox then "sandbox" else "prod" end) else "unknown" end' <<<"$snap" 2>/dev/null)" || out=""
  case "$out" in
    sandbox|prod) printf '%s' "$out" ;;
    *) printf 'unknown' ;;
  esac
}

# Fetch sandboxState + org id for an org alias via sf (does not change pin).
# Sets PIN_FETCH_ID and PIN_FETCH_STATE; returns 0 on success.
pin_fetch_identity() {
  local alias="$1" of=(-o "$alias") raw row
  PIN_FETCH_ID=""
  PIN_FETCH_STATE="unknown"
  raw="$(sf_try org display "${of[@]}")" || return 1
  PIN_FETCH_ID="$(jq -r '.result.id // empty' <<<"$raw")"
  row="$(sf_try data query "${of[@]}" -q "SELECT Id, IsSandbox FROM Organization LIMIT 1")" || return 0
  if jq -e '.result.records[0] | has("IsSandbox")' <<<"$row" >/dev/null 2>&1; then
    if jq -e '.result.records[0].IsSandbox == true' <<<"$row" >/dev/null; then
      PIN_FETCH_STATE="sandbox"
    else
      PIN_FETCH_STATE="prod"
    fi
    local oid
    oid="$(jq -r '.result.records[0].Id // empty' <<<"$row")"
    [[ -n "$oid" ]] && PIN_FETCH_ID="$oid"
  fi
  return 0
}

# Current pinned/default org identity from snapshot (if loadable).
pin_current_identity() {
  local snap
  PIN_CUR_ID=""
  PIN_CUR_STATE="unknown"
  snap="$(load_snapshot 2>/dev/null)" || return 1
  follow_org_cache "$snap"
  PIN_CUR_ID="$(org_id_raw "$snap")"
  PIN_CUR_STATE="$(sandbox_state "$snap")"
  return 0
}

# Read one line of confirm input (tty, pipe, or spawn $TERMINAL when launched from Waybar).
pin_read_confirm() {
  local prompt="$1" input=""
  printf '%s' "$prompt" >&2
  if [[ -t 0 ]]; then
    IFS= read -r input </dev/tty || true
  elif [[ -p /dev/stdin ]] 2>/dev/null || ! [[ -t 0 ]]; then
    IFS= read -r input || true
  elif [[ -n "${TERMINAL:-}" ]] && command -v "$TERMINAL" >/dev/null 2>&1; then
    input="$("$TERMINAL" -e bash -c 'IFS= read -r -p "Confirm: " line; printf "%s" "$line"' 2>/dev/null)" || true
  fi
  printf '%s' "$input"
}

# Gate pin/switch: same sandbox class → ok; unknown → require org id; cross boundary → confirm.
# Returns 0 if allowed, 1 if user declined, 2 if refused (unknown without id).
pin_confirm_switch() {
  local target_alias="$1" to_state="$2" to_id="$3"
  local from_state="${PIN_CUR_STATE:-unknown}" from_id="${PIN_CUR_ID:-}"

  # Normalize ids for comparison
  local tid="$to_id"
  sf_org_id_ok "$tid" || tid=""
  local fid="$from_id"
  sf_org_id_ok "$fid" || fid=""

  # Unknown environment: refuse unless user types the target 18-char org id.
  if [[ "$to_state" == "unknown" || "$from_state" == "unknown" ]]; then
    if [[ -z "$tid" ]]; then
      die "cannot pin: org environment unknown and org Id unreadable — try: sf org login web"
    fi
    local typed
    typed="$(pin_read_confirm "Org environment unknown. Type the 18-character org Id for '$target_alias' to confirm: ")" || true
    typed="$(neutralize_plaintext "$typed" 256)"
    if [[ "$typed" != "$tid" ]]; then
      printf 'bluechip: pin refused — org Id did not match.\n' >&2
      return 1
    fi
    return 0
  fi

  # Same sandbox class (prod→prod, sandbox→sandbox): low-friction.
  if [[ "$from_state" == "$to_state" ]]; then
    return 0
  fi

  # Crossing prod ↔ sandbox: explicit confirm.
  local expect="" prompt
  case "$to_state" in
    prod)    expect="PROD" ;;
    sandbox) expect="SANDBOX" ;;
    *)       expect="$target_alias" ;;
  esac
  prompt="Crossing ${from_state^^}→${to_state^^}. Type ${expect} or alias '${target_alias}' to confirm pin: "
  local typed
  typed="$(pin_read_confirm "$prompt")" || true
  typed="$(neutralize_plaintext "$typed" 256)"
  if [[ "$typed" == "$expect" || "$typed" == "$target_alias" ]]; then
    return 0
  fi
  printf 'bluechip: pin refused — confirmation did not match.\n' >&2
  return 1
}

# Salesforce org alias / username / record id — never interpolate raw CLI args into SOQL.
org_alias_ok() { [[ "$1" =~ ^[A-Za-z0-9._-]{1,80}$ ]]; }
sf_record_id_ok() { [[ "$1" =~ ^[a-zA-Z0-9]{15}([a-zA-Z0-9]{3})?$ ]]; }

soql_escape_literal() {
  local s="${1:-}"
  s="${s//\\/\\\\}"
  s="${s//\'/\\\'}"
  printf '%s' "$s"
}

# Drop query/fragment so Named Credential endpoints never leak tokens.
strip_url_secrets() {
  local u="${1:-}"
  u="${u%%\?*}"
  u="${u%%#*}"
  neutralize_plaintext "$u" 256
}

# Org identity blob for --json surfaces (three-valued sandboxState).
org_json_blob() {
  local snap="${1:-}"
  if [[ -z "$snap" ]]; then
    jq -nc '{id:null,sandboxState:"unknown",alias:null}'
    return
  fi
  jq -n --arg id "$(org_id "$snap")" --arg st "$(sandbox_state "$snap")" \
    --arg alias "$(jq -r '.display.alias // empty' <<<"$snap")" \
    '{
       id: (if $id == "unknown" or $id == "" then null else $id end),
       sandboxState: $st,
       alias: (if $alias == "" then null else $alias end)
     }'
}

probe_error_json() {
  local code="$1" message="$2"
  jq -nc --arg code "$code" --arg msg "$message" \
    '{
       ok: false,
       availability: "unknown",
       org: {id:null, sandboxState:"unknown", alias:null},
       error: {code:$code, message:$msg}
     }'
}

# Strip secret-shaped keys from any JSON (defense in depth — we also never SELECT them).
redact_secrets_json() {
  jq '
    def banned:
      ascii_downcase as $d
      | ($d == "password" or $d == "consumersecret" or $d == "clientsecret"
         or $d == "clientid" or $d == "accesstoken" or $d == "oauthtoken"
         or $d == "oauthrefreshtoken" or $d == "awsaccesssecret" or $d == "awsaccesskey"
         or $d == "authorization" or $d == "secret" or $d == "privatekey"
         or $d == "parametervalue" or $d == "metadata" or $d == "oauthscope"
         or ($d | test("secret|password|accesstoken|oauthrefresh|privatekey")));
    walk(if type == "object" then with_entries(select(.key | banned | not)) else . end)
  '
}

# Confirm-gated org write. Ban silent --yes on prod and unknown.
# Args: <action> <sandboxState> <orgId> <alias>
# Honors WANT_YES=1 only when sandboxState == sandbox.
write_confirm() {
  local action="$1" state="$2" oid="$3" alias="$4"
  local typed expect prompt

  if [[ "${WANT_YES:-0}" == "1" ]]; then
    if [[ "$state" == "sandbox" ]]; then
      return 0
    fi
    die "silent --yes is banned on ${state} (action: $action). Type the confirm string instead."
  fi

  case "$state" in
    sandbox)
      expect="${alias:-SANDBOX}"
      prompt="Confirm ${action} on SANDBOX. Type SANDBOX or alias '${expect}' : "
      typed="$(pin_read_confirm "$prompt")" || true
      typed="$(neutralize_plaintext "$typed" 256)"
      if [[ "$typed" == "SANDBOX" || "$typed" == "$expect" || ( -n "$alias" && "$typed" == "$alias" ) ]]; then
        return 0
      fi
      printf 'bluechip: %s refused — confirmation did not match.\n' "$action" >&2
      return 1
      ;;
    prod)
      prompt="Confirm ${action} on PROD. Type PROD : "
      typed="$(pin_read_confirm "$prompt")" || true
      typed="$(neutralize_plaintext "$typed" 256)"
      if [[ "$typed" == "PROD" ]]; then
        return 0
      fi
      printf 'bluechip: %s refused — type PROD to write on production.\n' "$action" >&2
      return 1
      ;;
    *)
      sf_org_id_ok "$oid" || die "cannot ${action}: org environment unknown and org Id unreadable"
      prompt="Org environment unknown. Type the 18-character org Id to confirm ${action}: "
      typed="$(pin_read_confirm "$prompt")" || true
      typed="$(neutralize_plaintext "$typed" 256)"
      if [[ "$typed" == "$oid" ]]; then
        return 0
      fi
      printf 'bluechip: %s refused — org Id did not match.\n' "$action" >&2
      return 1
      ;;
  esac
}
