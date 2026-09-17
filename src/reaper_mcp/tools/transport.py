from pydantic import Field

from reaper_mcp.tools.common import ProjectParams, Spec


class Cursor(ProjectParams):
    position_seconds: float = Field(ge=0, le=8640000)
    seek_playback: bool = False


SPECS = [
    Spec(
        f"reaper_{action}",
        f"transport.{action}",
        ProjectParams,
        "write",
        f"{action.capitalize()} playback in the current project.",
    )
    for action in ("play", "stop", "pause")
] + [
    Spec(
        "reaper_get_play_state",
        "transport.state",
        ProjectParams,
        "read",
        "Read playing, paused and recording flags.",
    ),
    Spec(
        "reaper_get_cursor_position",
        "transport.cursor",
        ProjectParams,
        "read",
        "Read the edit cursor in seconds.",
    ),
    Spec(
        "reaper_set_cursor_position",
        "transport.set_cursor",
        Cursor,
        "write",
        "Set edit cursor; optionally seek playback.",
    ),
]
