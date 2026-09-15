#!/usr/bin/env python3
"""Write stub-sf fixture trees for hygiene acceptance tests H1–H10."""
from __future__ import annotations

import json
import sys
from pathlib import Path


def write(p: Path, obj) -> None:
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj, indent=2) + "\n")


def ok(result) -> dict:
    return {"status": 0, "result": result}


def err(name: str, message: str) -> dict:
    return {"status": 1, "name": name, "message": message, "exitCode": 1}


def describe(name: str, fields: list[str], inaccessible: list[str] | None = None) -> dict:
    inaccessible = inaccessible or []
    fl = [{"name": "Id", "type": "id", "accessible": True}]
    for f in fields:
        fl.append({"name": f, "type": "string", "accessible": f not in inaccessible})
    return ok({"name": name, "queryable": True, "fields": fl})


def count(total: int, fills: list[int] | None = None) -> dict:
    rec: dict = {"total": total}
    for i, n in enumerate(fills or []):
        rec[f"f{i}"] = n
    return ok({"records": [rec], "totalSize": 1, "done": True})


def c_count(n: int) -> dict:
    return ok({"records": [{"c": n}], "totalSize": 1, "done": True})


def dupes(groups: list[int]) -> dict:
    return ok({"records": [{"ct": g} for g in groups], "totalSize": len(groups), "done": True})


ORG_DISPLAY = ok(
    {
        "id": "00D000000000001AAA",
        "alias": "acme",
        "username": "admin@example.com",
        "instanceUrl": "https://example.my.salesforce.com",
    }
)
ORG_ROW = ok(
    {
        "records": [
            {
                "Id": "00D000000000001AAA",
                "Name": "Acme",
                "IsSandbox": False,
                "OrganizationType": "Enterprise Edition",
                "InstanceName": "NA99",
            }
        ]
    }
)
ORG_ROW_EMPTY = ok({"records": []})
LIMITS = ok([{"name": "DailyApiRequests", "max": 10000, "remaining": 8000}])
LIMITS_FAIL = err("ENOTFOUND", "getaddrinfo ENOTFOUND")


def base_map(**extra) -> dict:
    m = {
        "org.display": "org-display.json",
        "org.list.limits": "limits.json",
        "query.Organization.row": "org-row.json",
    }
    m.update(extra)
    return m


def write_common(d: Path, *, limits=LIMITS, org_row=ORG_ROW) -> None:
    write(d / "org-display.json", ORG_DISPLAY)
    write(d / "limits.json", limits)
    write(d / "org-row.json", org_row)


def add_object(
    d: Path,
    mp: dict,
    api: str,
    *,
    fields: list[str],
    inaccessible: list[str] | None = None,
    total: int = 1200,
    fills: list[int] | None = None,
    stale: int = 100,
    orphaned: int = 10,
    dupe_groups: list[int] | None = None,
    overdue: int | None = None,
    describe_error: dict | None = None,
    completeness: dict | None = None,
    freshness: dict | None = None,
    ownership: dict | None = None,
    dupes_payload: dict | None = None,
    pipeline: dict | None = None,
) -> None:
    if describe_error:
        write(d / f"describe-{api}.json", describe_error)
    else:
        write(d / f"describe-{api}.json", describe(api, fields, inaccessible))
    mp[f"sobject.describe.{api}"] = f"describe-{api}.json"

    write(d / f"{api}-completeness.json", completeness or count(total, fills or [900] * len(fields)))
    mp[f"query.{api}.completeness"] = f"{api}-completeness.json"
    write(d / f"{api}-freshness.json", freshness or c_count(stale))
    mp[f"query.{api}.freshness"] = f"{api}-freshness.json"
    write(d / f"{api}-ownership.json", ownership or c_count(orphaned))
    mp[f"query.{api}.ownership"] = f"{api}-ownership.json"
    write(d / f"{api}-dupes.json", dupes_payload or dupes(dupe_groups or [5, 3]))
    mp[f"query.{api}.dupes"] = f"{api}-dupes.json"
    if overdue is not None or pipeline is not None:
        write(d / f"{api}-pipeline.json", pipeline or c_count(overdue or 0))
        mp[f"query.{api}.pipeline"] = f"{api}-pipeline.json"


def cfg(objects: list[dict]) -> list[dict]:
    return objects


