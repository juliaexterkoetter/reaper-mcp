# Contributing

Open an issue describing a reproducible problem or a concrete proposal. Include
version/doctor output with private paths redacted; never attach bridge tokens.
See [Development](docs/DEVELOPMENT.md) for build/test commands.

Create a focused branch from `main`: `feat/<area>`, `fix/<area>`, `docs/<area>` or
`test/<area>`. Use Conventional Commits, for example
`fix(bridge): reject oversized responses`. Keep commits reviewable and avoid
mixing independent changes. Push a branch for CI; merge only after review and
applicable checks pass. Never force-push `main`. Remove obsolete merged branches.

A tool change needs strict input validation, native and MCP policy enforcement,
stable identifiers, actionable errors, Undo semantics where appropriate, and
updated capability documentation. Test behavior and failure boundaries, not only
registration. No mocks in production, arbitrary shell/REAPER actions, silent
mutation retries, token logging or hidden network services.

Native API calls must stay on REAPER's main thread. Document official API sources
and significant decisions in an ADR. Do not copy source under incompatible
licenses. Clearly mark features that still require real REAPER acceptance.

Pull requests should state the concrete change, tests run, compatibility impact
and remaining limits. By contributing, you agree to license your original
contribution under this project's MIT license. Third-party licenses remain intact.
