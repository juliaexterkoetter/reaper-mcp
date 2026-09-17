# 1: mcp-transport

## Context
Clients need local, automatically started tools.

## Decision
Use the official Python MCP SDK over stdio. Reserve stdout for protocol messages; SDK runtime diagnostics go to stderr.
CLI administration can also write bounded, credential-free diagnostic events.

## Alternatives
HTTP introduces a separately managed service and authentication surface.

## Consequences
Client owns process lifecycle; no MCP listening port.
