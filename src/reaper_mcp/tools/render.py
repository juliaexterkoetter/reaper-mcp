from pydantic import Field

from reaper_mcp.tools.common import ConfirmParams, ProjectParams, Spec


class Settings(ProjectParams):
    sample_rate: int = Field(ge=8000, le=384000)
    channels: int = Field(ge=1, le=64)


class Render(ConfirmParams):
    output_directory: str | None = Field(default=None, max_length=32700)


SPECS = [
    Spec(
        "reaper_get_render_settings",
        "render.settings",
        ProjectParams,
        "read",
        "Read current render settings. Rendering supports single master-mix output only.",
    ),
    Spec(
        "reaper_set_render_settings",
        "render.set_settings",
        Settings,
        "write",
        "Set render sample rate and channel count, preserving other settings.",
    ),
    Spec(
        "reaper_render_project",
        "render.start",
        Render,
        "render",
        "Queue a render in a new directory after approval. Inspect status later.",
    ),
    Spec(
        "reaper_get_render_status",
        "render.status",
        ProjectParams,
        "read",
        "Inspect the last render job. Status reads may time out while rendering; retry later.",
    ),
]
