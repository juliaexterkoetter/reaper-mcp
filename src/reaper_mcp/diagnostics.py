"""Read-only diagnostics; never report readiness from process health alone."""

import sys
from pathlib import Path
from typing import Any

from reaper_mcp import PROTOCOL_VERSION, __version__
from reaper_mcp.bridge.client import BridgeClient
from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.install.codex import current
from reaper_mcp.install.manager import load_record, verify_owned


async def diagnose() -> dict[str, Any]:
    checks: list[dict[str, Any]] = []
    record = None
    try:
        record = load_record()
        if record is None:
            raise BridgeError("NOT_INSTALLED", "Run reaper-mcp install.")
        verify_owned(record)
        missing = [path for path in record.files if not Path(path).is_file()]
        if missing:
            raise BridgeError("MISSING_FILE", "Close REAPER and run reaper-mcp install to repair.")
        checks.append({"component": "installation", "ok": True})
    except (BridgeError, OSError) as exc:
        checks.append({"component": "installation", "ok": False, "detail": str(exc)})
    if record and record.codex_registration is not None:
        try:
            if current() != record.codex_registration:
                raise BridgeError("CODEX_CONFLICT", "Inspect codex mcp get reaper-mcp --json.")
            checks.append({"component": "codex", "ok": True})
        except BridgeError as exc:
            checks.append({"component": "codex", "ok": False, "detail": str(exc)})
    else:
        checks.append(
            {
                "component": "codex",
                "ok": False,
                "detail": "Not registered by this installer; run install without --skip-codex.",
            }
        )
    try:
        info = await BridgeClient().call("bridge.info")
        checks.append({"component": "native-bridge", "ok": True, "info": info})
    except (BridgeError, OSError) as exc:
        checks.append({"component": "native-bridge", "ok": False, "detail": str(exc)})
    return {
        "ready": all(c["ok"] for c in checks),
        "server_version": __version__,
        "protocol_version": PROTOCOL_VERSION,
        "checks": checks,
    }


async def version_report() -> dict[str, Any]:
    report: dict[str, Any] = {
        "server": __version__,
        "protocol": PROTOCOL_VERSION,
        "runtime": sys.version.split()[0],
        "extension": None,
        "reaper": None,
    }
    try:
        info = await BridgeClient(timeout=0.5).call("bridge.info")
        report["extension"] = info.get("extension_version")
        report["reaper"] = info.get("reaper_version")
        report["bridge_status"] = "connected"
    except (BridgeError, OSError) as exc:
        report["bridge_status"] = (
            exc.code if isinstance(exc, BridgeError) else "FILE_OPERATION_FAILED"
        )
    return report
