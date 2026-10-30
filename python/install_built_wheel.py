#!/usr/bin/env python3
# Install the single wheel that pip just wrote into <build>/python/dist.
# The filename depends on the compiled module's minimum OS version, so it
# cannot be predicted when CMake configures the build.

import pathlib
import subprocess
import sys

def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: install_built_wheel.py <build-dir> <site-packages>\n")
        return 2
    root = pathlib.Path(sys.argv[1])
    sitedir = sys.argv[2]
    wheels = sorted((root / "dist").glob("*.whl"))
    if len(wheels) != 1:
        sys.stderr.write("expected exactly one wheel in %s, found %s\n"
                         % (root / "dist", [w.name for w in wheels]))
        return 1
    (root / "dist" / "wheel-path.txt").write_text(str(wheels[0]))
    subprocess.check_call([
        sys.executable, "-m", "pip", "install",
        "--no-deps", "--no-index", "--upgrade",
        "--disable-pip-version-check",
        "--target", sitedir, str(wheels[0])])
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
