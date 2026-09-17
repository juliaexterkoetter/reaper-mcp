"""Shared typed registration and policy enforcement."""

from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any, Literal, Protocol

from mcp.server import MCPServer
from mcp.server.mcpserver.exceptions import ToolError
from mcp.types import ToolAnnotations
from pydantic import BaseModel, ConfigDict, Field

from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.config import Policy


class Bridge(Protocol):
    async def call(self, method: str, params: dict[str, Any] | None = None) -> Any: ...


class Params(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True, allow_inf_nan=False)


class ProjectParams(Params):
    project_id: str = Field(min_length=1, max_length=128, description="From reaper_get_project")


class ConfirmParams(ProjectParams):
    confirm: bool = False


@dataclass(frozen=True)
class Spec:
    name: str
    method: str
    model: type[Params]
    access: Literal["read", "write", "destructive", "render"]
    description: str


def handler(spec: Spec, bridge: Bridge, policy: Policy) -> Callable[..., Awaitable[Any]]:
    async def invoke(params: Params) -> Any:
        if policy == "read-only" and spec.access != "read":
            raise ToolError("PERMISSION_DENIED: server is read-only")
        if spec.access in ("destructive", "render") and policy != "full-control":
            if not getattr(params, "confirm", False):
                raise ToolError("CONFIRMATION_REQUIRED: obtain user approval and pass confirm=true")
        try:
            return {"result": await bridge.call(spec.method, params.model_dump())}
        except BridgeError as exc:
            raise ToolError(str(exc)) from exc

    invoke.__annotations__["params"] = spec.model
    return invoke


def register(server: MCPServer, bridge: Bridge, policy: Policy, specs: list[Spec]) -> None:
    for spec in specs:
        server.add_tool(
            handler(spec, bridge, policy),
            name=spec.name,
            description=spec.description,
            annotations=ToolAnnotations(
                read_only_hint=spec.access == "read",
                destructive_hint=spec.access == "destructive",
                open_world_hint=False,
            ),
        )