def emit(root: Path) -> None:
    # H1 — Lead object forbidden; Contact measured
    d = root / "h1"
    mp = base_map()
    write_common(d)
    add_object(
        d,
        mp,
        "Lead",
        fields=["Email"],
        describe_error=err("INVALID_TYPE", "sObject type 'Lead' is not supported."),
    )
    add_object(d, mp, "Contact", fields=["Email", "Phone"], total=800, fills=[700, 600])
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg(
            [
                {"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"},
                {"api": "Contact", "label": "Contacts", "filter": "", "fields": ["Email", "Phone"], "activityDays": 90, "dupeField": "Email"},
            ]
        ),
    )

    # H2 — Custom__c FLS hidden (in describe, accessible:false); Email measured
    d = root / "h2"
    mp = base_map()
    write_common(d)
    add_object(
        d,
        mp,
        "Lead",
        fields=["Email", "Custom__c"],
        inaccessible=["Custom__c"],
        fills=[900],  # only Email is COUNTed
    )
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg(
            [
                {
                    "api": "Lead",
                    "label": "Leads",
                    "filter": "IsConverted = false",
                    "fields": ["Email", "Custom__c"],
                    "activityDays": 90,
                    "dupeField": "Email",
                }
            ]
        ),
    )

    # H3 — NoSuch__c not in describe
    d = root / "h3"
    mp = base_map()
    write_common(d)
    add_object(d, mp, "Lead", fields=["Email"], fills=[900])
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg(
            [
                {
                    "api": "Lead",
                    "label": "Leads",
                    "filter": "",
                    "fields": ["Email", "NoSuch__c"],
                    "activityDays": 90,
                    "dupeField": "Email",
                }
            ]
        ),
    )

    # H4 — LastActivityDate / freshness query denied
    d = root / "h4"
    mp = base_map()
    write_common(d)
    add_object(
        d,
        mp,
        "Lead",
        fields=["Email"],
        fills=[900],
        freshness=err("INVALID_FIELD", "No such column 'LastActivityDate' on entity 'Lead'."),
    )
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg([{"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"}]),
    )

    # H5 — completeness 502; last-good seeded by the harness
    d = root / "h5"
    mp = base_map()
    write_common(d)
    add_object(
        d,
        mp,
        "Lead",
        fields=["Email"],
        completeness=err("TRANSIENT", "502 Bad Gateway"),
    )
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg([{"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"}]),
    )

    # H6 — every object forbidden
    d = root / "h6"
    mp = base_map()
    write_common(d)
    add_object(d, mp, "Lead", fields=["Email"], describe_error=err("INVALID_TYPE", "Lead hidden"))
    add_object(d, mp, "Contact", fields=["Email"], describe_error=err("INSUFFICIENT_ACCESS", "Contact hidden"))
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg(
            [
                {"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"},
                {"api": "Contact", "label": "Contacts", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"},
            ]
        ),
    )

    # H7 — empty object
    d = root / "h7"
    mp = base_map()
    write_common(d)
    add_object(d, mp, "Lead", fields=["Email"], total=0, fills=[0], stale=0, orphaned=0, dupe_groups=[])
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg([{"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"}]),
    )

    # H8 — integration user: object Read, field Read missing (same as H2 / SavvyCal trap)
    d = root / "h8"
    mp = base_map()
    write_common(d)
    add_object(
        d,
        mp,
        "Lead",
        fields=["Email", "SavvyCal_Id__c"],
        inaccessible=["SavvyCal_Id__c"],
        fills=[400],
    )
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg(
            [
                {
                    "api": "Lead",
                    "label": "Leads",
                    "filter": "",
                    "fields": ["Email", "SavvyCal_Id__c"],
                    "activityDays": 90,
                    "dupeField": "Email",
                }
            ]
        ),
    )

    # H9a — unauthenticated org display
    d = root / "h9-unauth"
    write(d / "org-display.json", err("NoDefaultOrgFound", "No authorization found for this org"))
    write(d / "map.json", {"org.display": "org-display.json"})
    write(
        d / "config.json",
        cfg([{"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"}]),
    )

    # H9b — offline / ENOTFOUND
    d = root / "h9-offline"
    write(d / "org-display.json", err("ENOTFOUND", "getaddrinfo ENOTFOUND login.salesforce.com"))
    write(d / "map.json", {"org.display": "org-display.json"})
    write(
        d / "config.json",
        cfg([{"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"}]),
    )

    # H10 — dupe groups hit the cap
    d = root / "h10"
    mp = base_map()
    write_common(d)
    add_object(d, mp, "Account", fields=["Phone"], total=5000, fills=[4000], dupe_groups=[12, 9, 8, 7, 6])
    write(d / "map.json", mp)
    write(
        d / "config.json",
        cfg(
            [
                {
                    "api": "Account",
                    "label": "Accounts",
                    "filter": "",
                    "fields": ["Phone"],
                    "activityDays": 180,
                    "dupeField": "Name",
                }
            ]
        ),
    )

    # Extra: limits miss (bar must not show 0% green)
    d = root / "limits-miss"
    mp = base_map()
    write_common(d, limits=LIMITS_FAIL)
    write(d / "map.json", mp)
    write(d / "config.json", cfg([{"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"}]))

    # Extra: Organization row unreadable → sandbox unknown (not PROD)
    d = root / "org-unknown"
    mp = base_map()
    write_common(d, org_row=ORG_ROW_EMPTY)
    add_object(d, mp, "Lead", fields=["Email"], fills=[10], total=10)
    write(d / "query-flows.json", ok({"records": []}))
    write(d / "query-changes.json", ok({"records": []}))
    write(d / "query-logs.json", ok({"records": []}))
    mp["query.FlowInterview.other"] = "query-flows.json"
    mp["query.SetupAuditTrail.other"] = "query-changes.json"
    mp["query.ApexLog.other"] = "query-logs.json"
    write(d / "map.json", mp)
    write(d / "config.json", cfg([{"api": "Lead", "label": "Leads", "filter": "", "fields": ["Email"], "activityDays": 90, "dupeField": "Email"}]))

    # Extra: SOQL injection in filter is rejected before sf
    d = root / "soql-inject"
    write_common(d)
    write(d / "map.json", base_map())
    write(
        d / "config.json",
        [
            {
                "api": "Lead",
                "label": "Leads",
                "filter": "Email != null; DROP TABLE Lead",
                "fields": ["Email"],
                "activityDays": 90,
                "dupeField": "Email",
            }
        ],
    )


def main() -> None:
    dest = Path(sys.argv[1] if len(sys.argv) > 1 else "tests/hygiene/cases")
    dest.mkdir(parents=True, exist_ok=True)
    emit(dest)
    print(f"wrote fixtures under {dest}")


if __name__ == "__main__":
    main()
