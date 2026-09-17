# MCP tools

Tools accept a typed `params` object, except ping/server-info which take no
arguments. Native tools return `{"result": ...}`. Runtime errors are MCP tool
errors with a stable application code in the message. MCP annotations describe
intent; both Python and native code enforce policy independently.

Call `reaper_get_project` first. Pass its `project_id` to project-bound tools;
a tab switch produces PROJECT_CHANGED. Project IDs are process-session scoped,
not persistent project GUIDs. They must not be cached across REAPER restarts.

| Tool | Access | Behavior |
|---|---|---|
| reaper_ping | read | MCP process health; not a REAPER connectivity check |
| reaper_get_server_info | read | MCP server and protocol version |
| reaper_get_project | read | Active project's name, path, session ID and state |
| reaper_list_projects | read | Open project tabs |
| reaper_save_project | destructive | Save existing file with confirm=true |
| reaper_undo | destructive | Native Undo, including edits made outside MCP |
| reaper_redo | destructive | Native Redo |

Saving a never-saved project is deliberately rejected instead of displaying a
Save As dialog. Save As is planned, pending safe path/overwrite handling.

Confirmation is a client assertion after user approval, not cryptographic proof
of consent. Use read-only policy when edits must be technically impossible.

## Tracks

Implemented: list/get, create/delete/rename, set volume/pan, mute/unmute,
solo/unsolo. Exact names are accepted but duplicates return AMBIGUOUS_TRACK;
use returned GUIDs. `volume_db: null` means silence. `relative: true` applies a
delta in dB, e.g. -3. Relative changes from silence are rejected. Pan ranges
from -100 (left) to +100 (right). All changes use native Undo blocks. Track
creation appends to the active project. Deletion requires confirmation.
The master track is not included. Lists have a 2000-track safety ceiling.

## Media items

Implemented: paginated list/get, split/move/trim/delete, volume, fade-in/out.
Times are absolute project seconds except fade durations. Trim keeps only the
specified interval; it uses REAPER's native split operation, preserving MIDI,
source offsets and take playback-rate handling. Trimming the start changes the
retained GUID: always use the returned value. Locked items are rejected. Source
files are never deleted. Trim and delete require confirmation. Fade tools set
manual fades and disable the respective auto-fade. Partial host failures are
reported with an instruction to inspect or Undo; no fake atomic rollback.

## Takes

`reaper_list_takes` and `reaper_get_active_take` accept an item GUID. Results
include take GUID/name, source offset, playback rate and MIDI status. Empty
items have no active take (null); a list is limited to 1000 takes. Take mutation
and source file access are not exposed.

## Transport

Implemented: play, stop, pause, get play state, get/set edit cursor. Cursor
positions use seconds and do not seek playback unless explicitly requested.
Repeated pause calls do not resume playback. Transport is excluded from Undo.
Read-only policy blocks transport changes as well as project edits.

## FX

Implemented: installed-plugin discovery, track FX list/get/add/remove,
enable/disable/bypass, parameter list/get/set. Plugins must match an enumerated
identifier or unique exact name; no FXADD, arbitrary paths or chain loading.
FX instances use GUIDs; parameter indices are checked against current counts.
Parameters use normalized 0..1 because units/scales differ between plugins;
read the plugin-formatted value alongside the normalized value. Bypass and
disable both set REAPER's enabled flag to false. Top-level normal track FX only:
input/master/take FX and nested container children are planned. Plugin loading
can invoke plugin UI and depends on third-party plugin stability.

## Markers and regions

List/create/update/delete are implemented using persistent GUIDs and the modern
REAPER marker API. This release targets REAPER 7.80 or later (the pinned SDK
baseline); older releases missing required APIs cannot load the extension.
Updates provide the complete name/time range; deletion requires confirmation.
Displayed marker numbers are informational, never identity selectors. Marker
colors and lanes are preserved when updating existing markers.

## Automation

List existing track envelopes and read paginated underlying points. Create and
update points in existing built-in volume/pan envelopes using dB/percentage,
with REAPER scaling conversion. Other envelope types are readable; generic FX
units and automation-item pools are not writable in this release. Deletion
operates on base points only and requires confirmation. Update/delete require
`expected_state_version` from the preceding envelope read, so stale indices
fail closed. Re-read after edits because sorting can change indices. Creation
of envelope lanes is planned; tools do not silently create lanes or change modes.

## Render (experimental)

Read settings, set sample rate/channels, queue a render and inspect its status.
Rendering requires approval and reserves a new child directory under the
configured output directory (or `<project>/renders`). The filename is `render`
with the current format's extension. Existing files are never selected as
outputs; original output settings are restored. Supported execution is limited
to one master-mix output, project/custom/time-selection bounds, no secondary
format and no add-to-project. Other modes fail closed. The built-in action is
verified by its English description at runtime; localized descriptions that do
not match are rejected. No arbitrary action tool is exposed.

The initial response is `queued`, not success. Native render may block REAPER's
timer; status calls can time out while busy. Retry read-only status later, never
repeat render.start on uncertainty. `output-produced` means a nonempty file was
observed, not that content/quality was verified; cancellation may leave a partial
file. Audition output. Jobs are session-local. Rendering/preset UX needs real
REAPER acceptance testing. Preset application remains planned.

## Environment

`reaper_get_environment` reports versions, the active project, REAPER resource
location, device sample rate (the native driver's text), and an explicitly
configured project sample rate when present. Query plugins separately with
`reaper_list_available_fx` to avoid returning every installed plugin each time.
`reaper_list_resource_presets` discovers FX-chain and track-template files,
with bounded traversal and no junction/symlink traversal. It does not load them.
Render-preset enumeration/application and plugin-preset navigation are planned;
unsupported data is labelled, never fabricated.
