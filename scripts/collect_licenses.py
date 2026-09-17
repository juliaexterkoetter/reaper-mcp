"""Collect notices for the installed runtime dependency closure before freezing."""

import shutil
import sys
from importlib.metadata import distribution
from pathlib import Path

from packaging.requirements import Requirement

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
seen: set[str] = set()


def collect(name: str) -> None:
    dist = distribution(name)
    key = dist.metadata["Name"]
    if key in seen:
        return
    seen.add(key)
    dest = root / key
    dest.mkdir(exist_ok=True)
    (dest / "METADATA.txt").write_text(dist.read_text("METADATA") or "", encoding="utf-8")
    for entry in dist.files or []:
        if any(word in entry.name.lower() for word in ("license", "copying", "notice")):
            source = Path(dist.locate_file(entry))
            if source.is_file():
                shutil.copy2(source, dest / entry.name)
    for text in dist.requires or []:
        requirement = Requirement(text)
        if requirement.marker is None or requirement.marker.evaluate({"extra": ""}):
            collect(requirement.name)


collect("reaper-mcp")
collect("pyinstaller")
python_license = Path(sys.base_prefix) / "LICENSE.txt"
if python_license.is_file():
    shutil.copy2(python_license, root / "Python-LICENSE.txt")
