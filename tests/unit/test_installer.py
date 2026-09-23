import json
import os
import platform
import struct

import pytest

from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.install import manager
from reaper_mcp.install.detection import (
    EXECUTABLE_NAME,
    EXTENSION_NAME,
    detect_reaper,
    elf_architecture,
    host_architecture,
    looks_native,
    pe_architecture,
)

WINDOWS = os.name == "nt"


def pe(path, machine=0x8664):
    path.parent.mkdir(parents=True, exist_ok=True)
    header = bytearray(128)
    header[:2] = b"MZ"
    struct.pack_into("<I", header, 60, 64)
    header[64:68] = b"PE\0\0"
    struct.pack_into("<H", header, 68, machine)
    path.write_bytes(header)
    return path


def elf(path, machine=0x3E):
    path.parent.mkdir(parents=True, exist_ok=True)
    header = bytearray(128)
    header[:4] = b"\x7fELF"
    header[4] = 2  # EI_CLASS: 64-bit
    header[5] = 1  # EI_DATA: little endian
    struct.pack_into("<H", header, 18, machine)
    path.write_bytes(header)
    return path


def native(path, machine=None):
    """A synthetic module of whatever format this host's REAPER would load."""
    if WINDOWS:
        return pe(path, 0x8664 if machine is None else machine)
    default = 0xB7 if platform.machine().lower() in ("aarch64", "arm64") else 0x3E
    return elf(path, default if machine is None else machine)


@pytest.fixture
def install_paths(tmp_path):
    executable = native(tmp_path / "REAPER" / EXECUTABLE_NAME)
    (executable.parent / "reaper.ini").write_text("[REAPER]\n")
    extension = native(tmp_path / "bundle" / EXTENSION_NAME)
    return executable, executable.parent, extension, tmp_path / "state"


@pytest.mark.parametrize("machine,expected", [(0x8664, "x64"), (0x14C, "x86"), (0xAA64, "arm64")])
def test_pe_architecture(tmp_path, machine, expected):
    assert pe_architecture(pe(tmp_path / "test.exe", machine)) == expected


@pytest.mark.parametrize("machine,expected", [(0x3E, "x86_64"), (0xB7, "arm64"), (0x28, "unknown")])
def test_elf_architecture(tmp_path, machine, expected):
    assert elf_architecture(elf(tmp_path / "test.so", machine)) == expected


def test_elf_rejects_32_bit(tmp_path):
    path = elf(tmp_path / "small.so")
    data = bytearray(path.read_bytes())
    data[4] = 1  # EI_CLASS: 32-bit
    path.write_bytes(data)
    with pytest.raises(BridgeError, match="INVALID_EXECUTABLE"):
        elf_architecture(path)


def test_invalid_pe(tmp_path):
    file = tmp_path / "bad.exe"
    file.write_bytes(b"not an executable")
    with pytest.raises(BridgeError, match="INVALID_EXECUTABLE"):
        pe_architecture(file)


def test_invalid_elf(tmp_path):
    file = tmp_path / "bad.so"
    file.write_bytes(b"not an executable")
    with pytest.raises(BridgeError, match="INVALID_EXECUTABLE"):
        elf_architecture(file)


def test_host_architecture_is_known():
    assert host_architecture() != "unknown"


def test_launcher_scripts_are_not_mistaken_for_reaper(tmp_path):
    """A shell wrapper named `reaper` on the PATH is common on Linux."""
    script = tmp_path / EXECUTABLE_NAME
    script.write_text('#!/bin/sh\nexec /opt/REAPER/reaper "$@"\n')
    assert not looks_native(script)
    assert looks_native(native(tmp_path / "real" / EXECUTABLE_NAME))
    assert not looks_native(tmp_path / "missing")


def test_portable_detection(install_paths):
    executable, resource, _, _ = install_paths
    assert detect_reaper(executable) == (executable, resource)


