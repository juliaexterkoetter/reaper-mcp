# Changelog

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
