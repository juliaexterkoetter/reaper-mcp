from pydantic import Field

from reaper_mcp.tools.common import ProjectParams, Spec


class Track(ProjectParams):
    track: str = Field(min_length=1, max_length=1024, description="Track GUID or exact unique name")


class Volume(Track):
    volume_db: float | None = Field(ge=-150, le=24, description="null means silence")
    relative: bool = False


class Pan(Track):
    pan_percent: float = Field(ge=-100, le=100, description="-100 left, 0 center, 100 right")


class Rename(Track):
    name: str = Field(min_length=1, max_length=1024)


class Create(ProjectParams):
    name: str = Field(min_length=1, max_length=1024)


class Delete(Track):
    confirm: bool = False


SPECS = [
    Spec(
        "reaper_list_tracks",
        "tracks.list",
        ProjectParams,
        "read",
        "List tracks with GUIDs, volume in dB and pan percentage.",
    ),
    Spec(
        "reaper_get_track",
        "tracks.get",
        Track,
        "read",
        "Get one track; duplicate names are rejected.",
    ),
    Spec(
        "reaper_set_track_volume",
        "tracks.volume",
        Volume,
        "write",
        "Set dB or apply a relative dB change. null means silence.",
    ),
    Spec(
        "reaper_set_track_pan",
        "tracks.pan",
        Pan,
        "write",
        "Set track pan percentage (-100 left to +100 right).",
    ),
    Spec("reaper_create_track", "tracks.create", Create, "write", "Append a named track."),
    Spec(
        "reaper_delete_track",
        "tracks.delete",
        Delete,
        "destructive",
        "Delete a track and its contents after approval.",
    ),
    Spec("reaper_rename_track", "tracks.rename", Rename, "write", "Rename one track."),
] + [
    Spec(
        f"reaper_{action}_track",
        f"tracks.{action}",
        Track,
        "write",
        f"{action.capitalize()} one track.",
    )
    for action in ("mute", "unmute", "solo", "unsolo")
]
