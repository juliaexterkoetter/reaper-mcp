"""Protect private files with an explicit per-user Windows DACL."""

import ctypes
import os
import re
import subprocess
import sys
from pathlib import Path

from reaper_mcp.bridge.errors import BridgeError


def reject_link(path: Path) -> None:
    if path.is_symlink() or (
        path.exists() and getattr(path.lstat(), "st_file_attributes", 0) & 0x400
    ):
        raise BridgeError("UNSAFE_PATH", f"Refusing a symlink or junction: {path}")


def secure_directory(path: Path) -> None:
    reject_link(path)
    path.mkdir(parents=True, exist_ok=True)
    if sys.platform != "win32":
        path.chmod(0o700)
        return
    from ctypes import wintypes

    whoami = Path(os.environ["SystemRoot"]) / "System32" / "whoami.exe"
    result = subprocess.run(
        [str(whoami), "/user", "/fo", "csv", "/nh"],
        capture_output=True,
        text=True,
        check=True,
        timeout=10,
    )
    match = re.search(r"S-1-\d+(?:-\d+)+", result.stdout)
    if not match:
        raise BridgeError("ACL_FAILED", "Cannot identify the current Windows user SID.")
    advapi = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    pointer = ctypes.c_void_p
    advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW.argtypes = [
        wintypes.LPCWSTR,
        wintypes.DWORD,
        ctypes.POINTER(pointer),
        pointer,
    ]
    advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW.restype = wintypes.BOOL
    advapi.GetSecurityDescriptorDacl.argtypes = [
        pointer,
        ctypes.POINTER(wintypes.BOOL),
        ctypes.POINTER(pointer),
        ctypes.POINTER(wintypes.BOOL),
    ]
    advapi.GetSecurityDescriptorDacl.restype = wintypes.BOOL
    advapi.SetNamedSecurityInfoW.argtypes = [
        wintypes.LPWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
        pointer,
        pointer,
        pointer,
        pointer,
    ]
    advapi.SetNamedSecurityInfoW.restype = wintypes.DWORD
    kernel.LocalFree.argtypes = [pointer]
    kernel.LocalFree.restype = pointer
    descriptor, dacl = pointer(), pointer()
    present, defaulted = wintypes.BOOL(), wintypes.BOOL()
    sddl = f"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;{match.group()})"
    if not advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW(
        sddl, 1, ctypes.byref(descriptor), None
    ):
        raise BridgeError("ACL_FAILED", "Cannot construct a private Windows DACL.")
    try:
        if (
            not advapi.GetSecurityDescriptorDacl(
                descriptor, ctypes.byref(present), ctypes.byref(dacl), ctypes.byref(defaulted)
            )
            or not present
            or not dacl
        ):
            raise BridgeError("ACL_FAILED", "Private Windows DACL is invalid.")
        code = advapi.SetNamedSecurityInfoW(str(path), 1, 0x80000004, None, None, dacl, None)
        if code:
            raise BridgeError(
                "ACL_FAILED", "Cannot restrict access. Use a local NTFS directory you own."
            )
    finally:
        kernel.LocalFree(descriptor)
