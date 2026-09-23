# 9: linux-platform-support

## Context
The extension was written against Win32 directly: Winsock, a share-mode-zero file
handle for single-instance locking, reparse-point attributes, and drive-type
queries for rejecting remote render targets. Linux is a first-class REAPER host
and the same extension model applies there.

## Decision
Confine per-platform primitives to reaper-extension/include/platform.hpp and keep
the bridge, adapter and operation code platform-neutral. POSIX equivalents:
flock for single-instance locking, lstat plus S_ISLNK for link detection, and a
statfs comparison against known network filesystem magic numbers for remoteness,
resolved through the nearest existing ancestor so a directory that does not exist
yet is still judged by the mount it will live on. Writes pass MSG_NOSIGNAL because
POSIX would otherwise raise SIGPIPE and terminate REAPER. WDL is fetched beside
the SDK because reaper_plugin.h includes SWELL's headers outside Win32.

## Alternatives
A socket abstraction library would replace a hundred lines of shim with a
dependency the project does not otherwise need. Rewriting the operations to be
platform-neutral at the call site was rejected: it spreads conditionals through
code that has nothing to do with the host.

## Consequences
Remoteness detection is heuristic on Linux. FUSE is deliberately absent from the
network list because local FUSE mounts are common and rejecting them would block
ordinary renders; an sshfs render target is therefore accepted. Unrecognised
filesystems are treated as local, matching how GetDriveTypeW reports an unknown
volume. Linux has no packaged installer or frozen bundle; the extension is built
from source and installed through the CLI.
