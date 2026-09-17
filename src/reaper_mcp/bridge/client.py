"""Bounded, authenticated loopback client. Mutations are never retried."""

import asyncio
from pathlib import Path
from typing import Any
from uuid import uuid4

from pydantic import ValidationError

from reaper_mcp import PROTOCOL_VERSION
from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.bridge.protocol import MAX_FRAME, Discovery, decode_response, encode_request
from reaper_mcp.config import BridgeConfig, resource_dir


class BridgeClient:
    def __init__(self, resource: Path | None = None, timeout: float = 5.0) -> None:
        self.resource = resource
        self.timeout = timeout

    def settings(self) -> tuple[Discovery, BridgeConfig]:
        root = (self.resource or resource_dir()) / "ReaperMCP"
        try:
            config = BridgeConfig.model_validate_json((root / "config.json").read_bytes())
            discovery = Discovery.model_validate_json((root / "bridge.json").read_bytes())
            return discovery, config
        except FileNotFoundError as exc:
            raise BridgeError(
                "REAPER_NOT_RUNNING", "Open REAPER after installing the extension."
            ) from exc
        except (OSError, ValidationError) as exc:
            raise BridgeError(
                "INVALID_CONFIGURATION", "Run reaper-mcp doctor; reinstall if needed."
            ) from exc

    async def _request(
        self, method: str, params: dict[str, Any], discovery: Discovery, config: BridgeConfig
    ) -> Any:
        request_id = str(uuid4())
        writer = None
        try:
            async with asyncio.timeout(self.timeout):
                reader, writer = await asyncio.open_connection(
                    "127.0.0.1", discovery.port, limit=MAX_FRAME
                )
                writer.write(encode_request(method, params, config.token, request_id))
                await writer.drain()
                return decode_response(await reader.readline(), request_id)
        except TimeoutError as exc:
            raise BridgeError(
                "BRIDGE_TIMEOUT", "Outcome may be unknown. Inspect state; do not retry edits."
            ) from exc
        except OSError as exc:
            raise BridgeError(
                "BRIDGE_NOT_CONNECTED", "Open REAPER or restart it; run reaper-mcp doctor."
            ) from exc
        except ValueError as exc:
            raise BridgeError("INVALID_RESPONSE", "Bridge exceeded the message limit.") from exc
        finally:
            if writer is not None:
                writer.close()
                try:
                    await writer.wait_closed()
                except OSError:
                    pass

    async def call(self, method: str, params: dict[str, Any] | None = None) -> Any:
        discovery, config = self.settings()
        info = await self._request("bridge.info", {}, discovery, config)
        if not isinstance(info, dict) or info.get("protocol_version") != PROTOCOL_VERSION:
            raise BridgeError(
                "PROTOCOL_VERSION_MISMATCH", "Install matching server and extension versions."
            )
        if method == "bridge.info":
            return info
        if method not in info.get("capabilities", []):
            raise BridgeError("UNSUPPORTED_CAPABILITY", f"Extension does not support {method}.")
        return await self._request(method, params or {}, discovery, config)
