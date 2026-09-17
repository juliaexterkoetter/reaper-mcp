# 7: Installer ownership and private configuration

## Context
Installing should not require manual DLL copying and uninstalling must never
remove files that belong to the user or other extensions. A token is insufficient
if other local accounts can read it.

## Decision
Detect Windows REAPER via registry/default paths or an explicit portable path.
Read PE headers for architecture. Use per-user resource paths, a strict manifest
with SHA-256 ownership records, atomic writes and rollback on installation errors.
Refuse modified or unowned targets. An instance file lock coordinates native
REAPER and installer; byte-range locking excludes concurrent installers.
Set a protected DACL using SetNamedSecurityInfoW, not obsolete SetFileSecurity.
Only the user SID and SYSTEM receive full control. No administrator elevation
is required for normal per-user resource directories.

## Alternatives
Blind overwrite and recursive uninstall are unsafe. Guessing portable locations
by scanning drives is slow and ambiguous. Hand-editing Codex configuration is
unnecessary when the official CLI can manage a named MCP entry.

## Consequences
Modified installations require reconciliation instead of being silently reset.
Resource paths on unsupported filesystems may fail private-DACL setup. An
explicit resource override is available for custom REAPER configurations.
Only one managed resource directory per Windows user is supported initially.

Sources:
- https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-setnamedsecurityinfow
- https://learn.microsoft.com/en-us/windows/win32/api/sddl/nf-sddl-convertstringsecuritydescriptortosecuritydescriptorw
- https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-getsecuritydescriptordacl
