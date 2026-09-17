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
