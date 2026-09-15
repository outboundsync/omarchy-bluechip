#!/usr/bin/env python3
"""Stub-sf fixtures for the Flow-build wave (callout-auth, types, pack, preflight)."""
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
            }
        ],
        "sandboxes": [],
        "scratchOrgs": [],
        "devHubs": [],
    }
)

ABSENT = {
    "Id": "0XA0000000000ABAAA",
    "DeveloperName": "HeadersAbsent_NC",
    "MasterLabel": "Headers Absent",
    "Endpoint": "https://api.example.com/v1",
    "PrincipalType": "NamedPrincipal",
    "Protocol": "Password",
    "NamedCredentialType": "Legacy",
    "GenerateAuthorizationHeader": False,
    "AllowMergeFieldsInHeader": False,
    "AllowMergeFieldsInBody": False,
    "LastModifiedDate": "2026-09-15T00:00:00.000+0000",
    "Password": "SUPER_SECRET_PASSWORD",
}

FORMULAS = {
    "Id": "0XA0000000000FOAAA",
    "DeveloperName": "FormulasOff_NC",
    "MasterLabel": "Formulas Off",
    "Endpoint": "https://api.example.com/v1",
    "PrincipalType": "NamedPrincipal",
    "Protocol": "Password",
    "NamedCredentialType": "Legacy",
    "GenerateAuthorizationHeader": False,
    "AllowMergeFieldsInHeader": False,
    "AllowMergeFieldsInBody": False,
    "LastModifiedDate": "2026-09-15T00:00:00.000+0000",
}

CUSTOM_OK = {
    "Id": "0XA0000000000OKAAA",
    "DeveloperName": "CustomHeader_OK",
    "MasterLabel": "Custom Header OK",
    "Endpoint": "https://api.example.com/v1",
    "PrincipalType": "NamedPrincipal",
    "Protocol": "Password",
    "NamedCredentialType": "Legacy",
    "GenerateAuthorizationHeader": False,
    "AllowMergeFieldsInHeader": True,
    "AllowMergeFieldsInBody": False,
    "LastModifiedDate": "2026-09-15T00:00:00.000+0000",
}

GEN_CUSTOM = {
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

APEX_IN = {
    "Id": "01p000000000001AAA",
    "Name": "IN_LeadSearch",
    "NamespacePrefix": None,
    "Status": "Active",
    "ApiVersion": 62.0,
    "Body": (
        "public class IN_LeadSearch {\n"
        "  public String company;\n"
        "  public String domain;\n"
        "  public class Address { public String city; }\n"
        "  public Address address;\n"
        "}\n"
    ),
    "SymbolTable": {
        "name": "IN_LeadSearch",
        "properties": [
            {"name": "company", "type": "String", "visibility": "PUBLIC"},
            {"name": "domain", "type": "String", "visibility": "PUBLIC"},
            {"name": "address", "type": {"name": "Address"}, "visibility": "PUBLIC"},
        ],
        "innerClasses": [
            {
                "name": "Address",
                "properties": [{"name": "city", "type": "String", "visibility": "PUBLIC"}],
            }
        ],
    },
}

APEX_OUT = {
    "Id": "01p000000000002AAA",
    "Name": "OUT_2XX",
    "NamespacePrefix": None,
    "Status": "Active",
    "ApiVersion": 62.0,
    "Body": "public class OUT_2XX { public Integer id; public String status; }",
    "SymbolTable": {
        "name": "OUT_2XX",
        "properties": [
            {"name": "id", "type": "Integer", "visibility": "PUBLIC"},
            {"name": "status", "type": "String", "visibility": "PUBLIC"},
        ],
        "innerClasses": [],
    },
}

FLOW_DEF = {
    "Id": "300000000000002AAA",
    "DeveloperName": "ZoomInfo_Callout",
    "MasterLabel": "ZoomInfo Callout",
    "ActiveVersionId": "301000000000002AAA",
}

AUTOPROC = {
    "Id": "0050000000000APAAA",
    "Username": "autoproc@00d000000000002",
    "Name": "Automated Process",
    "ProfileId": "00e0000000000APAAA",
    "Profile": {"Name": "Automated Process User"},
    "IsActive": True,
    "UserType": "AutomatedProcess",
}

INTEGRATION = {
    "Id": "005000000000009AAA",
    "Username": "integration@example.com",
    "Name": "Integration User",
    "ProfileId": "00e000000000001AAA",
    "Profile": {"Name": "Integration"},
    "IsActive": True,
}

FIELD_PERMS = [
    {
        "ParentId": "0PS000000000001AAA",
        "SobjectType": "Account",
        "Field": "Account.SavvyCal_Id__c",
        "PermissionsRead": True,
        "PermissionsEdit": False,
    }
]

OBJ_PERMS = [
    {
        "ParentId": "0PS000000000001AAA",
        "SobjectType": "Account",
        "PermissionsRead": True,
        "PermissionsEdit": True,
        "PermissionsCreate": False,
        "PermissionsDelete": False,
    }
]


def write_common(d: Path, *, alias: str, sandbox: bool, org_id: str) -> dict:
    write(d / "org-display.json", display(alias=alias, sandbox=sandbox, org_id=org_id))
    write(d / "org-row.json", org_row(org_id=org_id, sandbox=sandbox))
    write(d / "limits.json", LIMITS)
    write(d / "org-list.json", ORG_LIST)
    write(d / "flows.json", records())
    write(d / "flows-count.json", records({"c": 0}))
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


def nc_map(mp: dict) -> dict:
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
        }
    )
    return mp


