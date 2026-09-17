from typing import Literal

from pydantic import Field

from reaper_mcp.tools.common import Spec
from reaper_mcp.tools.tracks import Track


class Envelope(Track):
    envelope: str = Field(min_length=1, max_length=128, description="Envelope GUID")


class EnvelopeRead(Envelope):
    offset: int = Field(default=0, ge=0, le=10000000)
    limit: int = Field(default=200, ge=1, le=1000)


class PointCreate(Envelope):
    time_seconds: float = Field(ge=0, le=8640000)
    value: float | None = Field(ge=-150, le=100)
    unit: Literal["db", "pan_percent"]
    shape: int = Field(default=0, ge=0, le=5)
    tension: float = Field(default=0, ge=-1, le=1)


class PointUpdate(PointCreate):
    point_index: int = Field(ge=0, le=10000000)
    expected_state_version: int = Field(ge=0)


class PointDelete(Envelope):
    point_index: int = Field(ge=0, le=10000000)
    expected_state_version: int = Field(ge=0)
    confirm: bool = False


SPECS = [
    Spec(
        "reaper_list_envelopes",
        "automation.list",
        Track,
        "read",
        "List existing track envelopes and GUIDs.",
    ),
    Spec(
        "reaper_get_envelope",
        "automation.get",
        EnvelopeRead,
        "read",
        "Read paginated base-envelope points and project state version.",
    ),
    Spec(
        "reaper_create_automation_point",
        "automation.create",
        PointCreate,
        "write",
        "Create a point in an existing built-in volume or pan envelope using dB or percentage.",
    ),
    Spec(
        "reaper_update_automation_point",
        "automation.update",
        PointUpdate,
        "write",
        "Update a volume/pan point only if the project state still matches the read snapshot.",
    ),
    Spec(
        "reaper_delete_automation_point",
        "automation.delete",
        PointDelete,
        "destructive",
        "Delete a base-envelope point after approval and a state-version check.",
    ),
]
