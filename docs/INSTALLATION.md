# Installation

Windows x64 and REAPER 7.80+ are the initial runtime target. Runtime acceptance
is still experimental. Python developers can install the source package with
`python -m pip install .`; a public PyPI release is not currently published.

The Windows standalone release bundle includes Python and the native DLL.
The final installer/registration workflow is under development.

## Developer installation

Build the native extension as described in DEVELOPMENT.md, close REAPER, then:

```powershell
reaper-mcp install --extension .\dist\native\reaper_mcp.dll
```

Normal installations are detected from registry/default program locations and
use `%APPDATA%\REAPER`. A portable directory containing reaper.exe and reaper.ini
is selected explicitly:

```powershell
reaper-mcp install --reaper-path D:\Audio\REAPER\reaper.exe --extension .\dist\native\reaper_mcp.dll
```

For custom `-cfgfile` resource directories, pass `--resource-dir`. If several
installations are detected, the installer requires an explicit path instead of
guessing. Open REAPER once before installation so its resource directory exists.
ARM64 and x86 binaries are rejected by PE-header inspection.

Default policy: `confirm-destructive`. Select `--policy read-only` when editing
must be disabled. `full-control` skips confirmation flags; it never adds shell
or arbitrary-action access. A reinstall preserves the token and policy unless
an explicit policy is selected. Restart REAPER after installation/policy changes.

## Ownership and removal

`reaper-mcp uninstall` removes only the extension and private config recorded in
`%LOCALAPPDATA%\ReaperMCP\install.json`. Modified/unowned files are preserved and
produce an actionable error. Other plugins, project files, renders and user
notes are never recursively deleted. Close REAPER first. Failed file installation
rolls back successfully written files. Symlink/junction targets are rejected.

The private configuration directory has an explicit Windows DACL granting access
only to the current user and SYSTEM; installation fails if this cannot be set.
The installer is per-user and does not require administrator rights for normal
REAPER resource directories. A portable copy in a protected system directory
must be moved to a writable location or installed with suitable permissions.

## Codex registration and diagnosis

`install` now registers `reaper-mcp` through the official `codex mcp add`
command. Codex CLI must be on PATH. Use `--skip-codex` for another MCP client.
No TOML is rewritten by this project. An existing entry is preserved unless it
exactly matches the registration recorded by this installer. Reinstallation is
idempotent. Uninstallation removes only the matching owned entry; a changed
entry stops removal and explains the conflict. If registration fails, native
files remain installed and rerunning `install` after resolving the cause repairs
registration. Do not delete an unrelated entry to resolve a name conflict.

Run `reaper-mcp doctor` (or `status`) after opening REAPER. `--json` produces
machine-readable checks. Exit status 0 requires installed managed files, the
owned Codex registration, and an authenticated, protocol-compatible native bridge.
A successful `version` or MCP `reaper_ping` alone does not establish REAPER readiness.
`--skip-codex` installations report the absent Codex integration explicitly.

Official CLI: <https://developers.openai.com/codex/mcp>.
