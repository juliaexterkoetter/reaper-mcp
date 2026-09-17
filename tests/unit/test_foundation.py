from reaper_mcp import PROTOCOL_VERSION, __version__
from reaper_mcp.server import create_server


def test_versions():
    assert PROTOCOL_VERSION == 1
    assert __version__.startswith("0.1.")


def test_server_builds():
    assert create_server() is not None
