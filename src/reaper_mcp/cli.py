"""Accessible command-line administration. MCP stdout is never used for logs."""

import argparse
import asyncio
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
    commands.add_parser("logs")
    commands.add_parser("update")
    for name in ("doctor", "status"):
        commands.add_parser(name).add_argument("--json", action="store_true")
    setup = commands.add_parser("install")
    setup.add_argument("--reaper-path", type=Path)
    setup.add_argument("--resource-dir", type=Path)
    setup.add_argument("--extension", type=Path)
    setup.add_argument("--skip-codex", action="store_true")
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
            if not args.skip_codex:
                from reaper_mcp.install.codex import register

                register(record)
            print(f"[OK] Native extension installed in {record.resource_dir}. Open REAPER next.")
        elif args.command == "uninstall":
            from reaper_mcp.install.manager import uninstall_files

            print("[OK] Integration removed." if uninstall_files() else "[OK] Not installed.")
        elif args.command == "logs":
            from reaper_mcp.config import data_dir

            path = data_dir() / "events.jsonl"
            print(
                path.read_text(encoding="utf-8")
                if path.exists()
                else "No diagnostic events recorded.",
                end="\n",
            )
        elif args.command == "update":
            print("Download and run the newer Windows installer after closing REAPER:")
            print("https://github.com/juliaexterkoetter/reaper-mcp/releases")
        elif args.command in {"doctor", "status"}:
            from reaper_mcp.diagnostics import diagnose

            report = asyncio.run(diagnose())
            if args.json:
                print(json.dumps(report, indent=2))
            else:
                for check in report["checks"]:
                    label = "OK" if check["ok"] else "ERROR"
                    print(f"[{label}] {check['component']}: {check.get('detail', 'available')}")
            raise SystemExit(0 if report["ready"] else 1)
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
        from reaper_mcp.logging_setup import event

        event("cli", exc.code)
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
