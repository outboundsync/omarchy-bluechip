#!/usr/bin/env python3
"""Stub-sf fixtures for Wave 2 (FLS, offenders, clipboard)."""
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


def records(*rows: dict) -> dict:
    return ok({"records": list(rows), "totalSize": len(rows), "done": True})


def display(*, alias: str, sandbox: bool, org_id: str, user_id: str = "005000000000001AAA") -> dict:
    return ok(
        {
            "id": org_id,
            "alias": alias,
            "username": "admin@example.com",
            "userId": user_id,
            "instanceUrl": "https://example.my.salesforce.com",
            "accessToken": "SHOULD_NEVER_LEAK",
            "clientSecret": "CONSUMER_SECRET_LEAK",
        }
    )


def org_row(*, org_id: str, sandbox: bool) -> dict:
    return records(
        {
            "Id": org_id,
            "Name": "Acme",
            "IsSandbox": sandbox,
            "OrganizationType": "Enterprise Edition",
            "InstanceName": "NA99",
        }
    )


LIMITS = ok([{"name": "DailyApiRequests", "max": 10000, "remaining": 8000}])
ORG_LIST = ok(
    {
        "nonScratchOrgs": [
            {
                "alias": "sbx",
                "username": "admin@example.com",
                "connectedStatus": "Connected",
                "isDefaultUsername": True,
            },
            {
                "alias": "prod-org",
                "username": "admin@acme.com",
                "connectedStatus": "Connected",
                "isDefaultUsername": False,
            },
        ],
        "sandboxes": [],
        "scratchOrgs": [],
        "devHubs": [],
    }
)

INTEGRATION_USER = {
    "Id": "005000000000009AAA",
    "Username": "integration@example.com",
    "Name": "Integration User",
    "ProfileId": "00e000000000001AAA",
    "Profile": {"Name": "Integration"},
    "IsActive": True,
}

PSA = {
    "PermissionSetId": "0PS000000000001AAA",
    "PermissionSet": {
        "Name": "Integration",
        "Label": "Integration",
        "IsOwnedByProfile": False,
    },
}

PROFILE_PS = {
    "Id": "0PS0000000000PRFAA",
    "Name": "Integration Profile",
    "Label": "Integration Profile",
    "IsOwnedByProfile": True,
    "ProfileId": "00e000000000001AAA",
}

OBJ_PERMS = [
    {
        "ParentId": "0PS000000000001AAA",
        "SobjectType": "Account",
        "PermissionsRead": True,
        "PermissionsEdit": True,
        "PermissionsCreate": False,
        "PermissionsDelete": False,
    },
    {
        "ParentId": "0PS000000000001AAA",
        "SobjectType": "Contact",
        "PermissionsRead": False,
        "PermissionsEdit": False,
        "PermissionsCreate": False,
        "PermissionsDelete": False,
    },
    {
        "ParentId": "0PS000000000001AAA",
        "SobjectType": "Lead",
        "PermissionsRead": True,
        "PermissionsEdit": True,
        "PermissionsCreate": False,
        "PermissionsDelete": False,
    },
]

FIELD_PERMS = [
    {
        "ParentId": "0PS000000000001AAA",
        "SobjectType": "Account",
        "Field": "Account.SavvyCal_Id__c",
        "PermissionsRead": True,
        "PermissionsEdit": False,
    },
    {
        "ParentId": "0PS000000000001AAA",
        "SobjectType": "Contact",
        "Field": "Contact.Email",
        "PermissionsRead": False,
        "PermissionsEdit": False,
    },
]

EVENT_FILES = [
    {
        "Id": "0AT000000000001AAA",
        "EventType": "RestApi",
        "LogDate": "2026-09-15T00:00:00.000+0000",
        "LogFileLength": 200,
        "Sequence": 1,
        "Interval": "Hourly",
    }
]

# Two RestApi rows for user 005...009, one for 005...001; client + connected app present.
EVENT_CSV = (
    "EVENT_TYPE,TIMESTAMP,USER_ID,CLIENT_ID,CONNECTED_APP_ID\n"
    "RestApi,20260915100000.000,005000000000009AAA,ClientA,0H4000000000001AAA\n"
    "RestApi,20260915100100.000,005000000000009AAA,ClientA,0H4000000000001AAA\n"
    "RestApi,20260915100200.000,005000000000001AAA,ClientB,\n"
    "Authorization: Bearer SHOULD_NEVER_LEAK\n"
)

