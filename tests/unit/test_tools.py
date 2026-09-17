import pytest
from mcp.server.mcpserver.exceptions import ToolError

from reaper_mcp.server import create_server


class RecordingBridge:
    def __init__(self):
        self.calls = []

    async def call(self, method, params=None):
        self.calls.append((method, params))
        return {"method": method}


async def test_tools_have_typed_schemas():
    tools = {t.name: t for t in await create_server().list_tools()}
    assert tools["reaper_get_project"].annotations.read_only_hint
    assert tools["reaper_save_project"].annotations.destructive_hint
    assert "params" in tools["reaper_save_project"].input_schema["properties"]


async def test_read_only_prevents_bridge_call():
    bridge = RecordingBridge()
    server = create_server(bridge, "read-only")
    with pytest.raises(ToolError, match="PERMISSION_DENIED"):
        await server.call_tool(
            "reaper_save_project", {"params": {"project_id": "1", "confirm": True}}
        )
    assert not bridge.calls


async def test_confirmation_and_schema():
    bridge = RecordingBridge()
    server = create_server(bridge)
    with pytest.raises(ToolError, match="CONFIRMATION_REQUIRED"):
        await server.call_tool("reaper_save_project", {"params": {"project_id": "1"}})
    assert not bridge.calls
    await server.call_tool("reaper_save_project", {"params": {"project_id": "1", "confirm": True}})
    assert bridge.calls[0][0] == "project.save"
