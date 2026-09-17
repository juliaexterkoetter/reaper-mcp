First public Windows x64 alpha. Requires REAPER 7.80+ and Codex CLI on PATH.

Download the `windows-x64-setup.exe` asset for installation without Python.
Close REAPER first, run Setup, open REAPER and a saved test project, then open a
new terminal and run `reaper-mcp doctor`. Restart Codex and ask it to list tracks.
Portable REAPER paths can be chosen in Setup. The ZIP is an alternative standalone
bundle: extract the whole folder to a permanent location and run `reaper-mcp.exe install`.

Implemented: authenticated local bridge, projects/tracks/items/takes/transport,
track FX, markers/regions, existing-envelope base points, environment discovery,
isolated experimental render, installer, Codex registration and diagnostics.

CI validates Python, native C++, frozen MCP startup, real Codex CLI registration,
and Setup installation/removal using a synthetic installation fixture. **Real
REAPER/plugin acceptance remains to be performed on the user's Windows machine.**
No production mock or REAPER runtime simulation is included. Render output is
not automatically audio-verified and can require user interaction.

The alpha binaries are unsigned. `SHA256SUMS.txt` covers the distributed files.
Save As, preset application, audio analysis, semantic mixing, video and non-Windows
runtime support are planned. See README and docs/MCP_TOOLS.md for exact boundaries.
