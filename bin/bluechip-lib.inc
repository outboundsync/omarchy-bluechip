# shellcheck shell=bash
# Shared helpers for bluechip — neutralize remote strings, org id formatting.
# Sourced by bin/bluechip (and tests).

# Max length for Salesforce/API-sourced strings rendered to terminal, Waybar, or context.
PLAIN_TEXT_MAX="${BLUECHIP_PLAINTEXT_MAX:-500}"

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
  local snap="${1:-\{\}}"
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
sandbox_state() {
  jq -r 'if (.org | has("IsSandbox")) then (if .org.IsSandbox then "sandbox" else "prod" end) else "unknown" end' <<<"${1:-{}}" 2>/dev/null \
    || echo unknown
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
