#!/usr/bin/env python3
"""Run board watchdog lifecycle checks using host IOKit test doubles."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
here = Path(__file__).resolve().parent
driver = (root / "package/RamWatchdog.cpp").read_text()
body = "\n".join(line for line in driver.splitlines() if not line.startswith("#include"))
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    source = directory / "watchdog-test.cpp"
    binary = directory / ("watchdog-test.exe" if os.name == "nt" else "watchdog-test")
    source.write_text("\n".join([(here / "shim.h").read_text(), body, (here / "test.cpp").read_text()]))
    subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
