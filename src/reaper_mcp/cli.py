"""Accessible command-line administration."""

import argparse
import json
import sys

from reaper_mcp import PROTOCOL_VERSION, __version__


def main() -> None:
    parser = argparse.ArgumentParser(prog="reaper-mcp")
    parser.add_argument("command", choices=["serve", "version"])
    args = parser.parse_args()
    if args.command == "serve":
        from reaper_mcp.server import serve

        serve()
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


if __name__ == "__main__":
    main()
