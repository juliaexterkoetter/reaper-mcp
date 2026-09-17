import sys

from mcp import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client


async def test_real_mcp_stdio_process():
    parameters = StdioServerParameters(
        command=sys.executable, args=["-m", "reaper_mcp.cli", "serve"]
    )
    async with stdio_client(parameters) as (reader, writer):
        async with ClientSession(reader, writer) as session:
            info = await session.initialize()
            assert info.server_info.name == "REAPER MCP"
            catalog = await session.list_tools()
            assert "reaper_get_project" in {t.name for t in catalog.tools}
            result = await session.call_tool("reaper_ping", {})
            assert not result.is_error
