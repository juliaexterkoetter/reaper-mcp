import json

import pytest

from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.install import codex
from reaper_mcp.install.manager import InstallRecord


def record():
    return InstallRecord(reaper_path="C:/REAPER/reaper.exe", resource_dir="C:/User Files/REAPER")


@pytest.fixture
def cli(monkeypatch):
    state = {"entry": None, "calls": []}

    def run(*args):
        state["calls"].append(args)
        if args[0] == "list":
            return json.dumps([state["entry"]] if state["entry"] else [])
        if args[0] == "get":
            return json.dumps(state["entry"])
        if args[0] == "add":
            cmd = list(args[args.index("--") + 1 :])
            state["entry"] = {
                "name": "reaper-mcp",
                "enabled": True,
                "transport": {
                    "type": "stdio",
                    "command": cmd[0],
                    "args": cmd[1:],
                    "env": {"REAPER_MCP_RESOURCE": args[3].split("=", 1)[1]},
                    "env_vars": [],
                    "cwd": None,
                },
            }
        if args[0] == "remove":
            state["entry"] = None
        return ""

    monkeypatch.setattr(codex, "run_cli", run)
    monkeypatch.setattr(codex, "save_record", lambda *args: None)
    return state


def test_register_idempotent_and_remove(cli):
    owned = record()
    codex.register(owned)
    assert owned.codex_registration == cli["entry"]
    codex.register(owned)
    assert sum(c[0] == "add" for c in cli["calls"]) == 1
    codex.unregister(owned)
    assert cli["entry"] is None


def test_foreign_entry_preserved(cli):
    cli["entry"] = {"name": "reaper-mcp", "transport": {"command": "user-command"}}
    with pytest.raises(BridgeError, match="CODEX_CONFLICT"):
        codex.register(record())
    assert not any(c[0] == "add" for c in cli["calls"])


def test_modified_entry_preserved(cli):
    owned = record()
    codex.register(owned)
    cli["entry"] = {**cli["entry"], "enabled": False}
    with pytest.raises(BridgeError, match="CODEX_CONFLICT"):
        codex.unregister(owned)
    assert cli["entry"] is not None


def test_failed_manifest_removes_new_entry(cli, monkeypatch):
    def fail(*args):
        raise OSError("disk full")

    monkeypatch.setattr(codex, "save_record", fail)
    with pytest.raises(OSError, match="disk full"):
        codex.register(record())
    assert cli["entry"] is None


def test_missing_cli(monkeypatch):
    monkeypatch.setattr(codex.shutil, "which", lambda _: None)
    with pytest.raises(BridgeError, match="CODEX_NOT_FOUND"):
        codex.current()


def test_cmd_expansion_rejected(monkeypatch):
    monkeypatch.setattr(codex.shutil, "which", lambda _: "C:/codex.cmd")
    with pytest.raises(BridgeError, match="UNSAFE_CODEX_ARGUMENT"):
        codex.run_cli("add", "test", "--", "C:/%USERNAME%/tool.exe")
