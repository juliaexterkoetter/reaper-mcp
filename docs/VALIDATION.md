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
| Real host E2E | Opt-in read-only test, acceptance checklist and local MCP runs | Limited actual REAPER behavior; scope below |

Synthetic PE headers used in installer smoke tests are installation fixtures,
not runnable REAPER binaries. No test fixture is reachable from production tools.

CI also runs Ruff lint/format, strict mypy, Python wheel/sdist builds and native
CMake/CTest. The release workflow reruns validation before publishing artifacts.
See [Actions](https://github.com/juliaexterkoetter/reaper-mcp/actions) for exact
commit/run evidence and [Acceptance](ACCEPTANCE.md) for the remaining host checks.

Dependency versions are constrained, and resolved release build dependencies
are attached alongside SHA-256 checksums. Real host compatibility is limited to
the documented x64 REAPER API baseline until acceptance testing expands it.

## Local real-host validation — 2026-11-16

Environment: Windows x64, REAPER 7.84/x64, development Python MCP server and a
freshly compiled DLL copied only into a dedicated REAPER resource profile.
The disposable project was derived from the acceptance fixture and supplied
with a one-second synthetic tone. The personal installation was not replaced.
Each write required the disposable project path, session identity and bridge
PID matching the dedicated process. Undo, save and render used explicit client
approvals under the unchanged `confirm-destructive` policy.

The initial render failure was reproduced through MCP. Temporary native
inspection established a trailing semicolon in a single-entry `RENDER_TARGETS`
list after overrides. Correcting that exposed a second restoration failure:
the host resolved `RENDER_SRATE=0` to 44100. Both have native regression coverage.
The final focused render passed with 44,100 stereo frames, 24-bit PCM, one second
at 44.1 kHz, complete frame payload and nonzero PCM peak/RMS. Its path was inside
the reserved directory and exposed render settings matched their pre-render
values. Native tests separately check all three string overrides and the sample
rate on success and failure. These checks do not certify arbitrary plugins,
formats, cancellation behavior or perceptual audio quality.

Production checks: Ruff lint/format, strict mypy (28 source files), Python tests
(159 passed, 2 skipped), wheel/sdist build, MSVC Release build and all four CTest
executables passed. The native render executable exercises 18 cases. The Python
skips are not host-test passes. Standalone installation on a clean workstation
and broader client/plugin acceptance remain outstanding.

Raw responses, screenshots, logs, disposable media and the local Python QA
executor are intentionally not distributed with this change. Screenshots are
supplemental: repeated hashes can represent an unchanged view; PNG integrity
alone does not prove that a parameter change is visible. Functional claims rely
on actual MCP calls and subsequent state/file verification.

The subsequent full run passed all 33 scenarios in one isolated run:
metadata/reads, volume/pan, mute/unmute, solo/unsolo, track creation/rename,
Undo, save/reopen, item move/split, a Cockos ReaEQ parameter, an existing volume
envelope point, marker creation, invalid-GUID/confirmation checks and WAV
rendering. Takes were inspected only; take mutation is not implemented. The
run recorded no BRIDGE_TIMEOUT; this does not establish that previous timeout
causes are eliminated in all environments. Initial dialogs can block the native
timer, and long renders can still delay status reads.
