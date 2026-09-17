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
