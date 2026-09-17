# Development

Python 3.11+ (tested versions are in CI), Windows x64 REAPER 7 for runtime.

```
python -m venv .venv
# Activate .venv using your shell
python -m pip install -e ".[dev]"
pytest -q
ruff check src tests
ruff format --check src tests
mypy src
python -m build
```

Native Windows: install Visual Studio 2022/2026 Desktop development with C++, CMake and Git.

```
cmake -S reaper-extension -B build/extension -A x64
cmake --build build/extension --config Release
cmake --install build/extension --config Release --prefix dist/native
```

SDK and nlohmann JSON are fetched by immutable commit; their notices ship with
native artifacts. We only use the permissively licensed SDK headers, not the
sample plugins' implementation code. Windows/MSVC CI is authoritative for DLL
builds. A compile pass does not validate REAPER runtime behavior.
