"""Register using the official Codex CLI; never rewrite the user's TOML."""

import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.install.manager import InstallRecord, save_record

NAME = "reaper-mcp"


def run_cli(*args: str) -> str:
    executable = shutil.which("codex")
    if not executable:
        raise BridgeError("CODEX_NOT_FOUND", "Install Codex CLI, or use install --skip-codex.")
    # Windows may execute npm's .cmd shim through cmd.exe even with shell=False.
    # Reject expansion/control characters rather than interpolating untrusted paths.
    if Path(executable).suffix.lower() in {".cmd", ".bat"} and any(
        any(c in arg for c in '&|<>^%!"\r\n') for arg in args
    ):
        raise BridgeError("UNSAFE_CODEX_ARGUMENT", "Use the native codex.exe for this path.")
    try:
        result = subprocess.run(
            [executable, "mcp", *args], capture_output=True, text=True, timeout=30, check=False
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise BridgeError(
            "CODEX_CLI_FAILED", "Codex CLI failed or timed out; inspect codex mcp list."
        ) from exc
    if result.returncode:
        # Do not expose CLI output: other registrations can contain credentials.
        raise BridgeError("CODEX_CLI_FAILED", "Codex CLI returned an error; run codex mcp list.")
    return result.stdout


def current() -> dict[str, Any] | None:
    try:
        entries = json.loads(run_cli("list", "--json"))
        if not isinstance(entries, list):
            raise ValueError("Expected a list")
        if not any(isinstance(e, dict) and e.get("name") == NAME for e in entries):
            return None
        value = json.loads(run_cli("get", NAME, "--json"))
        if not isinstance(value, dict) or value.get("name") != NAME:
            raise ValueError("Unexpected entry")
        return value
    except (ValueError, TypeError) as exc:
        raise BridgeError("CODEX_INVALID_RESPONSE", "Unsupported Codex CLI JSON response.") from exc


def launch_command() -> list[str]:
    if getattr(sys, "frozen", False):
        return [sys.executable, "serve"]
    return [sys.executable, "-m", "reaper_mcp.cli", "serve"]


def register(record: InstallRecord, home: Path | None = None) -> None:
    existing = current()
    if existing is not None and existing != record.codex_registration:
        raise BridgeError("CODEX_CONFLICT", "Preserving an existing reaper-mcp registration.")
    command = launch_command()
    transport = {
        "type": "stdio",
        "command": command[0],
        "args": command[1:],
        "env": {"REAPER_MCP_RESOURCE": record.resource_dir},
        "env_vars": [],
        "cwd": None,
    }
    if existing is not None and existing.get("transport") == transport:
        return
    run_cli("add", NAME, "--env", f"REAPER_MCP_RESOURCE={record.resource_dir}", "--", *command)
    observed = current()
    if observed is None or observed.get("transport") != transport:
        raise BridgeError(
            "CODEX_REGISTRATION_FAILED",
            "Registration did not match; inspect codex mcp get reaper-mcp.",
        )
    previous = record.codex_registration
    record.codex_registration = observed
    try:
        save_record(record, home)
    except OSError:
        # Do not leave a new entry claiming ownership that could not be recorded.
        if previous is None and current() == observed:
            run_cli("remove", NAME)
        record.codex_registration = previous
        raise


def unregister(record: InstallRecord) -> None:
    if record.codex_registration is None:
        return
    existing = current()
    if existing is None:
        return
    if existing != record.codex_registration:
        raise BridgeError(
            "CODEX_CONFLICT",
            "Registration was changed; preserve it or remove it manually before uninstalling.",
        )
    run_cli("remove", NAME)
