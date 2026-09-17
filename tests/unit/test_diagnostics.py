from reaper_mcp import diagnostics
from reaper_mcp.bridge.errors import BridgeError


async def test_doctor_does_not_claim_readiness_without_reaper(monkeypatch):
    monkeypatch.setattr(diagnostics, "load_record", lambda: None)

    async def unavailable(self, method):
        raise BridgeError("REAPER_NOT_RUNNING", "Open REAPER.")

    monkeypatch.setattr(diagnostics.BridgeClient, "call", unavailable)
    result = await diagnostics.diagnose()
    assert not result["ready"]
    assert all(not check["ok"] for check in result["checks"])
    assert "REAPER_NOT_RUNNING" in result["checks"][-1]["detail"]


def test_log_omits_parameters_and_rotates(tmp_path, monkeypatch):
    from reaper_mcp import logging_setup

    monkeypatch.setattr(logging_setup, "data_dir", lambda: tmp_path)
    (tmp_path / "install.json").write_text("{}")
    log = tmp_path / "events.jsonl"
    log.write_text("x" * 1_000_001)
    logging_setup.event("cli", "CODEX_NOT_FOUND")
    assert "CODEX_NOT_FOUND" in log.read_text()
    assert (tmp_path / "events.previous.jsonl").stat().st_size == 1_000_001


async def test_version_reports_host_when_available(monkeypatch):
    async def info(self, method):
        return {"extension_version": "0.1.0a1", "reaper_version": "7.80/x64"}

    monkeypatch.setattr(diagnostics.BridgeClient, "call", info)
    report = await diagnostics.version_report()
    assert report["extension"] == "0.1.0a1"
    assert report["reaper"] == "7.80/x64"
    assert report["bridge_status"] == "connected"


async def test_version_preserves_server_info_without_host(monkeypatch):
    async def unavailable(self, method):
        raise BridgeError("REAPER_NOT_RUNNING", "Open REAPER.")

    monkeypatch.setattr(diagnostics.BridgeClient, "call", unavailable)
    report = await diagnostics.version_report()
    assert report["server"]
    assert report["protocol"] == 1
    assert report["extension"] is None
    assert report["bridge_status"] == "REAPER_NOT_RUNNING"
