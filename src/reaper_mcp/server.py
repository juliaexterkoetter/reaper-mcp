"""Official MCP SDK entry point. Stdout is reserved for MCP messages."""

from mcp.server import MCPServer

from reaper_mcp import PROTOCOL_VERSION, __version__


def create_server() -> MCPServer:
    server = MCPServer(
        "REAPER MCP",
        instructions="Control the local REAPER project. Inspect before editing. Use GUIDs. "
        "Request user approval before destructive operations. Never retry timed-out mutations.",
    )

    @server.tool()
    def reaper_ping() -> dict[str, str]:
        """Check the MCP process only; use status to check the native REAPER bridge."""
        return {"status": "ok", "component": "mcp-server"}

    @server.tool()
    def reaper_get_server_info() -> dict[str, str | int]:
        """Return local server and internal protocol versions."""
        return {"server_version": __version__, "protocol_version": PROTOCOL_VERSION}

    return server


def serve() -> None:
    create_server().run()
