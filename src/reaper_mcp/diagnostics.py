"""Read-only diagnostics; never report readiness from process health alone."""

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
