"""CI-only smoke of the distributed executable; no REAPER runtime is simulated."""

import asyncio
import json
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from mcp import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client


async def smoke(executable: Path) -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        resource = root / "REAPER portable with spaces"
        resource.mkdir()
        (resource / "reaper.ini").write_text("[REAPER]\n")
        header = bytearray(128)
        header[:2] = b"MZ"
        struct.pack_into("<I", header, 60, 64)
        header[64:68] = b"PE\0\0"
        struct.pack_into("<H", header, 68, 0x8664)
        (resource / "reaper.exe").write_bytes(header)
        env = {**os.environ, "LOCALAPPDATA": str(root / "local"), "CODEX_HOME": str(root / "codex")}
        Path(env["CODEX_HOME"]).mkdir()

        def run(*args: str, expected: int = 0) -> str:
            result = subprocess.run(
                [str(executable), *args], env=env, capture_output=True, text=True
            )
            assert result.returncode == expected, (result.stdout, result.stderr)
            return result.stdout

        assert json.loads(run("version"))["protocol"] == 1
        run("install", "--reaper-path", str(resource))
        run("install", "--reaper-path", str(resource))
        assert (resource / "UserPlugins" / "reaper_mcp.dll").is_file()
        report = json.loads(run("doctor", "--json", expected=1))
        assert not report["ready"]
        assert report["checks"][1]["ok"], report
        async with stdio_client(
            StdioServerParameters(command=str(executable), args=["serve"], env=env)
        ) as (reader, writer):
            async with ClientSession(reader, writer) as session:
                await session.initialize()
                catalog = await session.list_tools()
                assert len(catalog.tools) > 50
                assert not (await session.call_tool("reaper_ping", {})).is_error
                result = await session.call_tool("reaper_get_project", {})
                assert result.is_error  # No running REAPER; never invent a project.
        run("uninstall")
        assert not (resource / "UserPlugins" / "reaper_mcp.dll").exists()
        run("uninstall")
        if len(sys.argv) > 2:
            setup = Path(sys.argv[2]).resolve()
            destination = root / "Installed application"
            result = subprocess.run(
                [
                    str(setup),
                    "/VERYSILENT",
                    "/SUPPRESSMSGBOXES",
                    "/NORESTART",
                    f"/DIR={destination}",
                    f"/REAPERPATH={resource}",
                    f"/LOG={root / 'setup.log'}",
                ],
                env=env,
            )
            assert result.returncode == 0, (root / "setup.log").read_text(errors="replace")
            assert (resource / "UserPlugins" / "reaper_mcp.dll").is_file()
            result = subprocess.run(
                [
                    str(destination / "unins000.exe"),
                    "/VERYSILENT",
                    "/SUPPRESSMSGBOXES",
                    "/NORESTART",
                ],
                env=env,
            )
            assert result.returncode == 0
            assert not (resource / "UserPlugins" / "reaper_mcp.dll").exists()
    print("PASS: frozen MCP, official Codex registration, reinstall, doctor, uninstall")


asyncio.run(smoke(Path(sys.argv[1]).resolve()))
