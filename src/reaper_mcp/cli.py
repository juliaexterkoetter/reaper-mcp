"""Accessible command-line administration. MCP stdout is never used for logs."""

import argparse
import json
import sys
from pathlib import Path

from reaper_mcp import PROTOCOL_VERSION, __version__
from reaper_mcp.bridge.errors import BridgeError


def main() -> None:
    parser = argparse.ArgumentParser(prog="reaper-mcp")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("serve")
    commands.add_parser("version")
    setup = commands.add_parser("install")
    setup.add_argument("--reaper-path", type=Path)
    setup.add_argument("--resource-dir", type=Path)
    setup.add_argument("--extension", type=Path)
    setup.add_argument("--policy", choices=["read-only", "confirm-destructive", "full-control"])
    commands.add_parser("uninstall")
    args = parser.parse_args()
    try:
        if args.command == "serve":
            from reaper_mcp.server import serve

            serve()
        elif args.command == "install":
            from reaper_mcp.install.manager import install

            record = install(args.reaper_path, args.resource_dir, args.extension, args.policy)
            print(f"[OK] Native extension installed in {record.resource_dir}. Open REAPER next.")
        elif args.command == "uninstall":
            from reaper_mcp.install.manager import uninstall_files

            print("[OK] Integration removed." if uninstall_files() else "[OK] Not installed.")
        else:
            print(
                json.dumps(
                    {
                        "server": __version__,
                        "protocol": PROTOCOL_VERSION,
                        "runtime": sys.version.split()[0],
                    }
                )
            )
    except BridgeError as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        raise SystemExit(1) from None
    except OSError as exc:
        print(
            f"[ERROR] FILE_OPERATION_FAILED: {exc}. Close REAPER and check permissions.",
            file=sys.stderr,
        )
        raise SystemExit(1) from None


if __name__ == "__main__":
    main()