def emit(root: Path) -> None:
    # Four 401-class / healthy NCs
    d = root / "doctor"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(d / "named-credential.json", records(ABSENT, FORMULAS, CUSTOM_OK, GEN_CUSTOM))
    write(
        d / "nc-headers.json",
        records(
            {"NamedCredentialId": "0XA0000000000ABAAA", "headerCount": 0},
            {"NamedCredentialId": "0XA0000000000FOAAA", "headerCount": 1},
            {"NamedCredentialId": "0XA0000000000OKAAA", "headerCount": 1},
            {"NamedCredentialId": "0XA000000000001AAA", "headerCount": 2},
        ),
    )
    write(d / "external-credential.json", records(EC_ROW))
    write(d / "ec-principals.json", records(PRINCIPAL))
    write(d / "ec-authh.json", records({"ExternalCredentialId": "0xt000000000001AAA", "c": 1}))
    write(d / "user.json", records(AUTOPROC))
    write(d / "ecpa.json", records())
    write(d / "psa.json", records())
    write(d / "apex-class.json", records(APEX_IN, APEX_OUT))
    write(d / "flowdef.json", records(FLOW_DEF))
    write(d / "esr.json", records())
    mp.update(
        {
            "query.User.other": "user.json",
            "query.ExternalCredentialPrincipalAccess.other": "ecpa.json",
            "tooling.query.ExternalCredentialPrincipalAccess": "ecpa.json",
            "tooling.query.ExternalCredentialPrincipalAccess.list": "ecpa.json",
            "query.PermissionSetAssignment.other": "psa.json",
            "tooling.query.ApexClass": "apex-class.json",
            "tooling.query.ApexClass.list": "apex-class.json",
            "tooling.query.FlowDefinition": "flowdef.json",
            "tooling.query.FlowDefinition.list": "flowdef.json",
            "tooling.query.ExternalServiceRegistration": "esr.json",
            "tooling.query.ExternalServiceRegistration.list": "esr.json",
        }
    )
    write(d / "map.json", nc_map(mp))

    # Tooling miss
    d = root / "tooling-miss"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    mp.update(
        {
            "tooling.query.NamedCredential": "error:INVALID_TYPE:sObject type NamedCredential is not supported.",
            "tooling.query.NamedCredential.list": "error:INVALID_TYPE:sObject type NamedCredential is not supported.",
            "tooling.query.ExternalCredential": "error:INVALID_TYPE:sObject type ExternalCredential is not supported.",
            "tooling.query.ExternalCredential.list": "error:INVALID_TYPE:sObject type ExternalCredential is not supported.",
            "tooling.query.ApexClass": "error:INVALID_TYPE:sObject type ApexClass is not supported.",
            "tooling.query.ApexClass.list": "error:INVALID_TYPE:sObject type ApexClass is not supported.",
            "tooling.query.FlowDefinition": "error:INVALID_TYPE:no flow",
            "tooling.query.FlowDefinition.list": "error:INVALID_TYPE:no flow",
        }
    )
    write(d / "map.json", mp)

    # Types only (named class)
    d = root / "types"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(d / "apex-class.json", records(APEX_IN, APEX_OUT))
    write(d / "named-credential.json", records(GEN_CUSTOM))
    write(d / "nc-headers.json", records({"NamedCredentialId": "0XA000000000001AAA", "headerCount": 2}))
    write(d / "external-credential.json", records())
    write(d / "ec-principals.json", records())
    write(d / "ec-authh.json", records())
    write(d / "flowdef.json", records(FLOW_DEF))
    write(d / "esr.json", records())
    mp.update(
        {
            "tooling.query.ApexClass": "apex-class.json",
            "tooling.query.ApexClass.list": "apex-class.json",
            "tooling.query.FlowDefinition": "flowdef.json",
            "tooling.query.FlowDefinition.list": "flowdef.json",
            "tooling.query.ExternalServiceRegistration": "esr.json",
            "tooling.query.ExternalServiceRegistration.list": "esr.json",
        }
    )
    write(d / "map.json", nc_map(mp))

    # Preflight: integration user + missing Edit + principal granted
    d = root / "preflight"
    mp = write_common(d, alias="sbx", sandbox=True, org_id="00D000000000002AAA")
    write(d / "named-credential.json", records(ABSENT))
    write(d / "nc-headers.json", records({"NamedCredentialId": "0XA0000000000ABAAA", "headerCount": 0}))
    write(d / "external-credential.json", records(EC_ROW))
    write(d / "ec-principals.json", records(PRINCIPAL))
    write(d / "ec-authh.json", records({"ExternalCredentialId": "0xt000000000001AAA", "c": 1}))
    write(d / "user.json", records(INTEGRATION))
    write(d / "psa.json", records({"PermissionSetId": "0PS000000000001AAA"}))
    write(
        d / "profile-ps.json",
        records(
            {
                "Id": "0PS0000000000PRFAA",
                "Name": "Integration Profile",
                "Label": "Integration Profile",
                "IsOwnedByProfile": True,
                "ProfileId": "00e000000000001AAA",
            }
        ),
    )
    write(d / "objperm.json", records(*OBJ_PERMS))
    write(d / "fieldperm.json", records(*FIELD_PERMS))
    write(
        d / "ecpa.json",
        records(
            {
                "Id": "0ea000000000001AAA",
                "ParentId": "0PS000000000001AAA",
                "ExternalCredentialPrincipalId": "0xp000000000001AAA",
            }
        ),
    )
    write(d / "apex-class.json", records(APEX_IN))
    write(d / "flowdef.json", records(FLOW_DEF))
    write(d / "esr.json", records())
    mp.update(
        {
            "query.User.other": "user.json",
            "query.PermissionSetAssignment.other": "psa.json",
            "query.PermissionSet.other": "profile-ps.json",
            "query.ObjectPermissions.other": "objperm.json",
            "query.FieldPermissions.other": "fieldperm.json",
            "query.ExternalCredentialPrincipalAccess.other": "ecpa.json",
            "tooling.query.ExternalCredentialPrincipalAccess": "ecpa.json",
            "tooling.query.ExternalCredentialPrincipalAccess.list": "ecpa.json",
            "tooling.query.ApexClass": "apex-class.json",
            "tooling.query.ApexClass.list": "apex-class.json",
            "tooling.query.FlowDefinition": "flowdef.json",
            "tooling.query.FlowDefinition.list": "flowdef.json",
            "tooling.query.ExternalServiceRegistration": "esr.json",
            "tooling.query.ExternalServiceRegistration.list": "esr.json",
        }
    )
    write(d / "map.json", nc_map(mp))


def main() -> None:
    dest = Path(sys.argv[1] if len(sys.argv) > 1 else "tests/wave3/fixtures")
    dest.mkdir(parents=True, exist_ok=True)
    emit(dest)
    print(f"wrote fixtures under {dest}")


if __name__ == "__main__":
    main()
