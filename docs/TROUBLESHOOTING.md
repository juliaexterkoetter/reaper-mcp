# Troubleshooting

Run `reaper-mcp doctor --json` and `reaper-mcp version`. Return their output with
the Windows and REAPER versions. Paths/project metadata may be personal; redact
those if necessary. Never share `ReaperMCP/config.json`, which contains the token.
`reaper-mcp logs` contains bounded CLI error codes, not media or request contents.

| Symptom/code | Cause and next step |
|---|---|
| Command not found | Open a new terminal after Setup, or use the Start menu diagnostic shortcut. The executable is normally under `%LOCALAPPDATA%\Programs\ReaperMCP`. |
| REAPER_NOT_FOUND | Install x64 REAPER or select the portable folder in Setup. |
| RESOURCE_NOT_FOUND | Open REAPER once and close it. For a custom resource folder, select it in Setup or pass `--resource-dir`. |
| UNSUPPORTED_ARCHITECTURE | Use x64 REAPER and the x64 package. Native ARM/x86 are not supported. |
| REAPER_RUNNING / INSTALLER_BUSY | Close REAPER and any other installer; rerun Setup. |
| REAPER_NOT_RUNNING / BRIDGE_NOT_CONNECTED | Open/restart REAPER. If it is already open, confirm REAPER 7.80+ x64 and reinstall with it closed. An older version cannot load the required API set. |
| INVALID_CONFIGURATION / INVALID_MANIFEST | Preserve the diagnostic output and report the issue. Do not paste private configuration or delete user files. |
| MISSING_FILE | Close REAPER and rerun the same/newer installer to repair missing owned files. |
| MODIFIED_FILE / UNOWNED_FILE | A target file differs from the recorded installation. The installer preserves it; identify its origin before replacing anything. |
| CODEX_NOT_FOUND | Install the official Codex CLI and make `codex mcp list` work in the same user account. Rerun Setup. |
| CODEX_CONFLICT | Inspect `codex mcp get reaper-mcp --json`. An entry created/changed elsewhere is preserved. Resolve its ownership before removal or reinstall. |
| PROTOCOL_VERSION_MISMATCH | Close REAPER and reinstall matching Python server and native DLL versions. Restart Codex too. |
| PROJECT_CHANGED | A project tab changed. Inspect the project again and use its new session ID. |
| AMBIGUOUS_TRACK | Read tracks and use the specific GUID, not the duplicated name. |
| BRIDGE_TIMEOUT | An edit may already have happened. Inspect state or use Undo; never automatically repeat an edit. Native rendering can block status temporarily. |
| UNSUPPORTED_CAPABILITY | The running DLL does not expose that method. Check versions and restart REAPER after upgrades. |

Only one REAPER instance may own a given resource directory. A second instance
cannot take over its bridge. This alpha installs one integration per Windows
user. Multi-instance selection is planned.

If Setup reports integration failure, its application files remain available
for repair. In PowerShell, run:

```powershell
& "$env:LOCALAPPDATA\Programs\ReaperMCP\reaper-mcp.exe" install
```

This exposes the concrete CLI error. Fix that condition and rerun Setup.
Uninstallation preserves unrelated files, media, projects and renders. It refuses
to remove a modified managed file or changed Codex entry rather than guessing.
