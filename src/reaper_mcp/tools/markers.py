from typing import Self

from pydantic import Field, model_validator

from reaper_mcp.tools.common import ProjectParams, Spec


class MarkerCreate(ProjectParams):
    name: str = Field(min_length=1, max_length=1024)
    position_seconds: float = Field(ge=0, le=8640000)


class MarkerUpdate(MarkerCreate):
    guid: str = Field(min_length=1, max_length=128)


class RegionCreate(MarkerCreate):
    end_seconds: float = Field(gt=0, le=8640000)

    @model_validator(mode="after")
    def ordered(self) -> Self:
        if self.end_seconds <= self.position_seconds:
            raise ValueError("Region end must follow its start")
        return self


class RegionUpdate(RegionCreate):
    guid: str = Field(min_length=1, max_length=128)


class Delete(ProjectParams):
    guid: str = Field(min_length=1, max_length=128)
    confirm: bool = False


SPECS = []
for kind, create, update in [
    ("marker", MarkerCreate, MarkerUpdate),
    ("region", RegionCreate, RegionUpdate),
]:
    SPECS.extend(
        [
            Spec(
                f"reaper_list_{kind}s",
                f"{kind}s.list",
                ProjectParams,
                "read",
                f"List {kind}s by persistent GUID.",
            ),
            Spec(
                f"reaper_create_{kind}",
                f"{kind}s.create",
                create,
                "write",
                f"Create a named {kind} at an absolute project time.",
            ),
            Spec(
                f"reaper_update_{kind}",
                f"{kind}s.update",
                update,
                "write",
                f"Update a {kind} by GUID.",
            ),
            Spec(
                f"reaper_delete_{kind}",
                f"{kind}s.delete",
                Delete,
                "destructive",
                f"Delete a {kind} by GUID after approval.",
            ),
        ]
    )
