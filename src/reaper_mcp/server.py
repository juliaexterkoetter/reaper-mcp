"""Official MCP SDK entry point. Stdout is reserved for MCP messages."""

from mcp.server import MCPServer
from mcp.types import ToolAnnotations
from pydantic import ValidationError

from reaper_mcp import PROTOCOL_VERSION, __version__
from reaper_mcp.bridge.client import BridgeClient
from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.config import BridgeConfig, Policy, resource_dir
from reaper_mcp.tools.automation import SPECS as AUTOMATION_SPECS
from reaper_mcp.tools.common import Bridge, register
from reaper_mcp.tools.environment import SPECS as ENVIRONMENT_SPECS
from reaper_mcp.tools.fx import SPECS as FX_SPECS
from reaper_mcp.tools.items import SPECS as ITEM_SPECS
from reaper_mcp.tools.markers import SPECS as MARKER_SPECS
from reaper_mcp.tools.project import SPECS as PROJECT_SPECS
from reaper_mcp.tools.render import SPECS as RENDER_SPECS
from reaper_mcp.tools.takes import SPECS as TAKE_SPECS
from reaper_mcp.tools.tracks import SPECS as TRACK_SPECS
from reaper_mcp.tools.transport import SPECS as TRANSPORT_SPECS

SPECS = (
    PROJECT_SPECS
    + TRACK_SPECS
    + ITEM_SPECS
    + TAKE_SPECS
    + TRANSPORT_SPECS
    + FX_SPECS
    + MARKER_SPECS
    + AUTOMATION_SPECS
    + RENDER_SPECS
    + ENVIRONMENT_SPECS
)


def create_server(
    bridge: Bridge | None = None, policy: Policy = "confirm-destructive"
) -> MCPServer:
    server = MCPServer(
        "REAPER MCP",
        instructions="Control the local REAPER project. Inspect before editing. Use GUIDs. "
        "Request user approval before destructive operations. Never retry timed-out mutations.",
    )

    @server.tool(
        annotations=ToolAnnotations(
            read_only_hint=True,
            destructive_hint=False,
            idempotent_hint=True,
            open_world_hint=False,
        )
    )
    def reaper_ping() -> dict[str, str]:
        """Check the MCP process only; use status to check the native REAPER bridge."""
        return {"status": "ok", "component": "mcp-server"}

    @server.tool(
        annotations=ToolAnnotations(
            read_only_hint=True,
            destructive_hint=False,
            idempotent_hint=True,
            open_world_hint=False,
        )
    )
    def reaper_get_server_info() -> dict[str, str | int]:
        """Return local server and internal protocol versions."""
        return {"server_version": __version__, "protocol_version": PROTOCOL_VERSION}

    register(server, bridge or BridgeClient(), policy, SPECS)
    return server


def serve() -> None:
    policy: Policy = "confirm-destructive"
    try:
        config = resource_dir() / "ReaperMCP" / "config.json"
        if config.exists():
            policy = BridgeConfig.model_validate_json(config.read_bytes()).policy
    except BridgeError as exc:
        if exc.code != "NOT_INSTALLED":
            raise
    except (OSError, ValidationError) as exc:
        raise BridgeError("INVALID_CONFIGURATION", "Run reaper-mcp doctor.") from exc

    create_server(policy=policy).run()