def test_normal_resource_detection(tmp_path, monkeypatch):
    executable = native(tmp_path / "programs" / EXECUTABLE_NAME)
    resource = tmp_path / "roaming" / "REAPER"
    resource.mkdir(parents=True)
    (resource / "reaper.ini").write_text("[REAPER]\n")
    monkeypatch.setenv("APPDATA" if WINDOWS else "XDG_CONFIG_HOME", str(resource.parent))
    assert detect_reaper(executable) == (executable, resource)


def test_install_reinstall_uninstall_preserves_unrelated_files(install_paths):
    executable, resource, extension, home = install_paths
    record = manager.install_files(executable, resource, extension, home=home)
    private = resource / "ReaperMCP"
    config = json.loads((private / "config.json").read_text())
    assert len(config["token"]) == 64
    assert record == manager.load_record(home)
    manager.install_files(executable, resource, extension, policy="read-only", home=home)
    changed = json.loads((private / "config.json").read_text())
    assert changed["token"] == config["token"]
    assert changed["policy"] == "read-only"
    (private / "user-note.txt").write_text("keep")
    assert manager.uninstall_files(home)
    assert (private / "user-note.txt").read_text() == "keep"
    assert not (resource / "UserPlugins" / EXTENSION_NAME).exists()
    assert not manager.uninstall_files(home)


def test_modified_file_is_preserved(install_paths):
    executable, resource, extension, home = install_paths
    manager.install_files(executable, resource, extension, home=home)
    module = resource / "UserPlugins" / EXTENSION_NAME
    module.write_bytes(b"user modification")
    with pytest.raises(BridgeError, match="MODIFIED_FILE"):
        manager.uninstall_files(home)
    with pytest.raises(BridgeError, match="MODIFIED_FILE"):
        manager.install_files(executable, resource, extension, home=home)
    assert module.read_bytes() == b"user modification"


def test_unowned_file_not_overwritten(install_paths):
    executable, resource, extension, home = install_paths
    private = resource / "ReaperMCP"
    private.mkdir()
    config = private / "config.json"
    config.write_text("user configuration")
    with pytest.raises(BridgeError, match="UNOWNED_FILE"):
        manager.install_files(executable, resource, extension, home=home)
    assert config.read_text() == "user configuration"


def test_failed_install_rolls_back(install_paths, monkeypatch):
    executable, resource, extension, home = install_paths
    original = manager.atomic_write

    def fail_config(path, data):
        if path.name == "config.json":
            raise OSError("injected failure")
        original(path, data)

    monkeypatch.setattr(manager, "atomic_write", fail_config)
    with pytest.raises(OSError, match="injected failure"):
        manager.install_files(executable, resource, extension, home=home)
    assert not (resource / "UserPlugins" / EXTENSION_NAME).exists()
    assert manager.load_record(home) is None


def test_manifest_cannot_delete_arbitrary_paths(install_paths):
    executable, resource, extension, home = install_paths
    manager.install_files(executable, resource, extension, home=home)
    path = home / "install.json"
    record = json.loads(path.read_text())
    target = resource / "keep.txt"
    target.write_text("keep")
    record["files"][str(target)] = manager.digest(target.read_bytes())
    path.write_text(json.dumps(record))
    with pytest.raises(BridgeError, match="INVALID_MANIFEST"):
        manager.uninstall_files(home)
    assert target.exists()


def test_instance_guard_excludes_second_installer(tmp_path):
    with manager.instance_guard(tmp_path):
        with pytest.raises(BridgeError, match="INSTALLER_BUSY|REAPER_RUNNING"):
            with manager.instance_guard(tmp_path):
                pytest.fail("concurrent installer admitted")


def test_symlink_not_replaced(tmp_path):
    target = tmp_path / "target"
    target.write_text("keep")
    link = tmp_path / "link"
    try:
        link.symlink_to(target)
    except OSError:
        pytest.skip("Symlink creation is unavailable for this Windows account")
    with pytest.raises(BridgeError, match="UNSAFE_PATH"):
        manager.atomic_write(link, b"bad")
    assert target.read_text() == "keep"
