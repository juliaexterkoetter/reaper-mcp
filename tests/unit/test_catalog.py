import pytest
from mcp.server.mcpserver.exceptions import ToolError

from reaper_mcp.server import SPECS, create_server

EXAMPLE = {
    "project_id": "session-1",
    "track": "{TRACK}",
    "item": "{ITEM}",
    "fx": "{FX}",
    "name": "Example",
    "volume_db": -3.0,
    "pan_percent": 0.0,
    "position_seconds": 1.0,
    "start_seconds": 1.0,
    "end_seconds": 10.0,
    "seconds": 0.1,
    "plugin": "test.dll",
    "parameter": 0,
    "normalized_value": 0.5,
    "guid": "{MARKER}",
    "confirm": True,
    "sample_rate": 48000,
    "channels": 2,
    "envelope": "{ENV}",
    "time_seconds": 1.0,
    "value": -3.0,
    "unit": "db",
    "point_index": 0,
    "expected_state_version": 7,
}


class Recorder:
    def __init__(self):
        self.calls = []

    async def call(self, method, params=None):
        self.calls.append((method, params))
        return {"ok": True}


def arguments(spec):
    return {
        name: EXAMPLE[name]
        for name, field in spec.model.model_fields.items()
        if field.is_required() or name == "confirm"
    }


@pytest.mark.parametrize("spec", SPECS, ids=lambda s: s.name)
async def test_every_tool_routes_validated_arguments(spec):
    bridge = Recorder()
    result = await create_server(bridge).call_tool(spec.name, {"params": arguments(spec)})
    assert not result.is_error
    assert bridge.calls[0][0] == spec.method
    assert bridge.calls[0][1] == spec.model(**arguments(spec)).model_dump()


@pytest.mark.parametrize("spec", [s for s in SPECS if s.access != "read"], ids=lambda s: s.name)
async def test_read_only_blocks_every_mutation(spec):
    bridge = Recorder()
    with pytest.raises(ToolError, match="PERMISSION_DENIED"):
        await create_server(bridge, "read-only").call_tool(spec.name, {"params": arguments(spec)})
    assert not bridge.calls
