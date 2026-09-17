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
render files into the same directory. Restore output settings with RAII.

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
Rendering is experimental until real-host acceptance. English action-description
verification rejects unmatched localized builds rather than guessing. Status
reads can time out while REAPER is busy; retry reads, never the render request.
Filename/directory are intentionally isolated; audio format/rate processing
settings remain current. Restoring settings may leave the project dirty.
Same-user processes are trusted; hostile filesystem races from such processes
are outside the local security boundary. No remote filesystem output is intended.

Sources: official pinned REAPER SDK declarations for GetSetProjectInfo,
GetSetProjectInfo_String, Main_OnCommandEx and kbd_getTextFromCmd; see
https://www.reaper.fm/sdk/reascript/reascripthelp.html .
