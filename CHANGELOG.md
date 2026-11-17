# Changelog

## Unreleased

- Fix isolated rendering on REAPER 7.84 when the single target list ends in a
  semicolon; retain rejection of empty/multiple targets, outside destinations
  and existing files.
- Restore the original render sample rate after the host action resolves an
  automatic rate, including failure paths; extend native isolation regressions.
- Document limited local real-host validation without changing alpha status or
  claiming general plugin, audio-quality or clean-machine installer acceptance.
- Experimental Linux x86_64 support. The native extension builds as
  `reaper_mcp.so`, with the Win32 socket, locking, link and render-locality
  primitives moved behind `platform.hpp`; writes now pass `MSG_NOSIGNAL`, which
  on POSIX prevents a peer disconnect from terminating REAPER. See ADR 0009.
- POSIX single-instance lock keeps one permanent inode: neither the extension nor
  the installer unlinks `instance.lock`, which previously let two owners hold it.
  Symlinks, FIFOs and other non-regular lock files are refused.
- Render destinations are classified local, remote or unknown, and only local is
  accepted. sshfs and other network FUSE mounts, unidentifiable FUSE mounts,
  unrecognised filesystems, statfs failures other than a missing component and,
  on Windows, drives of unknown type are now rejected before anything is created.
- ELF headers are fully validated (class, data encoding, versions, type and
  header size) before the architecture is trusted.
- `reaper-mcp install` works on Linux: ELF architecture reading, detection of the
  usual REAPER locations, resource directory from `$XDG_CONFIG_HOME`, and launcher
  scripts on the PATH skipped in favour of the real binary.
- CI builds, tests and installs the Linux module, checking its file name and that
  `ReaperPluginEntry` is its only exported symbol, runs installer/native lock
  exclusion across processes and a CLI install lifecycle with the built module.
- No packaged installer or frozen bundle is provided for Linux.

## 0.1.0-alpha.1 — 2026-09-17

Initial public Windows x64 alpha targeting REAPER 7.80+.

- Official MCP stdio server with typed tools and separate native policy checks.
- Authenticated bounded loopback bridge and exclusive instance lifecycle.
- Project, track, item, take, transport, track FX, marker/region and existing
  envelope-point operations with stable identifiers and native Undo where applicable.
- Environment/plugin/resource discovery and experimental isolated render jobs.
- Per-user native installation with private ACLs, ownership manifest and repair.
- Official Codex CLI registration, doctor/status/version/logs and release guidance.
- Python/native tests, standalone Windows executable, Inno Setup packaging and CI.

Real REAPER/Codex/plugin acceptance remains outstanding. Save As, preset
application, audio analysis, semantic mixing, video and non-Windows runtime are
planned, not supported in this alpha.
