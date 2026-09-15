#!/usr/bin/env python3
"""Parse ApexClass Body / SymbolTable into property trees.

Read-only helper for `bluechip types`. Stdin JSON → stdout JSON.
Never prints Authorization values, ParameterValue, or class bodies.
"""
from __future__ import annotations

import json
import re
import sys

MAX_BODY = 65_536
MAX_CLASSES = 40
MAX_PROPS = 80
MAX_INNER = 24
MAX_NAME = 80
MAX_TYPE = 80

VIS_RE = r"(?:global|public|protected|private)"
CLASS_RE = re.compile(
    rf"\b({VIS_RE})\s+(?:virtual\s+|abstract\s+|with sharing\s+|without sharing\s+|inherited sharing\s+)*class\s+([A-Za-z_][A-Za-z0-9_]*)",
    re.IGNORECASE,
)
PROP_RE = re.compile(
    rf"\b({VIS_RE})\s+(?:static\s+|final\s+|transient\s+)*(?:@\w+\s+)*"
    rf"([A-Za-z_][\w.<>,\s]*?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:;|{{)",
    re.IGNORECASE,
)
METHOD_HINT_RE = re.compile(r"\(")
BANNED_NAME = re.compile(
    r"(password|secret|authorization|parametervalue|accesstoken|consumersecret)",
    re.IGNORECASE,
)


def _cap(s: object, n: int) -> str:
    t = "" if s is None else str(s)
    t = re.sub(r"\s+", " ", t).strip()
    if len(t) > n:
        t = t[:n] + "…"
    return t


def _kind(name: str) -> str:
    n = name or ""
    up = n.upper()
    if "EXTERNALSERVICE" in up.replace("_", ""):
        return "external_service"
    if up.startswith("IN_") or "_IN_" in up:
        return "http_callout_in"
    if "2XX" in up or up.startswith("OUT_"):
        return "http_callout_2xx"
    return "apex_defined"


def _type_name(typ: object) -> str:
    if typ is None:
        return "Object"
    if isinstance(typ, str):
        return _cap(typ, MAX_TYPE)
    if isinstance(typ, dict):
        name = typ.get("name") or typ.get("Name")
        refs = typ.get("referenceTo") or typ.get("parameters")
        if isinstance(refs, list) and refs and isinstance(refs[0], dict):
            inner = _type_name(refs[0])
            if name and str(name).lower() in {"list", "set", "map"}:
                return _cap(f"{name}<{inner}>", MAX_TYPE)
        if name:
            return _cap(name, MAX_TYPE)
    return "Object"


def _from_symbol_table(table: dict, prefix: str = "") -> tuple[list[dict], list[dict]]:
    props: list[dict] = []
    inner: list[dict] = []
    raw_props = table.get("properties") or table.get("Properties") or []
    if isinstance(raw_props, list):
        for p in raw_props[:MAX_PROPS]:
            if not isinstance(p, dict):
                continue
            name = _cap(p.get("name") or p.get("Name"), MAX_NAME)
            if not name or BANNED_NAME.search(name):
                continue
            path = f"{prefix}.{name}" if prefix else name
            vis = _cap(p.get("visibility") or p.get("modifiers") or "public", 24)
            if isinstance(p.get("modifiers"), list):
                vis = _cap(",".join(str(m) for m in p["modifiers"]), 24)
            props.append(
                {
                    "path": path,
                    "name": name,
                    "type": _type_name(p.get("type") or p.get("Type")),
                    "visibility": vis.lower() if isinstance(vis, str) else "public",
                }
            )
    raw_inner = table.get("innerClasses") or table.get("InnerClasses") or []
    if isinstance(raw_inner, list):
        for ic in raw_inner[:MAX_INNER]:
            if not isinstance(ic, dict):
                continue
            iname = _cap(ic.get("name") or ic.get("Name"), MAX_NAME)
            if not iname:
                continue
            iprefix = f"{prefix}.{iname}" if prefix else iname
            iprops, iinner = _from_symbol_table(ic, iprefix)
            inner.append(
                {
                    "name": iname,
                    "kind": _kind(iname),
                    "properties": iprops,
                    "inner": iinner,
                }
            )
    return props, inner


def _extract_block(src: str, start: int) -> tuple[str, int]:
    i = src.find("{", start)
    if i < 0:
        return "", start
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i + 1 : j], j + 1
    return src[i + 1 :], len(src)


