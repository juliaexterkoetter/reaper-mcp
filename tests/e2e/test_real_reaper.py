"""Opt-in real host test. No substitute bridge and no mutations."""

import os

import pytest

from reaper_mcp.bridge.client import BridgeClient

pytestmark = [
    pytest.mark.e2e,
    pytest.mark.skipif(os.environ.get("REAPER_MCP_E2E") != "1", reason="Requires real REAPER"),
]


async def test_real_host_readback():
    client = BridgeClient()
    info = await client.call("bridge.info")
    assert info["protocol_version"] == 1
    project = await client.call("project.get")
    assert project["project_id"]
    tracks = await client.call("tracks.list", {"project_id": project["project_id"]})
    assert isinstance(tracks, list)
