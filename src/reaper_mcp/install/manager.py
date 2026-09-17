"""Atomic installation with a bounded ownership manifest and rollback."""

import hashlib
import os
import secrets
import sys
import tempfile
from collections.abc import Iterator
from contextlib import contextmanager
from pathlib import Path
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, ValidationError

from reaper_mcp import __version__
from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.config import BridgeConfig, Policy, data_dir
from reaper_mcp.install.detection import detect_reaper, pe_architecture
from reaper_mcp.install.security import reject_link, secure_directory


class InstallRecord(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True)
    format_version: Literal[1] = 1
    version: str = __version__
    reaper_path: str
    resource_dir: str
    files: dict[str, str] = Field(default_factory=dict)
    codex_registration: dict[str, Any] | None = None


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def atomic_write(path: Path, data: bytes) -> None:
    reject_link(path)
    temp: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=path.parent, prefix=".reaper-mcp-", delete=False
        ) as out:
            temp = Path(out.name)
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
        os.replace(temp, path)
    finally:
        if temp is not None:
            temp.unlink(missing_ok=True)


def load_record(home: Path | None = None) -> InstallRecord | None:
    path = (home or data_dir()) / "install.json"
    reject_link(path)
    if not path.exists():
        return None
    try:
        record = InstallRecord.model_validate_json(path.read_bytes())
        root = Path(record.resource_dir)
        expected = {
            str(root / "UserPlugins" / "reaper_mcp.dll"),
            str(root / "ReaperMCP" / "config.json"),
        }
        if not root.is_absolute() or set(record.files) != expected:
            raise ValueError("Invalid managed paths")
        return record
    except (OSError, ValueError) as exc:
        raise BridgeError(
            "INVALID_MANIFEST", "Installation manifest is invalid; no files were changed."
        ) from exc


def save_record(record: InstallRecord, home: Path | None = None) -> None:
    atomic_write((home or data_dir()) / "install.json", record.model_dump_json(indent=2).encode())


@contextmanager
def instance_guard(root: Path) -> Iterator[None]:
    path = root / "instance.lock"
    reject_link(path)
    try:
        handle = path.open("a+b")
    except OSError as exc:
        raise BridgeError(
            "REAPER_RUNNING", "Close REAPER before installing or uninstalling."
        ) from exc
    try:
        try:
            if sys.platform == "win32":
                import msvcrt

                handle.seek(0)
                if not handle.read(1):
                    handle.write(b"0")
                    handle.flush()
                handle.seek(0)
                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl

                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as exc:
            raise BridgeError(
                "INSTALLER_BUSY", "Another installer owns this resource directory."
            ) from exc
        yield
    finally:
        handle.close()


def verify_owned(record: InstallRecord) -> None:
    root = Path(record.resource_dir)
    for directory in (root / "UserPlugins", root / "ReaperMCP"):
        reject_link(directory)
    for filename, expected in record.files.items():
        path = Path(filename)
        reject_link(path)
        if path.exists() and digest(path.read_bytes()) != expected:
            raise BridgeError("MODIFIED_FILE", f"Preserving a modified file: {path}")


def _install_files(
    executable: Path,
    resource: Path,
    extension: Path,
    policy: Policy | None = None,
    home: Path | None = None,
) -> InstallRecord:
    home = home or data_dir()
    resource = resource.resolve()
    if pe_architecture(extension) != "x64":
        raise BridgeError("UNSUPPORTED_ARCHITECTURE", "The extension must be a Windows x64 DLL.")
    record = load_record(home)
    if record and Path(record.resource_dir) != resource:
        raise BridgeError(
            "INSTALLATION_CONFLICT", "Uninstall the existing resource-directory integration first."
        )
    plugins, private = resource / "UserPlugins", resource / "ReaperMCP"
    reject_link(plugins)
    reject_link(private)
    if record:
        verify_owned(record)
    targets = (plugins / "reaper_mcp.dll", private / "config.json")
    for path in targets:
        reject_link(path)
        if path.exists() and not record:
            raise BridgeError("UNOWNED_FILE", f"Refusing to replace an unowned file: {path}")
    secure_directory(home)
    secure_directory(private)
    plugins.mkdir(parents=True, exist_ok=True)
    with instance_guard(private):
        config = BridgeConfig(token=secrets.token_hex(32), policy=policy or "confirm-destructive")
        if record and targets[1].exists():
            try:
                config = BridgeConfig.model_validate_json(targets[1].read_bytes())
            except ValidationError as exc:
                raise BridgeError(
                    "INVALID_CONFIGURATION", "Existing private configuration is invalid."
                ) from exc
            if policy is not None:
                config.policy = policy
        payloads = {
            targets[0]: extension.read_bytes(),
            targets[1]: config.model_dump_json().encode(),
        }
        backups: dict[Path, bytes | None] = {}
        try:
            for path, payload in payloads.items():
                backups[path] = path.read_bytes() if path.exists() else None
                atomic_write(path, payload)
            new = InstallRecord(
                reaper_path=str(executable),
                resource_dir=str(resource),
                files={str(p): digest(b) for p, b in payloads.items()},
                codex_registration=record.codex_registration if record else None,
            )
            save_record(new, home)
            return new
        except Exception:
            for path, previous in reversed(list(backups.items())):
                if path.exists() and digest(path.read_bytes()) == digest(payloads[path]):
                    if previous is None:
                        path.unlink()
                    else:
                        atomic_write(path, previous)
            raise


def bundled_extension() -> Path:
    root = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parents[1]))
    path = root / "native" / "reaper_mcp.dll"
    if not path.is_file():
        raise BridgeError(
            "EXTENSION_NOT_BUNDLED",
            "Use the Windows release bundle, or pass --extension after building the DLL.",
        )
    return path


def install(
    reaper_path: Path | None = None,
    resource: Path | None = None,
    extension: Path | None = None,
    policy: Policy | None = None,
) -> InstallRecord:
    if os.name != "nt":
        raise BridgeError("UNSUPPORTED_PLATFORM", "Installation currently requires Windows x64.")
    executable, resource = detect_reaper(reaper_path, resource)
    return install_files(executable, resource, extension or bundled_extension(), policy)


def _uninstall_files(home: Path | None = None) -> bool:
    home = home or data_dir()
    record = load_record(home)
    if record is None:
        return False
    verify_owned(record)
    private = Path(record.resource_dir) / "ReaperMCP"
    private.mkdir(exist_ok=True)
    with instance_guard(private):
        from reaper_mcp.install.codex import unregister

        unregister(record)
        for filename in record.files:
            Path(filename).unlink(missing_ok=True)
        (private / "bridge.json").unlink(missing_ok=True)
        (home / "install.json").unlink()
    (private / "instance.lock").unlink(missing_ok=True)
    # Do not recursively remove directories: they may contain user files or renders.
    return True


def install_files(
    executable: Path,
    resource: Path,
    extension: Path,
    policy: Policy | None = None,
    home: Path | None = None,
) -> InstallRecord:
    home = home or data_dir()
    secure_directory(home)
    with instance_guard(home):
        return _install_files(executable, resource, extension, policy, home)


def uninstall_files(home: Path | None = None) -> bool:
    home = home or data_dir()
    if not home.exists():
        return False
    with instance_guard(home):
        return _uninstall_files(home)
