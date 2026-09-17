from reaper_mcp.tools.common import ConfirmParams, Params, Spec

SPECS = [
    Spec(
        "reaper_get_project",
        "project.get",
        Params,
        "read",
        "Inspect the active project and its session ID.",
    ),
    Spec(
        "reaper_list_projects",
        "project.list",
        Params,
        "read",
        "List open project tabs; IDs are session-scoped.",
    ),
    Spec(
        "reaper_save_project",
        "project.save",
        ConfirmParams,
        "destructive",
        "Save the existing project file. Requires confirmation; unsaved projects are rejected.",
    ),
    Spec(
        "reaper_undo",
        "project.undo",
        ConfirmParams,
        "destructive",
        "Undo the last native REAPER edit, which may be a manual edit.",
    ),
    Spec(
        "reaper_redo",
        "project.redo",
        ConfirmParams,
        "destructive",
        "Redo the last native REAPER edit.",
    ),
]
