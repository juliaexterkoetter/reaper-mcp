# Repository instructions

Read README and relevant docs before changing behavior. This repository connects
to a real DAW: never substitute fake production responses or claim a host test
that was not executed.

## Architecture

Python uses the official MCP SDK over stdio. Stdout is protocol-only while
serving. Native C++ is a Windows x64 REAPER extension; API calls run exclusively
on the main-thread timer. Bridge methods are explicit allowlisted capabilities.
Mocks, synthetic PE files and substituted SDK functions belong only in tests.

Use GUIDs for tracks/items/takes/FX/markers. Project IDs are session-scoped.
Require current project identity for bound operations. Validate inputs on both
sides, maintain independent policy checks, use native Undo for project edits,
and never retry a timed-out mutation. Never log tokens or request/media content.
Do not introduce arbitrary command/action execution or a remote listener.

## Validation

Run `ruff check src tests scripts`, `ruff format --check src tests scripts`,
`mypy src`, `pytest -q`, and `python -m build` as applicable. Local socket tests
need an environment that permits loopback sockets. Native validation is Windows
MSVC + CTest; use CI when developing on Linux. Packaging changes require the
frozen MCP and Setup lifecycle smoke tests. Real host E2E is opt-in and must
remain explicitly distinguishable from test fixtures.

## Delivery

Use focused feature/fix/docs branches, Conventional Commits and validated merges
into main. No force-push to main. Update docs, changelog and ADRs when behavior or
architecture changes. Keep Supported/Experimental/Planned claims accurate.
Follow existing user authorization for publication; these instructions do not
introduce an additional approval gate. Preserve user files and unrelated Codex
configuration. Do not publish credentials or workspace attachment contents.
