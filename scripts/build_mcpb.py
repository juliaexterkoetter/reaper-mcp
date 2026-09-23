from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path, PurePosixPath
from zipfile import ZipFile


def run(*args: str) -> None:
    subprocess.run(args, check=True)


def prepare_manifest(source: Path, destination: Path, version: str) -> None:
    with source.open(encoding="utf-8") as stream:
        manifest = json.load(stream)

    manifest["version"] = version

    destination.write_text(
        json.dumps(manifest, indent=2) + "\n",
        encoding="utf-8",
    )


def validate_archive(path: Path) -> None:
    with ZipFile(path) as archive:
        names = archive.namelist()

        required = {
            "manifest.json",
            "server/reaper-mcp.exe",
            "server/_internal/native/reaper_mcp.dll",
        }

        missing = required.difference(names)

        if missing:
            raise RuntimeError("MCPB is missing required files: " + ", ".join(sorted(missing)))

        for name in names:
            parts = PurePosixPath(name).parts

            if name.startswith("/") or ".." in parts:
                raise RuntimeError(f"Unsafe archive path: {name}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()

    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)

    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    bundle = args.bundle.resolve()
    manifest = args.manifest.resolve()
    output = args.output.resolve()

    executable = bundle / "reaper-mcp.exe"
    internal = bundle / "_internal"

    if not executable.is_file():
        raise FileNotFoundError(executable)

    if not internal.is_dir():
        raise FileNotFoundError(internal)

    if not manifest.is_file():
        raise FileNotFoundError(manifest)

    output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="reaper-mcp-mcpb-") as temp:
        staging = Path(temp)

        prepare_manifest(
            manifest,
            staging / "manifest.json",
            args.version,
        )

        server = staging / "server"
        server.mkdir()

        shutil.copy2(executable, server / "reaper-mcp.exe")
        shutil.copytree(internal, server / "_internal")

        run("mcpb", "validate", str(staging))
        run("mcpb", "pack", str(staging), str(output))

    validate_archive(output)

    print(f"MCPB: {output}")
    print(f"SHA256: {sha256(output)}")


if __name__ == "__main__":
    main()
