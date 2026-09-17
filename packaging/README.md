# Windows packaging

The CI native job builds and tests the x64 DLL, then freezes the Python CLI with
PyInstaller 6.22.3 (one-directory, console/stdio enabled, no UPX). Runtime dependency
metadata and available license notices are bundled. Inno Setup 6 from the Windows
runner builds a per-user installer; no admin or user Python installation is needed.

The executable smoke test uses a synthetic PE header only as an installation
fixture. It does not pretend to execute REAPER. It runs the official Codex CLI in
an isolated CODEX_HOME, installs/reinstalls/uninstalls, initializes the frozen MCP
server with the official client, and verifies that an absent REAPER is an error.
The same script then installs and uninstalls the actual Setup executable silently.

Setup adds its application directory to the user's PATH only when absent and
records ownership of that segment. Open a new terminal afterward. DLL/config
ownership remains with the Python installer. A failed native integration is
reported explicitly and leaves application files available for repair. Uninstall
stops before removing application files if integration cleanup fails.

Sources: [PyInstaller](https://pyinstaller.org/en/stable/man/pyinstaller.html),
[Inno Setup events](https://jrsoftware.org/ishelp/topic_scriptevents.htm).
Unsigned alpha builds may display Windows reputation warnings. No signing
certificate is configured. Checksums establish download consistency, not identity.