FLOW_DEF = {
    "Id": "300000000000001AAA",
    "DeveloperName": "SavvyCal_Webhook",
    "MasterLabel": "SavvyCal Webhook",
    "ActiveVersionId": "301000000000001AAA",
}

INTERVIEW = {
    "Id": "4I9000000000001AAA",
    "InterviewLabel": "SavvyCal_Webhook",
    "InterviewStatus": "Error",
    "CurrentElement": "Callout_1",
    "PauseLabel": None,
    "CreatedDate": "2026-09-15T12:00:00.000+0000",
}


def write_common(d: Path, *, alias: str, sandbox: bool, org_id: str) -> dict:
    write(d / "org-display.json", display(alias=alias, sandbox=sandbox, org_id=org_id))
    write(d / "org-row.json", org_row(org_id=org_id, sandbox=sandbox))
    write(d / "limits.json", LIMITS)
    write(d / "org-list.json", ORG_LIST)
    write(d / "flows.json", records(INTERVIEW))
    write(d / "flows-count.json", records({"c": 1}))
    write(d / "changes.json", records())
    write(d / "apex-log.json", records())
    return {
        "org.display": "org-display.json",
        "org.list.limits": "limits.json",
        "org.list": "org-list.json",
        "query.Organization.row": "org-row.json",
        "query.FlowInterview.other": "flows.json",
        "query.FlowInterview.count": "flows-count.json",
        "query.SetupAuditTrail.other": "changes.json",
        "query.ApexLog.other": "apex-log.json",
    }


def emit(root: Path) -> None:
    d = root / "ok"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(d / "user.json", records(INTEGRATION_USER))
    write(d / "psa.json", records(PSA))
    write(d / "profile-ps.json", records(PROFILE_PS))
    write(d / "objperm.json", records(*OBJ_PERMS))
    write(d / "fieldperm.json", records(*FIELD_PERMS))
    write(d / "eventlog.json", records(*EVENT_FILES))
    write(d / "logfile.json", ok(EVENT_CSV))
    write(d / "flowdef.json", records(FLOW_DEF))
    mp.update(
        {
            "query.User.other": "user.json",
            "query.PermissionSetAssignment.other": "psa.json",
            "query.PermissionSet.other": "profile-ps.json",
            "query.ObjectPermissions.other": "objperm.json",
            "query.FieldPermissions.other": "fieldperm.json",
            "query.EventLogFile.other": "eventlog.json",
            "api.request.EventLogFile": "logfile.json",
            "tooling.query.FlowDefinition": "flowdef.json",
            "tooling.query.FlowDefinition.list": "flowdef.json",
        }
    )
    write(d / "map.json", mp)

    d = root / "fls-miss"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(d / "user.json", records(INTEGRATION_USER))
    write(d / "psa.json", records(PSA))
    write(d / "profile-ps.json", records(PROFILE_PS))
    mp.update(
        {
            "query.User.other": "user.json",
            "query.PermissionSetAssignment.other": "psa.json",
            "query.PermissionSet.other": "profile-ps.json",
            "query.ObjectPermissions.other": "error:INVALID_TYPE:sObject type ObjectPermissions is not supported.",
            "query.FieldPermissions.other": "error:INVALID_TYPE:sObject type FieldPermissions is not supported.",
            "tooling.query.ObjectPermissions": "error:INVALID_TYPE:sObject type ObjectPermissions is not supported.",
            "tooling.query.FieldPermissions": "error:INVALID_TYPE:sObject type FieldPermissions is not supported.",
        }
    )
    write(d / "map.json", mp)

    d = root / "offenders-miss"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    mp.update(
        {
            "query.EventLogFile.other": "error:INVALID_TYPE:sObject type EventLogFile is not supported.",
        }
    )
    write(d / "map.json", mp)

    d = root / "api-miss"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(d / "eventlog.json", records(*EVENT_FILES))
    mp.update(
        {
            "query.EventLogFile.other": "eventlog.json",
            "api.request.EventLogFile": "error:UNSUPPORTED:sf api request rest is not a sf command",
            "api.request": "error:UNSUPPORTED:sf api request rest is not a sf command",
        }
    )
    write(d / "map.json", mp)


def main() -> None:
    dest = Path(sys.argv[1] if len(sys.argv) > 1 else "tests/wave2/fixtures")
    dest.mkdir(parents=True, exist_ok=True)
    emit(dest)
    print(f"wrote fixtures under {dest}")


if __name__ == "__main__":
    main()
