"""Test-only bridge. Never imported by production code."""

from copy import deepcopy

from reaper_mcp.bridge.errors import BridgeError


class MockReaperBridge:
    def __init__(self):
        self.tracks = [{"guid": "{VOICE}", "name": "Voz", "volume_db": 0.0}]
        self.items = [{"guid": "{ITEM}", "position": 0.0, "length": 10.0}]
        self.fx = [{"guid": "{FX}", "name": "ReaComp"}]
        self.calls = []

    async def call(self, method, params=None):
        params = params or {}
        self.calls.append((method, params))
        if method == "bridge.info":
            return {"protocol_version": 1, "capabilities": ["tracks.list", "project.get"]}
        if method == "tracks.list":
            return deepcopy(self.tracks)
        if method == "project.get":
            return {"name": "Test project", "guid": "{PROJECT}"}
        if method == "items.list":
            return deepcopy(self.items)
        if method == "fx.list":
            return deepcopy(self.fx)
        raise BridgeError("METHOD_NOT_FOUND", method)
