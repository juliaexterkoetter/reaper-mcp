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
