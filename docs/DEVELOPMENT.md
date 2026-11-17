# Development

Python 3.11+ (tested versions are in CI), and REAPER 7.80+ for runtime on
Windows x64 or Linux x86_64.

```
python -m venv .venv
# Activate .venv using your shell
python -m pip install -c packaging/constraints.txt -e ".[dev]"
pytest -q
ruff check src tests scripts
ruff format --check src tests scripts
mypy src
python -m build
```

Native Windows: install Visual Studio 2022/2026 Desktop development with C++, CMake and Git.

```
cmake -S reaper-extension -B build/extension -A x64
cmake --build build/extension --config Release
cmake --install build/extension --config Release --prefix dist/native
```

Native Linux: install CMake, Git and a C++17 compiler. WDL is fetched alongside
the SDK because reaper_plugin.h includes SWELL's headers outside Win32.

```
cmake -S reaper-extension -B build/extension -DCMAKE_BUILD_TYPE=Release
cmake --build build/extension --parallel
cmake --install build/extension --prefix dist/native
```

The module must be named reaper_mcp.so and export only ReaperPluginEntry; the
Linux CI job checks both, because a mismatch only surfaces when REAPER fails to
load the extension.

SDK and nlohmann JSON are fetched by immutable commit; their notices ship with
native artifacts. We only use the permissively licensed SDK headers, not the
sample plugins' implementation code. Windows/MSVC CI is authoritative for DLL
builds and Linux CI for .so builds. A compile pass does not validate REAPER
runtime behavior.

Run `ctest --test-dir build/extension -C Release --output-on-failure` for native
fixture tests. Frozen binary and Setup smoke tests run via
`scripts/build_windows.ps1` after the native install step; they require Codex CLI
on PATH and Inno Setup 6. See packaging/README.md.

Python dependency versions are constrained in packaging/constraints.txt; SDK/JSON
commits are immutable. CI OS images, compiler patches, Inno Setup and Codex CLI
may evolve. Builds are repeatable workflows, not claimed byte-for-byte reproducible.
Release assets include a resolved Python dependency manifest and SHA-256 sums.

A tag `v0.1.0-alpha.1` starts the gated release workflow. It rebuilds and tests all
artifacts before publishing a prerelease. Future releases must update Python,
package-script and Inno versions together. Never label a stable release until
the real host acceptance checklist is completed.
