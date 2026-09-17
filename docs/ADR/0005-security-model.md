# 5: security-model

## Context
Tools can destroy work or overwrite files.

## Decision
Typed allowlist, read-only/confirm-destructive/full-control policies; explicit destructive confirmation, native validation.

## Alternatives
Arbitrary actions, eval and shell tools are rejected.

## Consequences
Confirmation flags express client intent, not proof of human consent. Host approval remains required for untrusted agents; read-only is strongest protection.
