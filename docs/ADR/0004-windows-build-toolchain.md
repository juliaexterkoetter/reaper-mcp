# 4: windows-build-toolchain

## Context
Windows x64 is the first runtime target.

## Decision
CMake with MSVC in Windows CI. Pin fetched SDK and JSON dependency.

## Alternatives
MinGW is useful for additional compile validation, not a replacement for MSVC verification.

## Consequences
No SDK binaries redistributed; include dependency license notices in artifacts.
