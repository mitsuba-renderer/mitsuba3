"""
Install build dependencies for stable ABI (abi3) wheel builds.

Reads build-system.requires from pyproject.toml, installs all dependencies,
then force-reinstalls drjit with the abi3 wheel variant.

Usage:
  python install_build_deps_abi3.py                # install all build deps + abi3 drjit
  python install_build_deps_abi3.py --drjit-only   # only force-install abi3 drjit
"""

import subprocess, sys, tempfile, tomllib
from pathlib import Path


def pip(*args):
    subprocess.check_call([sys.executable, "-m", "pip"] + list(args))


def main():
    drjit_only = "--drjit-only" in sys.argv

    pyproject = Path(__file__).resolve().parents[1] / "pyproject.toml"
    cfg = tomllib.loads(pyproject.read_text())
    reqs = cfg["build-system"]["requires"]

    drjit_req = next(r for r in reqs if r.startswith("drjit"))

    if not drjit_only:
        pip("install", *reqs)

    with tempfile.TemporaryDirectory() as tmpdir:
        pip("download", "--only-binary=:all:", "--python-version", "3.999",
            "--no-deps", "-d", tmpdir, drjit_req)
        pip("install", "--force-reinstall", "--no-deps",
            "--no-index", "--find-links", tmpdir, drjit_req)


if __name__ == "__main__":
    main()
