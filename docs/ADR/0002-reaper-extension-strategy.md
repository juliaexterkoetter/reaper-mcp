# 2: reaper-extension-strategy

## Context
Users must not register scripts or run an action manually.

## Decision
Load a native C++ extension with REAPER. Resolve APIs from the official SDK.

## Alternatives
ReaScript bootstrap and GUI automation add setup and lifecycle fragility.

## Consequences
Windows native builds required. All API calls run in the main-thread timer.
