"""Windows REAPER detection without scanning disks or guessing architecture."""

import os
import struct
import sys
from pathlib import Path

from reaper_mcp.bridge.errors import BridgeError


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


def detect_reaper(explicit: Path | None = None, resource: Path | None = None) -> tuple[Path, Path]:
    if explicit:
        candidates = [explicit / "reaper.exe" if explicit.is_dir() else explicit]
    elif os.name == "nt":
        candidates = registry_candidates()
        for variable in ("ProgramW6432", "ProgramFiles"):
            if os.environ.get(variable):
                candidates.append(Path(os.environ[variable]) / "REAPER (x64)" / "reaper.exe")
    else:
        raise BridgeError(
            "UNSUPPORTED_PLATFORM", "Automatic REAPER detection requires Windows x64."
        )
    existing = list(dict.fromkeys(p.resolve() for p in candidates if p.is_file()))
    if not existing:
        raise BridgeError(
            "REAPER_NOT_FOUND", "Install REAPER or pass --reaper-path for a portable copy."
        )
    if len(existing) != 1:
        raise BridgeError(
            "AMBIGUOUS_INSTALLATION", "Multiple REAPER installations found; pass --reaper-path."
        )
    executable = existing[0]
    if pe_architecture(executable) != "x64":
        raise BridgeError("UNSUPPORTED_ARCHITECTURE", "This release requires Windows x64 REAPER.")
    if resource is None:
        if (executable.parent / "reaper.ini").is_file():
            resource = executable.parent
        elif os.environ.get("APPDATA"):
            resource = Path(os.environ["APPDATA"]) / "REAPER"
        else:
            raise BridgeError("RESOURCE_NOT_FOUND", "Pass --resource-dir for this installation.")
    resource = resource.resolve()
    if not (resource / "reaper.ini").is_file():
        raise BridgeError(
            "RESOURCE_NOT_FOUND", "Open REAPER once, close it, then retry; or pass --resource-dir."
        )
    return executable, resource
