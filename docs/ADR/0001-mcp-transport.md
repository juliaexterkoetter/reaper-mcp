# 1: mcp-transport

## Context
Clients need local, automatically started tools.

## Decision
Use the official Python MCP SDK over stdio. Log only to stderr.

## Alternatives
HTTP introduces a separately managed service and authentication surface.

## Consequences
Client owns process lifecycle; no MCP listening port.
