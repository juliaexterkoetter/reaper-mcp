# Validation scope

The automated suite is intentionally separated into different evidence levels.
A passing fixture test is not described as a real REAPER test.

| Level | What runs | What it establishes |
|---|---|---|
| Python unit | Models, policies, catalog routing, units, paths, installer ownership/rollback, Codex conflicts, diagnostics | Validation and application behavior under controlled inputs |
| Python integration | Actual loopback sockets and actual MCP stdio subprocess/client | Framing, authentication, timeouts, error handling, protocol startup |
| Native CTest | Windows MSVC binaries with substituted SDK function pointers | Bridge lifecycle and adapter orchestration, item trim, render isolation |
| Frozen executable | Real packaged executable + official MCP client and installed Codex CLI | Bundle completeness, registration, reinstall, diagnosis and cleanup |
| Setup lifecycle | Actual compiled Inno installer and uninstaller on Windows CI | Distribution installation/removal with paths containing spaces |
| Real host E2E | Opt-in read-only test and manual acceptance checklist | Actual REAPER/plugin behavior; outstanding on the user's workstation |

Synthetic PE headers used in installer smoke tests are installation fixtures,
not runnable REAPER binaries. No test fixture is reachable from production tools.

CI also runs Ruff lint/format, strict mypy, Python wheel/sdist builds and native
CMake/CTest. The release workflow reruns validation before publishing artifacts.
See [Actions](https://github.com/juliaexterkoetter/reaper-mcp/actions) for exact
commit/run evidence and [Acceptance](ACCEPTANCE.md) for the remaining host checks.

Dependency versions are constrained, and resolved release build dependencies
are attached alongside SHA-256 checksums. Real host compatibility is limited to
the documented x64 REAPER API baseline until acceptance testing expands it.
