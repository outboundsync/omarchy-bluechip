#!/usr/bin/env python3
"""Write stub-sf fixture trees for Wave 1 (named-creds, trace, logs, MCP)."""
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

NC_ROW = {
    "Id": "0XA000000000001AAA",
    "DeveloperName": "ZoomInfo_NC",
    "MasterLabel": "ZoomInfo",
    "Endpoint": "https://api.zoominfo.com/v1?token=SHOULD_STRIP",
    "PrincipalType": "NamedPrincipal",
    "Protocol": "Password",
    "NamedCredentialType": "Legacy",
    "GenerateAuthorizationHeader": True,
    "AllowMergeFieldsInHeader": True,
    "AllowMergeFieldsInBody": False,
    "LastModifiedDate": "2026-09-01T00:00:00.000+0000",
    "Password": "SUPER_SECRET_PASSWORD",
    "consumerSecret": "NEVER_PRINT_ME",
}

EC_ROW = {
    "Id": "0xt000000000001AAA",
    "DeveloperName": "OutboundSync_EC",
    "MasterLabel": "OutboundSync Router",
    "AuthenticationProtocol": "OAuth",
    "LastModifiedDate": "2026-09-10T00:00:00.000+0000",
}

PRINCIPAL = {
    "Id": "0xp000000000001AAA",
    "ExternalCredentialId": "0xt000000000001AAA",
    "PrincipalName": "Integration",
    "PrincipalType": "NamedPrincipal",
    "SequenceNumber": 1,
}

TRACE = {
    "Id": "7tf000000000001AAA",
    "LogType": "USER_DEBUG",
    "StartDate": "2026-09-15T12:00:00.000+0000",
    "ExpirationDate": "2026-09-15T13:00:00.000+0000",
    "TracedEntityId": "005000000000001AAA",
    "DebugLevelId": "7dl000000000001AAA",
}

DEBUG_LEVEL = {
    "Id": "7dl000000000001AAA",
    "DeveloperName": "SFDC_DevConsole",
    "MasterLabel": "SFDC_DevConsole",
}

APEX_LOG = {
    "Id": "07L000000000001AAA",
    "Operation": "ApexDebug",
    "Status": "Success",
    "StartTime": "2026-09-15T12:05:00.000+0000",
    "LogLength": 120,
    "LogUserId": "005000000000001AAA",
    "Application": "Unknown",
}

USER = {"Id": "005000000000001AAA"}


def write_common(d: Path, *, alias: str, sandbox: bool, org_id: str) -> dict:
    write(d / "org-display.json", display(alias=alias, sandbox=sandbox, org_id=org_id))
    write(d / "org-row.json", org_row(org_id=org_id, sandbox=sandbox))
    write(d / "limits.json", LIMITS)
    write(d / "org-list.json", ORG_LIST)
    write(d / "user.json", records(USER))
    write(d / "apex-log.json", records(APEX_LOG))
    write(d / "apex-get.json", ok({"log": "12:05:00.0 (005...) USER_DEBUG [DEBUG] hello\nAuthorization: Bearer LEAK\npassword=nope"}))
    write(d / "flows.json", records())
    write(d / "flows-count.json", records({"c": 0}))
    write(d / "changes.json", records())
    return {
        "org.display": "org-display.json",
        "org.list.limits": "limits.json",
        "org.list": "org-list.json",
        "query.Organization.row": "org-row.json",
        "query.User.other": "user.json",
        "query.ApexLog.other": "apex-log.json",
        "query.FlowInterview.other": "flows.json",
        "query.FlowInterview.count": "flows-count.json",
        "query.SetupAuditTrail.other": "changes.json",
        "apex.get.log": "apex-get.json",
    }


