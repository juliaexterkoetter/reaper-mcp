# Security policy

This is an experimental local integration, currently maintained as an alpha.
There is no production support or security certification. Use the latest tested
prerelease; no older-version support window is promised.

## Reporting

Use the repository's private vulnerability reporting form if enabled:
<https://github.com/juliaexterkoetter/reaper-mcp/security/advisories/new>.
If unavailable, open an issue requesting a private contact without exploit
payloads, secrets or private project data. Never post the bridge token.

## Trust boundary

The MCP client and same-user processes are trusted. The bridge listens only on
127.0.0.1, authenticates every request with a random per-installation token, and
restricts methods to an explicit capability list. The private directory's DACL
grants access to the current Windows user and SYSTEM. This does not defend against
an administrator, malware running as the user, or a compromised MCP client.

No arbitrary shell, ReaScript, generic REAPER action or remote HTTP service is
exposed. REAPER API calls run on the main thread. Frame/peer/depth/time limits
bound bridge work. Loopback flooding may still affect responsiveness; there is
no hard real-time guarantee. Native/plugin bugs can crash a DAW process.

Both layers enforce read-only / confirm-destructive / full-control. Confirmation
is a boolean assertion by the client, not an independent human-approval channel.
Read-only is the appropriate technical barrier to edits. MCP host approval UI
and user review remain relevant. There is no automatic mutation retry.

Files installed into REAPER are tracked by hashes; modified or unowned files are
preserved. Uninstall only removes allowlisted managed paths and a matching owned
Codex registration. It does not delete audio, projects or renders. The installer
rejects direct junction/symlink targets; users must still trust the selected
installation/resource directories and their ancestors.

Render reserves a new directory, validates the predicted target, rejects
unsupported modes, and restores output settings. It remains experimental and
can produce partial output if cancelled. Never infer successful audio content
from file existence. Save overwrites the current project only after confirmation;
Save As is not exposed.

## Privacy and releases

No analytics, telemetry or audio upload is implemented by REAPER MCP. Tool results
include project metadata and are passed to the configured MCP client; that
client's data handling is outside this project's control. Runtime bridge traffic
is local. Dependency installation, Codex itself and release downloads may use
network services. Logs store bounded CLI error codes without parameters/tokens.

Build sources, dependency versions and checksums are published. Binaries are
currently unsigned; checksums do not substitute for a signing identity. Release
publication is gated on CI. Real REAPER acceptance is tracked separately.
