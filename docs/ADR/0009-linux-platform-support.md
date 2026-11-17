# 9: linux-platform-support

## Context
The extension was written against Win32 directly: Winsock, a share-mode-zero file
handle for single-instance locking, reparse-point attributes, and drive-type
queries for rejecting remote render targets. Linux is a first-class REAPER host
and the same extension model applies there.

## Decision
Confine per-platform primitives to reaper-extension/include/platform.hpp and keep
the bridge, adapter and operation code platform-neutral. POSIX equivalents:
flock for single-instance locking, lstat plus S_ISLNK for link detection, and
statfs for render-destination locality, resolved through the nearest existing
ancestor so a directory that does not exist yet is still judged by the mount it
will live on.

Locking: flock belongs to an inode, so the POSIX lock file is permanent and is
never unlinked by the extension or the installer. Unlinking on release let a
contender that had already opened the file lock the orphaned inode while a
newcomer created and locked a fresh one, so two owners coexisted. The file is
opened with O_NOFOLLOW, must be a regular file, and must still be the inode the
path names after flock succeeds.

Locality: destinations are classified as local, remote or unknown, and only
local is accepted. Local means a statfs magic number on an allowlist of local
filesystems, or a FUSE mount whose mountinfo type is fuseblk on a /dev block
device. Known network filesystems and network FUSE subtypes (sshfs, rclone, ...)
are remote. Lookup failures other than a missing component, unrecognised
filesystems and every other FUSE mount are unknown. On Windows, drive types that
are not positively local are likewise unknown. Writes pass MSG_NOSIGNAL because
POSIX would otherwise raise SIGPIPE and terminate REAPER. WDL is fetched beside
the SDK because reaper_plugin.h includes SWELL's headers outside Win32.

## Alternatives
A socket abstraction library would replace a hundred lines of shim with a
dependency the project does not otherwise need. Rewriting the operations to be
platform-neutral at the call site was rejected: it spreads conditionals through
code that has nothing to do with the host.

## Consequences
Locality fails closed: a render to an unrecognised filesystem, or to a local
FUSE filesystem that is not block-backed (bindfs, gocryptfs, mergerfs), is
refused even when it is in fact local, because mountinfo cannot show what such a
mount sits on. Extending the allowlist is the remedy for a filesystem that should
be accepted. On POSIX an instance.lock file remains in the private directory
after uninstallation. Linux remains experimental until the corrected
implementation has real REAPER 7.80+ acceptance. Linux has no packaged installer or frozen bundle; the extension is built
from source and installed through the CLI.
