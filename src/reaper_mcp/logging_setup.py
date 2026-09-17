"""Bounded diagnostic events without tokens, parameters, media, or project names."""

import json
from datetime import UTC, datetime

from reaper_mcp.bridge.errors import BridgeError
from reaper_mcp.config import data_dir


def event(component: str, code: str) -> None:
    root = data_dir()
    if not (root / "install.json").is_file():
        return
    path = root / "events.jsonl"
    try:
        from reaper_mcp.install.security import reject_link

        reject_link(path)
        if path.exists() and path.stat().st_size > 1_000_000:
            backup = root / "events.previous.jsonl"
            reject_link(backup)
            path.replace(backup)
        with path.open("a", encoding="utf-8") as out:
            out.write(
                json.dumps(
                    {"time": datetime.now(UTC).isoformat(), "component": component, "code": code}
                )
                + "\n"
            )
    except (OSError, BridgeError):
        pass
