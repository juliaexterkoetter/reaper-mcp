import asyncio
import json

import pytest

from reaper_mcp.bridge.client import BridgeClient
from reaper_mcp.bridge.errors import BridgeError


@pytest.fixture
async def endpoint(tmp_path):
    root = tmp_path / "ReaperMCP"
    root.mkdir()
    token = "a" * 64
    (root / "config.json").write_text(json.dumps({"token": token, "policy": "read-only"}))
    calls = []

    async def handler(reader, writer):
        req = json.loads(await reader.readline())
        calls.append(req)
        assert req["auth"] == token
        result = {"protocol_version": 1, "capabilities": ["tracks.list"]}
        if req["method"] == "tracks.list":
            result = [{"guid": "{VOICE}", "name": "Voz"}]
        writer.write(
            json.dumps({"jsonrpc": "2.0", "id": req["id"], "result": result}).encode() + b"\n"
        )
        await writer.drain()
        writer.close()
        await writer.wait_closed()

    server = await asyncio.start_server(handler, "127.0.0.1", 0)
    port = server.sockets[0].getsockname()[1]
    (root / "bridge.json").write_text(json.dumps({"protocol_version": 1, "port": port, "pid": 1}))
    async with server:
        yield BridgeClient(tmp_path), calls


async def test_real_loopback_handshake(endpoint):
    client, calls = endpoint
    assert (await client.call("tracks.list"))[0]["name"] == "Voz"
    assert [r["method"] for r in calls] == ["bridge.info", "tracks.list"]


async def test_capability_fail_closed(endpoint):
    client, calls = endpoint
    with pytest.raises(BridgeError, match="UNSUPPORTED_CAPABILITY"):
        await client.call("shell.execute")
    assert len(calls) == 1


async def test_missing_reaper(tmp_path):
    with pytest.raises(BridgeError, match="REAPER_NOT_RUNNING"):
        await BridgeClient(tmp_path).call("bridge.info")
