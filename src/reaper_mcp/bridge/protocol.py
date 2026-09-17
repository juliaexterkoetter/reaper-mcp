"""Strict internal JSON-RPC framing, independent of MCP revision."""

import json
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field

from reaper_mcp.bridge.errors import BridgeError

MAX_FRAME = 1024 * 1024


class Discovery(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True)
    protocol_version: Literal[1]
    port: int = Field(ge=1, le=65535)
    pid: int = Field(gt=0)


def encode_request(method: str, params: dict[str, Any], token: str, request_id: str) -> bytes:
    data = (
        json.dumps(
            {"jsonrpc": "2.0", "id": request_id, "method": method, "params": params, "auth": token},
            allow_nan=False,
        ).encode()
        + b"\n"
    )
    if len(data) > MAX_FRAME:
        raise BridgeError("REQUEST_TOO_LARGE", "Request exceeds 1 MiB.")
    return data


def decode_response(data: bytes, request_id: str) -> Any:
    try:
        if len(data) > MAX_FRAME or not data.endswith(b"\n"):
            raise ValueError("Invalid frame")
        obj = json.loads(data, parse_constant=lambda s: (_ for _ in ()).throw(ValueError(s)))
        if (
            not isinstance(obj, dict)
            or obj.get("jsonrpc") != "2.0"
            or obj.get("id") != request_id
            or ("result" in obj) == ("error" in obj)
        ):
            raise ValueError("Invalid envelope")
        if "error" in obj:
            error = obj["error"]
            if not isinstance(error["code"], int) or not isinstance(error["message"], str):
                raise ValueError("Invalid error")
            raise BridgeError(error.get("data", {}).get("code", "BRIDGE_ERROR"), error["message"])
        return obj["result"]
    except (ValueError, KeyError, TypeError, AttributeError) as exc:
        raise BridgeError("INVALID_RESPONSE", "Malformed bridge response.") from exc
