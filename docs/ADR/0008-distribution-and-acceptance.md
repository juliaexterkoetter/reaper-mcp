# ADR 0008: Standalone distribution and explicit acceptance boundary

Status: accepted for alpha.

Distribute a console-enabled PyInstaller one-directory bundle containing the
Python runtime, MCP server and native DLL. Preserve stdin/stdout for MCP. Inno
Setup installs per-user and invokes the same CLI installation/removal logic,
which owns the REAPER files and Codex registration. No manual DLL copy or Python
installation is part of the end-user workflow. Use the official Codex CLI,
not direct TOML editing, and preserve foreign/modified entries.

CI builds MSVC x64, runs native fixtures and Python checks, then tests the frozen
server through an actual MCP client and the official Codex CLI with isolated
configuration. It also runs the real Setup executable. A synthetic installation
fixture only tests detection/ownership; it does not establish host compatibility.
Real REAPER/plugin acceptance remains an explicit gate before a stable release.

Publish unsigned alpha artifacts with SHA-256 sums, dependency metadata and
license notices. Runtime versions are constrained; platform/compiler tools may
change. Do not claim bit-identical reproducibility or code-signing guarantees.
The `update` command points to releases; unattended updating is deferred until
signature verification and rollback semantics are designed.

Sources: [PyInstaller usage](https://pyinstaller.org/en/stable/man/pyinstaller.html),
[Inno Setup events](https://jrsoftware.org/ishelp/topic_scriptevents.htm),
[official Codex MCP configuration](https://developers.openai.com/codex/mcp).
