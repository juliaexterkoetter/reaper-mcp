# 6: Render isolation and honest completion reporting

## Context
Rendering can block the REAPER main thread, display plugin dialogs and overwrite
files. A socket timeout must never cause automatic resubmission of a render.
The official API exposes render settings and target enumeration, while project
rendering is triggered by a built-in action.

## Decision
Queue one session-local job and return before invoking the host. Verify the
built-in action's description at runtime. Permit only single master-mix output
with project/custom/time-selection bounds and no secondary format or insertion
into the project. Reserve a new output directory, use a fixed basename, and
validate the actual target is within it and absent. Temporarily redirect extra
render files into the same directory. Restore output settings and the original
render sample rate with RAII.

Report `queued`, `running`, `output-produced` or `failed`. A nonempty output is
not proof of complete or correct audio; cancellation can leave a partial file.
Never label that observation as a verified successful render. The native timer
has a reentrancy guard because render/plugin dialogs may pump Windows messages.

## Alternatives
A worker thread must not call the REAPER API. A separate REAPER process would
need a saved snapshot and could load different plugin state. Blindly issuing an
action and claiming completion would hide overwrite, cancellation and timeout
failures. Preserving arbitrary output wildcards would make target validation
more difficult.

## Consequences
Rendering remains experimental despite limited local real-host acceptance.
English action-description
verification rejects unmatched localized builds rather than guessing. Status
reads can time out while REAPER is busy; retry reads, never the render request.
Filename/directory are intentionally isolated; audio format/rate processing
settings remain current during rendering. The action can resolve an automatic
sample rate (0) to the device rate; restore that field afterward too. Restoring
settings may leave the project dirty.
Same-user processes are trusted; hostile filesystem races from such processes
are outside the local security boundary. No remote filesystem output is intended.

Sources: official pinned REAPER SDK declarations for GetSetProjectInfo,
GetSetProjectInfo_String, Main_OnCommandEx and kbd_getTextFromCmd; see
https://www.reaper.fm/sdk/reascript/reascripthelp.html .

## Real-host findings (2026-11-16)

On REAPER 7.84/x64, native inspection after directory/pattern overrides observed
one target followed by a semicolon. The old check rejected any semicolon as
multiple files, producing `UNSAFE_RENDER_TARGET` before the action. Accept at
most one final list terminator, then require a nonempty single entry and apply
the existing reserved-directory and no-overwrite checks. Do not discard
arbitrary empty entries or accept additional targets.

A subsequent real render produced a WAV but changed `RENDER_SRATE` from 0 to
44100. Preserve its original value alongside `RENDER_FILE`, `RENDER_PATTERN`
and `RENDER_EXTRAFILEDIR`. Native regressions verify restoration after successful
output, rejected targets, failed reads/setters, no output and action exceptions;
they also cover automatic and explicit sample rates.

The corrected host scenario verified a one-second stereo 24-bit PCM WAV at
44.1 kHz, complete frame data, nonzero samples, a reserved local destination and
MCP readback of restored settings. This does not change the public
`output-produced` contract into a guarantee of complete or correct audio.
No general audio-quality certification or audition is claimed. Native diagnostic
instrumentation was temporary and is not delivered; no mutating ReaScript was used.
