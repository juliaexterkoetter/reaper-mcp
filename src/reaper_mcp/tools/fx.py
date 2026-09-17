from pydantic import Field

from reaper_mcp.tools.common import Params, Spec
from reaper_mcp.tools.tracks import Track


class FX(Track):
    fx: str = Field(min_length=1, max_length=128, description="FX GUID from track FX list")


class Add(Track):
    plugin: str = Field(
        min_length=1, max_length=4096, description="Exact installed plugin name or identifier"
    )


class Remove(FX):
    confirm: bool = False


class Parameter(FX):
    parameter: int = Field(ge=0, le=100000)


class SetParameter(Parameter):
    normalized_value: float = Field(ge=0, le=1)


class Available(Params):
    query: str = Field(default="", max_length=256)
    offset: int = Field(default=0, ge=0, le=100000)
    limit: int = Field(default=100, ge=1, le=500)


class Parameters(FX):
    offset: int = Field(default=0, ge=0, le=100000)
    limit: int = Field(default=100, ge=1, le=500)


SPECS = [
    Spec(
        "reaper_list_available_fx",
        "fx.available",
        Available,
        "read",
        "Discover installed plugins, with a case-insensitive name filter and pagination.",
    ),
    Spec(
        "reaper_list_track_fx", "fx.list", Track, "read", "List top-level track FX and their GUIDs."
    ),
    Spec("reaper_get_fx", "fx.get", FX, "read", "Inspect one FX by GUID."),
    Spec(
        "reaper_add_fx",
        "fx.add",
        Add,
        "write",
        "Add an exact plugin discovered by reaper_list_available_fx.",
    ),
    Spec(
        "reaper_remove_fx", "fx.remove", Remove, "destructive", "Remove an FX after user approval."
    ),
    Spec(
        "reaper_list_fx_parameters",
        "fx.parameters",
        Parameters,
        "read",
        "List parameter names, normalized values and plugin-formatted values.",
    ),
    Spec("reaper_get_fx_parameter", "fx.parameter", Parameter, "read", "Read one FX parameter."),
    Spec(
        "reaper_set_fx_parameter",
        "fx.set_parameter",
        SetParameter,
        "write",
        "Set a normalized plugin parameter (0..1). Consult formatted values before editing.",
    ),
] + [
    Spec(f"reaper_{action}_fx", f"fx.{action}", FX, "write", f"{action.capitalize()} a track FX.")
    for action in ("enable", "disable", "bypass")
]
