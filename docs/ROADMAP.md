# Roadmap and acceptance status

## Implemented for 0.1.0-alpha.1

Architecture/ADRs; official MCP SDK foundation; test-only mock bridge; Windows
native extension and authenticated local bridge; projects/tracks/items/takes/
transport/FX; markers/regions; existing-envelope points; experimental render;
environment discovery; installer; official Codex registration and doctor;
Python/native/frozen/Setup CI; standalone Windows packaging and release workflow.
The tool catalog currently contains 65 tools, including process health/version.
See MCP_TOOLS.md for exact behavior rather than inferring support from area names.

## Stable-release gate

Complete docs/ACCEPTANCE.md on Windows with real REAPER, Codex and actual plugins.
Record readback, Undo, save/reopen, reconnect and installer lifecycle outcomes.
Audio-item, automation and render acceptance must establish host semantics not
covered by substituted SDK functions. Keep the release marked alpha until this
gate is met; native compilation and fixture tests alone are insufficient.

## Planned

- Save As with explicit overwrite/path policy.
- FX-chain/track-template application, plugin/render presets, master/input/take FX
  and nested FX containers.
- Envelope creation, generic parameter-unit handling and automation-item pools.
- Wider render modes, cancellation/progress, audio-result verification and
  localization beyond the conservatively checked built-in render action.
- LUFS/true-peak/spectrum/silence analysis and opt-in semantic mixing workflows.
- Video after audio workflows are validated.
- macOS/Linux/native ARM, explicit multiple REAPER-instance selection.
- Signed releases and a verified automatic updater; `update` currently provides
  a download link only.

Continue in isolated feature branches, validate and merge before starting the
next feature. Do not expose placeholder tools or describe planned behavior as
supported.