def _from_body(body: str) -> list[dict]:
    if not body:
        return []
    if len(body) > MAX_BODY:
        body = body[:MAX_BODY]
    # Drop comments so secrets in comments never become names.
    body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
    body = re.sub(r"//.*?$", " ", body, flags=re.M)
    classes: list[dict] = []

    def walk(src: str, prefix: str) -> list[dict]:
        found: list[dict] = []
        pos = 0
        while True:
            m = CLASS_RE.search(src, pos)
            if not m:
                break
            cname = _cap(m.group(2), MAX_NAME)
            block, end = _extract_block(src, m.end())
            pos = end
            if not cname:
                continue
            cprefix = f"{prefix}.{cname}" if prefix else cname
            props: list[dict] = []
            # Strip nested class blocks before property scan so inner fields
            # stay on the inner class.
            stripped = src_without_inner_classes(block)
            for pm in PROP_RE.finditer(stripped):
                typ = _cap(pm.group(2), MAX_TYPE)
                pname = _cap(pm.group(3), MAX_NAME)
                if not pname or BANNED_NAME.search(pname):
                    continue
                # Skip method-looking leftovers and class keywords.
                if METHOD_HINT_RE.search(typ) or pname.lower() == "class":
                    continue
                if typ.lower() in {"class", "interface", "enum"}:
                    continue
                path = f"{cprefix}.{pname}" if cprefix else pname
                props.append(
                    {
                        "path": path,
                        "name": pname,
                        "type": typ,
                        "visibility": (pm.group(1) or "public").lower(),
                    }
                )
                if len(props) >= MAX_PROPS:
                    break
            inner = walk(block, cprefix)[:MAX_INNER]
            found.append(
                {
                    "name": cname,
                    "kind": _kind(cname),
                    "properties": props,
                    "inner": inner,
                }
            )
            if len(found) >= MAX_INNER and prefix:
                break
        return found

    classes = walk(body, "")
    return classes[:MAX_CLASSES]


def src_without_inner_classes(block: str) -> str:
    out = []
    pos = 0
    while True:
        m = CLASS_RE.search(block, pos)
        if not m:
            out.append(block[pos:])
            break
        out.append(block[pos : m.start()])
        _, end = _extract_block(block, m.end())
        pos = end
    return "".join(out)


def parse_class(rec: dict) -> dict:
    name = _cap(rec.get("Name") or rec.get("name"), MAX_NAME)
    ns = rec.get("NamespacePrefix") or rec.get("namespace")
    ns = _cap(ns, MAX_NAME) if ns else None
    table = rec.get("SymbolTable") or rec.get("symbolTable")
    props: list[dict] = []
    inner: list[dict] = []
    source = "none"
    if isinstance(table, dict) and (table.get("properties") or table.get("innerClasses")):
        root = name or _cap(table.get("name") or table.get("Name"), MAX_NAME)
        props, inner = _from_symbol_table(table, root)
        source = "symbol_table"
    else:
        body = rec.get("Body") or rec.get("body") or ""
        parsed = _from_body(str(body))
        # Prefer the matching top-level class; otherwise first.
        match = next((c for c in parsed if c["name"] == name), None)
        chosen = match or (parsed[0] if parsed else None)
        if chosen:
            props = chosen.get("properties") or []
            inner = chosen.get("inner") or []
            if not name:
                name = chosen["name"]
        source = "body" if chosen else "none"
    return {
        "name": name or "Unknown",
        "namespace": ns,
        "kind": _kind(name or ""),
        "source": source,
        "properties": props,
        "inner": inner,
    }


def flatten_paths(cls: dict) -> list[dict]:
    rows: list[dict] = []

    def walk(node: dict) -> None:
        for p in node.get("properties") or []:
            rows.append(
                {
                    "path": p.get("path"),
                    "type": p.get("type"),
                    "class": node.get("name"),
                    "kind": node.get("kind"),
                }
            )
        for ic in node.get("inner") or []:
            walk(ic)

    walk(cls)
    return rows[: 200]


def main() -> int:
    try:
        raw = sys.stdin.read(1_048_576)
        data = json.loads(raw or "{}")
    except json.JSONDecodeError:
        print(json.dumps({"classes": [], "error": "invalid_json"}))
        return 0
    recs = data.get("classes") or data.get("records") or []
    if not isinstance(recs, list):
        recs = []
    out = []
    for rec in recs[:MAX_CLASSES]:
        if not isinstance(rec, dict):
            continue
        parsed = parse_class(rec)
        parsed["assignmentPaths"] = flatten_paths(parsed)
        out.append(parsed)
    json.dump({"classes": out}, sys.stdout, separators=(",", ":"))
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
