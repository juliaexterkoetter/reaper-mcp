"""Configuration locations; never log secret contents."""

import json
import os
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field

from reaper_mcp.bridge.errors import BridgeError

Policy = Literal["read-only", "confirm-destructive", "full-control"]


class BridgeConfig(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True)
    token: str = Field(pattern=r"^[0-9a-f]{64}$", repr=False)
    policy: Policy = "confirm-destructive"


def data_dir() -> Path:
    return Path(os.environ.get("LOCALAPPDATA", Path.home() / ".local" / "share")) / "ReaperMCP"


def resource_dir() -> Path:
    explicit = os.environ.get("REAPER_MCP_RESOURCE")
    if explicit:
        return Path(explicit)
    try:
        return Path(json.loads((data_dir() / "install.json").read_text())["resource_dir"])
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise BridgeError("NOT_INSTALLED", "Run reaper-mcp install first.") from exc
