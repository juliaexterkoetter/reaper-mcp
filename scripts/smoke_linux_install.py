"""Linux installation lifecycle smoke test against a built reaper_mcp.so.

Runs the real CLI with an isolated HOME: install, reinstall, refusal while the
native lock is held (through the C++ `platform_test hold-lock` helper), and
uninstall. REAPER itself is a synthetic ELF executable, so this is a fixture
test of the installer, not host acceptance.

    python scripts/smoke_linux_install.py dist/native/reaper_mcp.so \
        build/extension/platform_test
"""

import json
import os
import stat
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def synthetic_reaper(path: Path) -> None:
    header = bytearray(64)
    header[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHI", header, 16, 2, 0x3E, 1)  # ET_EXEC, x86_64, EV_CURRENT
    struct.pack_into("<H", header, 52, 64)
    path.write_bytes(bytes(header))
    path.chmod(0o755)


def cli(env: dict[str, str], *args: str, expect: int = 0) -> str:
    result = subprocess.run(
        [sys.executable, "-m", "reaper_mcp.cli", *args],
        env=env,
        capture_output=True,
        text=True,
        timeout=60,
    )
    output = result.stdout + result.stderr
    if result.returncode != expect:
        raise SystemExit(f"reaper-mcp {' '.join(args)} exited {result.returncode}:\n{output}")
    return output


def check(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAILED: {message}")


def main() -> None:
    extension, helper = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        home, resource = root / "home", root / "resource"
        (root / "REAPER").mkdir()
        resource.mkdir()
        home.mkdir()
        (resource / "reaper.ini").write_text("[REAPER]\n")
        synthetic_reaper(root / "REAPER" / "reaper")
        env = {**os.environ, "HOME": str(home)}
        env.pop("LOCALAPPDATA", None)
        install = (
            "install",
            "--skip-codex",
            "--reaper-path",
            str(root / "REAPER" / "reaper"),
            "--resource-dir",
            str(resource),
            "--extension",
            str(extension),
        )

        cli(env, *install)
        plugin = resource / "UserPlugins" / "reaper_mcp.so"
        private = resource / "ReaperMCP"
        config = private / "config.json"
        lock = private / "instance.lock"
        check(plugin.read_bytes() == extension.read_bytes(), "extension not installed verbatim")
        check(stat.S_IMODE(private.stat().st_mode) == 0o700, "private directory not 0700")
        check(stat.S_IMODE(config.stat().st_mode) == 0o600, "configuration not 0600")
        token = json.loads(config.read_text())["token"]
        inode = lock.stat().st_ino
        (private / "user-notes.txt").write_text("keep")
        (resource / "UserPlugins" / "other_plugin.so").write_text("keep")

        cli(env, *install)
        check(json.loads(config.read_text())["token"] == token, "reinstall replaced the token")

        holder = subprocess.Popen(
            [str(helper), "hold-lock", str(lock)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
        )
        assert holder.stdout is not None and holder.stdin is not None
        try:
            check(holder.stdout.readline().strip() == "held", "native helper could not lock")
            check("INSTALLER_BUSY" in cli(env, *install, expect=1), "install ran under REAPER")
            check("INSTALLER_BUSY" in cli(env, "uninstall", expect=1), "uninstall ran under REAPER")
            check(plugin.exists() and config.exists(), "files changed while REAPER held the lock")
        finally:
            holder.stdin.close()
            holder.wait(timeout=10)

        cli(env, "uninstall")
        check(not plugin.exists() and not config.exists(), "managed files not removed")
        check((private / "user-notes.txt").read_text() == "keep", "user file removed")
        check(
            (resource / "UserPlugins" / "other_plugin.so").read_text() == "keep",
            "unrelated plugin removed",
        )
        check(lock.stat().st_ino == inode, "lock file inode changed across the lifecycle")

        cli(env, *install)
        check(json.loads(config.read_text())["token"] != token, "reinstall reused a removed token")
        cli(env, "uninstall")
    print("Linux installation lifecycle passed")


if __name__ == "__main__":
    main()