def emit(root: Path) -> None:
    # Happy path — sandbox + full Tooling
    d = root / "ok"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(d / "named-credential.json", records(NC_ROW))
    write(d / "external-credential.json", records(EC_ROW))
    write(d / "nc-headers.json", records({"NamedCredentialId": "0XA000000000001AAA", "headerCount": 2}))
    write(d / "ec-principals.json", records(PRINCIPAL))
    write(d / "ec-authh.json", records({"ExternalCredentialId": "0xt000000000001AAA", "c": 1}))
    write(d / "traceflag.json", records(TRACE))
    write(d / "debuglevel.json", records(DEBUG_LEVEL))
    write(d / "create-tf.json", ok({"id": "7tf000000000009AAA", "success": True}))
    write(d / "create-dl.json", ok({"id": "7dl000000000009AAA", "success": True}))
    write(d / "delete-tf.json", ok({"id": "7tf000000000001AAA", "success": True}))
    mp.update(
        {
            "tooling.query.NamedCredential": "named-credential.json",
            "tooling.query.NamedCredential.list": "named-credential.json",
            "tooling.query.NamedCredentialParameter.group": "nc-headers.json",
            "tooling.query.ExternalCredential": "external-credential.json",
            "tooling.query.ExternalCredential.list": "external-credential.json",
            "tooling.query.ExternalCredentialPrincipal": "ec-principals.json",
            "tooling.query.ExternalCredentialPrincipal.list": "ec-principals.json",
            "tooling.query.ExternalCredentialParameter.group": "ec-authh.json",
            "tooling.query.TraceFlag": "traceflag.json",
            "tooling.query.TraceFlag.list": "traceflag.json",
            "tooling.query.DebugLevel": "debuglevel.json",
            "tooling.query.DebugLevel.list": "debuglevel.json",
            "tooling.create.TraceFlag": "create-tf.json",
            "tooling.create.DebugLevel": "create-dl.json",
            "tooling.delete.TraceFlag": "delete-tf.json",
        }
    )
    write(d / "map.json", mp)

    # Prod twin (same tooling; IsSandbox false)
    d = root / "prod"
    mp = write_common(d, alias="prod-org", sandbox=False, org_id="00D000000000001AAA")
    for name in (
        "named-credential.json",
        "external-credential.json",
        "nc-headers.json",
        "ec-principals.json",
        "ec-authh.json",
        "traceflag.json",
        "debuglevel.json",
        "create-tf.json",
        "create-dl.json",
        "delete-tf.json",
    ):
        src = root / "ok" / name
        (d / name).write_text(src.read_text())
    mp.update(
        {
            "tooling.query.NamedCredential": "named-credential.json",
            "tooling.query.NamedCredential.list": "named-credential.json",
            "tooling.query.NamedCredentialParameter.group": "nc-headers.json",
            "tooling.query.ExternalCredential": "external-credential.json",
            "tooling.query.ExternalCredential.list": "external-credential.json",
            "tooling.query.ExternalCredentialPrincipal": "ec-principals.json",
            "tooling.query.ExternalCredentialPrincipal.list": "ec-principals.json",
            "tooling.query.ExternalCredentialParameter.group": "ec-authh.json",
            "tooling.query.TraceFlag": "traceflag.json",
            "tooling.query.TraceFlag.list": "traceflag.json",
            "tooling.query.DebugLevel": "debuglevel.json",
            "tooling.query.DebugLevel.list": "debuglevel.json",
            "tooling.create.TraceFlag": "create-tf.json",
            "tooling.create.DebugLevel": "create-dl.json",
            "tooling.delete.TraceFlag": "delete-tf.json",
        }
    )
    write(d / "map.json", mp)

    # Tooling miss — no metadata either
    d = root / "tooling-miss"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(d / "denied.json", err("INVALID_TYPE", "sObject type 'NamedCredential' is not supported."))
    mp.update(
        {
            "tooling.query.NamedCredential": "error:INVALID_TYPE:sObject type NamedCredential is not supported.",
            "tooling.query.NamedCredential.list": "error:INVALID_TYPE:sObject type NamedCredential is not supported.",
            "tooling.query.ExternalCredential": "error:INVALID_TYPE:sObject type ExternalCredential is not supported.",
            "tooling.query.ExternalCredential.list": "error:INVALID_TYPE:sObject type ExternalCredential is not supported.",
            "tooling.query.TraceFlag": "error:INVALID_TYPE:sObject type TraceFlag is not supported.",
            "tooling.query.TraceFlag.list": "error:INVALID_TYPE:sObject type TraceFlag is not supported.",
        }
    )
    write(d / "map.json", mp)

    # Metadata list fallback
    d = root / "metadata-only"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(
        d / "md-nc.json",
        ok(
            [
                {
                    "fullName": "ZoomInfo_NC",
                    "id": "0XA000000000001AAA",
                    "lastModifiedDate": "2026-09-01T00:00:00.000+0000",
                    "type": "NamedCredential",
                }
            ]
        ),
    )
    write(
        d / "md-ec.json",
        ok(
            [
                {
                    "fullName": "OutboundSync_EC",
                    "id": "0xt000000000001AAA",
                    "lastModifiedDate": "2026-09-10T00:00:00.000+0000",
                    "type": "ExternalCredential",
                }
            ]
        ),
    )
    mp.update(
        {
            "tooling.query.NamedCredential": "error:INVALID_TYPE:no tooling",
            "tooling.query.NamedCredential.list": "error:INVALID_TYPE:no tooling",
            "tooling.query.ExternalCredential": "error:INVALID_TYPE:no tooling",
            "tooling.query.ExternalCredential.list": "error:INVALID_TYPE:no tooling",
            "org.list.metadata.NamedCredential": "md-nc.json",
            "org.list.metadata.ExternalCredential": "md-ec.json",
        }
    )
    write(d / "map.json", mp)


def main() -> None:
    dest = Path(sys.argv[1] if len(sys.argv) > 1 else "tests/wave1/fixtures")
    dest.mkdir(parents=True, exist_ok=True)
    emit(dest)
    print(f"wrote fixtures under {dest}")


if __name__ == "__main__":
    main()
