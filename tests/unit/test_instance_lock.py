"""Single-instance lock on POSIX: one permanent inode shared with the native extension.

The multiprocess cases use real child processes synchronised over pipes. Those
marked `native` drive the real C++ lock through the `platform_test hold-lock`
helper; set RMCP_PLATFORM_TEST to its path (CI does) or they are skipped.
"""

import os
import signal
import subprocess
import sys
import textwrap
import time
from contextlib import contextmanager
from pathlib import Path

import pytest
from test_installer import install_paths  # noqa: F401  (fixture)

from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.install import manager
from reaper_mcp.install.detection import EXTENSION_NAME

pytestmark = pytest.mark.skipif(sys.platform == "win32", reason="POSIX flock semantics")

NATIVE = os.environ.get("RMCP_PLATFORM_TEST")
native = pytest.mark.skipif(not NATIVE, reason="RMCP_PLATFORM_TEST not set")

# A contender that opens the lock file first and only calls flock once told to,
# which is the interleaving that broke exclusion when release unlinked the file.
PREOPENED = textwrap.dedent(
    """
    import fcntl, os, sys
    fd = os.open(sys.argv[1], os.O_RDWR)
    print("opened", flush=True)
    sys.stdin.readline()
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        print("locked", flush=True)
    except BlockingIOError:
        print("busy", flush=True)
    sys.stdin.readline()
    """
)


class Child:
    def __init__(self, *args: str) -> None:
        self.process = subprocess.Popen(
            list(args), stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True
        )

    def read(self) -> str:
        assert self.process.stdout is not None
        return self.process.stdout.readline().strip()

    def send(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("\n")
        self.process.stdin.flush()

    def close(self) -> int:
        if self.process.poll() is None:
            assert self.process.stdin is not None
            self.process.stdin.close()
        return self.process.wait(timeout=10)


@contextmanager
def native_holder(path: Path):
    """The real native lock, held until the context exits."""
    child = Child(str(NATIVE), "hold-lock", str(path))
    try:
        yield child.read(), child
    finally:
        child.close()


def test_release_keeps_one_inode(tmp_path):
    with manager.instance_guard(tmp_path):
        first = (tmp_path / "instance.lock").stat().st_ino
    assert (tmp_path / "instance.lock").is_file()
    with manager.instance_guard(tmp_path):
        assert (tmp_path / "instance.lock").stat().st_ino == first


def test_preopened_contender_keeps_exclusion(tmp_path):
    with manager.instance_guard(tmp_path):
        contender = Child(sys.executable, "-c", PREOPENED, str(tmp_path / "instance.lock"))
        assert contender.read() == "opened"
    try:
        contender.send()
        assert contender.read() == "locked"
        with pytest.raises(BridgeError, match="INSTALLER_BUSY"):
            with manager.instance_guard(tmp_path):
                pytest.fail("a second owner was admitted while the contender held the lock")
    finally:
        contender.close()


def test_symlinked_lock_is_refused(tmp_path):
    target = tmp_path / "victim"
    target.write_text("keep")
    (tmp_path / "instance.lock").symlink_to(target)
    with pytest.raises(BridgeError, match="UNSAFE_PATH"):
        with manager.instance_guard(tmp_path):
            pytest.fail("symlinked lock admitted")
    assert target.read_text() == "keep"


def test_fifo_lock_is_refused_without_blocking(tmp_path):
    os.mkfifo(tmp_path / "instance.lock")
    started = time.monotonic()
    with pytest.raises(BridgeError, match="UNSAFE_PATH"):
        with manager.instance_guard(tmp_path):
            pytest.fail("FIFO lock admitted")
    assert time.monotonic() - started < 5


def test_uninstall_and_reinstall_keep_the_lock_inode(install_paths):  # noqa: F811
    executable, resource, extension, home = install_paths
    manager.install_files(executable, resource, extension, home=home)
    lock = resource / "ReaperMCP" / "instance.lock"
    inode = lock.stat().st_ino
    assert manager.uninstall_files(home)
    assert lock.stat().st_ino == inode
    manager.install_files(executable, resource, extension, home=home)
    assert lock.stat().st_ino == inode


@native
def test_native_and_installer_exclude_each_other(install_paths):  # noqa: F811
    executable, resource, extension, home = install_paths
    record = manager.install_files(executable, resource, extension, home=home)
    lock = resource / "ReaperMCP" / "instance.lock"
    plugin = resource / "UserPlugins" / EXTENSION_NAME
    before = plugin.read_bytes()

    with native_holder(lock) as (state, _):
        assert state == "held"
        with pytest.raises(BridgeError, match="INSTALLER_BUSY"):
            manager.install_files(executable, resource, extension, home=home)
        with pytest.raises(BridgeError, match="INSTALLER_BUSY"):
            manager.uninstall_files(home)
        assert plugin.read_bytes() == before
        assert manager.load_record(home) == record

    with manager.instance_guard(lock.parent):
        with native_holder(lock) as (state, _):
            assert state == "busy"

    assert manager.uninstall_files(home)
    with native_holder(lock) as (state, _):
        assert state == "held"
        with pytest.raises(BridgeError, match="INSTALLER_BUSY"):
            manager.install_files(executable, resource, extension, home=home)
    manager.install_files(executable, resource, extension, home=home)


@native
def test_native_preopened_contender_keeps_exclusion(tmp_path):
    lock = tmp_path / "instance.lock"
    with native_holder(lock) as (state, _):
        assert state == "held"
        contender = Child(sys.executable, "-c", PREOPENED, str(lock))
        assert contender.read() == "opened"
    try:
        contender.send()
        assert contender.read() == "locked"
        with native_holder(lock) as (state, _):
            assert state == "busy"
    finally:
        contender.close()


@native
def test_installer_recovers_after_native_crash(install_paths):  # noqa: F811
    executable, resource, extension, home = install_paths
    manager.install_files(executable, resource, extension, home=home)
    lock = resource / "ReaperMCP" / "instance.lock"
    child = Child(str(NATIVE), "hold-lock", str(lock))
    assert child.read() == "held"
    child.process.send_signal(signal.SIGKILL)
    assert child.process.wait(timeout=10) == -signal.SIGKILL
    manager.install_files(executable, resource, extension, home=home)
    with native_holder(lock) as (state, _):
        assert state == "held"
