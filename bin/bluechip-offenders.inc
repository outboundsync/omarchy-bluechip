# shellcheck shell=bash
# Limit offenders — "who burned API since 9am?"
# Event Log File when the running user can see it. Flex/Digital Wallet stays
# unknown unless a stable CLI object exists. Never fake 0 on a miss.

OFFENDERS_MAX_FILES="${BLUECHIP_OFFENDERS_MAX_FILES:-3}"
OFFENDERS_MAX_BYTES="${BLUECHIP_OFFENDERS_MAX_BYTES:-524288}"
OFFENDERS_MAX_ROWS="${BLUECHIP_OFFENDERS_MAX_ROWS:-20000}"
SF_API_VERSION="${BLUECHIP_API_VERSION:-62.0}"

_offenders_error() {
  local code="$1" message="$2"
  local snap orgblob
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  orgblob="$(org_json_blob "$snap")"
  jq -n --argjson org "$orgblob" --arg c "$code" --arg m "$message" \
    '{ok:true,availability:"unknown",org:$org,since:null,files:[],offenders:{byUser:[],byClient:[],byConnectedApp:[]},sources:{eventLog:{availability:"unknown",reason:$c},flex:{availability:"unknown",reason:"flex_credits_not_queryable"}},error:{code:$c,message:$m}}'
}

