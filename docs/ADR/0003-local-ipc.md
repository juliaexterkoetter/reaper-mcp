# 3: local-ipc

## Context
Python and native REAPER must exchange bounded local messages.

## Decision
JSON-RPC 2.0, newline UTF-8 frames, ephemeral 127.0.0.1 TCP, installation token.

## Alternatives
Named pipes provide ACLs but require platform-specific asynchronous Python IO. Fixed ports conflict.

## Consequences
Private configuration and discovery files; bounded nonblocking polling, deadlines, no automatic mutation retries. One resource directory/instance initially.
