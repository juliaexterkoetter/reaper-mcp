from typing import Self

from pydantic import Field, model_validator

from reaper_mcp.tools.common import ProjectParams, Spec


class Item(ProjectParams):
    item: str = Field(min_length=1, max_length=128, description="Media item GUID")


class ItemList(ProjectParams):
    offset: int = Field(default=0, ge=0)
    limit: int = Field(default=200, ge=1, le=1000)


class Position(Item):
    position_seconds: float = Field(ge=0, le=8640000)


class Trim(Item):
    start_seconds: float = Field(ge=0, le=8640000)
    end_seconds: float = Field(gt=0, le=8640000)
    confirm: bool = False

    @model_validator(mode="after")
    def ordered_interval(self) -> Self:
        if self.end_seconds <= self.start_seconds:
            raise ValueError("end_seconds must be greater than start_seconds")
        return self


class Delete(Item):
    confirm: bool = False


class Volume(Item):
    volume_db: float | None = Field(ge=-150, le=24)
    relative: bool = False


class Fade(Item):
    seconds: float = Field(ge=0, le=8640000)


SPECS = [
    Spec("reaper_list_items", "items.list", ItemList, "read", "List media items with pagination."),
    Spec("reaper_get_item", "items.get", Item, "read", "Inspect an item by GUID."),
    Spec(
        "reaper_split_item",
        "items.split",
        Position,
        "write",
        "Split at an absolute project time; returns left and right GUIDs.",
    ),
    Spec(
        "reaper_move_item",
        "items.move",
        Position,
        "write",
        "Move to absolute project time in seconds.",
    ),
    Spec(
        "reaper_trim_item",
        "items.trim",
        Trim,
        "destructive",
        "Keep the absolute time interval using native splits. Returns the retained GUID.",
    ),
    Spec(
        "reaper_delete_item",
        "items.delete",
        Delete,
        "destructive",
        "Delete an item after approval; source media is preserved.",
    ),
    Spec(
        "reaper_set_item_volume",
        "items.volume",
        Volume,
        "write",
        "Set or adjust item volume in dB.",
    ),
    Spec(
        "reaper_set_item_fade_in",
        "items.fade_in",
        Fade,
        "write",
        "Set manual fade-in seconds; disables automatic fade-in.",
    ),
    Spec(
        "reaper_set_item_fade_out",
        "items.fade_out",
        Fade,
        "write",
        "Set manual fade-out seconds; disables automatic fade-out.",
    ),
]