_offenders_parse_since() {
  local spec="${1:-9am}" hour=9 minute=0 ap="" epoch now
  now="$(date +%s)"
  if [[ "$spec" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}([T ][0-9]{2}:[0-9]{2}(:[0-9]{2})?(Z|[+-][0-9]{2}:?[0-9]{2})?)?$ ]]; then
    epoch="$(date -d "$spec" +%s 2>/dev/null || date -j -f "%Y-%m-%dT%H:%M:%S" "${spec/Z/}" +%s 2>/dev/null || true)"
    [[ -n "$epoch" ]] && { printf '%s' "$epoch"; return 0; }
  fi
  if [[ "$spec" =~ ^([0-9]{1,2})(:([0-9]{2}))?(am|pm|AM|PM)$ ]]; then
    hour="${BASH_REMATCH[1]}"
    minute="${BASH_REMATCH[3]:-0}"
    ap="$(printf '%s' "${BASH_REMATCH[4]}" | tr '[:upper:]' '[:lower:]')"
    hour=$((10#$hour))
    minute=$((10#$minute))
    if [[ "$ap" == "pm" && "$hour" -lt 12 ]]; then hour=$((hour + 12)); fi
    if [[ "$ap" == "am" && "$hour" -eq 12 ]]; then hour=0; fi
  elif [[ "$spec" =~ ^([0-9]{1,2}):([0-9]{2})$ ]]; then
    hour=$((10#${BASH_REMATCH[1]}))
    minute=$((10#${BASH_REMATCH[2]}))
  elif [[ "$spec" =~ ^([0-9]{1,2})am$ ]]; then
    hour=$((10#${BASH_REMATCH[1]}))
    [[ "$hour" -eq 12 ]] && hour=0
  elif [[ "$spec" =~ ^([0-9]{1,2})pm$ ]]; then
    hour=$((10#${BASH_REMATCH[1]}))
    [[ "$hour" -lt 12 ]] && hour=$((hour + 12))
  else
    return 1
  fi
  (( hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 )) || return 1
  local today
  today="$(date +%Y-%m-%d)"
  epoch="$(date -d "${today} ${hour}:${minute}:00" +%s 2>/dev/null \
    || date -j -f "%Y-%m-%d %H:%M:%S" "${today} ${hour}:${minute}:00" +%s 2>/dev/null || true)"
  [[ -z "$epoch" ]] && return 1
  if (( epoch > now )); then
    epoch=$((epoch - 86400))
  fi
  printf '%s' "$epoch"
}

_offenders_soql_date() {
  local epoch="$1"
  date -u -d "@$epoch" +%Y-%m-%dT%H:%M:%S.000+0000 2>/dev/null \
    || date -u -r "$epoch" +%Y-%m-%dT%H:%M:%S.000+0000
}

_offenders_soql_day() {
  local epoch="$1"
  date -u -d "@$epoch" +%Y-%m-%d 2>/dev/null \
    || date -u -r "$epoch" +%Y-%m-%d
}

_offenders_fetch_logfile() {
  local id="$1" path raw
  sf_record_id_ok "$id" || return 1
  path="/services/data/v${SF_API_VERSION}/sobjects/EventLogFile/${id}/LogFile"
  raw="$(sf_try api request rest GET "$path" "${OFLAG[@]}")" && { printf '%s\n' "$raw"; return 0; }
  raw="$(sf_try api request rest --method GET --url "$path" "${OFLAG[@]}")" && { printf '%s\n' "$raw"; return 0; }
  printf '%s\n' "$raw"
  return 1
}

_offenders_csv_body() {
  local raw="$1"
  jq -r '
    if (.result | type) == "string" then .result
    elif .result.log then .result.log
    elif .result.body then .result.body
    elif .result.LogFile then .result.LogFile
    else empty end
  ' <<<"$raw" 2>/dev/null
}

_offenders_rank_csv() {
  local csv="$1" since_epoch="${2:-0}" tmp
  if ! command -v python3 >/dev/null 2>&1; then
    echo '{"ok":false,"reason":"parser_unavailable","byUser":[],"byClient":[],"byConnectedApp":[],"rows":0}'
    return 0
  fi
  tmp="$(mktemp "${TMPDIR:-/tmp}/bluechip-elf.XXXXXX")"
  chmod 600 "$tmp" 2>/dev/null || true
  printf '%s' "$csv" > "$tmp"
  OFFENDERS_MAX_ROWS="$OFFENDERS_MAX_ROWS" OFFENDERS_MAX_BYTES="$OFFENDERS_MAX_BYTES" python3 - "$since_epoch" "$tmp" <<'PY'
import csv, io, json, os, sys
since = int(sys.argv[1]) if len(sys.argv) > 1 and str(sys.argv[1]).isdigit() else 0
path = sys.argv[2]
max_bytes = int(os.environ.get("OFFENDERS_MAX_BYTES", "524288"))
max_rows = int(os.environ.get("OFFENDERS_MAX_ROWS", "20000"))
try:
    with open(path, encoding="utf-8", errors="replace") as fh:
        raw = fh.read(max_bytes + 1)
    if len(raw) > max_bytes:
        raw = raw[:max_bytes]
finally:
    try:
        os.remove(path)
    except OSError:
        pass
if not raw.strip():
    print(json.dumps({"ok": True, "reason": None, "byUser": [], "byClient": [], "byConnectedApp": [], "rows": 0, "truncated": False}))
    raise SystemExit
f = io.StringIO(raw.lstrip("\ufeff"))
try:
    reader = csv.DictReader(f)
except csv.Error as exc:
    print(json.dumps({"ok": False, "reason": "csv_unreadable", "byUser": [], "byClient": [], "byConnectedApp": [], "rows": 0}))
    raise SystemExit
users, clients, apps = {}, {}, {}
rows = 0
truncated = False

def parse_ts(val):
    s = (val or "").strip()
    if not s:
        return None
    digits = "".join(ch for ch in s if ch.isdigit())
    if len(digits) >= 14:
        try:
            import datetime
            dt = datetime.datetime.strptime(digits[:14], "%Y%m%d%H%M%S")
            return int(dt.replace(tzinfo=datetime.timezone.utc).timestamp())
        except ValueError:
            return None
    return None

try:
    for row in reader:
        rows += 1
        if rows > max_rows:
            truncated = True
            break
        keys = {((k or "").strip().upper().replace(" ", "_")): (v or "").strip() for k, v in row.items()}
        ts = parse_ts(keys.get("TIMESTAMP") or keys.get("EVENT_TIME") or keys.get("CREATED_DATE") or "")
        if since and ts is not None and ts < since:
            continue
        uid = keys.get("USER_ID") or keys.get("USERID") or ""
        cid = keys.get("CLIENT_ID") or keys.get("CLIENTID") or ""
        app = keys.get("CONNECTED_APP_ID") or keys.get("CONNECTEDAPPID") or keys.get("CONNECTED_APP") or ""
        if uid:
            rec = users.setdefault(uid, {"userId": uid, "count": 0, "clientIds": set(), "connectedAppIds": set()})
            rec["count"] += 1
            if cid:
                rec["clientIds"].add(cid)
            if app:
                rec["connectedAppIds"].add(app)
        if cid:
            clients[cid] = clients.get(cid, 0) + 1
        if app:
            apps[app] = apps.get(app, 0) + 1
except csv.Error:
    print(json.dumps({"ok": False, "reason": "csv_unreadable", "byUser": [], "byClient": [], "byConnectedApp": [], "rows": rows}))
    raise SystemExit

def top_map(d, key, n=15):
    items = sorted(d.items(), key=lambda kv: (-kv[1], kv[0]))[:n]
    return [{key: k, "count": v} for k, v in items]

by_user = []
for rec in sorted(users.values(), key=lambda r: (-r["count"], r["userId"]))[:15]:
    by_user.append({
        "userId": rec["userId"][:18],
        "count": rec["count"],
        "clientIds": sorted(rec["clientIds"])[:8],
        "connectedAppIds": sorted(rec["connectedAppIds"])[:8],
    })
print(json.dumps({
    "ok": True,
    "reason": None,
    "byUser": by_user,
    "byClient": top_map(clients, "clientId"),
    "byConnectedApp": top_map(apps, "connectedAppId"),
    "rows": rows,
    "truncated": truncated,
}))
PY
}

compute_offenders() {
  local since_spec="${1:-9am}"
  have_sf || { _offenders_error sf_missing "sf CLI not found"; return 1; }
  set_oflag
  local snap orgblob
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  orgblob="$(org_json_blob "$snap")"

  local since_epoch
  since_epoch="$(_offenders_parse_since "$since_spec")" || {
    _offenders_error config_invalid "could not parse --since '$since_spec' (use 9am or ISO)"
    return 0
  }
  local since_iso since_day
  since_iso="$(_offenders_soql_date "$since_epoch")"
  since_day="$(_offenders_soql_day "$since_epoch")"

  local raw
  raw="$(sf_try data query "${OFLAG[@]}" -q \
    "SELECT Id, EventType, LogDate, LogFileLength, Sequence, Interval FROM EventLogFile WHERE LogDate >= ${since_day} AND EventType IN ('API','RestApi','ApiTotalUsage','ApexCallout','BulkApi','BulkApi2','BulkApiRequest') ORDER BY LogDate DESC LIMIT 20")" \
    || {
      local why; why="$(classify_sf_error "$raw")"
      jq -n --argjson org "$orgblob" --arg c "$why" --arg since "$since_iso" --arg spec "$since_spec" \
        '{
           ok: true,
           availability: "unknown",
           org: $org,
           since: {spec:$spec, iso:$since},
           files: [],
           offenders: {byUser:[], byClient:[], byConnectedApp:[]},
           sources: {
             eventLog: {availability:"unknown", reason:$c},
             flex: {availability:"unknown", reason:"flex_credits_not_queryable"}
           },
           error: {code:$c, message:("EventLogFile query failed: "+$c+" — not a fake 0")}
         }'
      return 0
    }

  local files
  files="$(jq -c '[.result.records[]? | {id:.Id, eventType:.EventType, logDate:.LogDate, logFileLength:.LogFileLength, sequence:.Sequence, interval:.Interval}]' <<<"$raw")"

  local nfiles
  nfiles="$(jq -r 'length' <<<"$files")"
  if [[ "$nfiles" == "0" ]]; then
    jq -n --argjson org "$orgblob" --argjson files "$files" --arg since "$since_iso" --arg spec "$since_spec" \
      '{
         ok: true,
         availability: "ok",
         org: $org,
         since: {spec:$spec, iso:$since},
         files: $files,
         offenders: {byUser:[], byClient:[], byConnectedApp:[]},
         sources: {
           eventLog: {availability:"ok", reason:null},
           flex: {availability:"unknown", reason:"flex_credits_not_queryable"}
         },
         note: "No EventLogFile rows in the window. Empty is not the same as Event Log unavailable.",
         error: null
       }'
    return 0
  fi

  local ids=() id
  mapfile -t ids < <(jq -r '.[].id' <<<"$files" | head -n "$OFFENDERS_MAX_FILES")
  local bodies_ok=0 any_try=0 csv_all="" body_why=""
  for id in "${ids[@]}"; do
    [[ -z "$id" ]] && continue
    any_try=1
    local lraw body
    lraw="$(_offenders_fetch_logfile "$id")" || {
      body_why="$(classify_sf_error "$lraw")"
      continue
    }
    body="$(_offenders_csv_body "$lraw")"
    if [[ -z "$body" ]]; then
      body_why="logfile_unreadable"
      continue
    fi
    if ((${#body} > OFFENDERS_MAX_BYTES)); then
      body="${body:0:OFFENDERS_MAX_BYTES}"
    fi
    csv_all+="$body"$'\n'
    bodies_ok=1
  done

  if [[ $bodies_ok -eq 0 ]]; then
    local why="${body_why:-event_log_body_unavailable}"
    [[ $any_try -eq 0 ]] && why="event_log_body_unavailable"
    jq -n --argjson org "$orgblob" --argjson files "$files" --arg since "$since_iso" --arg spec "$since_spec" --arg c "$why" \
      '{
         ok: true,
         availability: "unknown",
         org: $org,
         since: {spec:$spec, iso:$since},
         files: $files,
         offenders: {byUser:[], byClient:[], byConnectedApp:[]},
         sources: {
           eventLog: {availability:"unknown", reason:$c},
           flex: {availability:"unknown", reason:"flex_credits_not_queryable"}
         },
         error: {code:$c, message:("Event Log files listed but LogFile body unavailable ("+$c+") — not a fake 0")}
       }'
    return 0
  fi

  local ranked
  ranked="$(_offenders_rank_csv "$csv_all" "$since_epoch")"
  if [[ "$(jq -r '.ok' <<<"$ranked")" != "true" ]]; then
    local pr; pr="$(jq -r '.reason // "csv_unreadable"' <<<"$ranked")"
    jq -n --argjson org "$orgblob" --argjson files "$files" --arg since "$since_iso" --arg spec "$since_spec" --arg c "$pr" \
      '{
         ok: true,
         availability: "unknown",
         org: $org,
         since: {spec:$spec, iso:$since},
         files: $files,
         offenders: {byUser:[], byClient:[], byConnectedApp:[]},
         sources: {
           eventLog: {availability:"unknown", reason:$c},
           flex: {availability:"unknown", reason:"flex_credits_not_queryable"}
         },
         error: {code:$c, message:("Event Log CSV could not be ranked ("+$c+") — not a fake 0")}
       }'
    return 0
  fi

  jq -n --argjson org "$orgblob" --argjson files "$files" --argjson ranked "$ranked" \
    --arg since "$since_iso" --arg spec "$since_spec" \
    '{
       ok: true,
       availability: "ok",
       org: $org,
       since: {spec:$spec, iso:$since},
       files: $files,
       offenders: {byUser:$ranked.byUser, byClient:$ranked.byClient, byConnectedApp:$ranked.byConnectedApp},
       rowsParsed: $ranked.rows,
       truncated: ($ranked.truncated // false),
       sources: {
         eventLog: {availability:"ok", reason:null},
         flex: {availability:"unknown", reason:"flex_credits_not_queryable"}
       },
       note: "Event Log files are daily; --since filters TIMESTAMP when present. Flex Credits are not queryable via this CLI.",
       error: null
     }' | redact_secrets_json
}

cmd_offenders() {
  local since="9am" want_json=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) want_json=1; shift ;;
      --since) since="${2:-}"; shift 2 ;;
      --since=*) since="${1#*=}"; shift ;;
      help|-h|--help)
        cat <<EOF
${BOLD}bluechip offenders${RESET} — who burned API (best-effort Event Log)

  ${CYAN}offenders${RESET} [--since 9am|ISO] [--json]
      Rank users / connected apps / client ids from EventLogFile when
      View Event Log Files is available. If Event Log or Flex is not
      queryable: availability unknown with a reason — never a fake 0.

Default --since is 9am local (rolls to yesterday if that is still future).
LogFile bodies are not cached (PII). Flex / Digital Wallet: unknown.
EOF
        return 0
        ;;
      *) die "unknown offenders option: $1" ;;
    esac
  done
  [[ "${WANT_JSON:-0}" -eq 1 ]] && want_json=1

  local h
  h="$(compute_offenders "$since")" || h="$(_offenders_error transient "offenders probe failed")"
  if [[ $want_json -eq 1 ]]; then
    printf '%s\n' "$h"
    return 0
  fi

  local snap env envcolor
  take_snapshot || true; snap="${SNAPSHOT_BLOB:-}"
  case "$(sandbox_state "${snap:-"{}"}")" in
    sandbox) env="SANDBOX"; envcolor="$YELLOW" ;;
    prod) env="PROD"; envcolor="$RED" ;;
    *) env="UNKNOWN"; envcolor="$GREY" ;;
  esac
  printf '%sLimit offenders%s  %s%s%s  %s%s%s  since %s\n\n' \
    "$BOLD" "$RESET" "$envcolor" "$env" "$RESET" "$GREY" "$(org_id "${snap:-"{}"}")" "$RESET" \
    "$(neutralize_plaintext "$(jq -r '.since.spec // "9am"' <<<"$h")" 32)"

  if [[ "$(jq -r '.availability' <<<"$h")" != "ok" ]]; then
    printf '  %sunknown%s  %s\n' "$GREY" "$RESET" \
      "$(neutralize_plaintext "$(jq -r '.error.message // "Event Log unavailable"' <<<"$h")" 240)"
    printf '  %sflex: %s%s\n' "$DIM" "$(jq -r '.sources.flex.reason // "unknown"' <<<"$h")" "$RESET"
    return 0
  fi

  local nu
  nu="$(jq -r '.offenders.byUser | length' <<<"$h")"
  if [[ "$nu" == "0" ]]; then
    printf '  %sno ranked offenders in window%s\n' "$DIM" "$RESET"
    printf '  %s%s%s\n' "$DIM" "$(neutralize_plaintext "$(jq -r '.note // empty' <<<"$h")" 200)" "$RESET"
    return 0
  fi
  printf '%sBy user%s\n' "$BOLD" "$RESET"
  jq -r '.offenders.byUser[] | [.userId, (.count|tostring), ([.clientIds[]?] | join(",")), ([.connectedAppIds[]?] | join(","))] | @tsv' <<<"$h" \
  | while IFS=$'\t' read -r uid cnt cids apps; do
      printf '  %6s  %s  client %s  app %s\n' \
        "$cnt" \
        "$(neutralize_plaintext "$uid" 22)" \
        "$(neutralize_plaintext "${cids:-—}" 40)" \
        "$(neutralize_plaintext "${apps:-—}" 40)"
    done
  printf '\n%sBy client id%s\n' "$BOLD" "$RESET"
  jq -r '.offenders.byClient[]? | [(.count|tostring), .clientId] | @tsv' <<<"$h" \
  | while IFS=$'\t' read -r cnt cid; do
      printf '  %6s  %s\n' "$cnt" "$(neutralize_plaintext "$cid" 40)"
    done
  printf '\n  %sflex: unknown (%s)%s\n' "$DIM" "$(jq -r '.sources.flex.reason' <<<"$h")" "$RESET"
}
