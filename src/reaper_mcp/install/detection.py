"""REAPER detection without scanning disks or guessing architecture."""

import os
import platform
import shutil
import struct
import sys
from pathlib import Path

from reaper_mcp.bridge.errors import BridgeError

EXECUTABLE_NAME = "reaper.exe" if os.name == "nt" else "reaper"
EXTENSION_NAME = "reaper_mcp.dll" if os.name == "nt" else "reaper_mcp.so"
# platform.machine() spells the same architecture differently per OS.
_ARCHITECTURE_ALIASES = {
    "amd64": "x86_64",
    "x86_64": "x86_64",
    "aarch64": "arm64",
    "arm64": "arm64",
}


def pe_architecture(path: Path) -> str:
    try:
        with path.open("rb") as handle:
            header = handle.read(64)
            if len(header) != 64 or header[:2] != b"MZ":
                raise ValueError
            offset = struct.unpack_from("<I", header, 60)[0]
            if offset < 64 or offset > 16 * 1024 * 1024:
                raise ValueError
            handle.seek(offset)
            coff = handle.read(6)
            if len(coff) != 6 or coff[:4] != b"PE\0\0":
                raise ValueError
            machine = struct.unpack_from("<H", coff, 4)[0]
        return {0x8664: "x64", 0x14C: "x86", 0xAA64: "arm64"}.get(machine, "unknown")
    except (OSError, ValueError, struct.error) as exc:
        raise BridgeError(
            "INVALID_EXECUTABLE", f"Cannot read Windows PE architecture: {path}"
        ) from exc


def elf_architecture(path: Path) -> str:
    try:
        with path.open("rb") as handle:
            header = handle.read(20)
            if len(header) != 20 or header[:4] != b"\x7fELF":
                raise ValueError
            if header[4] != 2:  # EI_CLASS; only 64-bit hosts are supported
                raise ValueError
            machine = struct.unpack_from("<H" if header[5] == 1 else ">H", header, 18)[0]
        return {0x3E: "x86_64", 0xB7: "arm64"}.get(machine, "unknown")
    except (OSError, ValueError, struct.error) as exc:
        raise BridgeError("INVALID_EXECUTABLE", f"Cannot read ELF architecture: {path}") from exc


def module_architecture(path: Path) -> str:
    """Architecture of a native executable or module, read from its own header."""
    return pe_architecture(path) if os.name == "nt" else elf_architecture(path)


def host_architecture() -> str:
    """The architecture a module must have to load into this host's REAPER."""
    if os.name == "nt":
        return "x64"
    return _ARCHITECTURE_ALIASES.get(platform.machine().lower(), "unknown")


def looks_native(path: Path) -> bool:
    """Whether a file carries a native executable header, without raising.

    Distributions and users routinely put a shell wrapper named `reaper` on the
    PATH, so a scan has to pass over those rather than fail on them.
    """
    try:
        with path.open("rb") as handle:
            magic = handle.read(4)
    except OSError:
        return False
    return magic == b"\x7fELF" if os.name != "nt" else magic[:2] == b"MZ"


def registry_candidates() -> list[Path]:
    if sys.platform != "win32":
        return []
    import winreg

    paths: list[Path] = []
    for hive in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
        for view in (winreg.KEY_WOW64_64KEY, winreg.KEY_WOW64_32KEY):
            try:
                with winreg.OpenKey(hive, r"Software\REAPER", 0, winreg.KEY_READ | view) as key:
                    value, _ = winreg.QueryValueEx(key, "")
                    paths.append(Path(str(value)) / "reaper.exe")
            except OSError:
                continue
    return paths


def linux_candidates() -> list[Path]:
    home = Path.home()
    paths = [
        Path("/opt/REAPER/reaper"),
        Path("/usr/local/opt/REAPER/reaper"),
        Path("/usr/bin/reaper"),
        Path("/usr/local/bin/reaper"),
        home / "opt" / "REAPER" / "reaper",
        home / ".local" / "bin" / "reaper",
    ]
    found = shutil.which("reaper")
    if found:
        paths.append(Path(found))
    return paths


def detect_reaper(explicit: Path | None = None, resource: Path | None = None) -> tuple[Path, Path]:
    scanned = explicit is None
    if explicit:
        candidates = [explicit / EXECUTABLE_NAME if explicit.is_dir() else explicit]
    elif os.name == "nt":
        candidates = registry_candidates()
        for variable in ("ProgramW6432", "ProgramFiles"):
            if os.environ.get(variable):
                candidates.append(Path(os.environ[variable]) / "REAPER (x64)" / "reaper.exe")
    elif sys.platform.startswith("linux"):
        candidates = linux_candidates()
    else:
        raise BridgeError(
            "UNSUPPORTED_PLATFORM", "Automatic REAPER detection requires Windows or Linux."
        )
    existing = list(dict.fromkeys(p.resolve() for p in candidates if p.is_file()))
    # A scan collects well-known locations, several of which commonly hold a
    # launcher script rather than REAPER itself; an explicitly given path is
    # reported on instead of skipped, so a typo does not look like "not found".
    if scanned:
        existing = [p for p in existing if looks_native(p)]
    if not existing:
        raise BridgeError(
            "REAPER_NOT_FOUND", "Install REAPER or pass --reaper-path for a portable copy."
        )
    if len(existing) != 1:
        raise BridgeError(
            "AMBIGUOUS_INSTALLATION", "Multiple REAPER installations found; pass --reaper-path."
        )
    executable = existing[0]
    expected = host_architecture()
    if module_architecture(executable) != expected:
        raise BridgeError(
            "UNSUPPORTED_ARCHITECTURE", f"This release requires a {expected} REAPER build."
        )
    if resource is None:
        if (executable.parent / "reaper.ini").is_file():
            resource = executable.parent
        elif os.name == "nt" and os.environ.get("APPDATA"):
            resource = Path(os.environ["APPDATA"]) / "REAPER"
        elif os.name != "nt":
            config = os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config"
            resource = Path(config) / "REAPER"
        else:
            raise BridgeError("RESOURCE_NOT_FOUND", "Pass --resource-dir for this installation.")
    resource = resource.resolve()
    if not (resource / "reaper.ini").is_file():
        raise BridgeError(
            "RESOURCE_NOT_FOUND", "Open REAPER once, close it, then retry; or pass --resource-dir."
        )
    return executable, resource
